/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <setjmp.h>

#include "ejs-gc.h"
#include "ejs-value.h"
#include "ejs-string.h"
#include "ejsval.h"

#define spew 0
#if spew
#define SPEW(x) x
#else
#define SPEW(x)
#endif

#define PAGE_SIZE 4096

#define OBJ_TO_PAGE(o) ((o) & ~PAGE_SIZE)

#define ALLOC_ALIGN 16
#define ALIGN(v,a) ((v) + (a)-1) & ~((a)-1)
#define IS_ALLOC_ALIGNED(v) (((unsigned long long)(v) & (ALLOC_ALIGN-1)) == 0)

// #if osx
#include <mach/vm_statistics.h>
#define MAP_FD VM_MAKE_TAG (VM_MEMORY_APPLICATION_SPECIFIC_16)
// #else
// #define MAP_FD -1
// #endif

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

typedef struct _RootSetEntry {
    EJS_SLIST_HEADER(struct _RootSetEntry);
    GCObjectPtr* root;
    char *name;
} RootSetEntry;

static RootSetEntry *root_set;

typedef struct _FreeListEntry {
    EJS_SLIST_HEADER(struct _FreeListEntry);
} FreeListEntry;

typedef char BitmapCell;

#define CELL_COLOR_MASK       0x03
#define CELL_GRAY_MASK        0x02
#define CELL_WHITE_MASK_START 0x00
#define CELL_BLACK_MASK_START 0x01
#define CELL_FREE             0x04 // cell is in the free list for this page

static unsigned int black_mask = CELL_BLACK_MASK_START;
static unsigned int white_mask = CELL_WHITE_MASK_START;

#define SET_GRAY(cell) EJS_MACRO_START                      \
    (cell) = ((cell) & ~CELL_COLOR_MASK) | CELL_GRAY_MASK;	\
    EJS_MACRO_END

#define SET_WHITE(cell) EJS_MACRO_START                 \
    (cell) = ((cell) & ~CELL_COLOR_MASK) | white_mask;	\
    EJS_MACRO_END

#define SET_BLACK(cell) EJS_MACRO_START                 \
    (cell) = ((cell) & ~CELL_COLOR_MASK) | black_mask;	\
    EJS_MACRO_END

#define SET_FREE(cell) EJS_MACRO_START			\
    (cell) = CELL_FREE;                         \
    EJS_MACRO_END

#define SET_ALLOCATED(cell) EJS_MACRO_START     \
    (cell) = ((cell) & ~CELL_FREE);				\
    EJS_MACRO_END

#define IS_FREE(cell) (((cell) & CELL_FREE) == CELL_FREE)
#define IS_GRAY(cell) (((cell) & CELL_COLOR_MASK) == CELL_GRAY_MASK)
#define IS_WHITE(cell) (((cell) & CELL_COLOR_MASK) == white_mask)
#define IS_BLACK(cell) (((cell) & CELL_COLOR_MASK) == black_mask)


typedef struct _PageInfo {
    EJS_LIST_HEADER(struct _PageInfo);
    size_t cell_size;
    char* page_data;
    BitmapCell* page_bitmap;
    FreeListEntry* freelist;
} PageInfo;

typedef struct _LargeObjectInfo {
    EJS_LIST_HEADER(struct _LargeObjectInfo);
    char *los_data;
    size_t size;
} LargeObjectInfo;

#define OBJECT_SIZE_LOW_LIMIT_BITS 3   // number of bits required to represent a pointer (3 == 8 bytes for 64 bit)
#define OBJECT_SIZE_HIGH_LIMIT_BITS 10 // max object size for the non-LOS allocator = 1024

#define HEAP_PAGELISTS_COUNT (OBJECT_SIZE_HIGH_LIMIT_BITS - OBJECT_SIZE_LOW_LIMIT_BITS)
static PageInfo* heap_pages[HEAP_PAGELISTS_COUNT];
static LargeObjectInfo* los_store;

static PageInfo*
find_page(GCObjectPtr ptr)
{
    for (int hp = 0; hp < HEAP_PAGELISTS_COUNT; hp++) {
        for (PageInfo *page = heap_pages[hp]; page; page = page->next) {
            if (ptr >= page->page_data && (char*)ptr < ((char*)page->page_data + PAGE_SIZE))
                return page;
        }
    }
    return NULL;
}

static void
set_gray (GCObjectPtr ptr)
{
    PageInfo *page = find_page(ptr);
    if (!page) {
        return;
    }
    assert((ptr - page->page_data) / page->cell_size >= 0 &&
           (ptr - page->page_data) / page->cell_size < PAGE_SIZE / page->cell_size); 
    SET_GRAY(page->page_bitmap[(ptr - page->page_data)/ page->cell_size]);
}

static void
set_black (GCObjectPtr ptr)
{
    PageInfo *page = find_page(ptr);
    if (!page) {
        return;
    }
    assert((ptr - page->page_data) / page->cell_size >= 0 &&
           (ptr - page->page_data) / page->cell_size < PAGE_SIZE / page->cell_size); 
    SET_BLACK(page->page_bitmap[(ptr - page->page_data)/ page->cell_size]);
}

static EJSBool
is_white (GCObjectPtr ptr)
{
    PageInfo *page = find_page(ptr);
    if (!page) {
        return EJS_FALSE;
    }
    assert((ptr - page->page_data) / page->cell_size >= 0 &&
           (ptr - page->page_data) / page->cell_size < PAGE_SIZE / page->cell_size); 
    return IS_WHITE(page->page_bitmap[(ptr - page->page_data)/ page->cell_size]);
}

static void
populate_initial_freelist(PageInfo *info)
{
    info->freelist = NULL;

    void* ptr = info->page_data;

    for (int i = 0; i < PAGE_SIZE / info->cell_size; i ++) {
        SET_FREE(info->page_bitmap[i]);
        EJS_SLIST_ATTACH(((FreeListEntry*)ptr), info->freelist);
        ptr = (void*)((uintptr_t)ptr + info->cell_size);
    }
}

static void
add_cell_to_freelist (PageInfo *info, int bitmap_cell_index)
{
    SET_FREE(info->page_bitmap[bitmap_cell_index]);
    EJS_SLIST_ATTACH(((FreeListEntry*)((char*)info->page_data + bitmap_cell_index * info->cell_size)), info->freelist);
}

static char*
alloc_from_os (size_t size)
{
    return (char*)mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, MAP_FD, 0);
}

static void
dealloc_from_os(char* ptr, size_t size)
{
    munmap(ptr, size);
}

static PageInfo*
init_page_info(char *page_data, size_t cell_size)
{
    PageInfo* info = (PageInfo*)malloc(sizeof(PageInfo));
    EJS_LIST_INIT(info);
    info->cell_size = cell_size;
    info->page_data = page_data;
    info->page_bitmap = (BitmapCell*)malloc (PAGE_SIZE / cell_size);
    info->freelist = NULL;
    memset (info->page_bitmap, CELL_FREE, PAGE_SIZE / cell_size);
    populate_initial_freelist(info);
    return info;
}

static PageInfo*
alloc_new_page(size_t cell_size)
{
    return init_page_info (alloc_from_os(PAGE_SIZE), cell_size);
}

static void
dealloc_page(char* page_addr)
{
    dealloc_from_os(page_addr, PAGE_SIZE);
}

void
_ejs_gc_init()
{
    memset (heap_pages, 0, sizeof(heap_pages));

#if POPULATE_INITIAL_HEAP
#define INITIAL_HEAP_SIZE 2 * 1024 * 1024
    // preallocate 2 megs worth of pages
    char* initial_heap = alloc_from_os(INITIAL_HEAP_SIZE);
    
    char* p = initial_heap;
    PageInfo *info;
    // and split them across the most commonly used areas (64 and 128 byte cells)
    for (int i = 0; i < INITIAL_HEAP_SIZE / (PAGE_SIZE * 2); i ++) {
        info = init_page_info(p, 2<<6);
        EJS_LIST_ATTACH(info, heap_pages[6-OBJECT_SIZE_LOW_LIMIT_BITS]);
        p += PAGE_SIZE;

        info = init_page_info(p, 2<<7);
        EJS_LIST_ATTACH(info, heap_pages[7-OBJECT_SIZE_LOW_LIMIT_BITS]);
        p += PAGE_SIZE;
    }
#endif

    _ejs_gc_worklist_init();

    root_set = NULL;
}

static void
_scan_ejsvalue (ejsval val)
{
    if (!EJSVAL_IS_GCTHING_IMPL(val)) return;

    if (EJSVAL_IS_NULL(val)) return;

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

#if CONSERVATIVE_STACKWALK
static GCObjectPtr *stack_bottom;

void
_ejs_gc_mark_thread_stack_bottom(GCObjectPtr* btm)
{
    stack_bottom = btm;
}

#else

StackEntry *llvm_gc_root_chain;

static void
mark_from_shadow_stack()
{
    for (StackEntry *R = llvm_gc_root_chain; R; R = R->Next) {
        unsigned i = 0;

        if (R->Map == NULL) {
            continue;
        }

        // For roots [NumMeta, NumRoots), the metadata pointer is null.
        for (unsigned e = R->Map->NumRoots; i != e; ++i) {
            ejsval candidate_val = *(ejsval*)&R->Roots[i];
            if (!EJSVAL_IS_GCTHING_IMPL(candidate_val)) {
                // FIXME we mark all ejsval's as roots, even those that aren't objects.
                continue;
            }
            GCObjectPtr gcptr = (GCObjectPtr)EJSVAL_TO_GCTHING_IMPL(candidate_val);

            if (!is_white(gcptr))         continue; // skip pointers to gray/black cells

            SPEW(printf ("found ptr to %p on stack\n", EJSVAL_TO_OBJECT(candidate_val)));
            _ejs_gc_worklist_push(gcptr);
            set_gray(gcptr);
        }
    }
}
#endif

static void
mark_in_range(char* low, char* high)
{
    char* p;
    for (p = low; p < high; p++) {
        ejsval candidate_val = *((ejsval*)p);
        if (EJSVAL_IS_GCTHING_IMPL(candidate_val)) {
            GCObjectPtr gcptr = (GCObjectPtr)EJSVAL_TO_GCTHING_IMPL(candidate_val);
            if (gcptr == NULL)            continue; // skip nulls.
            if (!IS_ALLOC_ALIGNED(gcptr)) continue; // skip values that can't possibly point to allocated cells.
            if (find_page(gcptr) == NULL) continue; // skip values outside our heap.

            // XXX more checks before we start treating the pointer like a GCObjectPtr?

            if (!is_white(gcptr))         continue; // skip pointers to gray/black cells

            if (EJSVAL_IS_STRING(candidate_val)) {
                SPEW(printf ("found ptr to %p(PrimString:%s) on stack\n", EJSVAL_TO_STRING(candidate_val), EJSVAL_TO_FLAT_STRING(candidate_val)));
                _ejs_gc_worklist_push(gcptr);
                set_gray(gcptr);
            }
            else {
                SPEW(printf ("found ptr to %p(%s) on stack\n", EJSVAL_TO_OBJECT(candidate_val), CLASSNAME(EJSVAL_TO_OBJECT(candidate_val))));
                _ejs_gc_worklist_push(gcptr);
                set_gray(gcptr);
            }
        }
    }
}

static int num_roots = 0;
static int white_objs = 0;
static int total_objs = 0;

static void
sweep_heap()
{
    // sweep the entire heap, freeing white nodes
    for (int hp = 0; hp < HEAP_PAGELISTS_COUNT; hp++) {
        for (PageInfo *info = heap_pages[hp]; info; info = info->next) {
            for (int c = 0; c < PAGE_SIZE / info->cell_size; c ++) {
                if (IS_FREE(info->page_bitmap[c]))
                    continue;

                total_objs++;

                if (IS_WHITE(info->page_bitmap[c])) {
                    white_objs++;
                    GCObjectPtr p = info->page_data + c * info->cell_size;
                    GCObjectHeader* headerp = (GCObjectHeader*)p;
                    if ((*headerp & EJS_SCAN_TYPE_OBJECT) != 0) {
                        SPEW(printf ("finalizing object %p(%s)\n", p, CLASSNAME(p)));
                        OP(p, finalize)((EJSObject*)p);
                    }
                    else {
                        SPEW(printf ("finalizing primitive string %p(%s)\n", p, _ejs_primstring_flatten((EJSPrimString*)p)->data.flat));
                    }
                    add_cell_to_freelist (info, c);
                }
            }
        }
    }
}

static void
mark_from_roots()
{
    // mark from our roots
    for (RootSetEntry *entry = root_set; entry; entry = entry->next) {
        num_roots++;
        if (*entry->root) {
            ejsval rootval = *((ejsval*)entry->root);
            if (!EJSVAL_IS_GCTHING_IMPL(rootval))
                continue;
            GCObjectPtr root_ptr = (GCObjectPtr)EJSVAL_TO_GCTHING_IMPL(rootval);
            if (root_ptr == NULL)
                continue;
            set_black (root_ptr);
            if (EJSVAL_IS_OBJECT(rootval)) {
                _scan_from_ejsobject((EJSObject*)root_ptr);
            }
            else {
                _scan_from_ejsprimstr((EJSPrimString*)root_ptr);
            }
        }
    }
}

static void
mark_thread_stack()
{
#if CONSERVATIVE_STACKWALK
    GCObjectPtr stack_top = NULL;
#endif

#if CONSERVATIVE_STACKWALK
        mark_in_range(((char*)&stack_top) + sizeof(GCObjectPtr), (char*)stack_bottom);
#else
        mark_from_shadow_stack();
#endif
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
        else
            _scan_from_ejsprimstr((EJSPrimString*)p);
    }
}

static void
_ejs_gc_collect_inner(EJSBool shutting_down)
{
    // very simple stop the world collector

    if (!shutting_down) {
        mark_from_roots();

        total_objs = num_roots;

        mark_thread_stack();

        process_worklist();
    }

    sweep_heap();

    SPEW(printf ("_ejs_gc_collect stats:\n");
         printf ("   num_roots: %d\n", num_roots);
         printf ("   total objects: %d\n", total_objs);
         printf ("   garbage objects: %d\n", white_objs));

    unsigned int tmp = black_mask;
    black_mask = white_mask;
    white_mask = tmp;

    if (shutting_down) {
        // NULL out all of our roots

        RootSetEntry *entry = root_set;
        while (entry) {
            RootSetEntry *next = entry->next;
            *entry->root = NULL;
            if (entry->name) free (entry->name);
            free (entry);
            entry = next;
        }

        root_set = NULL;

#undef SPEW
#define SPEW(x) x
        SPEW(printf ("final gc page statistics:\n");
             for (int hp = 0; hp < HEAP_PAGELISTS_COUNT; hp++) {
                 int len = 0;
                 for (PageInfo *info = heap_pages[hp]; info; info = info->next)
                     len++;
                 printf ("  size: %d     pages: %d\n", 1<<(hp + 3), len);
             })
#undef SPEW
#define SPEW(x)
    }
    else {
        for (int hp = 0; hp < HEAP_PAGELISTS_COUNT; hp++) {
            for (PageInfo *info = heap_pages[hp]; info; info = info->next) {
                for (int c = 0; c < PAGE_SIZE / info->cell_size; c ++) {
                    if (!IS_FREE(info->page_bitmap[c]) && !IS_WHITE(info->page_bitmap[c]))
                        continue;
                }  
            }
        }
    }
}

void
_ejs_gc_collect()
{
    _ejs_gc_collect_inner(EJS_FALSE);
}

int total_allocs = 0;

void
_ejs_gc_shutdown()
{
    _ejs_gc_collect_inner(EJS_TRUE);
    SPEW(printf ("total allocs = %d\n", total_allocs));
}

static inline int next_power_of_two(int x, int *bits)
{
    int n = 1 << (OBJECT_SIZE_LOW_LIMIT_BITS-1);
    *bits = OBJECT_SIZE_LOW_LIMIT_BITS;
    while (n < x) {
        n <<= 1;
        (*bits)++;
    }
    return n;
}

static GCObjectPtr
alloc_from_freelist(PageInfo *info)
{
    FreeListEntry *free_entry = info->freelist;
    if (!free_entry)
        return NULL;
    info->freelist = free_entry->next;
    GCObjectPtr rv = (GCObjectPtr)free_entry;
    // initialize the cell
    SET_WHITE(info->page_bitmap[(rv-info->page_data) / info->cell_size]);
    SET_ALLOCATED(info->page_bitmap[(rv-info->page_data) / info->cell_size]);

    *((GCObjectHeader*)rv) = 0;

    return rv;
}

static GCObjectPtr
alloc_from_los(size_t size, EJSBool has_finalizer)
{
    LargeObjectInfo *rv = (LargeObjectInfo*)calloc(1, sizeof(LargeObjectInfo));

    // we need to bump size up to a mulitple of the OS page size
    rv->los_data = alloc_from_os (size);
    rv->size = size;
    EJS_LIST_PREPEND (rv, los_store);
    return rv->los_data;
}

int num_allocs = 0;

GCObjectPtr
_ejs_gc_alloc(size_t size, EJSScanType scan_type)
{
    num_allocs ++;
    total_allocs ++;

    if (num_allocs == 1000) {
        _ejs_gc_collect();
        num_allocs = 0;
    }

    int bucket;
    size = next_power_of_two(size, &bucket);

    bucket -= OBJECT_SIZE_LOW_LIMIT_BITS; // we allocate nothing smaller than this

    if (bucket >= HEAP_PAGELISTS_COUNT)
        return alloc_from_los(size, scan_type);

    GCObjectPtr rv = NULL;
    for (PageInfo* info = heap_pages[bucket]; info; info = info->next) {
        rv = alloc_from_freelist(info);
        if (rv) {
            *((GCObjectHeader*)rv) = scan_type;
            return rv;
        }
    }

    // we need a new page
    PageInfo *info = alloc_new_page(size);
    EJS_LIST_PREPEND(info, heap_pages[bucket]);
    rv = alloc_from_freelist(info);
    *((GCObjectHeader*)rv) = scan_type;
    return rv;
}

void
__ejs_gc_add_named_root(ejsval* root, const char *name)
{
    RootSetEntry* entry = (RootSetEntry*)malloc(sizeof(RootSetEntry));
    entry->root = (GCObjectPtr*)root;
    entry->name = name ? strdup(name) : NULL;
    entry->next = root_set;
    root_set = entry;
}

void
_ejs_gc_remove_root(ejsval* root)
{
    abort();
}
