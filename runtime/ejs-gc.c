/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <setjmp.h>

#include "ejs-gc.h"
#include "ejs-function.h"
#include "ejs-generator.h"
#include "ejs-arguments.h"
#include "ejs-shapes.h"
#include "ejs-value.h"
#include "ejs-string.h"
#include "ejs-symbol.h"
#include "ejs-error.h"
#include "ejs-ops.h"
#include "ejsval.h"
#include "ejs-module.h"

#define clear_on_finalize 0

#define spew 0
#define sanity 0
#define gc_timings 0

#if spew
static int _ejs_spew_level = (spew);
#define SPEW(level,x) do { if ((level) < _ejs_spew_level) { x; } } while (0)
#else
#define SPEW(level,x)
#endif
#if sanity
#define SANITY(x) x
#else
#define SANITY(x)
#endif

// exceptions to throw if we're out of memory
static ejsval los_allocation_failed_exc EJSVAL_ALIGNMENT;
static ejsval page_allocation_failed_exc EJSVAL_ALIGNMENT;

void _ejs_gc_dump_heap_stats();

#if EJS_BITS_PER_WORD == 64
// 2GB
#define MAX_HEAP_SIZE (2LL * 1024LL * 1024LL * 1024LL)
#else
// 128MB
#define MAX_HEAP_SIZE (128LL * 1024LL * 1024LL)
#endif

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

#define USABLE_PAGE_SIZE PAGE_SIZE

#define CELLS_OF_SIZE(size) (USABLE_PAGE_SIZE / (size))
#define CELLS_IN_PAGE(page) CELLS_OF_SIZE((page)->cell_size)

// arenas are reserved in ARENA_PAGES * PAGE_SIZE chunks.  ARENA_PAGES=4096 gives us an arena size of 32MB
#define ARENA_PAGES 8192
#define ARENA_SIZE (PAGE_SIZE*ARENA_PAGES)

#define PTR_TO_ARENA_MASK (uintptr_t)(~(ARENA_SIZE-1))

// turn a random pointer into an arena pointer
#define PTR_TO_ARENA(ptr) ((void*)((uintptr_t)(ptr) & PTR_TO_ARENA_MASK))
#define PTR_TO_ARENA_PAGE_BASE(ptr) ((void*)EJS_ALIGN(PTR_TO_ARENA(ptr) + sizeof(Arena), PAGE_SIZE))
#define PTR_TO_ARENA_PAGE_INDEX(ptr) ((((uintptr_t)(ptr) & ~PTR_TO_ARENA_MASK) - ((uintptr_t)PTR_TO_ARENA_PAGE_BASE(ptr) & ~PTR_TO_ARENA_MASK)) / PAGE_SIZE)

#define PTR_TO_CELL(ptr,info) (((char*)(ptr) - (char*)(info)->page_start) / (info)->cell_size)

#define OBJ_TO_PAGE(o) ((o) & ~PAGE_SIZE)

#define IS_ALIGNED_TO(v,a) (((uintptr_t)(v) & ((a)-1)) == 0)
#define ALLOC_ALIGN 8
#define EJS_ALIGN(v,a) (((uintptr_t)(v) + (a)-1) & ~((a)-1))
#define IS_ALLOC_ALIGNED(v) IS_ALIGNED_TO(v, ALLOC_ALIGN)

#if IOS || OSX
#include <mach/vm_statistics.h>
#define MAP_FD VM_MAKE_TAG (VM_MEMORY_APPLICATION_SPECIFIC_16)
#else
#define MAP_FD -1
#endif

EJSBool gc_disabled;
int collect_every_alloc = 0;

// two header bits from the gc-reserved range (57-63; see ejs-types.h).
// YOUNG: set at allocation, cleared on first survival (profiling) or
// promotion (the nursery).  PINNED: set on every conservative hit during
// a full collection — the compacting major must sweep that cell in
// place; cleared by compaction's fixup walk (or the profile census when
// compaction is off).
#define EJS_GC_HEADER_YOUNG  (1ULL << 57)
#define EJS_GC_HEADER_PINNED (1ULL << 58)

#if CONCURRENT
#error "not implemented"
#else
#define LOCK_PAGE(info)
#define UNLOCK_PAGE(info)
#define LOCK_GC()
#define UNLOCK_GC()
#define LOCK_ARENAS()
#define UNLOCK_ARENAS()
#endif


#define MAX_WORKLIST_SEGMENT_SIZE 512
typedef struct _WorkListSegmnt {
    EJS_SLIST_HEADER(struct _WorkListSegmnt);
    int size;
    GCObjectPtr work_list[MAX_WORKLIST_SEGMENT_SIZE];
} WorkListSegment;

typedef struct {
    WorkListSegment *list;
    WorkListSegment *free_list;
} WorkList;

static WorkList work_list;

static void
_ejs_gc_worklist_init()
{
    work_list.list = NULL;
    work_list.free_list = NULL;
}

static void
_ejs_gc_worklist_push(GCObjectPtr obj)
{
    if (obj == NULL)
        return;

    WorkListSegment *segment;

    if (EJS_UNLIKELY(!work_list.list || work_list.list->size == MAX_WORKLIST_SEGMENT_SIZE)) {
        // we need a new segment
        if (work_list.free_list) {
            // take one from the free list
            segment = work_list.free_list;
            EJS_SLIST_DETACH_HEAD(segment, work_list.free_list);
        }
        else {
            segment = (WorkListSegment*)malloc (sizeof(WorkListSegment));
            segment->size = 0;
        }
        EJS_SLIST_ATTACH(segment, work_list.list);
    }
    else {
        segment = work_list.list;
    }

    segment->work_list[segment->size++] = obj;
}

static GCObjectPtr
_ejs_gc_worklist_pop()
{
    if (work_list.list == NULL || work_list.list->size == 0/* shouldn't happen, since we push the page to the free list if we hit 0 */)
        return NULL;

    WorkListSegment *segment = work_list.list;

    GCObjectPtr rv = segment->work_list[--segment->size];
    if (segment->size == 0) {
        EJS_SLIST_DETACH_HEAD(segment, work_list.list);
        EJS_SLIST_ATTACH(segment, work_list.free_list);
    }
    return rv;
}

#define WORKLIST_PUSH_AND_GRAY(x) EJS_MACRO_START        \
    if (is_white((GCObjectPtr)x)) {                      \
        _ejs_gc_worklist_push((GCObjectPtr)(x));         \
        set_gray ((GCObjectPtr)(x));                     \
    }                                                    \
    EJS_MACRO_END

#define WORKLIST_PUSH_AND_GRAY_CELL(x, cell) EJS_MACRO_START    \
    if (cell_is_white(cell)) {                                       \
        _ejs_gc_worklist_push((GCObjectPtr)(x));                \
        cell_set_gray(&cell);                                        \
    }                                                           \
    EJS_MACRO_END

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

static void
root_registry_foreach(void (*fn)(ejsval*))
{
    for (int i = 0; i < root_registry_count; i++)
        fn(root_registry[i]);
}

#ifndef MAP_NORESERVE
#define MAP_NORESERVE 0
#endif

// GC-heap pointers get NaN-boxed into a 47-bit ejsval payload, so every
// page must map below 2^47.  macOS hands out low addresses naturally;
// linux (48-bit VA, top-down mmap) does not — ask for a hinted region
// and bump the hint as regions fill.
static void*
mmap_boxable(size_t size, int prot, int extra_flags)
{
#ifdef TARGET_LINUX
    static uintptr_t hint = 0x280000000000UL; // well below 2^47
    for (int tries = 0; tries < 64; tries++) {
        void* res = mmap((void*)hint, size, prot, MAP_ANON | MAP_PRIVATE | extra_flags, MAP_FD, 0);
        if (res == MAP_FAILED) return NULL;
        if (((uintptr_t)res + size) < (1UL << 47)) {
            hint = (uintptr_t)res + size;
            return res;
        }
        // unboxable address: drop it and try a fresh hint
        munmap(res, size);
        hint += 0x100000000UL; // 4GB stride
    }
    return NULL;
#else
    void* res = mmap(NULL, size, prot, MAP_ANON | MAP_PRIVATE | extra_flags, MAP_FD, 0);
    return res == MAP_FAILED ? NULL : res;
#endif
}

static void*
alloc_from_os(size_t size)
{
    size = MAX(size, PAGE_SIZE);
    void* res = mmap_boxable(size, PROT_READ | PROT_WRITE, 0);
    SPEW(2, _ejs_log ("mmap = %p\n", res));
    return res;
}

static void
release_to_os(void* ptr, size_t size)
{
    munmap (ptr, size);
}

typedef struct _LargeObjectInfo LargeObjectInfo;
static void release_to_los (LargeObjectInfo *lobj);

typedef struct _PageInfo PageInfo;
typedef struct _Arena {
    void*     end;
    void*     pos;
    PageInfo* free_pages;
    void*     pages[ARENA_PAGES];
    PageInfo* page_infos[ARENA_PAGES];
    int       num_pages;
    // the nursery is a dedicated arena so "is young" is a
    // range check; old-gen page allocation skips nursery arenas
    EJSBool   is_nursery;
} Arena;

#define MAX_ARENAS (MAX_HEAP_SIZE / ARENA_SIZE)
static Arena *heap_arenas[MAX_ARENAS];
static int num_arenas;

// ---- the arena address-space reservation (gc-P4) ----------------
//
// All arenas are carved out of ONE contiguous reservation, mapped
// PROT_NONE at init and committed ARENA_SIZE at a time.  Two payoffs,
// both for the conservative scanner:
//
//   - the arena span is FIXED and disjoint from the C/LLVM heap for the
//     life of the process.  Before this, each arena was its own mmap:
//     once a late arena landed beyond the C heap, the conservative
//     prefilter span swallowed every malloc'd address, and during
//     codegen MILLIONS of stack words pointing into LLVM's own
//     allocations passed the prefilter into a per-word bsearch — the
//     bistable 6s-vs-60s self-compile (mmap layout luck decided).
//   - arena lookup is two compares + a shift into a direct map instead
//     of a bsearch per candidate word.
//
// Reserved address space costs nothing until committed; nothing foreign
// can ever be mapped inside the reservation.
#define ARENA_SHIFT 25
_Static_assert((1L << ARENA_SHIFT) == ARENA_SIZE, "ARENA_SHIFT matches ARENA_SIZE");

static char* arena_space;      // base, ARENA_SIZE-aligned
static char* arena_space_pos;  // next uncommitted chunk
static char* arena_space_end;  // base + MAX_HEAP_SIZE
static Arena* arena_map[MAX_ARENAS]; // direct map: (ptr - base) >> ARENA_SHIFT

static void
arena_space_reserve(void)
{
    size_t size = (size_t)MAX_HEAP_SIZE;
    char* res = mmap_boxable(size + ARENA_SIZE, PROT_NONE, MAP_NORESERVE);
    if (res == NULL) {
        _ejs_log ("gc: unable to reserve the arena address space\n");
        abort();
    }
    char* aligned = (char*)EJS_ALIGN(res, ARENA_SIZE);
    // trim the alignment slop so the reservation is exactly the span
    if (aligned > res)
        munmap (res, aligned - res);
    if (aligned + size < res + size + ARENA_SIZE)
        munmap (aligned + size, (res + size + ARENA_SIZE) - (aligned + size));
    arena_space = aligned;
    arena_space_pos = aligned;
    arena_space_end = aligned + size;
}

static inline Arena*
arena_lookup(GCObjectPtr ptr)
{
    uintptr_t off = (uintptr_t)((char*)ptr - arena_space);
    if (off >= (uintptr_t)MAX_HEAP_SIZE) return NULL;
    return arena_map[off >> ARENA_SHIFT];
}

// conservative-scan prefilter: [conservative_lo, conservative_hi) bounds
// every GC-managed address (the arena reservation + LOS blocks).  The
// stack scanners reject candidate words with two compares before any
// lookup.  Bounds only ever widen — stale coverage of freed LOS blocks
// is merely conservative, and a candidate inside the reservation that
// hits no committed arena rejects in the direct map.
static char *conservative_lo = (char*)UINTPTR_MAX;
static char *conservative_hi = NULL;
static inline void
conservative_bounds_add(void* start, size_t size)
{
    if ((char*)start < conservative_lo) conservative_lo = (char*)start;
    if ((char*)start + size > conservative_hi) conservative_hi = (char*)start + size;
}

// ---- LOS lookup: sorted range array -----------------------------
//
// A conservative candidate that misses the arena reservation resolves
// against the LOS by binary search over a sorted array of payload
// ranges.  This replaces a LOCKED LINEAR WALK of the whole LOS list —
// per stack word — which, with blocks scattered by mmap, could put
// hundreds of ms per pin scan on deep-recursion minors (found while
// gating sinking-P3; the [los_lo, los_hi) bounds prefilter landed then
// as a stopgap and remains as the quick reject).
static char *los_lo = (char*)UINTPTR_MAX;
static char *los_hi = NULL;

// ---- the cell lifecycle ----------------------------------------
//
// One bitmap byte per page cell.  A cell is FREE or ALLOCATED, and an
// allocated cell carries a tri-color mark; every state predicate and
// transition lives in this block, and the encoding is private to it.
//
// White/black are EPOCH-RELATIVE: the color bits hold GRAY or the
// parity of the mark epoch the cell was last colored in.  color ==
// (mark_epoch & 1) is black (marked this epoch); the complement is
// white.  mark_epoch_advance() — called at exactly one site, the end
// of a full collection — thus turns every surviving black cell white
// in O(1) without touching a bitmap.  (The old collector expressed
// the same aging as a white_mask/black_mask swap mutated at the same
// site; the epoch is that flip made explicit and single-owner.)

typedef char BitmapCell;

#define CELL_COLOR_MASK 0x03
#define CELL_GRAY       0x02
#define CELL_FREE       0x04 // cell is in the free list for this page

static unsigned int mark_epoch = 1; // parity 1: black starts at color 1

static inline BitmapCell cell_black_color(void) { return (BitmapCell)(mark_epoch & 1); }
static inline BitmapCell cell_white_color(void) { return (BitmapCell)((mark_epoch & 1) ^ 1); }

// the ONLY place the white/black meaning ever changes
static inline void
mark_epoch_advance(void)
{
    mark_epoch++;
}

static inline EJSBool cell_is_free (BitmapCell c) { return (c & CELL_FREE) == CELL_FREE; }
static inline EJSBool cell_is_gray (BitmapCell c) { return (c & CELL_COLOR_MASK) == CELL_GRAY; }
static inline EJSBool cell_is_white(BitmapCell c) { return (c & CELL_COLOR_MASK) == cell_white_color(); }
static inline EJSBool cell_is_black(BitmapCell c) { return (c & CELL_COLOR_MASK) == cell_black_color(); }

static inline void cell_set_gray (BitmapCell* c) { *c = (BitmapCell)((*c & ~CELL_COLOR_MASK) | CELL_GRAY); }
static inline void cell_set_white(BitmapCell* c) { *c = (BitmapCell)((*c & ~CELL_COLOR_MASK) | cell_white_color()); }
static inline void cell_set_black(BitmapCell* c) { *c = (BitmapCell)((*c & ~CELL_COLOR_MASK) | cell_black_color()); }
static inline void cell_set_free (BitmapCell* c) { *c = CELL_FREE; }
static inline void cell_set_allocated(BitmapCell* c) { *c = (BitmapCell)(*c & ~CELL_FREE); }

struct _PageInfo {
    EJS_LIST_HEADER(struct _PageInfo);
    void*       bump_ptr;
    void*       page_start;
    void*       page_end;
    BitmapCell* page_bitmap;
    LargeObjectInfo *los_info;
    int32_t     cell_size;
    int16_t     num_cells;
    int16_t     num_free_cells;
    // 0 = old gen; 1 = active young page (bump-allocated,
    // allocated-ness = below bump); 2 = young survivor page (holds
    // pinned young objects, bitmap-authoritative, no further bumping)
    uint8_t     young;
};

struct _LargeObjectInfo {
    EJS_LIST_HEADER(struct _LargeObjectInfo);
    size_t alloc_size;
    PageInfo page_info;
};

#define OBJECT_SIZE_LOW_LIMIT_BITS 4  // smallest object we'll allocate (1<<4 = 16)
#define OBJECT_SIZE_HIGH_LIMIT_BITS 8 // max object size for the non-LOS allocator = 256

// heap_pages is indexed by ffs(cell_size) - OBJECT_SIZE_LOW_LIMIT_BITS,
// i.e. 16B -> 1 .. 256B -> 5 ([0] is unused); +2 covers the inclusive
// top class.  Until gc-P5 the ffs comparisons below routed 256-byte
// cells to the LOS (ffs(256) = 9 > HIGH_LIMIT_BITS), so the top class
// existed only on paper — the pre-gc-P4 LOS had a linear lookup that
// made large cell populations quadratic to mark.  With the LOS bsearch
// and the direct arena map in, the class is enabled: single-cell shaped
// objects up to the 14-field cap (32+16+112 = 160) and >14-slot envs
// now take pages, not the LOS.
#define HEAP_PAGELISTS_COUNT (OBJECT_SIZE_HIGH_LIMIT_BITS - OBJECT_SIZE_LOW_LIMIT_BITS) + 2

static EJSList heap_pages[HEAP_PAGELISTS_COUNT];
static LargeObjectInfo *los_list;

// ---- LOS lookup: sorted range array -----------------------------
//
// A conservative candidate that misses the arena reservation resolves
// against the LOS by binary search over a sorted array of payload
// ranges.  This replaces a LOCKED LINEAR WALK of the whole LOS list —
// per stack word — which, with blocks scattered by mmap, could put
// hundreds of ms per pin scan on deep-recursion minors (found while
// gating sinking-P3; the [los_lo, los_hi) bounds prefilter landed then
// as a stopgap and remains as the quick reject).
typedef struct {
    char* start; // payload: page_info.page_start
    char* end;   // start + cell_size
    LargeObjectInfo* lobj;
} LOSRange;
static LOSRange* los_ranges;
static int los_range_count;
static int los_range_capacity;

// index of the first range with start > ptr, in [0, count]
static int
los_range_upper_bound(char* ptr)
{
    int lo = 0, hi = los_range_count;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (los_ranges[mid].start <= ptr) lo = mid + 1;
        else hi = mid;
    }
    return lo;
}

static void
los_ranges_add(LargeObjectInfo* lobj)
{
    char* start = (char*)lobj->page_info.page_start;
    if (start < los_lo) los_lo = start;
    if (start + lobj->page_info.cell_size > los_hi)
        los_hi = start + lobj->page_info.cell_size;

    if (los_range_count == los_range_capacity) {
        los_range_capacity = los_range_capacity ? los_range_capacity * 2 : 256;
        los_ranges = realloc (los_ranges, los_range_capacity * sizeof(LOSRange));
    }
    int at = los_range_upper_bound(start);
    memmove (&los_ranges[at + 1], &los_ranges[at],
             (los_range_count - at) * sizeof(LOSRange));
    los_ranges[at].start = start;
    los_ranges[at].end = start + lobj->page_info.cell_size;
    los_ranges[at].lobj = lobj;
    los_range_count++;
}

static void
los_ranges_remove(LargeObjectInfo* lobj)
{
    char* start = (char*)lobj->page_info.page_start;
    int at = los_range_upper_bound(start) - 1;
    EJS_ASSERT(at >= 0 && los_ranges[at].lobj == lobj);
    memmove (&los_ranges[at], &los_ranges[at + 1],
             (los_range_count - at - 1) * sizeof(LOSRange));
    los_range_count--;
}

// interior pointers match: a conservative reference may be a derived
// pointer whose base value the optimizer discarded — with an exact-base
// match a large object referenced ONLY through an interior pointer
// (e.g. a flat string's data) would be collected out from under it.
// Callers canonicalize through cell_idx 0, so an interior hit marks the
// base.
static PageInfo*
los_lookup(GCObjectPtr ptr, uint32_t *cell_idx)
{
    if ((char*)ptr < los_lo || (char*)ptr >= los_hi)
        return NULL;
    int at = los_range_upper_bound((char*)ptr) - 1;
    if (at < 0 || (char*)ptr >= los_ranges[at].end)
        return NULL;
    if (cell_idx)
        *cell_idx = 0;
    return &los_ranges[at].lobj->page_info;
}

// GC profiling instrumentation state (definitions live with the profile
// block further down, before the mark helpers use them)
static EJSBool gc_profile;
static struct timeval prof_start_tv;
static void profile_note_pin(PageInfo* page, uint32_t cell_idx, GCObjectPtr raw);
static void profile_report_shutdown(void);

// nursery state + hooks (definitions in the nursery block
// below; declared here because the shared mark helpers dispatch on
// minor-collection mode)
static EJSBool nursery_enabled;
static EJSBool in_minor_gc;
static void minor_conservative_hit(PageInfo* page, uint32_t cell_idx);
static EJSBool young_cell_is_allocated(PageInfo* page, uint32_t cell_idx);
static void mark_thread_stack(void);
static void mark_generator_stacks(void);
static PageInfo* alloc_new_page(size_t cell_size);
static GCObjectPtr alloc_from_page(PageInfo* info);
static void finalize_object(GCObjectPtr p);
static void nursery_init(void);
static void young_normalize_for_full_gc(void);
static void young_page_freed(PageInfo* info, Arena* arena);
static void _ejs_gc_minor_collect(const char* reason);
static GCObjectPtr young_alloc_slow(int idx, size_t cell_size, EJSScanType scan_type);
// allocator accounting, defined with the allocator further down
extern size_t alloc_size;
extern size_t alloc_size_at_last_gc;
extern int num_allocs;
static size_t heap_size_at_last_gc;

// gc-P4: the compacting major (EJS_GC_COMPACT=off for A/B) and THE
// full-collection growth knob — a full GC triggers when old-gen growth
// since the last one exceeds gc_growth_pct percent of the post-sweep
// footprint (floor: two arenas, so small programs keep a sane cadence).
// The knob replaces the old fixed 60MB constant; with compaction
// shrinking the heap, the trigger now adapts in BOTH directions.
static EJSBool compact_enabled;
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
typedef enum {
    GC_POLICY_YOUNG_ALLOC, // a nursery allocation is about to run
    GC_POLICY_OLD_ALLOC,   // an old-gen/LOS allocation is about to run
    GC_POLICY_AFTER_MINOR, // a minor just retired; promotions grew the old gen
    GC_POLICY_ALLOC_FAILED // allocator out of memory: forced full
} GCPolicyEvent;

static void
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

// allocated-ness of a cell: old pages answer from the bitmap; ACTIVE
// young pages (young==1) answer from the bump rule — everything below
// the bump cursor is an object, the bitmap holds only collection
// colors; SURVIVOR young pages (young==2) are bitmap-authoritative
// again (their pinned cells were re-marked at minor sweep)
static inline EJSBool
cell_is_allocated(PageInfo* page, uint32_t cell_idx, BitmapCell cell)
{
    if (page->young == 1) return young_cell_is_allocated(page, cell_idx);
    return !cell_is_free(cell);
}

void* ptr_to_arena(void* ptr) { return PTR_TO_ARENA(ptr); }
void* ptr_to_arena_page_base(void* ptr) { return PTR_TO_ARENA_PAGE_BASE(ptr); }
uintptr_t ptr_to_arena_page_index(void* ptr) { return PTR_TO_ARENA_PAGE_INDEX(ptr); }
uintptr_t ptr_to_cell(void* ptr, PageInfo* info ) { return PTR_TO_CELL(ptr, info); }

#if sanity
static void
verify_arena(Arena *arena)
{
    for (int i = 0; i < arena->num_pages; i ++) {
        EJS_ASSERT (arena->pages[i] == arena->page_infos[i]->page_start);
    }
}
#endif


static Arena*
arena_new()
{
    if (arena_space_pos == arena_space_end)
        return NULL; // the reservation IS the heap cap

    SPEW(1, _ejs_log ("num_arenas = %d, max = %d\n", num_arenas, MAX_ARENAS));

    void* arena_start = arena_space_pos;
    if (mprotect (arena_start, ARENA_SIZE, PROT_READ | PROT_WRITE) != 0)
        return NULL;

    Arena* new_arena = arena_start;

    memset (new_arena, 0, sizeof(Arena));

    new_arena->end = arena_start + ARENA_SIZE;
    new_arena->pos = (void*)EJS_ALIGN(arena_start + sizeof(Arena), PAGE_SIZE);

    LOCK_ARENAS();
    arena_space_pos += ARENA_SIZE;
    // sequential carving: heap_arenas stays address-sorted by construction
    heap_arenas[num_arenas++] = new_arena;
    arena_map[((char*)arena_start - arena_space) >> ARENA_SHIFT] = new_arena;
    UNLOCK_ARENAS();

    return new_arena;
}

static PageInfo*
alloc_page_info_from_arena(Arena *arena, void *page_data, size_t cell_size)
{
    // FIXME allocate the PageInfo and bitmap from the arena as well
    PageInfo* info = (PageInfo*)calloc(1, sizeof(PageInfo) + (sizeof(BitmapCell) * PAGE_SIZE / (1<<OBJECT_SIZE_LOW_LIMIT_BITS)));
    EJS_LIST_INIT(info);
    info->cell_size = cell_size;
    info->num_cells = CELLS_OF_SIZE(cell_size);
    info->num_free_cells = info->num_cells;
    EJS_ASSERT(info->num_cells > 0);
    info->page_start = page_data;
    info->page_end = info->page_start + PAGE_SIZE;
    // allocate a bitmap large enough to store any sized object so we can reuse the bitmap
    info->page_bitmap = (BitmapCell*)(((char*)info) + sizeof(PageInfo));
    info->bump_ptr = info->page_start;
    memset (info->page_bitmap, CELL_FREE, info->num_cells * sizeof(BitmapCell));
    return info;
}

static PageInfo*
alloc_page_from_arena(Arena *arena, size_t cell_size)
{
    void *page_data = (void*)EJS_ALIGN(arena->pos, PAGE_SIZE);
    if (arena->free_pages) {
        PageInfo* info = arena->free_pages;
        EJS_LIST_DETACH(info, arena->free_pages);
        info->cell_size = cell_size;
        info->num_cells = CELLS_OF_SIZE(cell_size);
        info->num_free_cells = info->num_cells;
        info->bump_ptr = info->page_start;
        memset (info->page_bitmap, CELL_FREE, info->num_cells * sizeof(BitmapCell));
        SPEW(3, _ejs_log ("alloc_page_from_arena from free pages for cell size %zd = %p\n", info->cell_size, info));
        return info;
    }
    else if (page_data < arena->end) {
        PageInfo* info = alloc_page_info_from_arena (arena, page_data, cell_size);
        int page_idx = arena->num_pages++;
        arena->pos = page_data + PAGE_SIZE;
        arena->pages[page_idx] = page_data;
        arena->page_infos[page_idx] = info;
        SPEW(3, _ejs_log ("alloc_page_from_arena from bump pointer for cell size %zd = %p\n", info->cell_size, info));
        return info;
    }
    else {
        return NULL;
    }
}

static PageInfo*
find_page_and_cell(GCObjectPtr ptr, uint32_t *cell_idx)
{
    // bounds prefilter: static data (atoms, module structs) and foreign
    // pointers reject in two compares
    if ((char*)ptr < conservative_lo || (char*)ptr >= conservative_hi)
        return NULL;

    Arena* arena = arena_lookup(ptr);
    if (EJS_LIKELY (arena != NULL)) {
        SANITY(verify_arena(arena));

        int page_index = PTR_TO_ARENA_PAGE_INDEX(ptr);

        if (page_index < 0 || page_index >= arena->num_pages) {
            return NULL;
        }

        PageInfo *page = arena->page_infos[page_index];

        // note: interior pointers are accepted (PTR_TO_CELL divides by the
        // cell size, so any pointer into a cell resolves to that cell).
        // optimized code compiled by ejs keeps addresses of closure env
        // slots live across calls with the env base pointer dead, so the
        // conservative scan must treat interior pointers as referencing
        // the containing object.

        if (cell_idx) {
            *cell_idx = PTR_TO_CELL(ptr, page);
            EJS_ASSERT(*cell_idx >= 0 && *cell_idx < CELLS_IN_PAGE(page));
        }

        return page;
    }

    return los_lookup(ptr, cell_idx);
}

static void
set_gray (GCObjectPtr ptr)
{
    uint32_t cell_idx;
    PageInfo *page = find_page_and_cell(ptr, &cell_idx);
    if (!page)
        return;

    cell_set_gray(&page->page_bitmap[cell_idx]);
}

static void
set_black (GCObjectPtr ptr)
{
    uint32_t cell_idx;
    PageInfo *page = find_page_and_cell(ptr, &cell_idx);
    if (!page)
        return;

    cell_set_black(&page->page_bitmap[cell_idx]);
}

static EJSBool
is_white (GCObjectPtr ptr)
{
    uint32_t cell_idx;
    PageInfo *page = find_page_and_cell(ptr, &cell_idx);
    if (!page)
        return EJS_FALSE;

    return cell_is_white(page->page_bitmap[cell_idx]);
}

static PageInfo*
alloc_new_page(size_t cell_size)
{
    EJS_ASSERT(cell_size >= (1 << OBJECT_SIZE_LOW_LIMIT_BITS));
    SPEW(2, _ejs_log ("allocating new page for cell size %zd\n", cell_size));
    PageInfo *rv = NULL;
    for (int i = 0; i < num_arenas; i ++) {
        // nursery arenas serve young allocation only
        if (heap_arenas[i]->is_nursery)
            continue;
        rv = alloc_page_from_arena(heap_arenas[i], cell_size);
        if (rv) {
            SPEW(2, _ejs_log ("  => %p", rv));
            return rv;
        }
    }

    // need a new arena
    SPEW(2, _ejs_log ("unable to find page in current arenas, allocating a new one"));
    LOCK_ARENAS();
    Arena* arena = arena_new();
    UNLOCK_ARENAS();
    if (arena == NULL)
        return NULL;
    rv = alloc_page_from_arena(arena, cell_size);
    SPEW(2, _ejs_log ("  => %p", rv));
    return rv;
}

static void
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

static void
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

    // THE growth knob (gc-P4 knob census = 1): a full collection
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

    // one reservation holds every arena the process will ever commit;
    // the conservative prefilter covers it from day one (candidates in
    // uncommitted space reject via the direct map)
    arena_space_reserve();
    conservative_bounds_add (arena_space, (size_t)MAX_HEAP_SIZE);

    // allocate an initial arenas
    for (int i = 0; i < 10; i ++)
        arena_new();

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

// the mark-path scan callback.  Slot-based per the new
// EJSValueFunc contract — this non-moving path only reads through the
// slot; the mover's evacuation callback is what rewrites it.
static void
_scan_ejsvalue (ejsval* slot)
{
    ejsval val = *slot;
    if (!EJSVAL_IS_TRACEABLE_IMPL(val)) return;

    GCObjectPtr gcptr = (GCObjectPtr)EJSVAL_TO_GCTHING_IMPL(val);

    if (gcptr == NULL) return;

    WORKLIST_PUSH_AND_GRAY(gcptr);
}

static void
_scan_from_ejsobject(EJSObject* obj)
{
    // freshly allocated objects are zeroed but not yet initialized (their
    // constructor may trigger a collection before _ejs_init_object runs);
    // there's nothing to scan in them yet.
    if (obj->ops == NULL)
        return;
    OP(obj,Scan)(obj, _scan_ejsvalue);
}

static void
_scan_from_ejsprimstr(EJSPrimString *primStr)
{
    EJSPrimStringType strtype = EJS_PRIMSTR_GET_TYPE(primStr);

    switch (strtype) {
    case EJS_STRING_ROPE:
        // inline _scan_ejsvalue's push logic here to save creating an ejsval from the primStr only to destruct
        // it in _scan_ejsvalue

        WORKLIST_PUSH_AND_GRAY(primStr->data.rope.left);
        WORKLIST_PUSH_AND_GRAY(primStr->data.rope.right);
        break;
    case EJS_STRING_DEPENDENT:
        WORKLIST_PUSH_AND_GRAY(primStr->data.dependent.dep);
        break;
    case EJS_STRING_FLAT:
        // nothing to do here
        break;
    }
}

static void
_scan_from_ejsprimsym(EJSPrimSymbol *primSymbol)
{
    _scan_ejsvalue (&primSymbol->description);
}

static void
_scan_from_ejsclosureenv(EJSClosureEnv *env)
{
    for (uint32_t i = 0; i < env->length; i ++) {
        _scan_ejsvalue (&env->slots[i]);
    }
}

static GCObjectPtr *stack_bottom;

void
_ejs_gc_mark_thread_stack_bottom(GCObjectPtr* btm)
{
    stack_bottom = btm;
    // the write barrier's transient-slot upper bound starts at
    // the main stack's bottom (generator push/pop moves it)
    _ejs_heap.current_stack_end = (void*)btm;
}

static void
mark_pointers_in_range(GCObjectPtr* low, GCObjectPtr* high)
{
    GCObjectPtr* p;
    for (p = low; p < high-1; p++) {
        GCObjectPtr gcptr;

#if OSX
        // really a 64 bit check here, since for 64 bit systems, ejsvals can be stuck in registers, so we need to check if it's a valid
        // ejsval gcthing as well.
        ejsval ep = *(ejsval*)p;
        if (EJSVAL_IS_GCTHING_IMPL(ep))
            gcptr = (GCObjectPtr)EJSVAL_TO_GCTHING_IMPL(ep);
        else
#endif
            gcptr = *p;

        if (gcptr == NULL)            continue; // skip nulls.
        if ((char*)gcptr < conservative_lo || (char*)gcptr >= conservative_hi)
            continue; // cheap prefilter: outside every arena/LOS block

        uint32_t cell_idx;

        PageInfo *page = find_page_and_cell(gcptr, &cell_idx);
        if (!page)                    continue; // skip values outside our heap.

        // XXX more checks before we start treating the pointer like a GCObjectPtr?
        BitmapCell cell = page->page_bitmap[cell_idx];
        if (!cell_is_allocated(page, cell_idx, cell)) continue;

        // during a minor collection conservative hits PIN young
        // cells in place; nothing else is this collection's business
        if (in_minor_gc) { minor_conservative_hit(page, cell_idx); continue; }

        // a conservative hit PINS: the compacting major must sweep this
        // cell in place.  Recorded even when the target is already
        // marked (the white check below is a marking optimization, not
        // a pin filter).  profile_note_pin sets the same bit plus stats.
        if (gc_profile) profile_note_pin(page, cell_idx, gcptr);
        else *(GCObjectHeader*)(page->page_start + ((size_t)cell_idx * page->cell_size)) |= EJS_GC_HEADER_PINNED;

        if (!cell_is_white(cell)) continue; // skip pointers to gray/black cells

        // canonicalize interior pointers to the start of their cell; the
        // worklist processing reads the object header from the pointer.
        gcptr = page->page_start + (cell_idx * page->cell_size);

        WORKLIST_PUSH_AND_GRAY_CELL(gcptr, page->page_bitmap[cell_idx]);
    }
}

// gc-frame slots are stack memory, so the conservative
// stack scan would see every precisely-rooted value a second time and
// pin it through its own slot — precision would never move anything.
// During a minor, the scan skips the frame records of the stack being
// scanned (their slots are walked precisely and rewritten).  Full GC
// never skips: it relies on the conservative scan seeing the slots.
typedef struct { char* lo; char* hi; } FrameSkipRange;
#define MAX_FRAME_SKIP 1024
static FrameSkipRange frame_skip[MAX_FRAME_SKIP];
static int frame_skip_count;

static void
set_frame_skip_chain(void* chain_head)
{
    frame_skip_count = 0;
    for (EJSGCFrame* f = (EJSGCFrame*)chain_head; f; f = f->prev) {
        if (frame_skip_count == MAX_FRAME_SKIP) break; // partial skip = extra pins only
        char* lo = (char*)f;
        char* hi = lo + 16 + 8 * f->count;
        // insertion sort by lo; chains are short and near-sorted
        int i = frame_skip_count++;
        while (i > 0 && frame_skip[i - 1].lo > lo) {
            frame_skip[i] = frame_skip[i - 1];
            i--;
        }
        frame_skip[i].lo = lo;
        frame_skip[i].hi = hi;
    }
}

static void
clear_frame_skip(void)
{
    frame_skip_count = 0;
}

static void
mark_ejsvals_in_range(void* low, void* high)
{
    // per-call skip cursor: ranges below `low` are behind us
    int fr = 0;
    while (fr < frame_skip_count && frame_skip[fr].hi <= (char*)low) fr++;
    void* p = low;
#if IOS
    while (((uintptr_t)p) & 0x7) {
        p++;
    }
#endif
    for (; p < high - sizeof(ejsval); p += sizeof(ejsval)) {
        // inside a gc-frame record?  its slots are precise roots
        while (fr < frame_skip_count && frame_skip[fr].hi <= (char*)p) fr++;
        if (fr < frame_skip_count && (char*)p >= frame_skip[fr].lo) continue;
        ejsval candidate_val = *((ejsval*)p);
        GCObjectPtr gcptr;
        if (EJSVAL_IS_GCTHING_IMPL(candidate_val)) {
            gcptr = (GCObjectPtr)EJSVAL_TO_GCTHING_IMPL(candidate_val);
        }
        else {
            // also treat the slot as a raw, untagged pointer: optimized
            // (opt -O2) code compiled by ejs unboxes closure envs and
            // objects once and keeps/spills the raw pointer, with the
            // tagged ejsval potentially dead.
            gcptr = *(GCObjectPtr*)p;
        }

        if (gcptr == NULL)            continue; // skip nulls.
        if ((char*)gcptr < conservative_lo || (char*)gcptr >= conservative_hi)
            continue; // cheap prefilter: outside every arena/LOS block

        uint32_t cell_idx;
        PageInfo *page = find_page_and_cell(gcptr, &cell_idx);
        if (page) {
            // XXX more checks before we start treating the pointer like a GCObjectPtr?
            BitmapCell cell = page->page_bitmap[cell_idx];
            if (!cell_is_allocated(page, cell_idx, cell)) continue;

            // minor collections only pin young cells here
            if (in_minor_gc) { minor_conservative_hit(page, cell_idx); continue; }

            // a conservative hit PINS: the compacting major must sweep
            // this cell in place (recorded even when already marked)
            if (gc_profile) profile_note_pin(page, cell_idx, gcptr);
            else *(GCObjectHeader*)(page->page_start + ((size_t)cell_idx * page->cell_size)) |= EJS_GC_HEADER_PINNED;

            if (!cell_is_white(cell)) continue; // skip pointers to gray/black cells

            // canonicalize interior pointers to the start of their cell; the
            // worklist processing reads the object header from the pointer.
            gcptr = page->page_start + (cell_idx * page->cell_size);

            WORKLIST_PUSH_AND_GRAY_CELL(gcptr, page->page_bitmap[cell_idx]);
        }
    }
}

static int num_roots = 0;
static int white_objs = 0;
static int large_objs = 0;
static int total_objs = 0;

static int num_object_allocs = 0;
static int num_closureenv_allocs = 0;
static int num_primstr_allocs = 0;
static int num_primsym_allocs = 0;

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

enum {
    PROF_SRC_CSTACK = 0,   // conservative C-stack ranges (incl. suspended segments)
    PROF_SRC_REGS = 1,     // spilled register file
    PROF_SRC_GENSTACK = 2, // suspended generator stacks + saved ucontexts
    PROF_SRC_COUNT
};
static const char* prof_src_names[PROF_SRC_COUNT] = { "cstack", "regs", "genstack" };
static int prof_pin_source = PROF_SRC_CSTACK;

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
static const char* prof_gc_reason = "?";

static void
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
static void
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

static void
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

static void
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

static void
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

EJSHeapContext _ejs_heap; // exported: the per-isolate context (the emitter seam)

typedef struct {
    Arena* nursery_arena;
    PageInfo* young_current[EJS_GC_NUM_SIZE_CLASSES];
    EJSList young_pages; // all young pages not currently being bumped
    EJSBool verify;      // EJS_GC_VERIFY: old-gen barrier-coverage check per minor
    size_t young_alloced;  // bytes of young pages handed out this cycle
    size_t young_budget;   // minor-collection trigger (EJS_GC_NURSERY_BUDGET)
    // minor worklist (objects whose slots still need processing)
    GCObjectPtr* wl;
    int wl_count, wl_cap;
    // the remset's second buffer.  A minor collection SWAPS buffers up
    // front and processes the snapshot; slots whose referent stays young
    // (pinned) re-append into the live buffer — old→young edges CARRY
    // across cycles for as long as the target remains in the nursery.
    ejsval** remset_other;
    // stats (reported under EJS_GC_PROFILE)
    uint64_t minors, minor_usec_total, minor_usec_max;
    uint64_t promoted_objs, promoted_bytes, minor_pins, remset_peak, overflow_minors;
} EJSHeapPriv;
static EJSHeapPriv heap_priv; // the private half of the (single) isolate's context

#define NURSERY_REMSET_CAPACITY (64 * 1024)

// EJS_GC_MINOR_SPEW=1: per-event tracing for nursery debugging
static EJSBool minor_spew;
#define MINOR_SPEW(...) EJS_MACRO_START if (minor_spew) _ejs_log (__VA_ARGS__); EJS_MACRO_END

// EJS_GC_WATCH=<hex addr>: log every lifecycle event touching the cell
// containing that address, with a C backtrace (debugging aid for the
// deterministic single-cell corruption hunt)
#include <execinfo.h>
static uintptr_t gc_watch_addr;
static void
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

static EJSBool
young_cell_is_allocated(PageInfo* page, uint32_t cell_idx)
{
    return page->page_start + (size_t)cell_idx * page->cell_size < page->bump_ptr;
}

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
static void
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
static EJSBool minor_scan_saw_young;

static void
minor_wl_push(GCObjectPtr p)
{
    if (heap_priv.wl_count == heap_priv.wl_cap) {
        heap_priv.wl_cap = heap_priv.wl_cap ? heap_priv.wl_cap * 2 : 4096;
        heap_priv.wl = realloc (heap_priv.wl, heap_priv.wl_cap * sizeof(GCObjectPtr));
    }
    heap_priv.wl[heap_priv.wl_count++] = p;
}

// rewrite an ejsval's payload in place, preserving its NaN-box tag
static inline void
rewrite_slot_payload(ejsval* slot, GCObjectPtr to)
{
    slot->asBits = (slot->asBits & ~EJSVAL_PAYLOAD_MASK)
        | ((uint64_t)(uintptr_t)to & EJSVAL_PAYLOAD_MASK);
}

// After memcpy'ing a cell, SELF-INTERIOR pointers still aim at the old
// cell (found the hard way: every inline-buffer flat string's data
// pointed at poison after promotion).  The two classes in the runtime:
// flat strings without an out-of-line buffer (data.flat = self+hdr) and
// small EJSArguments (args = self+sizeof).  Anything new that embeds a
// self-pointer must be added here — the planned trace-bitmap redesign
// subsumes this with offset-based addressing.
static void
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
        // shaped ordinary objects with EMBEDDED slot storage (gc-P5
        // single-cell allocation): the slots ejsval points into the
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
    PageInfo* info = (PageInfo*)heap_pages[bucket].head;
    while (info && !info->num_free_cells) info = info->next;
    if (!info) {
        info = alloc_new_page(cell_size);
        if (info == NULL) {
            _ejs_log ("gc: promotion allocation failed (size %zd)\n", cell_size);
            abort();
        }
        _ejs_list_prepend_node (&heap_pages[bucket], (EJSListNode*)info);
    }
    GCObjectPtr rv = alloc_from_page(info);
    return rv;
}

// conservative hit during a minor collection: young targets pin in
// place (never move this cycle) and join the scan worklist once; old
// targets are not this collection's problem
static void
minor_conservative_hit(PageInfo* page, uint32_t cell_idx)
{
    if (!page->young) return;
    if (page->young == 1 && !young_cell_is_allocated(page, cell_idx)) return;
    if (page->young == 2 && cell_is_free(page->page_bitmap[cell_idx])) return;
    BitmapCell cell = page->page_bitmap[cell_idx];
    if (cell_is_black(cell)) return; // already pinned this minor
    GCObjectPtr base = page->page_start + ((size_t)cell_idx * page->cell_size);
    if (_ejs_gc_is_forwarded(base)) return; // pins precede evacuation; stale hit
    cell_set_black(&page->page_bitmap[cell_idx]);
    heap_priv.minor_pins++;
    MINOR_SPEW("minor: pin %p\n", base);
    gc_watch_hit ("pin", base);
    minor_wl_push(base);
}

#define MAX_GENERATORS 256
static int generator_count = 0;
static EJSGenerator* generators[MAX_GENERATORS];

// walk every gc-frame chain — the running stack's (the
// seam head) plus every suspended generator's saved chain and every
// ACTIVE generator's parked caller segment.  Chains are per-stack and
// disjoint; records live in stack frames that stay mapped for exactly
// as long as they are linked (returns unlink, catches re-link their
// own frame past unwound callees, the generator hooks swap heads at
// every stack switch).
// how many young referents the current minor's precise frame walk
// EVACUATED (as opposed to found pinned/forwarded/old) — the direct
// measure that precision is actually moving things (EJS_GC_PROFILE)
static uint64_t gc_frame_moves;

static void
walk_gc_frames(void (*slot_fn)(ejsval*))
{
    for (EJSGCFrame* f = (EJSGCFrame*)_ejs_heap.gc_frame_head; f; f = f->prev)
        for (uintptr_t i = 0; i < f->count; i++)
            slot_fn(&f->slots[i]);
    for (EJSGenerator* g = _ejs_generator_registry; g; g = g->reg_next)
        for (EJSGCFrame* f = (EJSGCFrame*)g->gc_frame_head; f; f = f->prev)
            for (uintptr_t i = 0; i < f->count; i++)
                slot_fn(&f->slots[i]);
    for (int gi = 0; gi < generator_count; gi++)
        for (EJSGCFrame* f = (EJSGCFrame*)generators[gi]->caller_gc_frame_head; f; f = f->prev)
            for (uintptr_t i = 0; i < f->count; i++)
                slot_fn(&f->slots[i]);
}

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

// walk every live OLD cell (arena pages + LOS), calling `fn` on the
// object — the remset-overflow fallback and the EJS_GC_VERIFY check
static void
old_gen_walk(void (*fn)(GCObjectPtr))
{
    for (int a = 0; a < num_arenas; a++) {
        Arena* arena = heap_arenas[a];
        if (!arena || arena->is_nursery) continue;
        for (int pg = 0; pg < arena->num_pages; pg++) {
            PageInfo* info = arena->page_infos[pg];
            if (!info || info->young) continue;
            GCObjectPtr p = info->page_start;
            for (int c = 0; c < CELLS_IN_PAGE(info); c++, p += info->cell_size) {
                if (cell_is_free(info->page_bitmap[c])) continue;
                fn (p);
            }
        }
    }
    for (LargeObjectInfo* lobj = los_list; lobj; lobj = lobj->next) {
        if (cell_is_free(lobj->page_info.page_bitmap[0])) continue;
        fn (lobj->page_info.page_start);
    }
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
static int
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
    // cell counts (boxed or raw, base or interior)
    referrer_ctx = "stack";
    referrer_owner = NULL;
    void* volatile probe;
    for (void** w = (void**)&probe; w < (void**)stack_bottom; w++) {
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
static ejsval* verify_bad_slot;
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
static void
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
                _ejs_log ("EJS_GC_VERIFY: missed write barrier: old env %p slot %u holds unpromoted young ref\n", p, i);
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

// EJS_GC_PARANOID: after every minor, walk roots + modules + all live
// heap cells and validate every traceable value: it must resolve to an
// allocated cell whose header carries exactly one scan-type bit.
// Catches corruption at the collection that minted it.
static EJSBool gc_paranoid;
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
static void
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

// A FULL collection frees dead old objects, so every remset/rescan
// entry — slots INTERIOR to old cells — may now dangle into poisoned
// memory (found as 0xfffc_afaf… "object-tagged poison" values read by
// the next minor).  Rebuild the whole remembered state from a live
// old-gen walk instead: record every live old→young ejsval slot, re-add
// old strings with young raw children, and drop the LOS-pending list
// (the walk covers LOS objects).  Full collections are rare; one extra
// old-gen walk apiece is cheap insurance.
static void
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

static void
_ejs_gc_minor_collect(const char* reason)
{
    struct timeval tv0, tv1;
    gettimeofday (&tv0, NULL);

    if (in_minor_gc) {
        _ejs_log ("GC BUG: reentrant minor collection (reason=%s)\n", reason);
        abort();
    }

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
static GCObjectPtr
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
static void
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

static void
nursery_init(void)
{
    // nursery ON by default (gate decision 2026-07-25);
    // EJS_GC_NURSERY=off (or =0) selects the old collector for A/B.
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
    _ejs_heap.remset = malloc (NURSERY_REMSET_CAPACITY * sizeof(ejsval*));
    _ejs_heap.remset_capacity = NURSERY_REMSET_CAPACITY;
    heap_priv.remset_other = malloc (NURSERY_REMSET_CAPACITY * sizeof(ejsval*));
}
// ===================== end nursery =========================================

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

static void
mark_root_slot(ejsval* root)
{
    num_roots++;
    ejsval rootval = *root;
    if (!EJSVAL_IS_GCTHING_IMPL(rootval))
        return;
    GCObjectPtr root_ptr = (GCObjectPtr)EJSVAL_TO_GCTHING_IMPL(rootval);
    if (root_ptr == NULL)
        return;
    uint32_t cell_idx;
    PageInfo* page = find_page_and_cell(root_ptr, &cell_idx);
    if (!page)
        return;

    BitmapCell cell = page->page_bitmap[cell_idx];
    if (cell_is_free(cell))   return; // skip free cells
    if (!cell_is_white(cell)) return; // skip pointers to gray/black cells
    WORKLIST_PUSH_AND_GRAY_CELL(root_ptr, page->page_bitmap[cell_idx]);
}

static void
mark_from_roots()
{
    SPEW (2, _ejs_log ("marking from roots"));
    root_registry_foreach (mark_root_slot);
    SPEW (2, _ejs_log ("done marking from roots"));
}

static void
mark_from_modules()
{
    SPEW(2, _ejs_log ("marking from module exotics"));

    for (int i = 0; i < _ejs_num_modules; i ++) {
        EJSObject* mod = (EJSObject*)_ejs_modules[i];
        // modules are static globals whose object headers aren't set up
        // until _ejs_require_init; if a collection happens before that
        // (e.g. EJS_GC_EVERY_N_ALLOC during _ejs_init) there's nothing to
        // scan yet.
        if (mod->ops == NULL)
            continue;
        _scan_from_ejsobject(mod);
    }
}

#if TARGET_CPU_ARM
#define MARK_REGISTERS EJS_MACRO_START \
    GCObjectPtr __r0, __r1, __r2, __r3, __r4, __r5, __r6, __r7, __r8, __r9, __r10, __r11, __r12, __end; \
    __asm ("str r0, %0; str r1, %1; str r2, %2; str r3, %3; str r4, %4; str r5, %5; str r6, %6;" \
           "str r7, %7; str r8, %8; str r9, %9; str r10, %10; str r11, %11; str r12, %12;" \
          : "=m"(__r0), "=m"(__r1), "=m"(__r2), "=m"(__r3), "=m"(__r4),  \
            "=m"(__r5), "=m"(__r6), "=m"(__r7), "=m"(__r8),  "=m"(__r9), \
            "=m"(__r10), "=m"(__r11), "=m"(__r12));                      \
                                                                         \
    mark_pointers_in_range(&__end, &__r0);                               \
    EJS_MACRO_END
#elif TARGET_CPU_ARM64
// spill the callee-saved registers (x19-x28, plus fp) and treat them as
// roots.  code compiled by ejs (opt -O2) keeps live ejsvals in callee-saved
// registers across calls, and the mostly -O0 runtime doesn't reliably save
// all of them anywhere the stack scan would see.  (an empty MARK_REGISTERS
// here let live objects be collected and their cells reused -> heap
// corruption.)
#define MARK_REGISTERS EJS_MACRO_START                                  \
    GCObjectPtr __regs[21];                                             \
    __asm volatile ("stp x19, x20, [%0, #0]\n\t"                        \
                    "stp x21, x22, [%0, #16]\n\t"                       \
                    "stp x23, x24, [%0, #32]\n\t"                       \
                    "stp x25, x26, [%0, #48]\n\t"                       \
                    "stp x27, x28, [%0, #64]\n\t"                       \
                    "str x29, [%0, #80]\n\t"                            \
                    /* llvm will spill gprs into the callee-saved simd   \
                       registers under pressure, so scan those too */    \
                    "stp d8, d9,   [%0, #88]\n\t"                        \
                    "stp d10, d11, [%0, #104]\n\t"                       \
                    "stp d12, d13, [%0, #120]\n\t"                       \
                    "stp d14, d15, [%0, #136]"                          \
                    : : "r"(__regs) : "memory");                        \
    __regs[19] = __regs[20] = NULL;                                     \
    /* mark_pointers_in_range scans [low, high-1) */                    \
    mark_pointers_in_range(__regs, __regs + 21);                        \
    EJS_MACRO_END
#elif TARGET_CPU_AMD64
#define MARK_REGISTERS EJS_MACRO_START \
    GCObjectPtr __rax, __rbx, __rcx, __rdx, __rsi, __rdi, __rbp, __rsp, __r8, __r9, __r10, __r11, __r12, __r13, __r14, __r15, __end; \
    __asm ("movq %%rax, %0; movq %%rbx, %1; movq %%rcx, %2; movq %%rdx, %3; movq %%rsi, %4;" \
           "movq %%rdi, %5; movq %%rbp, %6; movq %%rsp, %7; movq %%r8, %8;  movq %%r9, %9;" \
           "movq %%r10, %10; movq %%r11, %11; movq %%r12, %12; movq %%r13, %13; movq %%r14, %14; movq %%r15, %15;" \
          : "=m"(__rax), "=m"(__rbx), "=m"(__rcx), "=m"(__rdx), "=m"(__rsi), \
            "=m"(__rdi), "=m"(__rbp), "=m"(__rsp), "=m"(__r8),  "=m"(__r9), \
            "=m"(__r10), "=m"(__r11), "=m"(__r12), "=m"(__r13), "=m"(__r14), "=m"(__r15)); \
                                                                        \
    mark_pointers_in_range(&__end, &__rax);                             \
    EJS_MACRO_END
#elif TARGET_CPU_X86
#define MARK_REGISTERS // just keep the build limping along
#else
#error "put code here to mark registers"
#endif

// (MAX_GENERATORS / generators[] / generator_count moved above
// walk_gc_frames, which walks the active chain's parked caller
// segments)

void
_ejs_gc_push_generator(EJSGenerator* gen)
{
    if (generator_count >= MAX_GENERATORS) {
        _ejs_log ("too many nested generators (max %d)\n", MAX_GENERATORS);
        abort();
    }
    generators[generator_count++] = gen;
    // keep the barrier's transient-slot bound on the CURRENT stack
    _ejs_heap.current_stack_end = gen->stack + gen->stack_size;
    // swap in this stack's gc-frame chain; the caller's segment
    // parks on the generator until the matching pop
    gen->caller_gc_frame_head = _ejs_heap.gc_frame_head;
    _ejs_heap.gc_frame_head = gen->gc_frame_head;
    gen->gc_frame_head = NULL; // the live chain is the seam head now
}

void
_ejs_gc_pop_generator()
{
    generator_count--;
    EJSGenerator* gen = generators[generator_count];
    _ejs_heap.current_stack_end = generator_count > 0
        ? generators[generator_count - 1]->stack + generators[generator_count - 1]->stack_size
        : (void*)stack_bottom;
    // park this stack's chain on the generator (walked while
    // suspended), restore the caller's segment
    gen->gc_frame_head = _ejs_heap.gc_frame_head;
    _ejs_heap.gc_frame_head = gen->caller_gc_frame_head;
    gen->caller_gc_frame_head = NULL;
}

static void
mark_thread_stack()
{
    prof_pin_source = PROF_SRC_REGS;
    MARK_REGISTERS;
    prof_pin_source = PROF_SRC_CSTACK;

    GCObjectPtr stack_top = NULL;

    // The CURRENT machine stack.  When the mutator is running on a
    // generator's malloc'd stack (collections happen inside
    // _ejs_gc_alloc, which generator bodies call), [&stack_top,
    // stack_bottom) is NOT a stack range — it spans from the malloc heap
    // to the main stack across unmapped memory.  Scan only up to the
    // running generator's stack end; mark_generator_stacks covers the
    // suspended caller segments.
    void* high = (void*)stack_bottom;
    if (generator_count > 0) {
        EJSGenerator* running = generators[generator_count - 1];
        high = running->stack + running->stack_size;
    }

    mark_ejsvals_in_range(((void*)&stack_top) + sizeof(GCObjectPtr), high);
}

// mark a known heap object as a root (page cell or LOS both resolve
// through find_page_and_cell; the pointer must be an object base)
static void minor_wl_push(GCObjectPtr p);
static void
mark_object_root(GCObjectPtr ptr)
{
    uint32_t cell_idx;
    PageInfo* page = find_page_and_cell(ptr, &cell_idx);
    if (!page)
        return;
    BitmapCell cell = page->page_bitmap[cell_idx];
    if (!cell_is_allocated(page, cell_idx, cell))
        return;
    if (in_minor_gc) {
        // minor collections: a young root pins; an old root's slots may hold
        // young references, so queue it for the precise minor scan
        // (duplicates are harmless — evacuation is idempotent)
        if (page->young) minor_conservative_hit(page, cell_idx);
        else minor_wl_push(ptr);
        return;
    }
    if (!cell_is_white(cell))
        return;
    WORKLIST_PUSH_AND_GRAY_CELL(ptr, page->page_bitmap[cell_idx]);
}

// The chain of ACTIVE generators (generators whose bodies are on the
// current stack chain; push on start/resume, pop on yield/completion —
// generators[generator_count-1] owns the stack we are executing on).
// mark_thread_stack scans the running stack; this covers the rest:
//
//   - each active generator OBJECT is a root for the cycle (its specop
//     scan conservatively marks its own suspended frames and both saved
//     ucontexts, i.e. the register files);
//   - the SUSPENDED CALLER segment behind each swap-in: frames from the
//     caller_stack_top recorded at the resume site up to that caller's
//     stack end — the main stack (stack_bottom) for the outermost
//     generator, the parent generator's stack end for nested ones.
//
// Suspended generators NOT in the chain need nothing here: if their
// object is reachable its scan covers their stack; if it is not, nothing
// on that stack is reachable either.
static void
mark_generator_stacks()
{
    prof_pin_source = PROF_SRC_CSTACK; // the suspended segments ARE C stack
    for (int i = 0; i < generator_count; i++) {
        EJSGenerator* gen = generators[i];

        mark_object_root((GCObjectPtr)gen);

        void* seg_high = (i == 0) ? (void*)stack_bottom
                                  : generators[i - 1]->stack + generators[i - 1]->stack_size;
        if (gen->caller_stack_top) {
            // this caller segment's frames are the chain parked
            // at push time (minor only; a full GC leaves skips empty)
            if (in_minor_gc) set_frame_skip_chain(gen->caller_gc_frame_head);
            mark_ejsvals_in_range(gen->caller_stack_top, seg_high);
            if (in_minor_gc) clear_frame_skip();
        }
    }
}

static void
process_worklist()
{
    GCObjectPtr p;
    while ((p = _ejs_gc_worklist_pop())) {
        set_black (p);
        GCObjectHeader* headerp = (GCObjectHeader*)p;
        if ((*headerp & EJS_SCAN_TYPE_OBJECT) != 0)
            _scan_from_ejsobject((EJSObject*)p);
        else if ((*headerp & EJS_SCAN_TYPE_PRIMSTR) != 0)
            _scan_from_ejsprimstr((EJSPrimString*)p);
        else if ((*headerp & EJS_SCAN_TYPE_PRIMSYM) != 0)
            _scan_from_ejsprimsym((EJSPrimSymbol*)p);
        else if ((*headerp & EJS_SCAN_TYPE_CLOSUREENV) != 0)
            _scan_from_ejsclosureenv((EJSClosureEnv*)p);
    }

    EJS_ASSERT(work_list.list == NULL);
}

// ============== mostly-copying major compaction (gc-P4) ===================
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

static void
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
        // NULL out all of our roots

        for (int i = 0; i < root_registry_count; i++)
            *root_registry[i] = _ejs_null;
        free (root_registry);
        root_registry = NULL;
        root_registry_count = root_registry_capacity = 0;

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

static size_t
calc_heap_size()
{
    size_t size = 0;
    for (int hp = 0; hp < HEAP_PAGELISTS_COUNT; hp++) {
        size += _ejs_list_length(&heap_pages[hp]) * PAGE_SIZE;
    }
    return size;
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
static size_t heap_size_at_last_gc = 0;

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

static GCObjectPtr
alloc_from_page(PageInfo *info)
{
    LOCK_PAGE(info);

    EJS_ASSERT (info->num_free_cells > 0);
    
    GCObjectPtr rv = NULL;
    uint32_t cell;

    SPEW(2, _ejs_log ("allocating object from page %p (cell size %zd)\n", info, info->cell_size));

    if (info->bump_ptr) {
        rv = (GCObjectPtr)EJS_ALIGN(info->bump_ptr, 8);
        cell = PTR_TO_CELL(info->bump_ptr, info);
        info->bump_ptr += info->cell_size;
        // check if we can service the next alloc request from the bump_ptr.  if we can't, switch
        // to the freelist code below.
        if (info->bump_ptr + info->cell_size >= info->page_end)
            info->bump_ptr = NULL;
    }
    else {
        for (cell = 0; cell < info->num_cells; cell ++) {
            if (cell_is_free(info->page_bitmap[cell])) {
                rv = info->page_start + (cell * info->cell_size);
                break;
            }
        }
    }

    EJS_ASSERT (rv);

    cell_set_allocated(&info->page_bitmap[cell]);
    cell_set_white(&info->page_bitmap[cell]);

    info->num_free_cells --;

    UNLOCK_PAGE(info);

    SPEW(2, _ejs_log ("allocated obj %p from page %p (cell size %zd), free cells remaining %zd\n", rv, info, info->cell_size, info->num_free_cells));

#if !clear_on_finalize
    memset(rv, 0, info->cell_size);
#endif
    return rv;
}

static GCObjectPtr
alloc_from_los(size_t size, EJSScanType scan_type)
{
    // allocate enough space for the object, our header, and our bitmap.  leave room enough to align the return value
    LargeObjectInfo *rv = alloc_from_os(size + sizeof(LargeObjectInfo) + 16);
    if (rv == NULL)
        return NULL;

    rv->page_info.page_bitmap = (char*)((void*)rv + sizeof(LargeObjectInfo)); // our bitmap comes right after the header
    rv->page_info.page_start = (void*)EJS_ALIGN((void*)rv + sizeof(LargeObjectInfo) + 8, 8);
    rv->page_info.cell_size = size;
    rv->page_info.num_cells = 1;
    rv->page_info.num_free_cells = 0;
    rv->page_info.los_info = rv;

    cell_set_white(&rv->page_info.page_bitmap[0]);
    cell_set_allocated(&rv->page_info.page_bitmap[0]);

    *((GCObjectHeader*)rv->page_info.page_start) = scan_type | EJS_GC_HEADER_YOUNG;

    rv->alloc_size = size;

    conservative_bounds_add (rv, size + sizeof(LargeObjectInfo) + 16);
    los_ranges_add (rv);
    EJS_LIST_PREPEND (rv, los_list);
    //_ejs_log ("alloc_from_los returning %p\n, los_list = %p\n", rv->page_info.page_start, los_list);
    return rv->page_info.page_start;
}

static void
release_to_los (LargeObjectInfo *lobj)
{
    los_ranges_remove (lobj);
    // the mapping covers the header + bitmap slop too, not just the
    // payload (releasing only alloc_size leaked the tail page)
    release_to_os (lobj, lobj->alloc_size + sizeof(LargeObjectInfo) + 16);
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

void
_ejs_gc_mark_conservative_range(void* low, void* high) {
    // only the generator scan uses this entry point (suspended stacks +
    // saved ucontexts) — attribute its pins accordingly
    int prev_src = prof_pin_source;
    prof_pin_source = PROF_SRC_GENSTACK;
    mark_ejsvals_in_range(low, high);
    prof_pin_source = prev_src;
}

static int
page_list_count (PageInfo* page)
{
    int count = 0;
    while (page) {
        count ++;
        page = page->next;
    }
    return count;
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

