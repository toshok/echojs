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
#include "ejs-value.h"
#include "ejs-string.h"
#include "ejsval.h"

#include <pthread.h>

#define spew 1
#define sanity 0
#define gc_timings 2

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

void _ejs_gc_dump_heap_stats();

// 128MB heap.  deal with it, suckers
#define MAX_HEAP_SIZE (128 * 1024 * 1024)

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

#define USABLE_PAGE_SIZE PAGE_SIZE

#define CELLS_OF_SIZE(size) (USABLE_PAGE_SIZE / (size))
#define CELLS_IN_PAGE(page) CELLS_OF_SIZE((page)->cell_size)

// arenas are reserved in ARENA_PAGES * PAGE_SIZE chunks.  ARENA_PAGES=2048 gives us an arena size of 8MB
#define ARENA_PAGES 2048
#define ARENA_SIZE (PAGE_SIZE*ARENA_PAGES)

#define PTR_TO_ARENA_MASK (uintptr_t)(~(ARENA_SIZE-1))

// turn a random pointer into an arena pointer
#define PTR_TO_ARENA(ptr) ((void*)((uintptr_t)(ptr) & PTR_TO_ARENA_MASK))
#define PTR_TO_ARENA_PAGE_BASE(ptr) ((void*)ALIGN(PTR_TO_ARENA(ptr) + sizeof(Arena), PAGE_SIZE))
#define PTR_TO_ARENA_PAGE_INDEX(ptr) ((((uintptr_t)(ptr) & ~PTR_TO_ARENA_MASK) - ((uintptr_t)PTR_TO_ARENA_PAGE_BASE(ptr) & ~PTR_TO_ARENA_MASK)) / PAGE_SIZE)

#define PTR_TO_CELL(ptr,info) (((ptr) - (info)->page_start) / (info)->cell_size)

#define OBJ_TO_PAGE(o) ((o) & ~PAGE_SIZE)

#define IS_ALIGNED_TO(v,a) (((uintptr_t)(v) & ((a)-1)) == 0)
#define ALLOC_ALIGN 8
#define ALIGN(v,a) (((uintptr_t)(v) + (a)-1) & ~((a)-1))
#define IS_ALLOC_ALIGNED(v) IS_ALIGNED_TO(v, ALLOC_ALIGN)

// #if osx
#include <mach/vm_statistics.h>
#define MAP_FD VM_MAKE_TAG (VM_MEMORY_APPLICATION_SPECIFIC_16)
// #else
// #define MAP_FD -1
// #endif

EJSBool gc_disabled;
int collect_every_alloc = 0;

typedef struct EJSFinalizerEntry {
    EJS_LIST_HEADER(struct EJSFinalizerEntry);
    GCObjectPtr gcobj;
} EJSFinalizerEntry;

static pthread_mutex_t big_gc_lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER;

#define LOCK_GC() do {                                                  \
        /*fprintf (stderr, "locking gc at %s:%d\n", __PRETTY_FUNCTION__, __LINE__); */ \
        pthread_mutex_lock(&big_gc_lock);                               \
    } while (0)
#define UNLOCK_GC() do {                        \
        /*fprintf (stderr, "unlocking gc at %s:%d\n", __PRETTY_FUNCTION__, __LINE__); */ \
        pthread_mutex_unlock(&big_gc_lock);     \
    } while (0)
static pthread_cond_t finalizer_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t finalizer_mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER;
static pthread_cond_t wait_for_finalizer_cond = PTHREAD_COND_INITIALIZER;
static pthread_t finalizer_thread;
static EJSFinalizerEntry* finalizer_list;


#define MAX_WORKLIST_SEGMENT_SIZE 128
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
    if (IS_WHITE(cell)) {                                       \
        _ejs_gc_worklist_push((GCObjectPtr)(x));                \
        SET_GRAY (cell);                                        \
    }                                                           \
    EJS_MACRO_END

typedef struct _RootSetEntry {
    EJS_LIST_HEADER(struct _RootSetEntry);
    ejsval* root;
} RootSetEntry;

static RootSetEntry *root_set;

static void*
alloc_from_os(size_t size, size_t align)
{
    if (align == 0)
        return mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, MAP_FD, 0);

    void* res = mmap(NULL, size*2, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, MAP_FD, 0);
    SPEW(2, fprintf (stderr, "mmap returned %p\n", res));

    if (((uintptr_t)res % align) == 0) {
        // the memory was aligned, unmap the second half of our mapping
        // XXX should we just rejoice and add both halves?
        SPEW(2, fprintf (stderr, "already aligned\n"));
        munmap (res + size, size);
    }
    else {
        SPEW(2, fprintf (stderr, "not aligned\n"));
        // align res, and unmap the areas before/after the new mapping
        void *aligned_res = (void*)ALIGN(res, align);
        // the area before
        munmap (res, (uintptr_t)aligned_res - (uintptr_t)res);
        // the area after
        munmap (aligned_res+size, size*2 - (uintptr_t)(aligned_res+size));
        res = aligned_res;
        SPEW(2, fprintf (stderr, "aligned ptr = %p\n", res));
    }
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
    int       num_pages;
    PageInfo* free_pages;
    void*     pages[ARENA_PAGES];
    PageInfo* page_infos[ARENA_PAGES];
} Arena;

#define MAX_ARENAS (MAX_HEAP_SIZE / ARENA_SIZE)
static Arena *heap_arenas[MAX_ARENAS];
static int num_arenas;

typedef char BitmapCell;

#define CELL_COLOR_MASK       0x03
#define CELL_GRAY_MASK        0x02
#define CELL_WHITE_MASK_START 0x00
#define CELL_BLACK_MASK_START 0x01
#define CELL_FREE             0x04 // cell is in the free list for this page
#define CELL_FINALIZABLE      0x08 // object in cell is ready to be finalized

static unsigned int black_mask = CELL_BLACK_MASK_START;
static unsigned int white_mask = CELL_WHITE_MASK_START;

#define SET_GRAY(cell) EJS_MACRO_START                                  \
    BitmapCell _bc;                                                     \
    do {                                                                \
        _bc = (cell);                                                   \
    } while (!__sync_bool_compare_and_swap (&cell, _bc, (_bc & ~CELL_COLOR_MASK) | CELL_GRAY_MASK)); \
    EJS_MACRO_END

#define SET_WHITE(cell) EJS_MACRO_START                                 \
    BitmapCell _bc;                                                     \
    do {                                                                \
        _bc = (cell);                                                   \
    } while (!__sync_bool_compare_and_swap (&cell, _bc, (_bc & ~CELL_COLOR_MASK) | white_mask)); \
    EJS_MACRO_END

#define SET_BLACK(cell) EJS_MACRO_START                                 \
    BitmapCell _bc;                                                     \
    do {                                                                \
        _bc = (cell);                                                   \
    } while (!__sync_bool_compare_and_swap (&cell, _bc, (_bc & ~CELL_COLOR_MASK) | black_mask)); \
    EJS_MACRO_END

#define SET_FREE(cell) EJS_MACRO_START                                  \
    BitmapCell _bc;                                                     \
    do {                                                                \
        _bc = (cell);                                                   \
    } while (!__sync_bool_compare_and_swap (&cell, _bc, CELL_FREE));    \
    EJS_MACRO_END

#define SET_FINALIZABLE(cell) EJS_MACRO_START                           \
    BitmapCell _bc;                                                     \
    do {                                                                \
        _bc = (cell);                                                   \
    } while (!__sync_bool_compare_and_swap (&cell, _bc, CELL_FINALIZABLE)); \
    EJS_MACRO_END

#define SET_ALLOCATED(cell) EJS_MACRO_START                             \
    BitmapCell _bc;                                                     \
    do {                                                                \
        _bc = (cell);                                                   \
    } while (!__sync_bool_compare_and_swap (&cell, _bc, (_bc & ~CELL_FREE))); \
    EJS_MACRO_END
    
#define IS_FREE(cell) (((cell) & CELL_FREE) == CELL_FREE)
#define IS_FINALIZABLE(cell) (((cell) & CELL_FINALIZABLE) == CELL_FINALIZABLE)
#define IS_GRAY(cell) (((cell) & CELL_COLOR_MASK) == CELL_GRAY_MASK)
#define IS_WHITE(cell) (((cell) & CELL_COLOR_MASK) == white_mask)
#define IS_BLACK(cell) (((cell) & CELL_COLOR_MASK) == black_mask)

struct _PageInfo {
    EJS_LIST_HEADER(struct _PageInfo);
    void*       bump_ptr;
    size_t      cell_size;
    size_t      num_cells;
    size_t      num_free_cells;
    void*       page_start;
    void*       page_end;
    BitmapCell* page_bitmap;
    LargeObjectInfo *los_info;
    pthread_mutex_t page_mutex;
};

struct _LargeObjectInfo {
    EJS_LIST_HEADER(struct _LargeObjectInfo);
    PageInfo page_info;
    size_t alloc_size;
};

#define OBJECT_SIZE_LOW_LIMIT_BITS 3   // number of bits required to represent a pointer (3 == 8 bytes for 64 bit)
#define OBJECT_SIZE_HIGH_LIMIT_BITS 10 // max object size for the non-LOS allocator = 1024

#define HEAP_PAGELISTS_COUNT (OBJECT_SIZE_HIGH_LIMIT_BITS - OBJECT_SIZE_LOW_LIMIT_BITS)
static EJSList heap_pages[HEAP_PAGELISTS_COUNT+1];
static LargeObjectInfo *los_list;

#if sanity
static void
verify_arena(Arena *arena)
{
    for (int i = 0; i < arena->num_pages; i ++) {
        assert (arena->pages[i] == arena->page_infos[i]->page_start);
    }
}
#endif


static Arena*
arena_new()
{
    if (num_arenas == MAX_ARENAS-1) {
        fprintf (stderr, "ejs heap exhausted");
        abort();
    }
    SPEW(1, fprintf (stderr, "num_arenas = %d, max = %d\n", num_arenas, MAX_ARENAS));

    void* arena_start = alloc_from_os(ARENA_SIZE, ARENA_SIZE);

    Arena* new_arena = arena_start;

    new_arena->end = arena_start + ARENA_SIZE;
    new_arena->pos = (void*)ALIGN(arena_start + sizeof(Arena), PAGE_SIZE);

    int insert_point = -1;
    for (int i = 0; i < num_arenas; i ++) {
        if (new_arena < heap_arenas[i]) {
            insert_point = i;
            break;
        }
    }
    if (insert_point == -1) insert_point = num_arenas;
    if (num_arenas-insert_point > 0)
        memmove (&heap_arenas[insert_point + 1], &heap_arenas[insert_point], (num_arenas-insert_point-1)*sizeof(Arena*));
    heap_arenas[insert_point] = new_arena;
    num_arenas++;

    return new_arena;
}

static void
arena_destroy (Arena* arena)
{
    release_to_os (arena, (intptr_t)arena->end - (intptr_t)arena);
}

static PageInfo*
alloc_page_info_from_arena(Arena *arena, void *page_data, size_t cell_size)
{
    // FIXME allocate the PageInfo and bitmap from the arena as well
    PageInfo* info = (PageInfo*)calloc(1, sizeof(PageInfo));
    EJS_LIST_INIT(info);
    info->cell_size = cell_size;
    info->num_cells = CELLS_OF_SIZE(cell_size);
    info->num_free_cells = info->num_cells;
    info->page_start = page_data;
    info->page_end = info->page_start + PAGE_SIZE;
    // allocate a bitmap large enough to store any sized object so we can reuse the bitmap
    info->page_bitmap = (BitmapCell*)malloc (PAGE_SIZE / (1<<OBJECT_SIZE_LOW_LIMIT_BITS));
    info->bump_ptr = info->page_start;
    memset (info->page_bitmap, CELL_FREE, info->num_cells * sizeof(BitmapCell));
    pthread_mutex_init (&info->page_mutex, NULL);
    return info;
}

static PageInfo*
alloc_page_from_arena(Arena *arena, size_t cell_size)
{
    void *page_data = (void*)ALIGN(arena->pos, PAGE_SIZE);
    if (arena->free_pages) {
        PageInfo* info = arena->free_pages;
        EJS_LIST_DETACH(info, arena->free_pages);
        info->cell_size = cell_size;
        info->num_cells = CELLS_OF_SIZE(cell_size);
        info->num_free_cells = info->num_cells;
        info->bump_ptr = info->page_start;
        memset (info->page_bitmap, CELL_FREE, info->num_cells * sizeof(BitmapCell));
        SPEW(3, fprintf (stderr, "alloc_page_from_arena for cell size %zd = %p\n", info->cell_size, info));
        return info;
    }
    else if (page_data < arena->end) {
        PageInfo* info = alloc_page_info_from_arena (arena, page_data, cell_size);
        int page_idx = arena->num_pages++;
        arena->pos = page_data + PAGE_SIZE;
        arena->pages[page_idx] = page_data;
        arena->page_infos[page_idx] = info;
        SPEW(3, fprintf (stderr, "alloc_page_from_arena for cell size %zd = %p\n", info->cell_size, info));
        return info;
    }
    else {
        return NULL;
    }
}

static int
compare_ptrs(const void* v1, const void* v2)
{
    Arena **a1 = (Arena**)v1;
    Arena **a2 = (Arena**)v2;
    ptrdiff_t diff = (intptr_t)*a1 - (intptr_t)*a2;
    if (diff < 0) return -1;
    if (diff == 0) return 0;
    return 1;
}

static Arena*
find_arena(GCObjectPtr ptr)
{
    void* arena_ptr = PTR_TO_ARENA(ptr);
    void* rv = bsearch (&arena_ptr, heap_arenas, num_arenas, sizeof(Arena*), compare_ptrs);
    if (!rv) return NULL;
    return *(Arena**)rv;
}

static PageInfo*
find_page_and_cell(GCObjectPtr ptr, int *cell_idx)
{
    LOCK_GC();

    Arena *arena = find_arena(ptr);
    if (EJS_LIKELY (arena != NULL)) {
        SANITY(verify_arena(arena));

        int page_index = PTR_TO_ARENA_PAGE_INDEX(ptr);

        if (page_index < 0 || page_index > arena->num_pages) {
            UNLOCK_GC();
            return NULL;
        }

        PageInfo *page = arena->page_infos[page_index];

        if (!IS_ALIGNED_TO(ptr, page->cell_size)) {
            UNLOCK_GC();
            return NULL; // can't possibly point to allocated cells.
        }

        if (cell_idx) {
            *cell_idx = PTR_TO_CELL(ptr, page);
            assert(*cell_idx >= 0 && *cell_idx < CELLS_IN_PAGE(page));
        }

        UNLOCK_GC();
        return page;
    }
    else {
        // check if it's in the LOS
        for (LargeObjectInfo *lobj = los_list; lobj; lobj = lobj->next) {
            if (lobj->page_info.page_start == ptr) {
                if (cell_idx) {
                    *cell_idx = 0;
                }
                UNLOCK_GC();
                return &lobj->page_info;
            }
        }
    }
    UNLOCK_GC();
    return NULL;
}

static void
set_gray (GCObjectPtr ptr)
{
    int cell_idx;
    PageInfo *page = find_page_and_cell(ptr, &cell_idx);
    if (!page)
        return;

    SET_GRAY(page->page_bitmap[cell_idx]);
}

static void
set_black (GCObjectPtr ptr)
{
    int cell_idx;
    PageInfo *page = find_page_and_cell(ptr, &cell_idx);
    if (!page)
        return;

    SET_BLACK(page->page_bitmap[cell_idx]);
}

static EJSBool
is_white (GCObjectPtr ptr)
{
    int cell_idx;
    PageInfo *page = find_page_and_cell(ptr, &cell_idx);
    if (!page)
        return EJS_FALSE;

    return IS_WHITE(page->page_bitmap[cell_idx]);
}

static PageInfo*
alloc_new_page(size_t cell_size)
{
    SPEW(2, fprintf (stderr, "allocating new page for cell size %zd\n", cell_size));
    PageInfo *rv = NULL;
    for (int i = 0; i < num_arenas; i ++) {
        rv = alloc_page_from_arena(heap_arenas[i], cell_size);
        if (rv)
            return rv;
    }

    // need a new arena
    Arena* arena = arena_new();
    return alloc_page_from_arena(arena, cell_size);
}

static void
finalize_object(GCObjectPtr p)
{
    GCObjectHeader* headerp = (GCObjectHeader*)p;
    if ((*headerp & EJS_SCAN_TYPE_OBJECT) != 0) {
        SPEW(2, fprintf (stderr, "finalizing object %p(%s)\n", p, CLASSNAME(p)));
        OP(p, finalize)((EJSObject*)p);
    }
    else if ((*headerp & EJS_SCAN_TYPE_CLOSUREENV) != 0) {
        SPEW(2, fprintf (stderr, "finalizing closureenv %p\n", p));
    }
    else if ((*headerp & EJS_SCAN_TYPE_PRIMSTR) != 0) {
        SPEW(2, {
                EJSPrimString* primstr = (EJSPrimString*)p;
                if (EJS_PRIMSTR_GET_TYPE(primstr) == EJS_STRING_FLAT) {
                    char* utf8 = ucs2_to_utf8(primstr->data.flat);
                    fprintf (stderr, "finalizing flat primitive string %p(%s)\n", p, utf8);
                    free (utf8);
                }
                else {
                    fprintf (stderr, "finalizing primitive string %p\n", p);
                }
            });
    }
}

static void*
_ejs_finalizer_thread ()
{
    while (EJS_TRUE) {
        pthread_mutex_lock (&finalizer_mutex);
        pthread_cond_wait (&finalizer_cond, &finalizer_mutex);
        if (!finalizer_list) {
            pthread_mutex_unlock (&finalizer_mutex);
            return NULL;
        }

        EJSFinalizerEntry *this_run;
        this_run = finalizer_list;
        finalizer_list = NULL;

        pthread_mutex_unlock (&finalizer_mutex);

        SPEW(1, fprintf (stderr, "starting finalizer run\n"));
        while (this_run) {
            EJSFinalizerEntry *fin = this_run;
            EJS_LIST_DETACH(fin, this_run);

            Arena *arena = PTR_TO_ARENA(fin->gcobj);
            PageInfo* info;
            int cell_idx;

            info = find_page_and_cell (fin->gcobj, &cell_idx);

            if (!info) {
                int page_index = PTR_TO_ARENA_PAGE_INDEX(fin->gcobj);
                PageInfo *page = arena->page_infos[page_index];
                fprintf (stderr, "fin->gcobj = %p\n", fin->gcobj);
                fprintf (stderr, "page_index = %d, using page %p\n", page_index, page);
                fprintf (stderr, "sizeof cell for page = %zd\n", arena->page_infos[page_index]->cell_size);
                fprintf (stderr, "aligned to cell size? %s\n", IS_ALIGNED_TO(fin->gcobj, arena->page_infos[page_index]->cell_size) ? "yes" : "no");
            }

            assert(info);
            if (IS_FREE(info->page_bitmap[cell_idx])) {
                continue;
            }
            if (!IS_FINALIZABLE(info->page_bitmap[cell_idx]))
                fprintf (stderr, "%p->page_bitmap[%d] (corresponding to object %p) is not finalizable\n", info, cell_idx, info->page_start + (cell_idx * info->cell_size));
            assert(IS_FINALIZABLE(info->page_bitmap[cell_idx]));

            finalize_object(fin->gcobj);
            memset (fin->gcobj, 0, info->cell_size);

            SET_FREE(info->page_bitmap[cell_idx]);
            SPEW(3, fprintf (stderr, "finalized object %p in page %p, num_free_cells == %zd\n", fin->gcobj, info, info->num_free_cells + 1));
            // if this page is empty, move it to this arena's free list
            pthread_mutex_lock (&info->page_mutex);
            info->num_free_cells ++;
            pthread_mutex_unlock (&info->page_mutex);

            if (info->num_free_cells == info->num_cells) {
                LOCK_GC();
                pthread_mutex_lock (&info->page_mutex);
                if (info->num_free_cells == info->num_cells) {
                    if (info->los_info) {
                        printf ("releasing large object!\n");
                        release_to_los (info->los_info);
                    }
                    else {
                        SPEW(2, fprintf (stderr, "page %p is empty, putting it on the free list\n", info));
                        // the page is empty, add it to the arena's free page list.
                        int bucket = ffs(info->cell_size) - OBJECT_SIZE_LOW_LIMIT_BITS;
                        _ejs_list_detach_node (&heap_pages[bucket], (EJSListNode*)info);
                        EJS_LIST_PREPEND (info, arena->free_pages);
                    }
                }
                pthread_mutex_unlock (&info->page_mutex);
                UNLOCK_GC();
            }
            free (fin);
        }
        SPEW(1, fprintf (stderr, "finished finalizer run\n"));
        pthread_mutex_lock (&finalizer_mutex);
        pthread_cond_signal (&wait_for_finalizer_cond);
        pthread_mutex_unlock (&finalizer_mutex);
    }
    return NULL;
}

void
_ejs_gc_init()
{
    gc_disabled = getenv("EJS_GC_DISABLE") != NULL;
    char* n_allocs = getenv("EJS_GC_EVERY_N_ALLOC");
    if (n_allocs)
        collect_every_alloc = atoi(n_allocs);

    // allocate an initial arena
    arena_new();

    _ejs_gc_worklist_init();

    root_set = NULL;

    // start up the finalizer thread
    pthread_create (&finalizer_thread, NULL, _ejs_finalizer_thread, NULL);
}

static void
_scan_ejsvalue (ejsval val)
{
    if (!EJSVAL_IS_TRACEABLE_IMPL(val)) return;

    GCObjectPtr gcptr = (GCObjectPtr)EJSVAL_TO_GCTHING_IMPL(val);

    if (gcptr == NULL) return;

    WORKLIST_PUSH_AND_GRAY(gcptr);
}

static void
_scan_from_ejsobject(EJSObject* obj)
{
    OP(obj,scan)(obj, _scan_ejsvalue);
}

static void
_scan_from_ejsprimstr(EJSPrimString *primStr)
{
    if (EJS_PRIMSTR_GET_TYPE(primStr) == EJS_STRING_ROPE) {
        // inline _scan_ejsvalue's push logic here to save creating an ejsval from the primStr only to destruct
        // it in _scan_ejsvalue

        WORKLIST_PUSH_AND_GRAY(primStr->data.rope.left);
        WORKLIST_PUSH_AND_GRAY(primStr->data.rope.right);
    }
}

static void
_scan_from_ejsclosureenv(EJSClosureEnv *env)
{
    for (int i = 0; i < env->length; i ++) {
        _scan_ejsvalue (env->slots[i]);
    }
}

static GCObjectPtr *stack_bottom;

void
_ejs_gc_mark_thread_stack_bottom(GCObjectPtr* btm)
{
    stack_bottom = btm;
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

        int cell_idx;

        PageInfo *page = find_page_and_cell(gcptr, &cell_idx);
        if (!page)                    continue; // skip values outside our heap.

        // XXX more checks before we start treating the pointer like a GCObjectPtr?
        BitmapCell cell = page->page_bitmap[cell_idx];
        if (IS_FREE(cell))   continue; // skip free cells
        if (!IS_WHITE(cell)) continue; // skip pointers to gray/black cells

        WORKLIST_PUSH_AND_GRAY_CELL(gcptr, cell);
    }
}

static void
mark_ejsvals_in_range(void* low, void* high)
{
    void* p;
    for (p = low; p < high - sizeof(ejsval); p++) {
        ejsval candidate_val = *((ejsval*)p);
        if (EJSVAL_IS_GCTHING_IMPL(candidate_val)) {
            GCObjectPtr gcptr = (GCObjectPtr)EJSVAL_TO_GCTHING_IMPL(candidate_val);

            if (gcptr == NULL)            continue; // skip nulls.

            int cell_idx;
            PageInfo *p = find_page_and_cell(gcptr, &cell_idx);
            if (p) {
                // XXX more checks before we start treating the pointer like a GCObjectPtr?
                BitmapCell cell = p->page_bitmap[cell_idx];
                if (IS_FREE(cell)) continue; // skip free cells
                if (!IS_WHITE(cell)) continue; // skip pointers to gray/black cells

                if (EJSVAL_IS_STRING(candidate_val)) {
                    SPEW(4, fprintf (stderr, "found ptr to %p(PrimString) on stack\n", EJSVAL_TO_STRING(candidate_val)));
                    WORKLIST_PUSH_AND_GRAY_CELL(gcptr, cell);
                }
                else {
                    //SPEW(fprintf (stderr, "found ptr to %p(%s) on stack\n", EJSVAL_TO_OBJECT(candidate_val), CLASSNAME(EJSVAL_TO_OBJECT(candidate_val))));
                    WORKLIST_PUSH_AND_GRAY_CELL(gcptr, cell);
                }
            }
        }
    }
}

static int
finalizer_length(EJSFinalizerEntry *entry)
{
    int length = 0;
    for (; entry; entry = entry->next) {
        length++;
    }
    return length;
}

static int num_roots = 0;
static int white_objs = 0;
static int large_objs = 0;
static int total_objs = 0;

static void
sweep_heap()
{
    int pages_visited = 0;
    int pages_skipped = 0;

    // if we're getting too far ahead of the finalizer, wait
    if (finalizer_list) {
        pthread_mutex_lock (&finalizer_mutex);
        if (finalizer_list) {
            SPEW (2, fprintf (stderr, "finalizer list is %d entries long, waiting...\n", finalizer_length(finalizer_list)));
            pthread_cond_wait (&wait_for_finalizer_cond, &finalizer_mutex);
            SPEW (2, fprintf (stderr, "finalizer thread signaled us, waking up.\n"));
        }
        pthread_mutex_unlock (&finalizer_mutex);
    }

    // sweep the entire heap, freeing white nodes
    for (int a = 0, e = num_arenas; a < e; a ++) {
        Arena* arena = heap_arenas[a];

        for (int p = 0, pe = arena->num_pages; p < pe; p++) {
            PageInfo *info = arena->page_infos[p];

            if (info->num_free_cells == info->num_cells) {
                pages_skipped++;
            }
            else {
                pages_visited ++;

                for (int c = 0, ce = info->num_cells; c < ce; c ++) {
                    BitmapCell cell = info->page_bitmap[c];

                    if (IS_FREE(cell))
                        continue;

                    total_objs++;

                    if (!IS_FINALIZABLE(cell) && IS_WHITE(cell)) {
                        white_objs++;
                        EJSFinalizerEntry *fin = (EJSFinalizerEntry*)calloc(1, sizeof(EJSFinalizerEntry));
                        fin->gcobj = info->page_start + c * info->cell_size;
                        // this should likely be an append, and an EJSList* type so we have constant time appends
                        pthread_mutex_lock (&finalizer_mutex);

                        EJS_LIST_PREPEND(fin, finalizer_list); 

                        pthread_mutex_unlock (&finalizer_mutex);

                        SET_FINALIZABLE(info->page_bitmap[c]);
                    }
                }
            }
        }
    }
    
    // sweep the large object store
    SPEW(2, fprintf (stderr, "sweeping los: "));
    LargeObjectInfo *lobj = los_list;
    while (lobj) {
        large_objs ++;
        PageInfo *info = &lobj->page_info;
        BitmapCell cell = info->page_bitmap[0];
        LargeObjectInfo *next = lobj->next;
        if (!IS_FINALIZABLE(cell) && IS_WHITE(cell)) {
            SPEW(2, { fprintf (stderr, "l"); fflush(stderr); });
            white_objs++;
            EJSFinalizerEntry *fin = (EJSFinalizerEntry*)calloc(1, sizeof(EJSFinalizerEntry));
            fin->gcobj = info->page_start;
            EJS_LIST_DETACH(lobj, los_list);

            // this should likely be an append, and an EJSList* type so we have constant time appends
            pthread_mutex_lock (&finalizer_mutex);

            EJS_LIST_PREPEND(fin, finalizer_list);

            pthread_mutex_unlock (&finalizer_mutex);

            SET_FINALIZABLE(info->page_bitmap[0]);
        }
        else {
            SPEW(2, { fprintf (stderr, "L"); fflush(stderr); });
        }
        lobj = next;
    }
    SPEW(2, { fprintf (stderr, "\n"); });

    if (white_objs) {
        pthread_mutex_lock (&finalizer_mutex);
        pthread_cond_signal (&finalizer_cond);
        pthread_mutex_unlock (&finalizer_mutex);
    }
}

static void
mark_from_roots()
{
    // mark from our roots
    for (RootSetEntry *entry = root_set; entry; entry = entry->next) {
        num_roots++;
        if (entry->root) {
            ejsval rootval = *((ejsval*)entry->root);
            if (!EJSVAL_IS_GCTHING_IMPL(rootval))
                continue;
            GCObjectPtr root_ptr = (GCObjectPtr)EJSVAL_TO_GCTHING_IMPL(rootval);
            if (root_ptr == NULL)
                continue;
            int cell_idx;
            PageInfo* page = find_page_and_cell(root_ptr, &cell_idx);
            if (!page)
                continue;

            BitmapCell cell = page->page_bitmap[cell_idx];
            WORKLIST_PUSH_AND_GRAY_CELL(root_ptr, cell);
        }
    }
}

#if IOS
#define MARK_REGISTERS
#elif OSX
#define MARK_REGISTERS EJS_MACRO_START \
    GCObjectPtr __rax, __rbx, __rcx, __rdx, __rsi, __rdi, __rbp, __rsp, __r8, __r9, __r10, __r11, __r12, __r13, __r14, __r15; \
    __asm ("movq %%rax, %0; movq %%rbx, %1; movq %%rcx, %2; movq %%rdx, %3; movq %%rsi, %4;" \
           "movq %%rdi, %5; movq %%rbp, %6; movq %%rsp, %7; movq %%r8, %8;  movq %%r9, %9;" \
           "movq %%r10, %10; movq %%r11, %11; movq %%r12, %12; movq %%r13, %13; movq %%r14, %14; movq %%r15, %15;" \
          : "=m"(__rax), "=m"(__rbx), "=m"(__rcx), "=m"(__rdx), "=m"(__rsi), \
            "=m"(__rdi), "=m"(__rbp), "=m"(__rsp), "=m"(__r8),  "=m"(__r9), \
            "=m"(__r10), "=m"(__r11), "=m"(__r12), "=m"(__r13), "=m"(__r14), "=m"(__r15)); \
                                                                        \
    mark_pointers_in_range(&__r15, &__rax);                             \
    EJS_MACRO_END
#else
#error "put code here to mark registers"
#endif

static void
mark_thread_stack()
{
    MARK_REGISTERS;

    GCObjectPtr stack_top = NULL;

    mark_ejsvals_in_range(((void*)&stack_top) + sizeof(GCObjectPtr), stack_bottom);
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
        else if ((*headerp & EJS_SCAN_TYPE_CLOSUREENV) != 0)
            _scan_from_ejsclosureenv((EJSClosureEnv*)p);
    }
}

static void
_ejs_gc_collect_inner(EJSBool shutting_down)
{
#if gc_timings > 1
    struct timeval tvbefore, tvafter;
#endif

    // very simple stop the world collector
    SPEW(1, fprintf (stderr, "collection started\n"));

    num_roots = 0;
    white_objs = 0;
    large_objs = 0;
    total_objs = 0;

#if gc_timings > 1
    gettimeofday (&tvbefore, NULL);
#endif

    if (!shutting_down) {
        mark_from_roots();

        total_objs = num_roots;

        mark_thread_stack();

        process_worklist();
    }

#if gc_timings > 1
    gettimeofday (&tvafter, NULL);
#endif

#if gc_timings > 1
    {
        uint64_t usec_before = tvbefore.tv_sec * 1000000 + tvbefore.tv_usec;
        uint64_t usec_after = tvafter.tv_sec * 1000000 + tvafter.tv_usec;

        fprintf (stderr, "gc scan took %gms\n", (usec_after - usec_before) / 1000.0);
    }
#endif

#if gc_timings > 1
    gettimeofday (&tvbefore, NULL);
#endif

    sweep_heap();

#if gc_timings > 1
    {
        gettimeofday (&tvafter, NULL);
    }
#endif

#if gc_timings > 1
    {
        uint64_t usec_before = tvbefore.tv_sec * 1000000 + tvbefore.tv_usec;
        uint64_t usec_after = tvafter.tv_sec * 1000000 + tvafter.tv_usec;

        fprintf (stderr, "gc sweep took %gms\n", (usec_after - usec_before) / 1000.0);
    }
#endif

#if gc_timings > 1
    fprintf (stderr, "_ejs_gc_collect stats:\n");
    fprintf (stderr, "   num_roots: %d\n", num_roots);
    fprintf (stderr, "   total objects: %d\n", total_objs);
    fprintf (stderr, "   num large objects: %d\n", large_objs);
    fprintf (stderr, "   garbage objects: %d\n", white_objs);
#endif

    unsigned int tmp = black_mask;
    black_mask = white_mask;
    white_mask = tmp;

    if (shutting_down) {
        // NULL out all of our roots

        RootSetEntry *entry = root_set;
        while (entry) {
            RootSetEntry *next = entry->next;
            *entry->root = _ejs_null;
            free (entry);
            entry = next;
        }

        root_set = NULL;

        SPEW(1, fprintf (stderr, "final gc page statistics:\n");
             for (int hp = 0; hp < HEAP_PAGELISTS_COUNT; hp++) {
                 int len = 0;

                 EJS_LIST_FOREACH (&heap_pages[hp], PageInfo, page, {
                         len ++;
                 });

                 fprintf (stderr, "  size: %d     pages: %d\n", 1<<(hp + 3), len);
             });
    }
#if sanity
    else {
        for (int hp = 0; hp < HEAP_PAGELISTS_COUNT; hp++) {
            EJS_LIST_FOREACH (&heap_pages[hp], PageInfo, page, {
                for (int c = 0; c < CELLS_IN_PAGE (page); c ++) {
                    if (!IS_FREE(page->page_bitmap[c]) && !IS_WHITE(page->page_bitmap[c]))
                        continue;
                }  
            })
        }
    }
#endif
    SPEW(1, fprintf (stderr, "collection finished\n"));
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

void
_ejs_gc_collect()
{
#if gc_timings
    struct timeval tvbefore, tvafter;

    gettimeofday (&tvbefore, NULL);

    int heap_size = calc_heap_size();
#endif

    _ejs_gc_collect_inner(EJS_FALSE);

#if gc_timings
    gettimeofday (&tvafter, NULL);

    uint64_t usec_before = tvbefore.tv_sec * 1000000 + tvbefore.tv_usec;
    uint64_t usec_after = tvafter.tv_sec * 1000000 + tvafter.tv_usec;

    fprintf (stderr, "gc collect took %gms\n", (usec_after - usec_before) / 1000.0);
    fprintf (stderr, "   for a heap size of %zdMB\n", heap_size/(1024*1024));
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
    SPEW(1, fprintf (stderr, "total allocs = %d\n", total_allocs));
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
    pthread_mutex_lock (&info->page_mutex);

    assert (info->num_free_cells > 0);
    
    GCObjectPtr rv = NULL;
    int cell;

    SPEW(2, fprintf (stderr, "allocating object from page %p (cell size %zd)\n", info, info->cell_size));

    if (info->bump_ptr) {
        rv = (GCObjectPtr)info->bump_ptr;
        cell = PTR_TO_CELL(info->bump_ptr, info);
        info->bump_ptr += info->cell_size;
        // check if we can service the next alloc request from the bump_ptr.  if we can't, switch
        // to the freelist code below.
        if (info->bump_ptr + info->cell_size >= info->page_end)
            info->bump_ptr = NULL;
    }
    else {
        for (cell = 0; cell < info->num_cells; cell ++) {
            if (IS_FREE(info->page_bitmap[cell])) {
                rv = info->page_start + (cell * info->cell_size);
                break;
            }
        }
    }

    assert (rv);

    SET_ALLOCATED(info->page_bitmap[cell]);
    SET_WHITE(info->page_bitmap[cell]);

    info->num_free_cells --;

    pthread_mutex_unlock (&info->page_mutex);

    SPEW(2, fprintf (stderr, "allocated obj %p from page %p (cell size %zd), free cells remaining %zd\n", rv, info, info->cell_size, info->num_free_cells));
    return rv;
}

static GCObjectPtr
alloc_from_los(size_t size, EJSScanType scan_type)
{
    // allocate enough space for the object, our header, and our bitmap.  leave room enough to align the return value
    LargeObjectInfo *rv = alloc_from_os(size + sizeof(LargeObjectInfo) + 16, 0);

    rv->page_info.page_bitmap = (char*)((void*)rv + sizeof(LargeObjectInfo)); // our 1 byte bitmap comes right after the header
    rv->page_info.page_start = (void*)ALIGN((void*)rv + sizeof(LargeObjectInfo) + 1, 8);
    rv->page_info.cell_size = size;
    rv->page_info.num_cells = 1;
    rv->page_info.num_free_cells = 0;
    rv->page_info.los_info = rv;
    SET_WHITE(rv->page_info.page_bitmap[0]);
    SET_ALLOCATED(rv->page_info.page_bitmap[0]);

    rv->alloc_size = size;

    EJS_LIST_PREPEND (rv, los_list);
    //fprintf (stderr, "alloc_from_los returning %p\n, los_list = %p\n", rv->page_info.page_start, los_list);
    return rv->page_info.page_start;
}

static void
release_to_los (LargeObjectInfo *lobj)
{
    release_to_os (lobj, lobj->alloc_size);
}

size_t alloc_size = 0;
int num_allocs = 0;

GCObjectPtr
_ejs_gc_alloc(size_t size, EJSScanType scan_type)
{
    alloc_size += size;

    num_allocs ++;
    total_allocs ++;

    if (!gc_disabled && ((num_allocs == 1000000 /*|| alloc_size >= 20*1024*1024*/) || (collect_every_alloc && collect_every_alloc == num_allocs))) {
        _ejs_gc_collect();
        alloc_size = 0;
        num_allocs = 0;
    }

    int bucket;
    size = pow2_ceil(size);

    bucket = ffs(size) - OBJECT_SIZE_LOW_LIMIT_BITS;

    if (bucket > HEAP_PAGELISTS_COUNT) {
        fprintf (stderr, "need to alloc from los!!!\n");
        return alloc_from_los(size, scan_type);
    }

    LOCK_GC();

    GCObjectPtr rv = NULL;
    PageInfo* info = (PageInfo*)heap_pages[bucket].head;
    if (!info || !info->num_free_cells) {
        info = alloc_new_page(size);
        _ejs_list_prepend_node (&heap_pages[bucket], (EJSListNode*)info);
    }

    rv = alloc_from_page(info);
    *((GCObjectHeader*)rv) = scan_type;

    if (info->num_free_cells == 0) {
        // if the page is full, bump it to the end of the list (if there's more than 1 page in the list)
        if (heap_pages[bucket].head != heap_pages[bucket].tail) {
            _ejs_list_pop_head (&heap_pages[bucket]);
            _ejs_list_append_node (&heap_pages[bucket], (EJSListNode*)info);
        }
    }

    UNLOCK_GC();

    return rv;
}

void
_ejs_gc_add_root(ejsval* root)
{
    RootSetEntry* entry = (RootSetEntry*)malloc(sizeof(RootSetEntry));
    EJS_LIST_INIT(entry);
    entry->root = root;
    EJS_LIST_PREPEND(entry, root_set);
}

void
_ejs_gc_remove_root(ejsval* root)
{
    RootSetEntry *entry = NULL;

    for (entry = root_set; entry; entry = entry->next) {
        if (entry->root == root) {
            EJS_LIST_DETACH(entry, root_set);
            free (entry);
            return;
        }
    }
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
    fprintf (stderr, "arenas:\n");
    for (int i = 0; i < num_arenas; i ++) {
        fprintf (stderr, "  [%d] - %p - %p\n", i, heap_arenas[i], heap_arenas[i]->end);
    }

    for (int i = 0; i < HEAP_PAGELISTS_COUNT; i ++) {
#if gc_timings > 3
        EJSBool printed_something = EJS_FALSE;
#endif
        fprintf (stderr, "heap_pages[%d, size %d] : %d pages\n", i, 1 << (i + OBJECT_SIZE_LOW_LIMIT_BITS), _ejs_list_length (&heap_pages[i]));
#if gc_timings > 3
        EJS_LIST_FOREACH (&heap_pages[i], PageInfo, page, {
            GCObjectPtr p = page->page_start;
            for (int c = 0; c < CELLS_IN_PAGE (page); c ++, p += page->cell_size) {
                if (IS_FREE(page->page_bitmap[c]) || IS_FINALIZABLE(page->page_bitmap[c]))
                    continue;
                GCObjectHeader* headerp = (GCObjectHeader*)p;
                if ((*headerp & EJS_SCAN_TYPE_OBJECT) != 0)          fprintf (stderr, "O");
                else if ((*headerp & EJS_SCAN_TYPE_CLOSUREENV) != 0) fprintf (stderr, "C");
                else if ((*headerp & EJS_SCAN_TYPE_PRIMSTR) != 0)    fprintf (stderr, "S");
                printed_something = EJS_TRUE;
            }
        })
        if (printed_something)
            fprintf (stderr, "\n");
#endif
    }

    fprintf (stderr, "\n");

    if (los_list) {
        fprintf (stderr, "large object store: ");
        for (LargeObjectInfo* lobj = los_list; lobj; lobj = lobj->next) {
            GCObjectHeader* headerp = (GCObjectHeader*)lobj->page_info.page_start;
            if ((*headerp & EJS_SCAN_TYPE_OBJECT) != 0)          fprintf (stderr, "O");
            else if ((*headerp & EJS_SCAN_TYPE_CLOSUREENV) != 0) fprintf (stderr, "C");
            else if ((*headerp & EJS_SCAN_TYPE_PRIMSTR) != 0)    fprintf (stderr, "S");
        }
        fprintf (stderr, "\n");
    }
}

/////////
ejsval _ejs_GC;

static ejsval
_ejs_GC_collect (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    _ejs_gc_collect();
    return _ejs_undefined;
}

void
_ejs_GC_init(ejsval global)
{
    _ejs_GC = _ejs_object_new (_ejs_Object_prototype, &_ejs_object_specops);

    ejsval __ejs = _ejs_object_getprop (global, _ejs_atom___ejs);
    _ejs_object_setprop (__ejs, _ejs_atom_GC, _ejs_GC);

#define OBJ_METHOD(x) EJS_INSTALL_ATOM_FUNCTION(_ejs_GC, x, _ejs_GC_##x)

    OBJ_METHOD(collect);

#undef OBJ_METHOD
}

