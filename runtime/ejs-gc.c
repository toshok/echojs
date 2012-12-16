#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <setjmp.h>

#include "ejs-gc.h"
#include "ejs-value.h"
#include "ejs-string.h"

static int page_size;

// a 4 meg heap
#define INITIAL_HEAP_SIZE 4 * 1024 * 1024

#define OBJ_TO_PAGE(o) ((o) & ~page_size)

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

static GCObjectPtr *stack_bottom;

void
_ejs_gc_set_stack_bottom (GCObjectPtr* bottom)
{
  stack_bottom = bottom;
}

int
llen(GCObjectPtr l)
{
  if (l == NULL)
    return 0;
  return 1 + llen(l->next_link);
}

int
lindex(GCObjectPtr l, GCObjectPtr el)
{
  if (el == NULL)
    return -1;

  int n = 0;
  do {
    if (l == el)
      return n;
    l = l->next_link;
    n++;
  } while (l);

  return -1;
}

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

static void
set_gray (GCObjectPtr obj)
{
  obj->tag = (obj->tag & ~COLOR_MASK) | GRAY_MASK;
}

static void
set_white (GCObjectPtr obj)
{
  obj->tag = (obj->tag & ~COLOR_MASK) | white_mask;
}

static void
set_black (GCObjectPtr obj)
{
  obj->tag = (obj->tag & ~COLOR_MASK) | black_mask;
}

static EJSBool
is_white (GCObjectPtr obj)
{
  return (obj->tag & COLOR_MASK) == white_mask;
}

static EJSBool
is_black (GCObjectPtr obj)
{
  return (obj->tag & COLOR_MASK) == black_mask;
}

static EJSBool
is_gray (GCObjectPtr obj)
{
  return (obj->tag & COLOR_MASK) == GRAY_MASK;
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
_scan_ejsvalue (EJSValue *obj)
{
  if (obj == NULL)
    return;

  GCObjectPtr gcptr = (GCObjectPtr)obj;

  if (EJSVAL_IS_OBJECT(obj)) {
    // grey objects are in the mark stack, black objects we know are not garbage
    if (!is_white(gcptr))
      return;

    from_heap_to_worklist (gcptr);
  }
  else {
    set_black (gcptr);
  }
}

static void
_scan_from_ejsobject(GCObjectPtr obj)
{
  switch (EJSVAL_TAG(obj)) {
  case EJSValueTagObject: {
    EJSObject *ejsobj = (EJSObject*)obj;
    _ejs_propertymap_foreach_value (ejsobj->map, _scan_ejsvalue);
    _scan_ejsvalue (ejsobj->proto);

    if (!strcmp (CLASSNAME(ejsobj), "Array")) {
      EJSArray* ejsa = (EJSArray*)ejsobj;
      _ejs_array_foreach_element (ejsa, _scan_ejsvalue);
    }
    else if (!strcmp (CLASSNAME(ejsobj), "String")) {
      EJSString* ejss = (EJSString*)ejsobj;
      _scan_ejsvalue (ejss->primStr);
    }
    else if (!strcmp (CLASSNAME(ejsobj), "Function")) {
      EJSFunction *ejsf = (EJSFunction*)ejsobj;
      _scan_ejsvalue (ejsf->name);
      _scan_ejsvalue (ejsf->env);
      if (ejsf->bound_this)
	_scan_ejsvalue (ejsf->_this);
    }
    break;
  }
  }
}

static EJSBool
points_to_heap_object(GCObjectPtr ptr)
{
  if (ptr == NULL)
    return FALSE;

  if ((unsigned int)ptr & 0x7)
    return FALSE;

  return lindex (heap, ptr) >= 0;
}

/// @brief The map for a single function's stack frame.  One of these is
///        compiled as constant data into the executable for each function.
///
/// Storage of metadata values is elided if the %metadata parameter to
/// @llvm.gcroot is null.
struct FrameMap {
  int32_t NumRoots;    //< Number of roots in stack frame.
  int32_t NumMeta;     //< Number of metadata entries.  May be < NumRoots.
  const void *Meta[0]; //< Metadata for each root.
};

/// @brief A link in the dynamic shadow stack.  One of these is embedded in
///        the stack frame of each function on the call stack.
struct StackEntry {
  struct StackEntry *Next;    //< Link to next stack entry (the caller's).
  const struct FrameMap *Map; //< Pointer to constant FrameMap.
  void *Roots[0];      //< Stack roots (in-place array).
};

/// @brief The head of the singly-linked list of StackEntries.  Functions push
///        and pop onto this in their prologue and epilogue.
///
/// Since there is only a global list, this technique is not threadsafe.
struct StackEntry *llvm_gc_root_chain;

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
  for (struct StackEntry *R = llvm_gc_root_chain; R; R = R->Next) {
    unsigned i = 0;

    // For roots [NumMeta, NumRoots), the metadata pointer is null.
    for (unsigned e = R->Map->NumRoots; i != e; ++i) {
      GCObjectPtr candidate = (GCObjectPtr)R->Roots[i];
      if (candidate == NULL) continue;
      if (is_white (candidate)) {
	printf ("Found reference to %p on stack\n", candidate);
	from_heap_to_worklist (candidate);
      }
    }
  }
}

void
_ejs_gc_collect()
{
  jmp_buf jmpbuf;
  setjmp (jmpbuf);

  GCObjectPtr stack_top = NULL;

  // very simple stop the world collector

  int num_roots = 0;
  int white_objs = 0;
  int total_objs = 0;

  // mark from our roots
  for (RootSetEntry *entry = root_set; entry; entry = entry->next) {
    num_roots++;
    if (*entry->root) {
      set_black (*entry->root);
      if (EJSVAL_IS_OBJECT(*entry->root)) {
	_scan_from_ejsobject(*entry->root);
      }
    }
  }

  //printf ("stack: top %p, bottom %p\n", &stack_top, stack_bottom);

  visitLLVMGCRoots();

  GCObjectPtr p;
  while ((p = work_list)) {
    _scan_from_ejsobject(p);
    from_worklist_to_heap(p);
  }

  total_objs = num_roots;

  // sweep the entire heap, freeing white nodes
  p = heap;
  while (p) {
    GCObjectPtr next = p->next_link;
    total_objs++;
    if (is_white(p)) {
      white_objs++;
      heap = detach (heap, p);

      EJSObject *ejsobj = (EJSObject*)p;

      switch (EJSVAL_TAG(p)) {
      case EJSValueTagObject: {
	if (!strcmp (CLASSNAME(ejsobj), "Array")) {
	  _ejs_array_finalize ((EJSArray*)ejsobj);
	}
	else if (!strcmp (CLASSNAME(ejsobj), "String")) {
	  _ejs_string_finalize ((EJSString*)ejsobj);
	}
	else if (!strcmp (CLASSNAME(ejsobj), "Function")) {
	  _ejs_function_finalize ((EJSFunction*)ejsobj);
	}
	break;
      }
      default:
	 // XXX this should use _ejs_value_finalize()
	break;
      }
      free(p);
    }
    p = next;
  }

  unsigned int tmp = black_mask;
  black_mask = white_mask;
  white_mask = tmp;
  printf ("_ejs_gc_collect stats:\n");
  printf ("   num_roots: %d\n", num_roots);
  printf ("   total objects: %d\n", total_objs);
  printf ("   garbage objects: %d\n", white_objs);

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

void
_ejs_gc_shutdown()
{
  // walk the entire heap freeing everything, and null out our root set
  GCObjectPtr p = heap;
  while (p) {
    GCObjectPtr next = p->next_link;
    free(p);
    p = next;
  }

  for (RootSetEntry *entry = root_set; entry; entry = entry->next) {
    *entry->root = NULL;
    if (entry->name) free (entry->name);
    free (entry);
  }
}

int age = 0;
EJSBool _ejs_gc_started;

GCObjectPtr
_ejs_gc_alloc(size_t size)
{
  //if (_ejs_gc_started && age++ == 20) {
    _ejs_gc_collect();
    age = 0;
  //}
  GCObjectPtr ptr = (GCObjectPtr)calloc (1, size);
  heap = attach (heap, ptr);
  return ptr;
}

void
__ejs_gc_add_named_root(EJSValue** root, const char *name)
{
  RootSetEntry* entry = malloc(sizeof(RootSetEntry));
  entry->root = (GCObjectPtr*)root;
  entry->name = name ? strdup(name) : NULL;
  entry->next = root_set;
  root_set = entry;
}

void
_ejs_gc_remove_root(EJSValue** root)
{
  abort();
}
