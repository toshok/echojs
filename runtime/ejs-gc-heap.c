/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

// heap geography: the arena address-space reservation, arenas, page
// allocation, the large-object store and its sorted-range lookup, and
// find_page_and_cell — the pointer->cell resolution every scan uses.

#include "ejs-gc-internal.h"

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

Arena *heap_arenas[MAX_ARENAS];
int num_arenas;

// ---- the arena address-space reservation ------------------------
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
char *conservative_lo = (char*)UINTPTR_MAX;
char *conservative_hi = NULL;
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
// ranges.  A linear walk of the whole LOS list here — per stack word,
// with blocks scattered by mmap — costs hundreds of ms per pin scan
// on deep-recursion minors; the [los_lo, los_hi) bounds check is the
// quick reject.
static char *los_lo = (char*)UINTPTR_MAX;
static char *los_hi = NULL;

// TRUE when ptr lies in GC-managed storage (the arena reservation or the
// LOS span): such an address can be freed and recycled by the collector,
// so identity caches keyed on it (the shape-lookup and propertymap
// caches) must not admit it — a recycled address would false-hit with
// the previous occupant's entry.  Statics (runtime atoms, module string
// literals) are outside both ranges and cache safely.
EJSBool
_ejs_gc_ptr_is_gc_managed (void* ptr)
{
    if ((uintptr_t)((char*)ptr - arena_space) < (uintptr_t)MAX_HEAP_SIZE)
        return EJS_TRUE;
    return (char*)ptr >= los_lo && (char*)ptr < los_hi;
}

EJSList heap_pages[HEAP_PAGELISTS_COUNT];
LargeObjectInfo *los_list;

// ---- LOS lookup: sorted range array -----------------------------
//
// A conservative candidate that misses the arena reservation resolves
// against the LOS by binary search over a sorted array of payload
// ranges.  A linear walk of the whole LOS list here — per stack word,
// with blocks scattered by mmap — costs hundreds of ms per pin scan
// on deep-recursion minors; the [los_lo, los_hi) bounds check is the
// quick reject.
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


Arena*
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

// one reservation holds every arena the process will ever commit; the
// conservative prefilter covers it from day one (candidates in
// uncommitted space reject via the direct map)
void
heap_space_init(void)
{
    arena_space_reserve();
    conservative_bounds_add (arena_space, (size_t)MAX_HEAP_SIZE);

    // allocate an initial arenas
    for (int i = 0; i < 10; i ++)
        arena_new();
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

PageInfo*
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

PageInfo*
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

PageInfo*
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

// walk every live OLD cell (arena pages + LOS), calling `fn` on the
// object — the remset-overflow fallback and the EJS_GC_VERIFY check
void
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

size_t
calc_heap_size()
{
    size_t size = 0;
    for (int hp = 0; hp < HEAP_PAGELISTS_COUNT; hp++) {
        size += _ejs_list_length(&heap_pages[hp]) * PAGE_SIZE;
    }
    return size;
}

GCObjectPtr
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

GCObjectPtr
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

void
release_to_los (LargeObjectInfo *lobj)
{
    los_ranges_remove (lobj);
    // the mapping covers the header + bitmap slop too, not just the
    // payload (releasing only alloc_size leaked the tail page)
    release_to_os (lobj, lobj->alloc_size + sizeof(LargeObjectInfo) + 16);
}
