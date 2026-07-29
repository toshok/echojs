/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

// observability: EJS_GC_PROFILE instrumentation, EJS_GC_WATCH,
// EJS_GC_VERIFY barrier checks, EJS_GC_PARANOID heap validation, and
// the heap-stats dumps.

#include "ejs-gc-internal.h"

// ---- measurement instrumentation (EJS_GC_PROFILE=1) -----------
//
// Two header bits from the gc-reserved range (57-63; see ejs-types.h — the
// shapes machinery masks its 24-bit index, so these are invisible to it):
//
//   YOUNG:  set at allocation, cleared on the first collection the object
//           survives.  "young" therefore means "allocated since the last
//           collection" — exactly the population a generational nursery
//           would manage, so per-cycle young-survival is THE
//           number that sizes the nursery payoff.
//   PINNED: set (once per cycle) when a CONSERVATIVE reference — C stack,
//           spilled registers, generator stacks/contexts — hits the
//           object.  Under the mover these are the objects that cannot
//           be evacuated this cycle; their count/bytes/sources size the
//           payoff of precise JS frames and decide its ordering.
//
// The YOUNG bit is set unconditionally (an OR folded into the header
// store the allocator already does); everything else is gated on
// gc_profile so the measured path stays clean when profiling is off.
// (The YOUNG/PINNED #defines live near the top of the file — the mark
// helpers set PINNED for the compacting major.)

EJSBool gc_profile;             // EJS_GC_PROFILE (parsed in _ejs_gc_init)
struct timeval prof_start_tv;   // process start, for the shutdown report

// (the PROF_SRC_* enum lives in ejs-gc-internal.h; the scanners set
// prof_pin_source as they change source)
static const char* prof_src_names[PROF_SRC_COUNT] = { "cstack", "regs", "genstack" };
int prof_pin_source = PROF_SRC_CSTACK;

#define PROF_NBUCKETS 12 // ffs buckets 16B.. + [0] = LOS
static uint64_t prof_alloc_count[PROF_NBUCKETS];
static uint64_t prof_alloc_bytes[PROF_NBUCKETS];
static uint64_t prof_kind_count[4]; // primstr, primsym, object, closureenv
static uint64_t prof_alloc_total_count = 0;
static uint64_t prof_alloc_total_bytes = 0;
// the young population: allocations since the last collection
static uint64_t prof_young_count = 0;
static uint64_t prof_young_bytes = 0;
// per-cycle pin accounting (reset after each report)
static uint64_t prof_pin_count[PROF_SRC_COUNT];
static uint64_t prof_pin_bytes[PROF_SRC_COUNT];
static uint64_t prof_pin_young = 0, prof_pin_old = 0;
static uint64_t prof_pin_env_interior = 0, prof_pin_los = 0;
static uint64_t prof_collections = 0;
static uint64_t prof_total_pause_usec = 0;
const char* prof_gc_reason = "?";

void
profile_note_alloc(size_t size, int ffs_bucket, EJSScanType scan_type)
{
    int idx;
    if (ffs_bucket > OBJECT_SIZE_HIGH_LIMIT_BITS + 1)
        idx = 0; // LOS
    else {
        idx = ffs_bucket - OBJECT_SIZE_LOW_LIMIT_BITS;
        if (idx < 1) idx = 1;
        if (idx >= PROF_NBUCKETS) idx = PROF_NBUCKETS - 1;
    }
    prof_alloc_count[idx]++;
    prof_alloc_bytes[idx] += size;
    prof_alloc_total_count++;
    prof_alloc_total_bytes += size;
    switch (scan_type) {
    case EJS_SCAN_TYPE_PRIMSTR:    prof_kind_count[0]++; break;
    case EJS_SCAN_TYPE_PRIMSYM:    prof_kind_count[1]++; break;
    case EJS_SCAN_TYPE_OBJECT:     prof_kind_count[2]++; break;
    case EJS_SCAN_TYPE_CLOSUREENV: prof_kind_count[3]++; break;
    }
    prof_young_count++;
    prof_young_bytes += size;
}

// a conservative reference hit an allocated cell: under the mover this
// object is pinned for the cycle.  counted once per cycle per object
// (dedupe via the PINNED header bit), attributed to the scan source that
// found it first, split young/old, with env-interior-pointer and LOS
// sub-counts.  runs BEFORE the white-check filter: a hit on an
// already-marked object still pins it.
void
profile_note_pin(PageInfo* page, uint32_t cell_idx, GCObjectPtr raw)
{
    GCObjectPtr base = page->page_start + (cell_idx * page->cell_size);
    GCObjectHeader* h = (GCObjectHeader*)base;
    if (*h & EJS_GC_HEADER_PINNED)
        return;
    *h |= EJS_GC_HEADER_PINNED;
    prof_pin_count[prof_pin_source]++;
    prof_pin_bytes[prof_pin_source] += page->cell_size;
    if (*h & EJS_GC_HEADER_YOUNG) prof_pin_young++; else prof_pin_old++;
    if (raw != base && (*h & EJS_SCAN_TYPE_CLOSUREENV)) prof_pin_env_interior++;
    if (page->los_info) prof_pin_los++;
}

// per-cycle results filled by profile_pre_sweep (which must run after
// marking and BEFORE the sweep frees the dead cells), printed with the
// pause by profile_report_cycle_end
static uint64_t prof_cycle_live_count, prof_cycle_live_bytes;
static uint64_t prof_cycle_ysurv_count, prof_cycle_ysurv_bytes;

static void
profile_visit_live_cell(GCObjectHeader* h, size_t bytes)
{
    prof_cycle_live_count++;
    prof_cycle_live_bytes += bytes;
    if (*h & EJS_GC_HEADER_YOUNG) {
        prof_cycle_ysurv_count++;
        prof_cycle_ysurv_bytes += bytes;
        *h &= ~EJS_GC_HEADER_YOUNG; // survived one collection: no longer young
    }
    // reset pins for the next cycle — but the census runs PRE-sweep and
    // the compacting major reads pins POST-sweep (and clears them in its
    // fixup walk); clearing here would un-pin every C-visible object
    // right before evacuation decides what may move
    if (!compact_enabled)
        *h &= ~EJS_GC_HEADER_PINNED;
}

void
profile_pre_sweep(void)
{
    prof_cycle_live_count = prof_cycle_live_bytes = 0;
    prof_cycle_ysurv_count = prof_cycle_ysurv_bytes = 0;
    for (int i = 0; i < HEAP_PAGELISTS_COUNT; i++) {
        EJS_LIST_FOREACH (&heap_pages[i], PageInfo, page, {
            GCObjectPtr p = page->page_start;
            for (int c = 0; c < CELLS_IN_PAGE(page); c++, p += page->cell_size) {
                BitmapCell cell = page->page_bitmap[c];
                if (cell_is_free(cell) || cell_is_white(cell)) continue;
                profile_visit_live_cell((GCObjectHeader*)p, page->cell_size);
            }
        });
    }
    for (LargeObjectInfo* lobj = los_list; lobj; lobj = lobj->next) {
        BitmapCell cell = lobj->page_info.page_bitmap[0];
        if (cell_is_free(cell) || cell_is_white(cell)) continue;
        profile_visit_live_cell((GCObjectHeader*)lobj->page_info.page_start,
                                lobj->page_info.cell_size);
    }
}

void
profile_report_cycle_end(uint64_t pause_usec)
{
    prof_collections++;
    prof_total_pause_usec += pause_usec;
    double surv_pct = prof_young_bytes
        ? 100.0 * (double)prof_cycle_ysurv_bytes / (double)prof_young_bytes : 0.0;
    _ejs_log ("EJS_GC_PROFILE: gc#%llu reason=%s pause=%.2fms "
              "live=%llu objs/%.2fMB | young allocd=%llu/%.2fMB "
              "survived=%llu/%.2fMB (%.1f%% of bytes) | pins: "
              "cstack=%llu/%lluKB regs=%llu/%lluKB genstack=%llu/%lluKB "
              "envint=%llu los=%llu young=%llu old=%llu\n",
              (unsigned long long)prof_collections, prof_gc_reason,
              pause_usec / 1000.0,
              (unsigned long long)prof_cycle_live_count,
              prof_cycle_live_bytes / (1024.0 * 1024.0),
              (unsigned long long)prof_young_count,
              prof_young_bytes / (1024.0 * 1024.0),
              (unsigned long long)prof_cycle_ysurv_count,
              prof_cycle_ysurv_bytes / (1024.0 * 1024.0),
              surv_pct,
              (unsigned long long)prof_pin_count[PROF_SRC_CSTACK],
              (unsigned long long)(prof_pin_bytes[PROF_SRC_CSTACK] / 1024),
              (unsigned long long)prof_pin_count[PROF_SRC_REGS],
              (unsigned long long)(prof_pin_bytes[PROF_SRC_REGS] / 1024),
              (unsigned long long)prof_pin_count[PROF_SRC_GENSTACK],
              (unsigned long long)(prof_pin_bytes[PROF_SRC_GENSTACK] / 1024),
              (unsigned long long)prof_pin_env_interior,
              (unsigned long long)prof_pin_los,
              (unsigned long long)prof_pin_young,
              (unsigned long long)prof_pin_old);
    prof_young_count = prof_young_bytes = 0;
    memset (prof_pin_count, 0, sizeof (prof_pin_count));
    memset (prof_pin_bytes, 0, sizeof (prof_pin_bytes));
    prof_pin_young = prof_pin_old = 0;
    prof_pin_env_interior = prof_pin_los = 0;
}

void
profile_report_shutdown(void)
{
    static EJSBool reported = EJS_FALSE; // atexit + GC_ON_SHUTDOWN may both fire
    if (reported) return;
    reported = EJS_TRUE;

    struct timeval now;
    gettimeofday (&now, NULL);
    double wall = (now.tv_sec - prof_start_tv.tv_sec)
        + (now.tv_usec - prof_start_tv.tv_usec) / 1e6;
    _ejs_log ("EJS_GC_PROFILE: totals: allocs=%llu bytes=%.2fMB wall=%.2fs "
              "(%.1fMB/s, %.0f allocs/s) collections=%llu total-pause=%.2fms\n",
              (unsigned long long)prof_alloc_total_count,
              prof_alloc_total_bytes / (1024.0 * 1024.0), wall,
              prof_alloc_total_bytes / (1024.0 * 1024.0) / (wall > 0 ? wall : 1),
              prof_alloc_total_count / (wall > 0 ? wall : 1),
              (unsigned long long)prof_collections,
              prof_total_pause_usec / 1000.0);
    _ejs_log ("EJS_GC_PROFILE: kinds: primstr=%llu primsym=%llu object=%llu "
              "closureenv=%llu\n",
              (unsigned long long)prof_kind_count[0],
              (unsigned long long)prof_kind_count[1],
              (unsigned long long)prof_kind_count[2],
              (unsigned long long)prof_kind_count[3]);
    for (int i = 1; i < PROF_NBUCKETS; i++) {
        if (!prof_alloc_count[i]) continue;
        _ejs_log ("EJS_GC_PROFILE: size<=%4d: %llu allocs, %.2fMB requested\n",
                  1 << (OBJECT_SIZE_LOW_LIMIT_BITS + i - 1),
                  (unsigned long long)prof_alloc_count[i],
                  prof_alloc_bytes[i] / (1024.0 * 1024.0));
    }
    if (prof_alloc_count[0])
        _ejs_log ("EJS_GC_PROFILE: LOS:       %llu allocs, %.2fMB requested\n",
                  (unsigned long long)prof_alloc_count[0],
                  prof_alloc_bytes[0] / (1024.0 * 1024.0));
}

// ======================= the nursery ============================
//
// One dedicated arena; size-class pages inside it are bump-allocated
// (the seam's per-class bump/limit cursors ARE the allocation state —
// emitted code bumps them inline).  Minor GC is mostly-
// copying: conservative hits pin young cells in place (established
// FIRST), then every precise slot — root list, module exports,
// remembered-set entries, and the transitive scan through the
// slot-based Scan protocol — evacuates its young referent into the old
// gen, installs a P1 forwarding record, and is rewritten.  Young pages
// end the cycle reset (no survivors) or as survivor pages (pins only —
// pins merely delay promotion).  The old gen stays mark-sweep.

// EJS_GC_WATCH=<hex addr>: log every lifecycle event touching the cell
// containing that address, with a C backtrace (debugging aid for the
// deterministic single-cell corruption hunt)
#include <execinfo.h>
uintptr_t gc_watch_addr;
void
gc_watch_hit(const char* what, void* p)
{
    if (EJS_LIKELY(gc_watch_addr == 0)) return;
    if ((uintptr_t)p > gc_watch_addr || gc_watch_addr - (uintptr_t)p >= 256) return;
    _ejs_log ("EJS_GC_WATCH: %s cell=%p (minor#%llu, in_minor=%d)\n",
              what, p, (unsigned long long)heap_priv.minors, (int)in_minor_gc);
    void* frames[24];
    int n = backtrace (frames, 24);
    backtrace_symbols_fd (frames, n, 2);
}

// EJS_GC_PARANOID: reverse-lookup for the sweep's death detector — when
// a young cell dies, name everything that still references it (old gen,
// LOS, roots, modules, the C stack).  A hit is a missed barrier/scan of
// that owner; zero hits means the pointer was in-flight in mutator
// state the conservative scan cannot see.
static GCObjectPtr referrer_target;
static const char* referrer_ctx;
static GCObjectPtr referrer_owner;
static int referrer_hits;
// the minor collection's entry frame pointer: the raw-stack sweep's
// floor (set per minor while EJS_GC_PARANOID is on)
void** paranoid_stack_floor;
static void
referrer_check_slot(ejsval* slot)
{
    ejsval v = *slot;
    if (!EJSVAL_IS_TRACEABLE_IMPL(v)) return;
    if ((GCObjectPtr)EJSVAL_TO_GCTHING_IMPL(v) == referrer_target) {
        GCObjectHeader oh = referrer_owner ? *(GCObjectHeader*)referrer_owner : 0;
        _ejs_log ("EJS_GC_PARANOID: dying young %p still referenced: ctx=%s owner=%p (hdr %llx) slot=%p\n",
                  referrer_target, referrer_ctx, (void*)referrer_owner,
                  (unsigned long long)oh, (void*)slot);
        referrer_hits++;
    }
}
static void
referrer_check_object(GCObjectPtr p)
{
    GCObjectHeader header = *(GCObjectHeader*)p;
    referrer_owner = p;
    if ((header & EJS_SCAN_TYPE_OBJECT) != 0) {
        EJSObject* obj = (EJSObject*)p;
        if (obj->ops != NULL) OP(obj,Scan)(obj, referrer_check_slot);
    } else if ((header & EJS_SCAN_TYPE_CLOSUREENV) != 0) {
        EJSClosureEnv* env = (EJSClosureEnv*)p;
        for (uint32_t i = 0; i < env->length; i++)
            referrer_check_slot(&env->slots[i]);
    } else if ((header & EJS_SCAN_TYPE_PRIMSYM) != 0) {
        referrer_check_slot(&((EJSPrimSymbol*)p)->description);
    }
}
int
paranoid_report_referrers(GCObjectPtr p)
{
    referrer_target = p;
    referrer_hits = 0;
    referrer_ctx = "oldgen";
    old_gen_walk (referrer_check_object);
    referrer_ctx = "roots";
    referrer_owner = NULL;
    root_registry_foreach (referrer_check_slot);
    referrer_ctx = "modules";
    for (int i = 0; i < _ejs_num_modules; i++) {
        EJSObject* mod = (EJSObject*)_ejs_modules[i];
        referrer_owner = (GCObjectPtr)mod;
        if (mod->ops) OP(mod,Scan)(mod, referrer_check_slot);
    }
    // raw C-stack sweep: any word whose payload lands inside the dying
    // cell counts (boxed or raw, base or interior).  Floor the sweep at
    // the minor's entry frame: everything deeper is COLLECTOR frames —
    // the sweep loop's own cell cursor, evacuation temporaries — written
    // AFTER the conservative pin scan ran, so a hit there is the checker
    // reading its own machinery, not a missed mutator reference.  (The
    // P6.3 file split's codegen shift surfaced exactly that self-hit.)
    referrer_ctx = "stack";
    referrer_owner = NULL;
    void* volatile probe;
    void** stack_lo = paranoid_stack_floor ? paranoid_stack_floor : (void**)&probe;
    for (void** w = stack_lo; w < (void**)stack_bottom; w++) {
        uintptr_t masked = (uintptr_t)*w & 0x00007fffffffffffULL;
        if ((char*)masked >= (char*)p && (char*)masked < (char*)p + 16) {
            _ejs_log ("EJS_GC_PARANOID: dying young %p: raw stack word at %p = %p\n",
                      p, (void*)w, *w);
            referrer_hits++;
        }
    }
    return referrer_hits;
}

// EJS_GC_VERIFY: after the remset has been processed, no live old slot
// may still reference an unforwarded, unpinned young object — such an
// edge is a missed write barrier.  Report and abort.
ejsval* verify_bad_slot;
static void
verify_check_slot(ejsval* slot)
{
    ejsval v = *slot;
    if (!EJSVAL_IS_TRACEABLE_IMPL(v)) return;
    GCObjectPtr p = (GCObjectPtr)EJSVAL_TO_GCTHING_IMPL(v);
    if (p == NULL || !_ejs_gc_is_young(p)) return;
    uint32_t cell_idx;
    PageInfo* page = find_page_and_cell(p, &cell_idx);
    if (!page) return;
    GCObjectPtr base = page->page_start + ((size_t)cell_idx * page->cell_size);
    if (_ejs_gc_is_forwarded(base)) return;      // will be rewritten by its recorder
    if (cell_is_black(page->page_bitmap[cell_idx])) { minor_scan_saw_young = EJS_TRUE; return; } // pinned in place
    verify_bad_slot = slot;
}
void
verify_check_object(GCObjectPtr p)
{
    GCObjectHeader header = *(GCObjectHeader*)p;
    if ((header & EJS_SCAN_TYPE_OBJECT) != 0) {
        EJSObject* obj = (EJSObject*)p;
        if (obj->ops != NULL)
            OP(obj,Scan)(obj, verify_check_slot);
        if (verify_bad_slot) {
            _ejs_log ("EJS_GC_VERIFY: missed write barrier: old object %p (class %s) slot %p holds unpromoted young ref (bits %llx)\n",
                      p, obj->ops ? obj->ops->class_name : "<uninit>",
                      (void*)verify_bad_slot,
                      (unsigned long long)verify_bad_slot->asBits);
            abort();
        }
    }
    else if ((header & EJS_SCAN_TYPE_CLOSUREENV) != 0) {
        EJSClosureEnv* env = (EJSClosureEnv*)p;
        for (uint32_t i = 0; i < env->length; i++) {
            verify_check_slot(&env->slots[i]);
            if (verify_bad_slot) {
                _ejs_log ("EJS_GC_VERIFY: missed write barrier: old env %p (hdr %llx, len %u) slot %u holds unpromoted young ref (bits %llx, target hdr %llx)\n",
                          p, (unsigned long long)header, env->length, i,
                          (unsigned long long)verify_bad_slot->asBits,
                          (unsigned long long)*(GCObjectHeader*)EJSVAL_TO_GCTHING_IMPL(*verify_bad_slot));
                abort();
            }
        }
    }
    else if ((header & EJS_SCAN_TYPE_PRIMSTR) != 0) {
        EJSPrimString* ps = (EJSPrimString*)p;
        EJSPrimString* kids[2] = { NULL, NULL };
        switch (EJS_PRIMSTR_GET_TYPE(ps)) {
        case EJS_STRING_ROPE: kids[0] = ps->data.rope.left; kids[1] = ps->data.rope.right; break;
        case EJS_STRING_DEPENDENT: kids[0] = ps->data.dependent.dep; break;
        default: break;
        }
        for (int k = 0; k < 2; k++) {
            if (!kids[k] || !_ejs_gc_is_young(kids[k])) continue;
            uint32_t ci;
            PageInfo* pg = find_page_and_cell(kids[k], &ci);
            if (!pg) continue;
            if (_ejs_gc_is_forwarded(pg->page_start + (size_t)ci * pg->cell_size)) continue;
            if (cell_is_black(pg->page_bitmap[ci])) continue;
            _ejs_log ("EJS_GC_VERIFY: old primstr %p (type %d) child %d -> unpromoted young %p\n",
                      p, EJS_PRIMSTR_GET_TYPE(ps), k, (void*)kids[k]);
            abort();
        }
    }
}

// EJS_GC_PARANOID: after every minor, walk roots + modules + all live
// heap cells and validate every traceable value: it must resolve to an
// allocated cell whose header carries exactly one scan-type bit.
// Catches corruption at the collection that minted it.
EJSBool gc_paranoid;
static const char* paranoid_ctx;
static GCObjectPtr paranoid_owner;
static void
paranoid_check_slot(ejsval* slot)
{
    ejsval v = *slot;
    if (!EJSVAL_IS_TRACEABLE_IMPL(v)) return;
    GCObjectPtr p = (GCObjectPtr)EJSVAL_TO_GCTHING_IMPL(v);
    if (p == NULL) return;
    uint32_t ci;
    PageInfo* pg = find_page_and_cell(p, &ci);
    const char* why = NULL;
    if (!pg) return; // static atoms/primstrings live outside the heap
    if (0) why = "";
    else if (!cell_is_allocated(pg, ci, pg->page_bitmap[ci])) why = "target cell free";
    else {
        GCObjectHeader h = *(GCObjectHeader*)(pg->page_start + (size_t)ci * pg->cell_size);
        uint32_t st = (uint32_t)(h & 0xf);
        if (st != 1 && st != 2 && st != 4 && st != 8) why = "bad scan type";
        else if (_ejs_gc_is_forwarded(pg->page_start + (size_t)ci * pg->cell_size)) why = "target forwarded";
    }
    if (why) {
        GCObjectHeader oh = paranoid_owner ? *(GCObjectHeader*)paranoid_owner : 0;
        const char* ocls = "?";
        if (paranoid_owner && (oh & EJS_SCAN_TYPE_OBJECT) && ((EJSObject*)paranoid_owner)->ops)
            ocls = ((EJSObject*)paranoid_owner)->ops->class_name;
        else if (paranoid_owner && (oh & EJS_SCAN_TYPE_CLOSUREENV)) ocls = "<closureenv>";
        else if (paranoid_owner && (oh & EJS_SCAN_TYPE_PRIMSTR)) ocls = "<primstr>";
        _ejs_log ("EJS_GC_PARANOID [%s]: owner %p (class %s, hdr %llx) slot %p value %llx: %s\n",
                  paranoid_ctx, (void*)paranoid_owner, ocls, (unsigned long long)oh,
                  (void*)slot, (unsigned long long)v.asBits, why);
        abort();
    }
}
static void
paranoid_check_object(GCObjectPtr p)
{
    paranoid_owner = p;
    GCObjectHeader header = *(GCObjectHeader*)p;
    if ((header & EJS_SCAN_TYPE_OBJECT) != 0) {
        EJSObject* obj = (EJSObject*)p;
        if (obj->ops != NULL) OP(obj,Scan)(obj, paranoid_check_slot);
    }
    else if ((header & EJS_SCAN_TYPE_CLOSUREENV) != 0) {
        EJSClosureEnv* env = (EJSClosureEnv*)p;
        for (uint32_t i = 0; i < env->length; i++)
            paranoid_check_slot(&env->slots[i]);
    }
    else if ((header & EJS_SCAN_TYPE_PRIMSYM) != 0)
        paranoid_check_slot(&((EJSPrimSymbol*)p)->description);
}
void
paranoid_sweep_check(void)
{
    paranoid_ctx = "roots";
    root_registry_foreach (paranoid_check_slot);
    paranoid_ctx = "modules";
    for (int i = 0; i < _ejs_num_modules; i++) {
        EJSObject* mod = (EJSObject*)_ejs_modules[i];
        if (mod->ops) OP(mod,Scan)(mod, paranoid_check_slot);
    }
    paranoid_ctx = "oldgen";
    old_gen_walk (paranoid_check_object);
    paranoid_ctx = "young";
    for (PageInfo* page = (PageInfo*)heap_priv.young_pages.head; page; page = page->next) {
        GCObjectPtr p = page->page_start;
        for (int c = 0; c < CELLS_IN_PAGE(page); c++, p += page->cell_size) {
            EJSBool allocated = (page->young == 1)
                ? young_cell_is_allocated(page, (uint32_t)c)
                : !cell_is_free(page->page_bitmap[c]);
            if (allocated && !_ejs_gc_is_forwarded(p))
                paranoid_check_object(p);
        }
    }
}

void
_ejs_gc_dump_heap_stats()
{
    _ejs_log ("arenas:\n");
    for (int i = 0; i < num_arenas; i ++) {
        _ejs_log ("  [%d] - %p - %p\n", i, heap_arenas[i], heap_arenas[i]->end);
    }

    for (int i = 0; i < HEAP_PAGELISTS_COUNT; i ++) {
#if gc_timings > 3
        EJSBool printed_something = EJS_FALSE;
#endif
        _ejs_log ("heap_pages[%d, size %d] : %d pages\n", i, 1 << (i + OBJECT_SIZE_LOW_LIMIT_BITS), _ejs_list_length (&heap_pages[i]));
#if gc_timings > 3
        EJS_LIST_FOREACH (&heap_pages[i], PageInfo, page, {
            GCObjectPtr p = page->page_start;
            for (int c = 0; c < CELLS_IN_PAGE (page); c ++, p += page->cell_size) {
                if (cell_is_free(page->page_bitmap[c]))
                    continue;
                GCObjectHeader* headerp = (GCObjectHeader*)p;
                if ((*headerp & EJS_SCAN_TYPE_OBJECT) != 0)          _ejs_log ("O");
                else if ((*headerp & EJS_SCAN_TYPE_CLOSUREENV) != 0) _ejs_log ("C");
                else if ((*headerp & EJS_SCAN_TYPE_PRIMSTR) != 0)    _ejs_log (((*headerp >> EJS_GC_USER_FLAGS_SHIFT) & 0x10) != 0 ? "s" : "S");
                else if ((*headerp & EJS_SCAN_TYPE_PRIMSYM) != 0)    _ejs_log ("X");
                printed_something = EJS_TRUE;
            }
        })
        if (printed_something)
            _ejs_log ("\n");
#endif
    }

    _ejs_log ("\n");

#if spew >= 2
    if (los_list) {
        _ejs_log ("large object store: ");
        for (LargeObjectInfo* lobj = los_list; lobj; lobj = lobj->next) {
            GCObjectHeader* headerp = (GCObjectHeader*)lobj->page_info.page_start;
            if ((*headerp & EJS_SCAN_TYPE_OBJECT) != 0)          _ejs_log ("O");
            else if ((*headerp & EJS_SCAN_TYPE_CLOSUREENV) != 0) _ejs_log ("C");
            else if ((*headerp & EJS_SCAN_TYPE_PRIMSTR) != 0)    _ejs_log ("S");
            else if ((*headerp & EJS_SCAN_TYPE_PRIMSYM) != 0)    _ejs_log ("X");
        }
        _ejs_log ("\n");
    }
#endif
}
