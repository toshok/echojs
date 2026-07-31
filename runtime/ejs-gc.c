/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

// the collector core: lifecycle API (init/alloc/shutdown), the cell
// free path, the root registry, the collection policy, the write-
// barrier entry points, and the GC JS object.  The module map lives
// in ejs-gc-internal.h.

#include "ejs-gc-internal.h"

// exceptions to throw if we're out of memory
static ejsval los_allocation_failed_exc EJSVAL_ALIGNMENT;
static ejsval page_allocation_failed_exc EJSVAL_ALIGNMENT;

EJSBool gc_disabled;
int collect_every_alloc = 0;

// the cell-lifecycle epoch (ejs-gc-internal.h owns the encoding);
// parity 1 at startup so black starts at color 1
unsigned int mark_epoch = 1;

// ---- the root registry -----------------------------------------
//
// Registered roots are the addresses of ejsval slots in static or
// malloc'd storage (atoms, well-knowns, the OOM exceptions).  A
// growable array: O(1) add, swap-with-last remove, and ONE iteration
// helper every collector phase shares — full-GC mark, minor
// evacuation, compaction fixup, and the debug walks see the same set
// by construction.  (The predecessor was a malloc'd linked list with
// five hand-rolled walks.)
static ejsval** root_registry;
static int root_registry_count;
static int root_registry_capacity;

void
root_registry_foreach(void (*fn)(ejsval*))
{
    for (int i = 0; i < root_registry_count; i++)
        fn(root_registry[i]);
}

// the shutdown collection NULLs every root before the final sweep
void
root_registry_shutdown(void)
{
    for (int i = 0; i < root_registry_count; i++)
        *root_registry[i] = _ejs_null;
    free (root_registry);
    root_registry = NULL;
    root_registry_count = root_registry_capacity = 0;
}

// The compacting major (EJS_GC_COMPACT=off for A/B) and THE
// full-collection growth knob — a full GC triggers when old-gen growth
// since the last one exceeds gc_growth_pct percent of the post-sweep
// footprint (floor: two arenas, so small programs keep a sane cadence).
// The knob replaces the old fixed 60MB constant; with compaction
// shrinking the heap, the trigger now adapts in BOTH directions.
EJSBool compact_enabled;
static int gc_growth_pct = 50;

static size_t
full_gc_trigger(void)
{
    size_t t = heap_size_at_last_gc * (size_t)gc_growth_pct / 100;
    size_t floor_ = 2 * (size_t)ARENA_SIZE;
    return t > floor_ ? t : floor_;
}

// ---- the collection policy -------------------------------------
//
// Every collection the runtime initiates on its own behalf is decided
// HERE (GC.collect() and the shutdown collection are driver requests,
// not policy).  Two inputs: old-gen growth since the last full
// collection — alloc_size - alloc_size_at_last_gc, promotions included
// — against full_gc_trigger(), and the EJS_GC_EVERY_N_ALLOC stress
// knob (minor cadence in nursery mode, full cadence in old mode).
// Each event preserves its historical baseline/counter resets exactly:
// AFTER_MINOR deliberately leaves num_allocs alone (the stress-minor
// cadence owns it), and ALLOC_FAILED collects even under
// EJS_GC_DISABLE — it is the allocator's last resort before throwing.
// (GCPolicyEvent lives in ejs-gc-internal.h; the minor collection
// reports AFTER_MINOR from its retirement path.)
void
gc_policy(GCPolicyEvent ev, const char* reason)
{
    if (ev == GC_POLICY_ALLOC_FAILED) {
        _ejs_gc_collect (reason);
        alloc_size_at_last_gc = alloc_size;
        num_allocs = 0;
        return;
    }

    if (gc_disabled)
        return;

    switch (ev) {
    case GC_POLICY_YOUNG_ALLOC:
        if (collect_every_alloc && collect_every_alloc == num_allocs) {
            num_allocs = 0;
            _ejs_gc_minor_collect ("every_n_alloc");
        }
        break;
    case GC_POLICY_OLD_ALLOC:
        if (alloc_size - alloc_size_at_last_gc >= full_gc_trigger()) {
            _ejs_gc_collect ("alloc_size");
            alloc_size_at_last_gc = alloc_size;
            num_allocs = 0;
        }
        else if (!nursery_enabled && collect_every_alloc && collect_every_alloc == num_allocs) {
            _ejs_gc_collect ("every_n_alloc");
            alloc_size_at_last_gc = alloc_size;
            num_allocs = 0;
        }
        break;
    case GC_POLICY_AFTER_MINOR:
        // when nearly every allocation is young, this is the only
        // place the growth trigger can fire
        if (alloc_size - alloc_size_at_last_gc >= full_gc_trigger()) {
            _ejs_gc_collect ("promotion growth");
            alloc_size_at_last_gc = alloc_size;
        }
        break;
    case GC_POLICY_ALLOC_FAILED: // handled above
        break;
    }
}

void
finalize_object(GCObjectPtr p)
{
    GCObjectHeader* headerp = (GCObjectHeader*)p;
    if ((*headerp & EJS_SCAN_TYPE_OBJECT) != 0) {
        SPEW(2, _ejs_log ("finalizing object %p(%s)\n", p, CLASSNAME(p)));
        OP(p,Finalize)((EJSObject*)p);
    }
    else if ((*headerp & EJS_SCAN_TYPE_CLOSUREENV) != 0) {
        SPEW(2, _ejs_log ("finalizing closureenv %p\n", p));
    }
    else if ((*headerp & EJS_SCAN_TYPE_PRIMSTR) != 0) {
        EJSPrimString* primstr = (EJSPrimString*)p;
        if (EJS_PRIMSTR_GET_TYPE(primstr) == EJS_STRING_FLAT) {
            SPEW(2, {
                    char* utf8 = ucs2_to_utf8(primstr->data.flat);
                    SPEW(2, _ejs_log ("finalizing flat primitive string %p(%s)\n", p, utf8));
                    free (utf8);
                });
            if (EJS_PRIMSTR_HAS_OOL_BUFFER(primstr))
                free(primstr->data.flat);
        }
        else {
            SPEW(2, _ejs_log ("finalizing primitive string %p\n", p));
        }
    }
    else if ((*headerp & EJS_SCAN_TYPE_PRIMSYM) != 0) {
        SPEW(2, _ejs_log ("finalizing primitive symbol %p\n", p));
    }
}

void
_ejs_finalize_obj(GCObjectPtr ptr, Arena* arena, PageInfo* info, uint32_t cell_idx)
{
    EJS_ASSERT(info);
    if (cell_is_free(info->page_bitmap[cell_idx])) {
        return;
    }

    finalize_object(ptr);
    memset (ptr,
#if clear_on_finalize
            0x00,
#else
            0xaf,
#endif
            info->cell_size);

    cell_set_free(&info->page_bitmap[cell_idx]);
    SPEW(3, _ejs_log ("finalized object %p in page %p, num_free_cells == %zd\n", ptr, info, info->num_free_cells + 1));
    // if this page is empty, move it to this arena's free list
    LOCK_PAGE(info);
    info->num_free_cells ++;
    UNLOCK_PAGE(info);

    if (info->num_free_cells == info->num_cells) {
        LOCK_GC();
        if (info->num_free_cells == info->num_cells) {
            if (info->los_info) {
                SPEW(2, _ejs_log ("releasing large object (size %zd)!\n", info->los_info->alloc_size));
                release_to_los (info->los_info);
            }
            else if (info->young) {
                // a young survivor page emptied by a FULL sweep lives on
                // heap_priv.young_pages, not a heap_pages bucket —
                // detaching from the bucket list would silently unlink
                // it from its young_pages neighbors while leaving that
                // list's head/tail stale
                young_page_freed (info, arena);
            }
            else {
                EJS_ASSERT(arena);
                SPEW(2, _ejs_log ("page %p is empty, putting it on the free list\n", info));
                LOCK_PAGE(info);
                // the page is empty, add it to the arena's free page list.
                int bucket = ffs(info->cell_size) - OBJECT_SIZE_LOW_LIMIT_BITS;
                _ejs_list_detach_node (&heap_pages[bucket], (EJSListNode*)info);
                EJS_LIST_PREPEND (info, arena->free_pages);
                UNLOCK_PAGE(info);
            }
        }
        UNLOCK_GC();
    }
}

void
_ejs_gc_init()
{
    gc_disabled = getenv("EJS_GC_DISABLE") != NULL;
    char* n_allocs = getenv("EJS_GC_EVERY_N_ALLOC");
    if (n_allocs)
        collect_every_alloc = atoi(n_allocs);

    // allocation/survival/pin instrumentation.  The summary
    // goes through atexit because _ejs_gc_shutdown is compiled out by
    // default (GC_ON_SHUTDOWN in main.c).
    gc_profile = getenv("EJS_GC_PROFILE") != NULL;
    gettimeofday (&prof_start_tv, NULL);
    if (gc_profile)
        atexit (profile_report_shutdown);

    // the compacting major is the default; EJS_GC_COMPACT=off
    // restores plain mark-sweep for A/B and differential runs
    {
        char* e = getenv("EJS_GC_COMPACT");
        compact_enabled = !(e && (strcmp(e, "off") == 0 || strcmp(e, "0") == 0));
    }

    // THE growth knob (knob census = 1): a full collection
    // triggers when old-gen growth exceeds EJS_GC_GROWTH percent of the
    // post-sweep footprint
    {
        char* growth = getenv("EJS_GC_GROWTH");
        if (growth) gc_growth_pct = atoi(growth);
        if (gc_growth_pct <= 0) gc_growth_pct = 50;
    }

    // the forwarding helpers are inert until the mover, so
    // exercise them here on a scratch buffer when asked — a build whose
    // header layout breaks the forwarding contract fails loudly instead
    // of waiting for the collector to discover it.
    if (getenv("EJS_GC_SELFTEST")) {
        uint64_t scratch[2] = { EJS_SCAN_TYPE_OBJECT, 0 };
        uint64_t target[2] = { 0, 0 };
        EJS_ASSERT(!_ejs_gc_is_forwarded(&scratch));
        _ejs_gc_forward(&scratch, &target);
        EJS_ASSERT(_ejs_gc_is_forwarded(&scratch));
        EJS_ASSERT(_ejs_gc_forwarding_addr(&scratch) == (GCObjectPtr)&target);
        _ejs_log ("EJS_GC_SELFTEST: forwarding helpers ok\n");
    }

    // the arena reservation + initial arenas (ejs-gc-heap.c)
    heap_space_init();

    _ejs_gc_worklist_init();

    // the generational nursery (EJS_GC_NURSERY=off selects
    // the old single-generation collector for A/B and differential runs)
    nursery_init();
}

void
_ejs_gc_allocate_oom_exceptions()
{
    _ejs_gc_add_root (&los_allocation_failed_exc);
    los_allocation_failed_exc  = _ejs_nativeerror_new_utf8 (EJS_ERROR, "LOS allocation failed");

    _ejs_gc_add_root (&page_allocation_failed_exc);
    page_allocation_failed_exc = _ejs_nativeerror_new_utf8 (EJS_ERROR, "page allocation failed");
}

static int num_object_allocs = 0;
static int num_closureenv_allocs = 0;
static int num_primstr_allocs = 0;
static int num_primsym_allocs = 0;

int total_allocs = 0;

void
_ejs_gc_shutdown()
{
    _ejs_gc_collect_inner(EJS_TRUE);
    SPEW(1, _ejs_log ("total allocs = %d\n", total_allocs));

    if (gc_profile)
        profile_report_shutdown();

    _ejs_log ("gc allocation stats (_ejs_gc_shutdown):\n");
    _ejs_log ("  objects: %d\n", num_object_allocs);
    _ejs_log ("  closureenv: %d\n", num_closureenv_allocs);
    _ejs_log ("  primstr: %d\n", num_primstr_allocs);
    _ejs_log ("  primsym: %d\n", num_primsym_allocs);
}

/* Compute the smallest power of 2 that is >= x. */
static inline size_t
pow2_ceil(size_t x)
{

	x--;
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;
#if (SIZEOF_PTR == 8)
	x |= x >> 32;
#endif
	x++;
	return (x);
}

size_t alloc_size = 0;
int num_allocs = 0;
size_t alloc_size_at_last_gc = 0;

GCObjectPtr
_ejs_gc_alloc(size_t size, EJSScanType scan_type)
{
    GCObjectPtr rv = NULL;

    num_allocs ++;
    total_allocs ++;

    switch (scan_type) {
    case EJS_SCAN_TYPE_PRIMSTR: num_primstr_allocs ++; break;
    case EJS_SCAN_TYPE_PRIMSYM: num_primsym_allocs ++; break;
    case EJS_SCAN_TYPE_OBJECT: num_object_allocs ++; break;
    case EJS_SCAN_TYPE_CLOSUREENV: num_closureenv_allocs ++; break;
    }

    int bucket;
    int bucket_size = MAX(pow2_ceil(size), 1<<OBJECT_SIZE_LOW_LIMIT_BITS);

    bucket = ffs(bucket_size);

    if (gc_profile)
        profile_note_alloc(size, bucket, scan_type);

    // nursery-eligible allocations bump-allocate in the young
    // arena and do NOT feed alloc_size (the full-GC trigger tracks
    // OLD-gen growth: promotions and direct old allocations).  The
    // every-N stress knob triggers MINOR collections here — the full-GC
    // stress semantics of old mode are unchanged (below).
    if (nursery_enabled && !gc_disabled && bucket <= OBJECT_SIZE_HIGH_LIMIT_BITS + 1) {
        if (in_minor_gc) {
            _ejs_log ("GC BUG: young allocation during a minor collection\n");
            abort();
        }
        gc_policy (GC_POLICY_YOUNG_ALLOC, NULL);
        int idx = bucket - OBJECT_SIZE_LOW_LIMIT_BITS - 1; // 16B -> 0
        void* p = _ejs_heap.bump[idx];
        if (EJS_LIKELY((char*)p + bucket_size <= (char*)_ejs_heap.limit[idx])) {
            _ejs_heap.bump[idx] = (char*)p + bucket_size;
            memset (p, 0, bucket_size);
            *(GCObjectHeader*)p = scan_type | EJS_GC_HEADER_YOUNG;
            gc_watch_hit ("young-alloc-fast", p);
            return p;
        }
        rv = young_alloc_slow(idx, bucket_size, scan_type);
        if (rv) { gc_watch_hit ("young-alloc-slow", rv); return rv; }
        // nursery unusable (pathologically pinned): fall through to the
        // old allocator
    }

    alloc_size += size;

    gc_policy (GC_POLICY_OLD_ALLOC, NULL);

    retry_allocation:
    {
    if (bucket > OBJECT_SIZE_HIGH_LIMIT_BITS + 1) {
        SPEW(2, _ejs_log ("need to alloc %zd from los!!!\n", size));
        rv = alloc_from_los(size, scan_type);
        if (rv && nursery_enabled) {
            // LOS objects are old at birth: their construction stores
            // bypass the barrier, so they start DIRTY and get a precise
            // scan at the next minor
            _ejs_gc_remember_slow(rv);
        }
        if (rv == NULL) {
            if (num_allocs == 0) {
                _ejs_log ("los allocation (size = %d) failed twice, throwing", size);
                _ejs_throw (los_allocation_failed_exc);
            }
            else {
                _ejs_log ("los allocation (size = %d) failed, trying to collect", size);
                UNLOCK_GC();
                gc_policy (GC_POLICY_ALLOC_FAILED, "los allocation fail");
                goto retry_allocation;
            }
        }
        return rv;
    }

    bucket -= OBJECT_SIZE_LOW_LIMIT_BITS;

    LOCK_GC();

    PageInfo* info = (PageInfo*)heap_pages[bucket].head;
    if (!info || !info->num_free_cells) {
        info = alloc_new_page(bucket_size);
        if (info == NULL) {
            if (num_allocs == 0) {
                _ejs_throw (page_allocation_failed_exc);
            }
            else {
                _ejs_log ("page allocation failed, trying to collect");
                UNLOCK_GC();
                gc_policy (GC_POLICY_ALLOC_FAILED, "page allocation fail");
                goto retry_allocation;
            }
        }
        _ejs_list_prepend_node (&heap_pages[bucket], (EJSListNode*)info);
    }

    rv = alloc_from_page(info);
    // zero the cell: recycled cells are filled with 0xaf on finalize, and a
    // collection can scan this object before its constructor initializes it
    // (any allocation between _ejs_gc_alloc and _ejs_init_object can
    // trigger one).  zeroed contents are inert to the scanner.
    memset (rv, 0, info->cell_size);
    *((GCObjectHeader*)rv) = scan_type | EJS_GC_HEADER_YOUNG;

    if (info->num_free_cells == 0) {
        // if the page is full, bump it to the end of the list (if there's more than 1 page in the list)
        if (heap_pages[bucket].head != heap_pages[bucket].tail) {
            _ejs_list_pop_head (&heap_pages[bucket]);
            _ejs_list_append_node (&heap_pages[bucket], (EJSListNode*)info);
        }
    }

    UNLOCK_GC();
    }
    return rv;
}

void
_ejs_gc_add_root(ejsval* root)
{
    if (root_registry_count == root_registry_capacity) {
        root_registry_capacity = root_registry_capacity ? root_registry_capacity * 2 : 512;
        root_registry = realloc (root_registry, root_registry_capacity * sizeof(ejsval*));
    }
    root_registry[root_registry_count++] = root;
}

void
_ejs_gc_remove_root(ejsval* root)
{
    for (int i = 0; i < root_registry_count; i++) {
        if (root_registry[i] == root) {
            root_registry[i] = root_registry[--root_registry_count];
            return;
        }
    }
}

// object-remembering barrier: mark `owner` dirty and queue it for
// the next minor's rescan.  The inline half (ejs-gc.h) already filtered
// non-young values, young owners, and already-dirty owners.
void
_ejs_gc_remember_slow(void* owner)
{
    GCObjectHeader* h = (GCObjectHeader*)owner;
    *h |= EJS_GC_HEADER_DIRTY;
    EJSHeapContext* c = &_ejs_heap;
    if (EJS_LIKELY(c->remset_count < c->remset_capacity))
        c->remset[c->remset_count++] = owner;
    else
        c->remset_overflowed = 1;
}

// the emitted barrier's out-of-line half: emit.ts inlines only the
// value-is-young range check (double payloads may false-positive; the
// full filter reruns here)
void
_ejs_gc_remember_val(ejsval owner, ejsval val)
{
    void* p = (void*)EJSVAL_TO_GCTHING_IMPL(owner);
    if (p) _ejs_gc_remember(p, val);
}

/////////
ejsval _ejs_GC;

static EJS_NATIVE_FUNC(_ejs_GC_collect) {
    _ejs_gc_collect("GC.collect called");
    return _ejs_undefined;
}

// committed old-gen page bytes (the compaction gate's observable:
// this number DROPS when the heap shrinks)
static EJS_NATIVE_FUNC(_ejs_GC_heapSize) {
    return NUMBER_TO_EJSVAL((double)calc_heap_size());
}

static EJS_NATIVE_FUNC(_ejs_GC_dumpAllocationStats) {
    char* tag = NULL;

    if (argc > 0) {
        tag = ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(args[0]));
    }

    if (tag) {
        _ejs_log ("gc allocation stats (%s):\n", tag);
    }
    else {
        _ejs_log ("gc allocation stats:\n");
    }

    _ejs_log ("  objects: %d\n", num_object_allocs);
    _ejs_log ("  closureenv: %d\n", num_closureenv_allocs);
    _ejs_log ("  primstr: %d\n", num_primstr_allocs);

    num_object_allocs = 0;
    num_closureenv_allocs = 0;
    num_primstr_allocs = 0;

    if (tag) free (tag);

    return _ejs_undefined;
}

static EJS_NATIVE_FUNC(_ejs_GC_dumpLiveStrings) {
    _ejs_log ("strings:\n");
    for (int i = 0; i < HEAP_PAGELISTS_COUNT; i ++) {
        EJS_LIST_FOREACH (&heap_pages[i], PageInfo, page, {
            GCObjectPtr p = page->page_start;
            for (int c = 0; c < CELLS_IN_PAGE (page); c ++, p += page->cell_size) {
                if (cell_is_free(page->page_bitmap[c]))
                    continue;
                GCObjectHeader* headerp = (GCObjectHeader*)p;
                
                if ((*headerp & EJS_SCAN_TYPE_PRIMSTR) == 0)
                    continue;

                EJSPrimString* primstr = (EJSPrimString*)p;
                _ejs_log(" [%p len = %d]", primstr, primstr->length);
                switch (EJS_PRIMSTR_GET_TYPE(primstr)) {
                case EJS_STRING_DEPENDENT:
                    _ejs_log (" dependent %p, off %d : ", primstr->data.dependent.dep, primstr->data.dependent.off);
                    break;
                case EJS_STRING_ROPE:
                    _ejs_log (" rope %p, %p : ", primstr->data.rope.left, primstr->data.rope.right);
                    break;
                case EJS_STRING_FLAT:
                    _ejs_log ("%s ", EJS_PRIMSTR_HAS_OOL_BUFFER(primstr) ? " ool" : "");
                    break;
                }
                char* utf8 = _ejs_string_to_utf8(primstr);
                char buf[256];
                if (primstr->length > 256) {
                    memmove (buf, primstr, 252);
                    buf[252] = '.';
                    buf[253] = '.';
                    buf[254] = '.';
                    buf[255] = 0;
                }
                else {
                    memmove (buf, utf8, primstr->length);
                    buf[primstr->length] = 0;
                }
                _ejs_logstr (buf);
            }
        });
    }

    // XXX
    return _ejs_undefined;
}

void
_ejs_GC_init(ejsval ejs_obj)
{
    _ejs_GC = _ejs_object_new (_ejs_Object_prototype, &_ejs_Object_specops);
    _ejs_object_setprop (ejs_obj, _ejs_atom_GC, _ejs_GC);

#define OBJ_METHOD(x) EJS_INSTALL_ATOM_FUNCTION(_ejs_GC, x, _ejs_GC_##x)

    OBJ_METHOD(collect);
    OBJ_METHOD(heapSize);
    OBJ_METHOD(dumpAllocationStats);
    OBJ_METHOD(dumpLiveStrings);

#undef OBJ_METHOD
}

