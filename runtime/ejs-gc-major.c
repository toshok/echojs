/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

// full collections: mark orchestration, the sweep, the mostly-copying
// major compaction, and the post-cycle epoch advance.

#include "ejs-gc-internal.h"

int num_roots = 0;
static int white_objs = 0;
static int large_objs = 0;
static int total_objs = 0;

static void
sweep_heap()
{
#if spew
    int pages_visited = 0;
    int pages_skipped = 0;
#endif

    // sweep the entire heap, freeing white nodes
    for (int a = 0, e = num_arenas; a < e; a ++) {
        Arena* arena = heap_arenas[a];

        if (!arena)
            continue;

        for (int p = 0, pe = arena->num_pages; p < pe; p++) {
            PageInfo *info = arena->page_infos[p];

            if (info->num_free_cells == info->num_cells) {
#if spew
                pages_skipped++;
#endif
            }
            else {
#if spew
                pages_visited ++;
#endif

                for (int c = 0, ce = info->num_cells; c < ce; c ++) {
                    BitmapCell cell = info->page_bitmap[c];

                    if (cell_is_free(cell))
                        continue;

                    total_objs++;

                    if (cell_is_white(cell)) {
                        white_objs++;

                        GCObjectPtr gcobj = (GCObjectPtr)(info->page_start + c * info->cell_size);
                        _ejs_finalize_obj(gcobj, arena, info, c);
                    }
                }
            }
        }
    }
    
    // sweep the large object store
    SPEW(2, _ejs_log ("sweeping los: "));
    LargeObjectInfo *lobj = los_list;
    while (lobj) {
        large_objs ++;
        PageInfo *info = &lobj->page_info;
        BitmapCell cell = info->page_bitmap[0];
        LargeObjectInfo *next = lobj->next;
        if (cell_is_white(cell)) {
            //            SPEW(2, { _ejs_log ("l"); fflush(stderr); });
            white_objs++;

            EJS_LIST_DETACH(lobj, los_list);
            _ejs_finalize_obj(info->page_start, NULL, info, 0);
        }
        else {
            //            SPEW(2, { _ejs_log ("L"); fflush(stderr); });
        }
        lobj = next;
    }
    SPEW(2, { _ejs_log ("\n"); });
}

// ============== mostly-copying major compaction ===========================
//
// Mark-sweep never shrinks: live old-gen cells sit wherever history put
// them and sparse pages hold whole pages hostage for a cell or two.
// After the sweep, this pass evacuates the live UNPINNED cells of the
// sparsest pages of each size class into the free space of the denser
// ones, rewrites every reference through the P1 forwarding records, and
// returns the emptied pages to their arenas — the heap actually shrinks,
// and the proportional growth target then adapts downward.
//
// Pinned cells sweep in place, exactly like the minor's young pins:
// conservative hits (C stack, spilled registers, generator stacks) set
// PINNED during marking, and every registered generator object pins too
// (the registry is an intrusive list of raw pointers).  LOS objects
// never move.  EJS_GC_COMPACT=off restores plain mark-sweep for A/B and
// differential runs.
static uint64_t compact_moved_objs, compact_moved_bytes, compact_freed_pages;

// TRUE while the fixup pass runs — the env guard's forwarded-env check
// keys off it (see _ejs_gc_validate_closureenv)
EJSBool in_compact_fixup;

static void
compact_fixup_slot(ejsval* slot)
{
    ejsval v = *slot;
    if (!EJSVAL_IS_TRACEABLE_IMPL(v)) return;
    GCObjectPtr p = (GCObjectPtr)EJSVAL_TO_GCTHING_IMPL(v);
    if (p == NULL) return;
    // boxed payloads are object bases, and statics outside the heap have
    // headers too, so the forwarded-bit read is always safe
    if (_ejs_gc_is_forwarded(p))
        rewrite_slot_payload(slot, _ejs_gc_forwarding_addr(p));
}

static void
compact_fixup_primstr_child(EJSPrimString** childp)
{
    GCObjectPtr p = (GCObjectPtr)*childp;
    if (p && _ejs_gc_is_forwarded(p))
        *childp = (EJSPrimString*)_ejs_gc_forwarding_addr(p);
}

static void
compact_fixup_object(GCObjectPtr p)
{
    GCObjectHeader* h = (GCObjectHeader*)p;
    if (*h & EJS_GC_HEADER_FORWARDED)
        return; // an evacuated source; its copy is walked on its own page
    *h &= ~EJS_GC_HEADER_PINNED; // pins are per-cycle
    if ((*h & EJS_SCAN_TYPE_OBJECT) != 0) {
        EJSObject* obj = (EJSObject*)p;
        if (obj->ops != NULL)
            OP(obj,Scan)(obj, compact_fixup_slot);
    }
    else if ((*h & EJS_SCAN_TYPE_PRIMSTR) != 0) {
        EJSPrimString* ps = (EJSPrimString*)p;
        switch (EJS_PRIMSTR_GET_TYPE(ps)) {
        case EJS_STRING_ROPE:
            compact_fixup_primstr_child(&ps->data.rope.left);
            compact_fixup_primstr_child(&ps->data.rope.right);
            break;
        case EJS_STRING_DEPENDENT:
            compact_fixup_primstr_child(&ps->data.dependent.dep);
            break;
        case EJS_STRING_FLAT:
            break;
        }
    }
    else if ((*h & EJS_SCAN_TYPE_PRIMSYM) != 0)
        compact_fixup_slot(&((EJSPrimSymbol*)p)->description);
    else if ((*h & EJS_SCAN_TYPE_CLOSUREENV) != 0) {
        EJSClosureEnv* env = (EJSClosureEnv*)p;
        if (EJS_UNLIKELY(_ejs_gc_env_guard))
            _ejs_gc_validate_closureenv(NULL, env, "compact_fixup");
        for (uint32_t i = 0; i < env->length; i++)
            compact_fixup_slot(&env->slots[i]);
    }
}

static EJSBool
compact_page_has_pins(PageInfo* pg)
{
    GCObjectPtr p = pg->page_start;
    for (int c = 0; c < pg->num_cells; c++, p += pg->cell_size)
        if (!cell_is_free(pg->page_bitmap[c])
            && (*(GCObjectHeader*)p & EJS_GC_HEADER_PINNED))
            return EJS_TRUE;
    return EJS_FALSE;
}

// destination cell in `bucket`: first page (from the cursor on) with
// free capacity.  Sources were detached from the bucket list before
// evacuation, so every listed page qualifies.  The selection accounting
// guarantees capacity; running dry is a bug.
static GCObjectPtr
compact_alloc_dest(int bucket, PageInfo** cursor, PageInfo** dest_page)
{
    PageInfo* pg = *cursor ? *cursor : (PageInfo*)heap_pages[bucket].head;
    while (pg && !pg->num_free_cells)
        pg = pg->next;
    if (!pg) {
        _ejs_log ("GC BUG: compaction ran out of destination space (bucket %d)\n", bucket);
        abort();
    }
    *cursor = pg;
    *dest_page = pg;
    return alloc_from_page(pg);
}

static void
compact_evacuate_page(int bucket, PageInfo* pg, PageInfo** cursor)
{
    GCObjectPtr from = pg->page_start;
    for (int c = 0; c < pg->num_cells; c++, from += pg->cell_size) {
        if (cell_is_free(pg->page_bitmap[c]))
            continue;
        PageInfo* dest_page;
        GCObjectPtr to = compact_alloc_dest(bucket, cursor, &dest_page);
        memcpy (to, from, pg->cell_size);
        // the copy is live THIS cycle: keep it marked so the coming
        // color flip turns it white with every other survivor
        cell_set_black(&dest_page->page_bitmap[PTR_TO_CELL(to, dest_page)]);
        minor_fixup_evacuated(from, to, pg->cell_size);
        _ejs_gc_forward(from, to);
        gc_watch_hit ("compact-evacuate-from", from);
        compact_moved_objs++;
        compact_moved_bytes += pg->cell_size;
    }
}

typedef struct { PageInfo* page; int live; } CompactPageStat;

static int
compact_stat_cmp(const void* a, const void* b)
{
    return ((const CompactPageStat*)a)->live - ((const CompactPageStat*)b)->live;
}

static void
compact_old_gen(void)
{
    // every registered generator pins: the registry reaches them through
    // raw intrusive pointers (reg_next/reg_prev), and their machine
    // state is re-scanned conservatively by their specops
    for (EJSGenerator* g = _ejs_generator_registry; g; g = g->reg_next)
        *(GCObjectHeader*)g |= EJS_GC_HEADER_PINNED;

    uint64_t moved_before = compact_moved_objs;
    uint64_t freed_before = compact_freed_pages;

    EJSList evac_pages;
    memset (&evac_pages, 0, sizeof(evac_pages));

    // 1. selection + evacuation, per size class: sparse-first, evacuate
    //    while the rest of the class has room
    for (int bucket = 0; bucket < HEAP_PAGELISTS_COUNT; bucket++) {
        int count = 0;
        for (PageInfo* pg = (PageInfo*)heap_pages[bucket].head; pg; pg = pg->next)
            count++;
        if (count < 2)
            continue;

        CompactPageStat* stats = (CompactPageStat*)malloc (count * sizeof(CompactPageStat));
        size_t total_free = 0;
        int n = 0;
        for (PageInfo* pg = (PageInfo*)heap_pages[bucket].head; pg; pg = pg->next) {
            stats[n].page = pg;
            stats[n].live = pg->num_cells - pg->num_free_cells;
            n++;
            total_free += pg->num_free_cells;
        }
        qsort (stats, n, sizeof(CompactPageStat), compact_stat_cmp);

        // choose the COMPLETE source set first, sparse-first: a page
        // accepted as a source leaves the destination pool, and the
        // remaining pool must hold every already-accepted live cell
        // plus this page's.  (Selecting and evacuating in one pass let
        // an early DESTINATION later be picked as a source via its
        // stale live count — evacuating more cells than the accounting
        // reserved space for.)
        size_t dest_free = total_free;
        size_t src_live = 0;
        EJSList src_pages;
        memset (&src_pages, 0, sizeof(src_pages));
        for (int i = 0; i < n; i++) {
            PageInfo* pg = stats[i].page;
            size_t live = (size_t)stats[i].live;
            if (live == 0)
                continue; // the sweep freelists empties; belt only
            if (dest_free - pg->num_free_cells < src_live + live)
                break; // the sparsest candidate doesn't fit; denser ones won't either
            if (compact_page_has_pins(pg))
                continue; // pinned cells sweep in place; the page stays a destination
            _ejs_list_detach_node (&heap_pages[bucket], (EJSListNode*)pg);
            _ejs_list_append_node (&src_pages, (EJSListNode*)pg);
            dest_free -= pg->num_free_cells;
            src_live += live;
        }

        // sources are off the bucket list now: every listed page is a
        // pure destination, so the cursor can walk it freely
        PageInfo* cursor = NULL;
        PageInfo* src;
        while ((src = (PageInfo*)src_pages.head) != NULL) {
            _ejs_list_detach_node (&src_pages, (EJSListNode*)src);
            compact_evacuate_page (bucket, src, &cursor);
            _ejs_list_append_node (&evac_pages, (EJSListNode*)src);
        }
        free (stats);
    }

    // 2. fixup: rewrite every reference that can name a moved cell, and
    //    clear the cycle's pins while walking the live set.  Runs even
    //    when nothing was evacuated — the pins must reset either way.
    //    (During this pass an owner walked before its env edge is
    //    rewritten legitimately reaches a forwarded env — the flag lets
    //    the env guard treat that transient as valid.)
    in_compact_fixup = EJS_TRUE;
    root_registry_foreach (compact_fixup_slot);
    for (int i = 0; i < _ejs_num_modules; i++) {
        EJSObject* mod = (EJSObject*)_ejs_modules[i];
        if (mod->ops)
            OP(mod,Scan)(mod, compact_fixup_slot);
    }
    // gc-frame slots' referents were all conservatively pinned (full GC
    // never skips frame records), so these rewrites are no-ops today;
    // walked anyway so precision changes can't silently break this pass
    walk_gc_frames(compact_fixup_slot);
    for (int i = 0; i < _ejs_heap.remset_count; i++) {
        GCObjectPtr o = (GCObjectPtr)_ejs_heap.remset[i];
        if (_ejs_gc_is_forwarded(o))
            _ejs_heap.remset[i] = _ejs_gc_forwarding_addr(o);
    }
    old_gen_walk (compact_fixup_object); // old pages (sources skip via FORWARDED) + LOS
    for (PageInfo* pg = (PageInfo*)heap_priv.young_pages.head; pg; pg = pg->next) {
        GCObjectPtr p = pg->page_start;
        for (int c = 0; c < CELLS_IN_PAGE(pg); c++, p += pg->cell_size)
            if (!cell_is_free(pg->page_bitmap[c]))
                compact_fixup_object(p);
    }
    in_compact_fixup = EJS_FALSE;

    // 3. release the sources: nothing reads the forwarding records
    //    anymore; the pages go back to their arenas.  No finalizers run —
    //    the objects live on at their new addresses.
    PageInfo* pg;
    while ((pg = (PageInfo*)evac_pages.head) != NULL) {
        _ejs_list_detach_node (&evac_pages, (EJSListNode*)pg);
        memset (pg->page_start, 0xa7, PAGE_SIZE); // 0xa7: FORWARDED must stay clear in poison
        memset (pg->page_bitmap, CELL_FREE, pg->num_cells * sizeof(BitmapCell));
        pg->num_free_cells = pg->num_cells;
        pg->bump_ptr = pg->page_start;
        Arena* arena = (Arena*)PTR_TO_ARENA(pg->page_start);
        EJS_LIST_PREPEND (pg, arena->free_pages);
        compact_freed_pages++;
    }

    if (gc_profile)
        _ejs_log ("EJS_GC_PROFILE: compact: moved=%llu freed-pages=%llu\n",
                  (unsigned long long)(compact_moved_objs - moved_before),
                  (unsigned long long)(compact_freed_pages - freed_before));
}
// ============== end mostly-copying major compaction ======================

void
_ejs_gc_collect_inner(EJSBool shutting_down)
{
#if gc_timings > 1
    struct timeval tvbefore, tvafter;
#endif

    // very simple stop the world collector
    SPEW(1, _ejs_log ("collection started\n"));

    num_roots = 0;
    white_objs = 0;
    large_objs = 0;
    total_objs = 0;

    // full collections need young pages in bitmap-authoritative
    // form (active bump pages have no valid FREE bits or counts)
    young_normalize_for_full_gc();

#if gc_timings > 1
    gettimeofday (&tvbefore, NULL);
#endif

    struct timeval prof_tv_begin, prof_tv_end;
    if (gc_profile)
        gettimeofday (&prof_tv_begin, NULL);

    struct timeval fg[8];
    if (!shutting_down) {
        gettimeofday (&fg[0], NULL);
        mark_from_roots();

        total_objs = num_roots;

        mark_from_modules();
        gettimeofday (&fg[1], NULL);

        mark_thread_stack();

        mark_generator_stacks();
        gettimeofday (&fg[2], NULL);

        // dirty objects await their deferred minor scan and may
        // hold the only reference to young data — root them
        for (int i = 0; i < _ejs_heap.remset_count; i++)
            mark_object_root((GCObjectPtr)_ejs_heap.remset[i]);
        gettimeofday (&fg[3], NULL);

        process_worklist();
        gettimeofday (&fg[4], NULL);

        // survival + pin census must walk the heap BEFORE the
        // sweep frees the white cells
        if (gc_profile)
            profile_pre_sweep();
        gettimeofday (&fg[5], NULL);
        if (gc_profile) {
#define FGUS(a,b) ((long long)(((b).tv_sec - (a).tv_sec) * 1000000LL + ((b).tv_usec - (a).tv_usec)))
            _ejs_log ("EJS_GC_PROFILE: full-gc phases: roots+modules=%lldus stacks=%lldus remset-roots=%lldus (remset=%d) worklist=%lldus census=%lldus\n",
                      FGUS(fg[0],fg[1]), FGUS(fg[1],fg[2]), FGUS(fg[2],fg[3]),
                      _ejs_heap.remset_count, FGUS(fg[3],fg[4]), FGUS(fg[4],fg[5]));
#undef FGUS
        }
    }

#if gc_timings > 1
    gettimeofday (&tvafter, NULL);
#endif

#if gc_timings > 1
    {
        uint64_t usec_before = tvbefore.tv_sec * 1000000 + tvbefore.tv_usec;
        uint64_t usec_after = tvafter.tv_sec * 1000000 + tvafter.tv_usec;

        _ejs_log ("gc scan took %gms\n", (usec_after - usec_before) / 1000.0);
    }
#endif

#if gc_timings > 1
    gettimeofday (&tvbefore, NULL);
#endif

    sweep_heap();

    // mostly-copying: evacuate the sparse pages' unpinned live
    // cells, rewrite every reference, return emptied pages to their
    // arenas.  (Skipped on the shutdown collection — nothing left to
    // move for.)
    if (compact_enabled && !shutting_down)
        compact_old_gen();

    // the remembered state may dangle into cells this sweep just
    // freed — rebuild it from the live old gen
    if (!shutting_down)
        remset_rebuild_after_full_gc();

    if (gc_profile && !shutting_down) {
        gettimeofday (&prof_tv_end, NULL);
        uint64_t usec = (prof_tv_end.tv_sec - prof_tv_begin.tv_sec) * 1000000ULL
            + (prof_tv_end.tv_usec - prof_tv_begin.tv_usec);
        profile_report_cycle_end (usec);
    }

#if gc_timings > 1
    {
        gettimeofday (&tvafter, NULL);
    }
#endif

#if gc_timings > 1
    {
        uint64_t usec_before = tvbefore.tv_sec * 1000000 + tvbefore.tv_usec;
        uint64_t usec_after = tvafter.tv_sec * 1000000 + tvafter.tv_usec;

        _ejs_log ("gc sweep took %gms\n", (usec_after - usec_before) / 1000.0);
    }
#endif

#if gc_timings > 1
    _ejs_log ("_ejs_gc_collect stats:\n");
    _ejs_log ("   num_roots: %d\n", num_roots);
    _ejs_log ("   total objects: %d\n", total_objs);
    _ejs_log ("   num large objects: %d\n", large_objs);
    _ejs_log ("   garbage objects: %d\n", white_objs);
#endif

    // age the survivors: this epoch's black is next epoch's white
    mark_epoch_advance();

    if (shutting_down) {
        root_registry_shutdown();

        SPEW(1, _ejs_log ("final gc page statistics:\n");
             for (int hp = 0; hp < HEAP_PAGELISTS_COUNT; hp++) {
                 int len = 0;

                 EJS_LIST_FOREACH (&heap_pages[hp], PageInfo, page, {
                         len ++;
                 });

                 _ejs_log ("  size: %d     pages: %d\n", 1<<(hp + 3), len);
             });
    }
#if sanity
    else {
        for (int hp = 0; hp < HEAP_PAGELISTS_COUNT; hp++) {
            EJS_LIST_FOREACH (&heap_pages[hp], PageInfo, page, {
                for (int c = 0; c < CELLS_IN_PAGE (page); c ++) {
                    if (!cell_is_free(page->page_bitmap[c]) && !cell_is_white(page->page_bitmap[c]))
                        continue;
                }  
            })
        }
    }
#endif
    SPEW(1, _ejs_log ("collection finished\n"));
}

// heap footprint measured after the last collection's sweep.  The
// collection trigger scales with this: a fixed allocation budget on a
// growing live set makes total GC work quadratic in heap size (shapes
// shapes moved per-object property storage into the GC heap, which pushed
// stage2's self-compile off that cliff — hours of back-to-back full
// marks of a ~900MB heap).  Letting the heap grow ~gc_growth_pct%
// between full collections keeps total mark work linear (see
// full_gc_trigger; compaction shrinks this after a drop in live set,
// so the cadence adapts back down too).
size_t heap_size_at_last_gc = 0;

void
_ejs_gc_collect(const char *reason)
{
    SPEW(1, _ejs_log ("_ejs_gc_collect(%s)\n", reason));
    prof_gc_reason = reason;
#if gc_timings > 0
    struct timeval tvbefore, tvafter;

    gettimeofday (&tvbefore, NULL);

    int heap_size = calc_heap_size();
#endif

    _ejs_gc_collect_inner(EJS_FALSE);

    // post-sweep footprint drives the proportional collection trigger
    // (see heap_size_at_last_gc)
    heap_size_at_last_gc = calc_heap_size();

#if gc_timings > 0
    gettimeofday (&tvafter, NULL);

    uint64_t usec_before = tvbefore.tv_sec * 1000000 + tvbefore.tv_usec;
    uint64_t usec_after = tvafter.tv_sec * 1000000 + tvafter.tv_usec;

    _ejs_log ("gc collect took %gms\n", (usec_after - usec_before) / 1000.0);
    _ejs_log ("   for a heap size of %zdMB\n", heap_size/(1024*1024));
#if gc_timings > 1
    _ejs_gc_dump_heap_stats();
#endif
#endif
}
