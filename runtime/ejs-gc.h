
#ifndef _ejs_gc_h_
#define _ejs_gc_h_

#include "ejs.h"

typedef struct GCObjectHeader* GCObjectPtr;

typedef struct GCObjectHeader {
  EJSValueTag tag;

  GCObjectPtr prev_link;
  GCObjectPtr next_link;
} GCObjectHeader;

extern void _ejs_gc_init();
extern void _ejs_gc_shutdown();
extern void _ejs_gc_collect();
extern GCObjectPtr _ejs_gc_alloc(size_t size);

#define _ejs_gc_new(T) (T*)_ejs_gc_alloc(sizeof(T))

#define _ejs_gc_add_named_root(v) __ejs_gc_add_named_root(&v, #v)

extern void __ejs_gc_add_named_root(EJSValue** val, const char *name);

extern void _ejs_gc_remove_root(EJSValue** root);

extern void _ejs_gc_set_stack_bottom (GCObjectPtr* bottom);
#endif /* _ejs_gc_h */
