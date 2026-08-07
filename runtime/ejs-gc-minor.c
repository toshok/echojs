/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

// the generational nursery: young pages over the seam cursors, the
// mostly-copying minor collection (pin, evacuate, forward, rewrite),
// and the remembered-set discipline.

#include "ejs-gc-internal.h"

EJSHeapContext _ejs_heap; // exported: the per-isolate context (the emitter seam)

EJSBool nursery_enabled; // EJS_GC_NURSERY=off selects the single-generation collector
EJSBool in_minor_gc;     // the shared mark helpers dispatch on this

EJSHeapPriv heap_priv; // the private half of the (single) isolate's context

#define NURSERY_REMSET_CAPACITY (64 * 1024)

// EJS_GC_MINOR_SPEW=1: per-event tracing for nursery debugging
static EJSBool minor_spew;
#define MINOR_SPEW(...) EJS_MACRO_START if (minor_spew) _ejs_log (__VA_ARGS__); EJS_MACRO_END

// the seam cursors are authoritative while a page is being bumped; fold
// them back into the page before any collection looks at bump_ptr
static void
young_flush_bumps(void)
{
    for (int i = 0; i < EJS_GC_NUM_SIZE_CLASSES; i++) {
        if (heap_priv.young_current[i])
            heap_priv.young_current[i]->bump_ptr = _ejs_heap.bump[i];
    }
}

static void
young_page_retire_current(int idx)
{
    PageInfo* page = heap_priv.young_current[idx];
    if (!page) return;
    page->bump_ptr = _ejs_heap.bump[idx];
    _ejs_list_append_node (&heap_priv.young_pages, (EJSListNode*)page);
    heap_priv.young_current[idx] = NULL;
    _ejs_heap.bump[idx] = _ejs_heap.limit[idx] = NULL;
}

// grab a fresh page from the nursery arena for class idx, or NULL when
// the nursery is exhausted (the caller runs a minor collection)
static PageInfo*
young_page_install(int idx, size_t cell_size)
{
    Arena* arena = heap_priv.nursery_arena;
    PageInfo* info = NULL;
    if (in_minor_gc) {
        _ejs_log ("GC BUG: young_page_install during a minor collection\n");
        abort();
    }
    if (arena->free_pages) {
        info = arena->free_pages;
        EJS_LIST_DETACH(info, arena->free_pages);
        info->cell_size = cell_size;
        info->num_cells = CELLS_OF_SIZE(cell_size);
        info->num_free_cells = info->num_cells;
    } else {
        info = alloc_page_from_arena(arena, cell_size);
        if (!info) return NULL;
    }
    info->young = 1;
    info->bump_ptr = info->page_start;
    heap_priv.young_alloced += PAGE_SIZE;
    // colors start at the CURRENT white (a young cell must never read
    // as black mid-cycle); allocated-ness comes from the bump rule
    memset (info->page_bitmap, cell_white_color(), info->num_cells * sizeof(BitmapCell));
    heap_priv.young_current[idx] = info;
    _ejs_heap.bump[idx] = info->page_start;
    _ejs_heap.limit[idx] = info->page_end;
    return info;
}

// an emptied young page leaves heap_priv.young_pages for the nursery
// arena's free list (called from _ejs_finalize_obj when a full sweep
// kills a survivor page's last cell)
void
young_page_freed(PageInfo* info, Arena* arena)
{
    EJS_ASSERT(arena && arena->is_nursery);
    _ejs_list_detach_node (&heap_priv.young_pages, (EJSListNode*)info);
    info->young = 0;
    info->bump_ptr = info->page_start;
    EJS_LIST_PREPEND (info, arena->free_pages);
}

// set when a scan leaves a still-young (pinned) referent behind — the
// dirty owner carries to the next cycle
EJSBool minor_scan_saw_young;

// identity-hash assignment counter (bits live in each object's header
// and move with it; see ejs-gc.h)
uint32_t _ejs_gc_idhash_next;

void
minor_wl_push(GCObjectPtr p)
{
    if (heap_priv.wl_count == heap_priv.wl_cap) {
        heap_priv.wl_cap = heap_priv.wl_cap ? heap_priv.wl_cap * 2 : 4096;
        heap_priv.wl = realloc (heap_priv.wl, heap_priv.wl_cap * sizeof(GCObjectPtr));
    }
    heap_priv.wl[heap_priv.wl_count++] = p;
}

// After memcpy'ing a cell, SELF-INTERIOR pointers still aim at the old
// cell (an inline-buffer flat string's data would point at the swept
// original after promotion).  The two classes in the runtime:
// flat strings without an out-of-line buffer (data.flat = self+hdr) and
// small EJSArguments (args = self+sizeof).  Anything new that embeds a
// self-pointer must be added here — the planned trace-bitmap redesign
// subsumes this with offset-based addressing.
void
minor_fixup_evacuated(GCObjectPtr from, GCObjectPtr to, size_t cell_size)
{
    GCObjectHeader h = *(GCObjectHeader*)to;
    if (h & EJS_SCAN_TYPE_PRIMSTR) {
        EJSPrimString* s = (EJSPrimString*)to;
        if (EJS_PRIMSTR_GET_TYPE(s) == EJS_STRING_FLAT) {
            char* d = (char*)s->data.flat;
            if (d >= (char*)from && d < (char*)from + cell_size)
                s->data.flat = (jschar*)((char*)to + (d - (char*)from));
        }
    }
    else if (h & EJS_SCAN_TYPE_OBJECT) {
        EJSObject* o = (EJSObject*)to;
        if (o->ops == &_ejs_Arguments_specops) {
            EJSArguments* a = (EJSArguments*)o;
            char* d = (char*)a->args;
            if (d >= (char*)from && d < (char*)from + cell_size)
                a->args = (ejsval*)((char*)to + (d - (char*)from));
        }
        // shaped ordinary objects with EMBEDDED slot storage (single-cell
        // allocation): the slots ejsval points into the
        // cell.  Shape bits are only ever set on ordinary objects, so
        // the header test suffices; dictionary mode (shape 0) keeps
        // the map pointer in the union and must not be touched.
        else if (((h >> EJS_GC_HEADER_SHAPE_SHIFT) & EJS_GC_HEADER_SHAPE_MASK)
                     != EJS_SHAPE_DICT
                 && !EJSVAL_IS_NULL(o->slots)) {
            char* d = (char*)EJSVAL_TO_CLOSUREENV_IMPL(o->slots);
            if (d >= (char*)from && d < (char*)from + cell_size)
                rewrite_slot_payload(&o->slots,
                                     (GCObjectPtr)((char*)to + (d - (char*)from)));
        }
    }
}

// allocate an old-gen cell for a promotion.  Never triggers collection
// (we are inside one); grows a new arena if need be, aborts loudly on
// genuine OOM.
static GCObjectPtr
old_alloc_cell_for_promotion(size_t cell_size)
{
    int bucket = ffs((int)cell_size) - OBJECT_SIZE_LOW_LIMIT_BITS;

    // O(1) page selection, mirroring _ejs_gc_alloc: use the head page or
    // mint a new one, and rotate pages to the tail as they fill.  A
    // promotion storm never rescans full pages (the old linear walk here
    // was quadratic across a storm); partially-freed interior pages are
    // picked up again after compaction, same as the mutator path.
    PageInfo* info = (PageInfo*)heap_pages[bucket].head;
    if (!info || !info->num_free_cells) {
        info = alloc_new_page(cell_size);
        if (info == NULL) {
            _ejs_log ("gc: promotion allocation failed (size %zd)\n", cell_size);
            abort();
        }
        _ejs_list_prepend_node (&heap_pages[bucket], (EJSListNode*)info);
    }
    GCObjectPtr rv = alloc_from_page(info);
    if (info->num_free_cells == 0
        && heap_pages[bucket].head != heap_pages[bucket].tail) {
        _ejs_list_pop_head (&heap_pages[bucket]);
        _ejs_list_append_node (&heap_pages[bucket], (EJSListNode*)info);
    }
    return rv;
}

// ---- sticky-pin cache ---------------------------------------------
//
// A suspended generator's stack (and its saved ucontexts) are frozen,
// so its conservative hit set is identical from one minor to the next.
// The first scan after suspension captures every young hit here; later
// minors replay the pins in O(pins) instead of walking the whole stack
// segment word by word.  The oracle-class workload this pays for:
// thousands of live suspended generators otherwise rescanned per minor
// (measured: 3,698 generators, ~3ms of every 7.4ms pause).
//
// Replayed pins go through minor_conservative_hit like any other hit,
// so worklist membership and content rescans are identical to a real
// scan; only the word-walk is skipped.  Captured entries record hits
// REGARDLESS of the already-black dedup (another stack may have pinned
// first this cycle but be gone the next).
typedef struct {
    uint32_t count;
    uint32_t capacity;
    EJSBool valid;
    struct { PageInfo* page; uint32_t cell_idx; } hits[];
} PinCache;

static PinCache** pin_capture; // non-NULL while capturing a scan

static void
pin_cache_append(PinCache** cachep, PageInfo* page, uint32_t cell_idx)
{
    PinCache* c = *cachep;
    if (!c || c->count == c->capacity) {
        uint32_t newcap = c ? c->capacity * 2 : 16;
        c = realloc (c, sizeof(PinCache) + newcap * sizeof(c->hits[0]));
        if (!*cachep) { c->count = 0; c->valid = EJS_FALSE; }
        c->capacity = newcap;
        *cachep = c;
    }
    c->hits[c->count].page = page;
    c->hits[c->count].cell_idx = cell_idx;
    c->count++;
}

EJSBool
_ejs_gc_pin_cache_replay(void** cache)
{
    if (!in_minor_gc)
        return EJS_FALSE;
    PinCache* c = (PinCache*)*cache;
    if (!c || !c->valid)
        return EJS_FALSE;
    for (uint32_t i = 0; i < c->count; i++)
        minor_conservative_hit (c->hits[i].page, c->hits[i].cell_idx);
    return EJS_TRUE;
}

void
_ejs_gc_pin_cache_begin(void** cache)
{
    if (!in_minor_gc)
        return;
    PinCache* c = (PinCache*)*cache;
    if (c) {
        c->count = 0;
        c->valid = EJS_FALSE;
    }
    pin_capture = (PinCache**)cache;
}

void
_ejs_gc_pin_cache_end(void)
{
    if (!pin_capture)
        return;
    if (*pin_capture)
        (*pin_capture)->valid = EJS_TRUE;
    else {
        // a scan with zero hits still caches (the common tail-call case)
        pin_cache_append (pin_capture, NULL, 0);
        (*pin_capture)->count = 0;
        (*pin_capture)->valid = EJS_TRUE;
    }
    pin_capture = NULL;
}

void
_ejs_gc_pin_cache_invalidate(void** cache)
{
    PinCache* c = (PinCache*)*cache;
    if (c)
        c->valid = EJS_FALSE;
}

void
_ejs_gc_pin_cache_free(void** cache)
{
    free (*cache);
    *cache = NULL;
}

// conservative hit during a minor collection: young targets pin in
// place (never move this cycle) and join the scan worklist once; old
// targets are not this collection's problem
void
minor_conservative_hit(PageInfo* page, uint32_t cell_idx)
{
    if (!page->young) return;
    if (page->young == 1 && !young_cell_is_allocated(page, cell_idx)) return;
    if (page->young == 2 && cell_is_free(page->page_bitmap[cell_idx])) return;
    GCObjectPtr base = page->page_start + ((size_t)cell_idx * page->cell_size);
    if (_ejs_gc_is_forwarded(base)) return; // pins precede evacuation; stale hit
    if (pin_capture)
        pin_cache_append (pin_capture, page, cell_idx);
    BitmapCell cell = page->page_bitmap[cell_idx];
    if (cell_is_black(cell)) return; // already pinned this minor
    cell_set_black(&page->page_bitmap[cell_idx]);
    heap_priv.minor_pins++;
    MINOR_SPEW("minor: pin %p\n", base);
    gc_watch_hit ("pin", base);
    minor_wl_push(base);
}

// how many young referents the current minor's precise frame walk
// EVACUATED (as opposed to found pinned/forwarded/old) — the direct
// measure that precision is actually moving things (EJS_GC_PROFILE)
static uint64_t gc_frame_moves;

// the minor collection's slot callback (the slot-protocol payoff: every precise
// scan — roots, modules, remset, transitive object scan — goes through
// here).  Young referents evacuate (or stay pinned); the slot is
// rewritten to the object's final address.
static void
minor_process_slot(ejsval* slot)
{
    ejsval v = *slot;
    if (!EJSVAL_IS_TRACEABLE_IMPL(v)) return;
    GCObjectPtr p = (GCObjectPtr)EJSVAL_TO_GCTHING_IMPL(v);
    if (p == NULL || !_ejs_gc_is_young(p)) return;

    uint32_t cell_idx;
    PageInfo* page = find_page_and_cell(p, &cell_idx);
    EJS_ASSERT(page && page->young);
    GCObjectPtr base = page->page_start + ((size_t)cell_idx * page->cell_size);

    if (_ejs_gc_is_forwarded(base)) {
        rewrite_slot_payload(slot, _ejs_gc_forwarding_addr(base));
        return;
    }
    if (cell_is_black(page->page_bitmap[cell_idx])) {
        // pinned: stays put, already queued for scanning.  The current
        // owner must stay dirty so the edge is revisited next cycle.
        minor_scan_saw_young = EJS_TRUE;
        return;
    }

    // evacuate: copy the whole cell, clear YOUNG on the copy (it is
    // promoted), forward the old cell, rewrite this slot
    GCObjectPtr to = old_alloc_cell_for_promotion(page->cell_size);
    memcpy (to, base, page->cell_size);
    // promoted: not young; and not DIRTY — the memcpy'd bit would make
    // the carry logic think the copy is already queued (it is not)
    *(GCObjectHeader*)to &= ~(EJS_GC_HEADER_YOUNG | EJS_GC_HEADER_DIRTY);
    minor_fixup_evacuated(base, to, page->cell_size);
    _ejs_gc_forward(base, to);
    rewrite_slot_payload(slot, to);
    gc_watch_hit ("evacuate-from", base);
    MINOR_SPEW("minor: evac %p -> %p (hdr %llx)\n", base, to, (unsigned long long)*(GCObjectHeader*)to);
    heap_priv.promoted_objs++;
    heap_priv.promoted_bytes += page->cell_size;
    minor_wl_push(to);
}

// evacuate/pin-resolve a RAW GC pointer field (rope/dependent string
// children — the only raw object->object pointers in the heap)
static void
minor_process_primstr_child(EJSPrimString** childp)
{
    GCObjectPtr p = (GCObjectPtr)*childp;
    if (p == NULL || !_ejs_gc_is_young(p)) return;
    uint32_t cell_idx;
    PageInfo* page = find_page_and_cell(p, &cell_idx);
    EJS_ASSERT(page && page->young);
    GCObjectPtr base = page->page_start + ((size_t)cell_idx * page->cell_size);
    if (_ejs_gc_is_forwarded(base)) {
        *childp = (EJSPrimString*)_ejs_gc_forwarding_addr(base);
        return;
    }
    if (cell_is_black(page->page_bitmap[cell_idx])) { minor_scan_saw_young = EJS_TRUE; return; }
    GCObjectPtr to = old_alloc_cell_for_promotion(page->cell_size);
    memcpy (to, base, page->cell_size);
    // promoted: not young; and not DIRTY — the memcpy'd bit would make
    // the carry logic think the copy is already queued (it is not)
    *(GCObjectHeader*)to &= ~(EJS_GC_HEADER_YOUNG | EJS_GC_HEADER_DIRTY);
    minor_fixup_evacuated(base, to, page->cell_size);
    _ejs_gc_forward(base, to);
    *childp = (EJSPrimString*)to;
    MINOR_SPEW("minor: evac-child %p -> %p\n", base, to);
    heap_priv.promoted_objs++;
    heap_priv.promoted_bytes += page->cell_size;
    minor_wl_push(to);
}

// scan one object's outgoing edges with minor_process_slot — the exact
// shape of process_worklist's dispatch, on the slot-based protocol
static void
minor_scan_object(GCObjectPtr p)
{
    GCObjectHeader header = *(GCObjectHeader*)p;
    if ((header & EJS_SCAN_TYPE_OBJECT) != 0) {
        EJSObject* obj = (EJSObject*)p;
        if (obj->ops != NULL)
            OP(obj,Scan)(obj, minor_process_slot);
    }
    else if ((header & EJS_SCAN_TYPE_PRIMSTR) != 0) {
        EJSPrimString* primStr = (EJSPrimString*)p;
        EJSBool child_still_young = EJS_FALSE;
        switch (EJS_PRIMSTR_GET_TYPE(primStr)) {
        case EJS_STRING_ROPE:
            minor_process_primstr_child(&primStr->data.rope.left);
            minor_process_primstr_child(&primStr->data.rope.right);
            child_still_young = _ejs_gc_is_young(primStr->data.rope.left)
                || _ejs_gc_is_young(primStr->data.rope.right);
            break;
        case EJS_STRING_DEPENDENT:
            minor_process_primstr_child(&primStr->data.dependent.dep);
            child_still_young = _ejs_gc_is_young(primStr->data.dependent.dep);
            break;
        case EJS_STRING_FLAT:
            break;
        }
        if (child_still_young)
            minor_scan_saw_young = EJS_TRUE;
    }
    else if ((header & EJS_SCAN_TYPE_PRIMSYM) != 0) {
        minor_process_slot(&((EJSPrimSymbol*)p)->description);
    }
    else if ((header & EJS_SCAN_TYPE_CLOSUREENV) != 0) {
        EJSClosureEnv* env = (EJSClosureEnv*)p;
        for (uint32_t i = 0; i < env->length; i++)
            minor_process_slot(&env->slots[i]);
    }
}

// the overflow fallback scans every live old object — it must maintain
// the same DIRTY-bit discipline as normal processing (clear, scan,
// re-dirty on remaining pinned-young refs), or bits desync from the
// swapped-away buffer and later stores skip re-queuing forever
static void
minor_scan_object_if_live(GCObjectPtr p)
{
    *(GCObjectHeader*)p &= ~EJS_GC_HEADER_DIRTY;
    minor_scan_saw_young = EJS_FALSE;
    minor_scan_object(p);
    if (minor_scan_saw_young)
        _ejs_gc_remember_slow(p);
}

// A FULL collection frees dead old objects, so every remset/rescan
// entry — slots INTERIOR to old cells — may now dangle into poisoned
// memory (found as 0xfffc_afaf… "object-tagged poison" values read by
// the next minor).  Rebuild the whole remembered state from a live
// old-gen walk instead: record every live old→young ejsval slot, re-add
// old strings with young raw children, and drop the LOS-pending list
// (the walk covers LOS objects).  Full collections are rare; one extra
// old-gen walk apiece is cheap insurance.
void
remset_rebuild_after_full_gc(void)
{
    if (!nursery_enabled) return;
    // entries are heap OBJECTS: drop the ones the sweep freed, keep the
    // rest (their DIRTY bits are still set)
    int kept = 0;
    for (int i = 0; i < _ejs_heap.remset_count; i++) {
        GCObjectPtr o = (GCObjectPtr)_ejs_heap.remset[i];
        uint32_t ci;
        PageInfo* pg = find_page_and_cell(o, &ci);
        if (!pg || !cell_is_allocated(pg, ci, pg->page_bitmap[ci]))
            continue;
        _ejs_heap.remset[kept++] = _ejs_heap.remset[i];
    }
    _ejs_heap.remset_count = kept;
}

void
_ejs_gc_minor_collect(const char* reason)
{
    struct timeval tv0, tv1;
    gettimeofday (&tv0, NULL);

    if (in_minor_gc) {
        _ejs_log ("GC BUG: reentrant minor collection (reason=%s)\n", reason);
        abort();
    }

    // everything below this frame is collector machinery: the paranoid
    // checker's raw-stack sweep must not read it (see ejs-gc-debug.c)
    if (gc_paranoid)
        paranoid_stack_floor = (void**)__builtin_frame_address(0);

    young_flush_bumps();

    in_minor_gc = EJS_TRUE;
    heap_priv.minors++;
    MINOR_SPEW("minor: begin %llu\n", (unsigned long long)heap_priv.minors);
    uint64_t promoted_objs_before = heap_priv.promoted_objs;
    uint64_t promoted_bytes_before = heap_priv.promoted_bytes;
    uint64_t pins_before = heap_priv.minor_pins;
    int remset_used = _ejs_heap.remset_count;
    EJSBool overflowed = _ejs_heap.remset_overflowed != 0;
    if ((uint64_t)_ejs_heap.remset_count > heap_priv.remset_peak)
        heap_priv.remset_peak = _ejs_heap.remset_count;

    // 0. swap the remset buffers up front: EVERY minor_process_slot call
    //    from here on (roots, modules, remset snapshot, transitive scan)
    //    may carry an old→pinned-young edge into the LIVE buffer for the
    //    next cycle — the snapshot is what this cycle processes
    void** snapshot = _ejs_heap.remset;
    int snapshot_count = _ejs_heap.remset_count;
    EJSBool snapshot_overflowed = _ejs_heap.remset_overflowed != 0;
    _ejs_heap.remset = heap_priv.remset_other;
    heap_priv.remset_other = snapshot;
    _ejs_heap.remset_count = 0;
    _ejs_heap.remset_overflowed = 0;

    // 1. conservative pins FIRST: C stacks, registers, and EVERY live
    //    generator's suspended stack + saved contexts (the registry
    //    walk) — all ambiguous references must pin before any object
    //    moves; a generator discovered mid-trace would pin too late.
    //    The shared mark helpers dispatch to minor_conservative_hit
    //    while in_minor_gc is set.
    struct timeval ph0, ph1, ph2, ph3, ph4, ph5;
    int gen_count = 0;
    gettimeofday (&ph0, NULL);
    // each conservative range scan skips the gc-frame records of
    // the stack it is scanning — those slots are precise roots, and
    // seeing them conservatively would pin every frame-held value
    // through its own slot (precision would never move anything)
    set_frame_skip_chain(_ejs_heap.gc_frame_head);
    mark_thread_stack();
    mark_generator_stacks();
    for (EJSGenerator* g = _ejs_generator_registry; g; g = g->reg_next) {
        set_frame_skip_chain(g->gc_frame_head);
        _ejs_generator_scan_conservative(g);
        gen_count++;
    }
    clear_frame_skip();
    gettimeofday (&ph1, NULL);

    // 1.5 the emitted gc-frame chains — precise, relocatable
    //     JS-frame roots.  Runs AFTER the conservative pins on purpose:
    //     an object visible to both a gc-frame slot and a C frame (an
    //     ejsval argument into the very runtime call that triggered this
    //     minor, say) is pinned, and minor_process_slot leaves pinned
    //     targets in place — the pin must win or the C frame's copy
    //     dangles.  Everything frame-held and NOT C-visible evacuates
    //     and gets its slot rewritten.
    {
        uint64_t promoted_before_frames = heap_priv.promoted_objs;
        walk_gc_frames(minor_process_slot);
        gc_frame_moves = heap_priv.promoted_objs - promoted_before_frames;
    }

    // 2. precise roots: the root registry and module exports evacuate
    root_registry_foreach (minor_process_slot);
    for (int i = 0; i < _ejs_num_modules; i++) {
        EJSObject* mod = (EJSObject*)_ejs_modules[i];
        if (mod->ops == NULL) continue;
        OP(mod,Scan)(mod, minor_process_slot);
    }
    gettimeofday (&ph2, NULL);

    // 3. the remembered set snapshot (or, after overflow, every live
    //    old object)
    if (snapshot_overflowed) {
        heap_priv.overflow_minors++;
        old_gen_walk (minor_scan_object_if_live);
    } else {
        for (int i = 0; i < snapshot_count; i++) {
            GCObjectPtr owner = (GCObjectPtr)snapshot[i];
            // the object may have died and been swept by an interleaved
            // FULL collection; its cell reads FREE then — skip.  (A
            // reused cell scans as whatever lives there now: merely
            // conservative.)
            uint32_t ci;
            PageInfo* pg = find_page_and_cell(owner, &ci);
            if (!pg || !cell_is_allocated(pg, ci, pg->page_bitmap[ci]))
                continue;
            *(GCObjectHeader*)owner &= ~EJS_GC_HEADER_DIRTY;
            minor_scan_saw_young = EJS_FALSE;
            minor_scan_object(owner);
            // still holds pinned-young references: stay dirty
            if (minor_scan_saw_young)
                _ejs_gc_remember_slow(owner);
        }
    }

    // 4. transitive closure.  Objects scanned here (promoted copies,
    //    pinned young, generator roots) that still reference pinned-
    //    young data must carry a dirty mark so the next cycle revisits
    //    them (young owners filter out inside remember).
    gettimeofday (&ph3, NULL);
    while (heap_priv.wl_count > 0) {
        GCObjectPtr o = heap_priv.wl[--heap_priv.wl_count];
        minor_scan_saw_young = EJS_FALSE;
        minor_scan_object (o);
        if (minor_scan_saw_young && !_ejs_gc_is_young(o)
            && !(*(GCObjectHeader*)o & EJS_GC_HEADER_DIRTY))
            _ejs_gc_remember_slow(o);
    }
    gettimeofday (&ph4, NULL);

    // 5. optional barrier-coverage verification
    if (heap_priv.verify && !snapshot_overflowed) {
        verify_bad_slot = NULL;
        old_gen_walk (verify_check_object);
        // generator specops re-run their conservative scans inside the
        // verify walk (side effect: fresh pins pushed on the worklist);
        // drain them before the sweep decides survivor pages
        while (heap_priv.wl_count > 0)
            minor_scan_object (heap_priv.wl[--heap_priv.wl_count]);
    }

    // 6. sweep the young pages: dead cells finalize; forwarded cells are
    //    just space; pages with pins become survivor pages, the rest reset
    for (int i = 0; i < EJS_GC_NUM_SIZE_CLASSES; i++)
        young_page_retire_current(i);

    EJSList survivor_pages;
    memset (&survivor_pages, 0, sizeof(survivor_pages));
    PageInfo* page;
    while ((page = (PageInfo*)heap_priv.young_pages.head) != NULL) {
        for (int sc = 0; sc < EJS_GC_NUM_SIZE_CLASSES; sc++) {
            if (heap_priv.young_current[sc] == page
                || ((char*)_ejs_heap.bump[sc] > (char*)page->page_start
                    && (char*)_ejs_heap.bump[sc] <= (char*)page->page_end)) {
                _ejs_log ("GC BUG: sweeping page %p that is still active for class %d (bump=%p)\n",
                          page->page_start, sc, _ejs_heap.bump[sc]);
                abort();
            }
        }
        int survivors = 0;
        GCObjectPtr p = page->page_start;
        for (int c = 0; c < CELLS_IN_PAGE(page); c++, p += page->cell_size) {
            EJSBool allocated = (page->young == 1)
                ? young_cell_is_allocated(page, (uint32_t)c)
                : !cell_is_free(page->page_bitmap[c]);
            if (!allocated) { cell_set_free(&page->page_bitmap[c]); continue; }
            if (_ejs_gc_is_forwarded(p)) {
                // evacuated: the space is reusable; poison it now that
                // every slot has been processed
                gc_watch_hit ("sweep-poison-forwarded", p);
                memset (p, 0xa7, page->cell_size); // NOT 0xaf: bit 59 (FORWARDED) must stay clear in poison
                cell_set_free(&page->page_bitmap[c]);
                continue;
            }
            if (cell_is_black(page->page_bitmap[c])) {
                // pinned survivor: stays young, stays put; back to white
                // so the next cycle (minor or full) sees it fresh
                cell_set_white(&page->page_bitmap[c]);
                cell_set_allocated(&page->page_bitmap[c]);
                survivors++;
                continue;
            }
            MINOR_SPEW("minor: free %p (hdr %llx)\n", p, (unsigned long long)*(GCObjectHeader*)p);
            if (gc_paranoid) {
                // who still references this about-to-die young object?
                // (reverse lookup across every location the minor is
                // supposed to have processed)
                if (paranoid_report_referrers(p) > 0)
                    abort();
            }
            gc_watch_hit ("sweep-poison-dead", p);
            finalize_object(p);
            memset (p, 0xa7, page->cell_size); // NOT 0xaf: bit 59 (FORWARDED) must stay clear in poison
            cell_set_free(&page->page_bitmap[c]);
        }
        _ejs_list_detach_node (&heap_priv.young_pages, (EJSListNode*)page);
        if (survivors == 0) {
            page->young = 0;
            page->bump_ptr = page->page_start;
            page->num_free_cells = page->num_cells;
            EJS_LIST_PREPEND(page, heap_priv.nursery_arena->free_pages);
        } else {
            page->young = 2;
            page->num_free_cells = page->num_cells - survivors;
            _ejs_list_append_node (&survivor_pages, (EJSListNode*)page);
        }
    }
    heap_priv.young_pages = survivor_pages;
    gettimeofday (&ph5, NULL);

    // 7. cycle accounting (the remset swapped/reset in step 0; carried
    //    edges are already in the live buffer); promoted bytes feed the
    //    FULL collection trigger (they are old-gen growth)
    heap_priv.young_alloced = 0;
    alloc_size += heap_priv.promoted_bytes - promoted_bytes_before;

    // seam/private-state consistency: every class was retired in step 6;
    // nothing may have reinstalled a bump cursor mid-minor
    for (int sc = 0; sc < EJS_GC_NUM_SIZE_CLASSES; sc++) {
        if (_ejs_heap.bump[sc] != NULL || heap_priv.young_current[sc] != NULL) {
            _ejs_log ("GC BUG: minor end: class %d seam desync (bump=%p current=%p)\n",
                      sc, _ejs_heap.bump[sc], (void*)heap_priv.young_current[sc]);
            abort();
        }
    }

    MINOR_SPEW("minor: end %llu\n", (unsigned long long)heap_priv.minors);
    in_minor_gc = EJS_FALSE;

    gettimeofday (&tv1, NULL);
    uint64_t usec = (tv1.tv_sec - tv0.tv_sec) * 1000000ULL + (tv1.tv_usec - tv0.tv_usec);
    heap_priv.minor_usec_total += usec;
    if (usec > heap_priv.minor_usec_max) heap_priv.minor_usec_max = usec;
    if (gc_paranoid)
        paranoid_sweep_check();
    if (gc_profile) {
#define PHUS(a,b) (((b).tv_sec - (a).tv_sec) * 1000000LL + ((b).tv_usec - (a).tv_usec))
        _ejs_log ("EJS_GC_PROFILE: minor#%llu reason=%s pause=%.3fms promoted=%llu/%lluKB pins=%llu gcframe_moves=%llu remset=%d gens=%d phases[pins=%lld roots=%lld dirty=%lld wl=%lld sweep=%lld]us%s\n",
                  (unsigned long long)heap_priv.minors, reason, usec / 1000.0,
                  (unsigned long long)(heap_priv.promoted_objs - promoted_objs_before),
                  (unsigned long long)((heap_priv.promoted_bytes - promoted_bytes_before) / 1024),
                  (unsigned long long)(heap_priv.minor_pins - pins_before),
                  (unsigned long long)gc_frame_moves,
                  remset_used, gen_count,
                  (long long)PHUS(ph0,ph1), (long long)PHUS(ph1,ph2), (long long)PHUS(ph2,ph3),
                  (long long)PHUS(ph3,ph4), (long long)PHUS(ph4,ph5),
                  overflowed ? " OVERFLOW" : "");
#undef PHUS
    }

    // promotions grow the old gen; the policy may schedule a full
    gc_policy (GC_POLICY_AFTER_MINOR, NULL);
}

// the young allocation slow path: refill the class's bump page, running
// a minor collection when the nursery is exhausted
GCObjectPtr
young_alloc_slow(int idx, size_t cell_size, EJSScanType scan_type)
{
    young_page_retire_current(idx);
    // the budget bounds the per-minor sweep (pause target <1ms) — the
    // arena is the hard capacity, the budget the soft trigger
    if (heap_priv.young_alloced >= heap_priv.young_budget)
        _ejs_gc_minor_collect("nursery budget");
    if (!young_page_install(idx, cell_size)) {
        _ejs_gc_minor_collect("nursery exhausted");
        if (!young_page_install(idx, cell_size)) {
            // nursery still full (all survivor pages): give up on the
            // nursery for this allocation and take the old path
            return NULL;
        }
    }
    void* p = _ejs_heap.bump[idx];
    _ejs_heap.bump[idx] = (char*)p + cell_size;
    memset (p, 0, cell_size);
    *(GCObjectHeader*)p = scan_type | EJS_GC_HEADER_YOUNG;
    return p;
}

// Full collections see young pages too.  Active (bump-rule) pages have
// no valid FREE bits or num_free_cells, so normalize them to
// bitmap-authoritative survivor form first: cells below the bump are
// allocated, the rest free, and the page leaves bump service.  After
// this the existing mark/sweep machinery handles them verbatim (their
// objects remain YOUNG by address range; the next minor collection
// evacuates or re-pins whatever survives the full GC).
void
young_normalize_for_full_gc(void)
{
    if (!nursery_enabled) return;
    young_flush_bumps();
    for (int i = 0; i < EJS_GC_NUM_SIZE_CLASSES; i++)
        young_page_retire_current(i);
    for (PageInfo* page = (PageInfo*)heap_priv.young_pages.head; page; page = page->next) {
        if (page->young != 1) continue;
        int allocated = 0;
        for (int c = 0; c < CELLS_IN_PAGE(page); c++) {
            if (young_cell_is_allocated(page, (uint32_t)c)) {
                cell_set_allocated(&page->page_bitmap[c]);
                allocated++;
            } else {
                cell_set_free(&page->page_bitmap[c]);
            }
        }
        page->num_free_cells = page->num_cells - allocated;
        page->young = 2;
    }
    heap_priv.young_alloced = 0;
}

void
nursery_init(void)
{
    // nursery ON by default; EJS_GC_NURSERY=off (or =0) selects the
    // single-generation collector for A/B.
    {
        char* e = getenv("EJS_GC_NURSERY");
        nursery_enabled = !(e && (strcmp(e, "off") == 0 || strcmp(e, "0") == 0));
    }
    heap_priv.verify = getenv("EJS_GC_VERIFY") != NULL;
    minor_spew = getenv("EJS_GC_MINOR_SPEW") != NULL;
    gc_paranoid = getenv("EJS_GC_PARANOID") != NULL;
    if (getenv("EJS_GC_WATCH"))
        gc_watch_addr = (uintptr_t)strtoull(getenv("EJS_GC_WATCH"), NULL, 16);
    // 1MB balances pause and throughput (measured 2026-07-25): minor p99
    // ~1.3ms on the bench corpus (512KB reaches 0.68ms at ~10% self-
    // compile cost; 4MB buys self-compile ~3% at ~5ms p99)
    heap_priv.young_budget = 1024 * 1024;
    char* budget_env = getenv("EJS_GC_NURSERY_BUDGET");
    if (budget_env) heap_priv.young_budget = (size_t)atoll(budget_env);
    if (!nursery_enabled) return;

    Arena* arena = arena_new();
    if (!arena) {
        _ejs_log ("gc: could not allocate the nursery arena; nursery disabled\n");
        nursery_enabled = EJS_FALSE;
        return;
    }
    arena->is_nursery = EJS_TRUE;
    heap_priv.nursery_arena = arena;
    _ejs_heap.nursery_base = (void*)arena;
    _ejs_heap.nursery_end = arena->end;
    _ejs_heap.remset = malloc (NURSERY_REMSET_CAPACITY * sizeof(void*));
    _ejs_heap.remset_capacity = NURSERY_REMSET_CAPACITY;
    heap_priv.remset_other = malloc (NURSERY_REMSET_CAPACITY * sizeof(void*));
}
// ===================== end nursery =========================================
