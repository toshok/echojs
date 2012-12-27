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

// a 4 meg heap
#define INITIAL_HEAP_SIZE 4 * 1024 * 1024

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

#define MAX_WORKLIST_PART_SIZE 128
typedef struct _WorkListSegmnt {
    EJS_SLIST_HEADER(struct _WorkListSegmnt);
    int size;
    GCObjectPtr work_list[MAX_WORKLIST_PART_SIZE];
} WorkListSegment;

static WorkListSegment* work_list;

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

#define CELL_OBJ_FINALIZER    0x08 // object in this cell has a finalizer, invoke it before moving to CELL_FREE state
#define CELL_FREE             0x04 // cell is in the free list for this page
#define CELL_COLOR_MASK       0x03
#define CELL_GRAY_MASK        0x02
#define CELL_WHITE_MASK_START 0x00
#define CELL_BLACK_MASK_START 0x01

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

#define HAS_FINALIZER(cell) (((cell) & CELL_OBJ_FINALIZER) == CELL_OBJ_FINALIZER)
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
        abort();
    }
    SET_GRAY(page->page_bitmap[(ptr - page->page_data)/ page->cell_size]);
}

static void
set_black (GCObjectPtr ptr)
{
    PageInfo *page = find_page(ptr);
    if (!page) {
        abort();
    }
    SET_BLACK(page->page_bitmap[(ptr - page->page_data)/ page->cell_size]);
}

static EJSBool
is_white (GCObjectPtr ptr)
{
    PageInfo *page = find_page(ptr);
    if (!page) {
        abort();
    }
    return IS_WHITE(page->page_bitmap[(ptr - page->page_data)/ page->cell_size]);
}

static void
populate_initial_freelist(PageInfo *info, int cell_size)
{
    info->freelist = NULL;

    void* ptr = info->page_data;

    for (int i = 0; i < PAGE_SIZE / cell_size; i ++) {
        SET_FREE(info->page_bitmap[i]);
        EJS_SLIST_ATTACH(((FreeListEntry*)ptr), info->freelist);
        ptr = (void*)((uintptr_t)ptr + cell_size);
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
alloc_page(size_t cell_size)
{
    PageInfo* rv = (PageInfo*)calloc(1, sizeof(PageInfo));
    rv->cell_size = cell_size;
    rv->page_data = alloc_from_os(PAGE_SIZE);
    rv->page_bitmap = (BitmapCell*)malloc (PAGE_SIZE / cell_size);
    memset (rv->page_bitmap, CELL_FREE, PAGE_SIZE / cell_size);
    populate_initial_freelist(rv, cell_size);
    return rv;
}

static void
dealloc_page(char* page_addr)
{
    dealloc_from_os(page_addr, PAGE_SIZE);
}

void
_ejs_gc_init()
{
    // XXX todo preallocate a number of pages of proper sizes

    memset (heap_pages, 0, sizeof(heap_pages));
#if notyet
    // allocate our initial heap
    for (int i = 0; i < INITIAL_HEAP_SIZE / PAGE_SIZE; i ++) {
        PageInfo *info = alloc_page();
        EJS_LIST_ATTACH(info, heap_pages);
    }
#else
#endif

    work_list = NULL;
    root_set = NULL;
}

void
push_to_worklist(GCObjectPtr obj)
{
    if (obj == NULL)
        return;

    WorkListSegment *segment = work_list;

    if (!segment || segment->size == MAX_WORKLIST_PART_SIZE) {
        segment = (WorkListSegment*)malloc (sizeof(WorkListSegment));
        EJS_SLIST_INIT(segment);
        segment->size = 0;
        EJS_SLIST_ATTACH(segment, work_list);
    }

    if (segment && segment->size < MAX_WORKLIST_PART_SIZE)
        segment->work_list[segment->size++] = obj;
    else
        set_gray (obj);
}

GCObjectPtr
pop_from_worklist()
{
    WorkListSegment *segment = work_list;

    if (segment == NULL)
        return NULL;
    if (segment->size == 0) // shouldn't happen
        return NULL;

    GCObjectPtr rv = segment->work_list[--segment->size];
    if (segment->size == 0) {
        work_list = segment->next;
        free (segment);
    }
    set_black (rv);
    return rv;
}

static void _scan_from_ejsobject(GCObjectPtr obj);

static void
_scan_ejsvalue (ejsval val)
{
    if (!EJSVAL_IS_GCTHING_IMPL(val))
        return;

    if (EJSVAL_IS_NULL(val))
        return;

    GCObjectPtr gcptr = (GCObjectPtr)EJSVAL_TO_GCTHING_IMPL(val);

    if (gcptr == NULL)
        return;

    if (EJSVAL_IS_OBJECT(val)) {
        // grey objects are in the mark stack, black objects we know are not garbage
        if (!is_white(gcptr))
            return;

        push_to_worklist(gcptr);
    }
    else {
        set_black(gcptr);
    }
}

static void
_scan_from_ejsobject(GCObjectPtr obj)
{
    OP(obj,scan)((EJSObject*)obj, _scan_ejsvalue);
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

        // For roots [NumMeta, NumRoots), the metadata pointer is null.
        for (unsigned e = R->Map->NumRoots; i != e; ++i) {
            ejsval candidate_val = *(ejsval*)&R->Roots[i];
            if (!EJSVAL_IS_OBJECT(candidate_val)) {
                // FIXME we mark all ejsval's as roots, even those that aren't objects.
                continue;
            }
            GCObjectPtr candidate = (GCObjectPtr)EJSVAL_TO_OBJECT(candidate_val);
            if (candidate == NULL) continue;
            if (is_white (candidate)) {
                SPEW(printf ("Found reference to %p on stack\n", candidate);)
                    push_to_worklist (candidate);
            }
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
                SPEW(printf ("found ptr to %p(PrimString) on stack\n", EJSVAL_TO_STRING(candidate_val));)
                    set_black(gcptr);
            }
            else {
                SPEW(printf ("found ptr to %p(%s) on stack\n", EJSVAL_TO_OBJECT(candidate_val), CLASSNAME(EJSVAL_TO_OBJECT(candidate_val)));)
                    push_to_worklist(gcptr);
            }
        }
    }
}

static void
_ejs_gc_collect_inner(EJSBool shutting_down)
{
#if CONSERVATIVE_STACKWALK
    GCObjectPtr stack_top = NULL;
#endif

    // very simple stop the world collector

    int num_roots = 0;
    int white_objs = 0;
    int total_objs = 0;

    if (!shutting_down) {
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
                    _scan_from_ejsobject(root_ptr);
                }
            }
        }

#if CONSERVATIVE_STACKWALK
        mark_in_range(((char*)&stack_top) + sizeof(GCObjectPtr), (char*)stack_bottom);
#else
        mark_from_shadow_stack();
#endif

        GCObjectPtr p;
        while ((p = pop_from_worklist())) {
            _scan_from_ejsobject(p);
        }

        total_objs = num_roots;
    }

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
                    if (HAS_FINALIZER(info->page_bitmap[c])) {
                        SPEW(printf ("finalizing object %p(%s)\n", p, CLASSNAME(p));)
                            OP(p, finalize)((EJSObject*)p);
                    }
                    else {
                        SPEW(printf ("finalizing primitive string %p(%s)\n", p, ((EJSPrimString*)p)->data);)
                            }
                    add_cell_to_freelist (info, c);
                }
            }
        }
    }

    SPEW(printf ("_ejs_gc_collect stats:\n");
         printf ("   num_roots: %d\n", num_roots);
         printf ("   total objects: %d\n", total_objs);
         printf ("   garbage objects: %d\n", white_objs);)

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

        SPEW(printf ("final gc page statistics:\n");
             for (int hp = 0; hp < HEAP_PAGELISTS_COUNT; hp++) {
                 int len = 0;
                 for (PageInfo *info = heap_pages[hp]; info; info = info->next)
                     len++;
                 printf ("  size: %d     pages: %d\n", 1<<(hp + 3), len);
             })
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

void
_ejs_gc_shutdown()
{
    _ejs_gc_collect_inner(EJS_TRUE);
}

size_t alloced_size;

int next_power_of_two(int x, int *bits)
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
    return rv;
}

static GCObjectPtr
alloc_from_los(size_t size, EJSBool has_finalizer)
{
    LargeObjectInfo *rv = (LargeObjectInfo*)calloc(1, sizeof(LargeObjectInfo));

    // we need to bump size up to a mulitple of the OS page size
    rv->los_data = alloc_from_os (size);
    rv->size = size;
    EJS_LIST_ATTACH (rv, los_store);
    return rv->los_data;
}

int num_allocs = 0;

GCObjectPtr
_ejs_gc_alloc(size_t size, EJSBool has_finalizer)
{
    num_allocs ++;
    if (num_allocs == 50) {
        _ejs_gc_collect();
        num_allocs = 0;
    }

    int bucket;
    size = next_power_of_two(size, &bucket);

    bucket -= OBJECT_SIZE_LOW_LIMIT_BITS; // we allocate nothing smaller than this

    if (bucket >= HEAP_PAGELISTS_COUNT)
        return alloc_from_los(size, has_finalizer);

    GCObjectPtr rv = NULL;
    for (PageInfo* info = heap_pages[bucket]; info; info = info->next) {
        rv = alloc_from_freelist(info);
        if (rv)
            return rv;
    }

    // we need a new page
    PageInfo *info = alloc_page(size);
    EJS_LIST_ATTACH(info, heap_pages[bucket]);
    return alloc_from_freelist(info);
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
