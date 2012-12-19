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

static int page_size;

// a 4 meg heap
#define INITIAL_HEAP_SIZE 4 * 1024 * 1024

#define OBJ_TO_PAGE(o) ((o) & ~page_size)
#define ALLOC_ALIGN 16


// #if osx
#include <mach/vm_statistics.h>
#define MAP_FD VM_MAKE_TAG (VM_MEMORY_APPLICATION_SPECIFIC_16)
// #else
// #define MAP_FD -1
// #endif

static GCObjectPtr heap;
static GCObjectPtr work_list;

typedef struct _RootSetEntry {
  GCObjectPtr* root;
  char *name;
  struct _RootSetEntry *next;
} RootSetEntry;

RootSetEntry *root_set;

/* a stupid simple gc for ejs */

static void*
alloc_page()
{
  void* rv = mmap(NULL, page_size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, MAP_FD, 0);
  return rv;
}

static void
dealloc_page(void* page_addr)
{
  munmap(page_addr, page_size);
}

void
_ejs_gc_init()
{
  page_size = getpagesize();

#if false
  // allocate our initial heap of 4 megs
  for (int i = 0; i < INITIAL_HEAP_SIZE / page_size; i ++)
    alloc_page();
#endif

  heap = NULL;
  work_list = NULL;
  root_set = NULL;
}

GCObjectPtr
attach(GCObjectPtr list, GCObjectPtr obj)
{
  if (list)
    list->prev_link = obj;

  obj->prev_link = NULL;
  obj->next_link = list;

  return obj;
}

GCObjectPtr
detach(GCObjectPtr list, GCObjectPtr obj)
{
  GCObjectPtr rv = list;
  if (rv == obj)
    rv = obj->next_link;

  if (obj->prev_link)
    obj->prev_link->next_link = obj->next_link;
  if (obj->next_link)
    obj->next_link->prev_link = obj->prev_link;

  obj->prev_link = obj->next_link = NULL;
  return rv;
}

#define COLOR_MASK       0x30000000
#define GRAY_MASK        0x30000000
#define WHITE_MASK_START 0x00000000
#define BLACK_MASK_START 0x20000000

static unsigned int black_mask = BLACK_MASK_START;
static unsigned int white_mask = WHITE_MASK_START;

static EJS_ALWAYS_INLINE void
set_gray (GCObjectPtr obj)
{
  obj->gc_data = (obj->gc_data & ~COLOR_MASK) | GRAY_MASK;
}

static EJS_ALWAYS_INLINE void
set_white (GCObjectPtr obj)
{
  obj->gc_data = (obj->gc_data & ~COLOR_MASK) | white_mask;
}

static EJS_ALWAYS_INLINE void
set_black (GCObjectPtr obj)
{
  obj->gc_data = (obj->gc_data & ~COLOR_MASK) | black_mask;
}

static EJS_ALWAYS_INLINE EJSBool
is_white (GCObjectPtr obj)
{
  return (obj->gc_data & COLOR_MASK) == white_mask;
}

static EJS_ALWAYS_INLINE EJSBool
is_black (GCObjectPtr obj)
{
  return (obj->gc_data & COLOR_MASK) == black_mask;
}

static EJS_ALWAYS_INLINE EJSBool
is_gray (GCObjectPtr obj)
{
  return (obj->gc_data & COLOR_MASK) == GRAY_MASK;
}

void
from_heap_to_worklist(GCObjectPtr obj)
{
  heap = detach(heap, obj);
  work_list = attach(work_list, obj);
  set_gray (obj);
}

void
from_worklist_to_heap(GCObjectPtr obj)
{
  work_list = detach(work_list, obj);
  heap = attach(heap, obj);
  set_black (obj);
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

    from_heap_to_worklist (gcptr);
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

int
llen(GCObjectPtr p)
{
  int l = 0;
  while (p) {
    l ++;
    p = p->next_link;
  }
  return l;
}

int
lindex(GCObjectPtr p, GCObjectPtr list)
{
  if (!p)
    return -1;

  int l = 0;
  while (list) {
    if (p == list)
      return l;
    l ++;
    list = list->next_link;
  }

  return -1;
}

#else

StackEntry *llvm_gc_root_chain;

/// @brief Calls Visitor(root, meta) for each GC root on the stack.
///        root and meta are exactly the values passed to
///        @llvm.gcroot.
///
/// Visitor could be a function to recursively mark live objects.  Or itp
/// might copy them to another heap or generation.
///
/// @param Visitor A function to invoke for every GC root on the stack.
void visitLLVMGCRoots()
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
#if spew
	printf ("Found reference to %p on stack\n", candidate);
#endif
	from_heap_to_worklist (candidate);
      }
    }
  }
}
#endif

static void
_ejs_gc_collect_inner(EJSBool shutting_down)
{
#if CONSERVATIVE_STACKWALK
  GCObjectPtr stack_top;
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
    char* stackp;
    for (stackp = (char*)&stack_top; stackp < ((char*)stack_bottom)-sizeof(ejsval); stackp++) {
      ejsval candidate_val = *((ejsval*)stackp);
      if (EJSVAL_IS_GCTHING_IMPL(candidate_val)) {
	GCObjectPtr gcptr = (GCObjectPtr)EJSVAL_TO_GCTHING_IMPL(candidate_val);
	if (lindex (gcptr, heap) == -1)
	  continue;
	if (is_white(gcptr)) {
	  if (EJSVAL_IS_STRING(candidate_val)) {
#if spew
	    printf ("found ptr to %p(PrimString) on stack\n", EJSVAL_TO_STRING(candidate_val));
#endif
	    set_black(gcptr);
	  }
	  else {
#if spew
	    printf ("found ptr to %p(%s) on stack\n", EJSVAL_TO_OBJECT(candidate_val), CLASSNAME(EJSVAL_TO_OBJECT(candidate_val)));
#endif
	    from_heap_to_worklist(gcptr);
	  }
	}
      }
    }
#else
    visitLLVMGCRoots();
#endif

    GCObjectPtr p;
    while ((p = work_list)) {
      _scan_from_ejsobject(p);
      from_worklist_to_heap(p);
    }

    total_objs = num_roots;
  }

  // sweep the entire heap, freeing white nodes
  GCObjectPtr p = heap;
  while (p) {
    GCObjectPtr next = p->next_link;
    total_objs++;
    if (is_white(p)) {
      white_objs++;
      heap = detach (heap, p);

      if ((p->gc_data & 0x01) != 0) {
#if spew
	printf ("finalizing object %p(%s)\n", p, CLASSNAME(p));
#endif
	OP(p,finalize)((EJSObject*)p);
      }
      else {
#if spew
	printf ("finalizing object without finalize flag\n");
#endif
      }

      free(p);
    }
    p = next;
  }

  unsigned int tmp = black_mask;
  black_mask = white_mask;
  white_mask = tmp;

#if spew
    printf ("_ejs_gc_collect stats:\n");
    printf ("   num_roots: %d\n", num_roots);
    printf ("   total objects: %d\n", total_objs);
    printf ("   garbage objects: %d\n", white_objs);
#endif

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
  }
  else {
    // sanity check, verify the heap is entirely white
    p = heap;
    while (p) {
      if (!is_white(p)) {
	printf ("sanity check failed.  non-white object in heap after polarity reversal.\n");
	printf ("it is %s\n", is_gray(p) ? "gray" : "black");
	abort();
      }
      p = p->next_link;
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

GCObjectPtr
_ejs_gc_alloc(size_t size)
{
  alloced_size += size;
  if (alloced_size > 1024 * 1024) {
    _ejs_gc_collect();
    alloced_size = 0;
  }
  GCObjectPtr ptr = (GCObjectPtr)calloc (1, size);
  ptr->gc_data = white_mask;
  heap = attach (heap, ptr);
  return ptr;
}

void
__ejs_gc_add_named_root(ejsval* root, const char *name)
{
  RootSetEntry* entry = malloc(sizeof(RootSetEntry));
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
