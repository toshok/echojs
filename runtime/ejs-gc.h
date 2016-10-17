/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#ifndef _ejs_gc_h_
#define _ejs_gc_h_

#include "ejs.h"

EJS_BEGIN_DECLS

#define CONSERVATIVE_STACKWALK 1

typedef enum {
    EJS_SCAN_TYPE_PRIMSTR    = 1 << 0,
    EJS_SCAN_TYPE_PRIMSYM    = 1 << 1,
    EJS_SCAN_TYPE_OBJECT     = 1 << 2,
    EJS_SCAN_TYPE_CLOSUREENV = 1 << 3
} EJSScanType;

#define EJS_GC_INTERNAL_FLAGS_MASK 0x0000ffff
#define EJS_GC_USER_FLAGS_SHIFT 24
#define EJS_GC_USER_FLAGS_MASK 0xffff0000

typedef void* GCObjectPtr;

extern void _ejs_GC_init(ejsval global);
extern void _ejs_gc_init();
extern void _ejs_gc_allocate_oom_exceptions();
extern void _ejs_gc_shutdown();
extern void _ejs_gc_collect();

extern GCObjectPtr _ejs_gc_alloc(size_t size, EJSScanType scan_type);

#define _ejs_gc_new(T) (T*)_ejs_gc_alloc(sizeof(T), EJS_SCAN_TYPE_OBJECT)
#define _ejs_gc_new_obj(T,sz) (T*)_ejs_gc_alloc(sz, EJS_SCAN_TYPE_OBJECT)
#define _ejs_gc_new_primstr(sz) (EJSPrimString*)_ejs_gc_alloc(sz, EJS_SCAN_TYPE_PRIMSTR)
#define _ejs_gc_new_primsym(sz) (EJSPrimSymbol*)_ejs_gc_alloc(sz, EJS_SCAN_TYPE_PRIMSYM)
#define _ejs_gc_new_closureenv(sz) (EJSClosureEnv*)_ejs_gc_alloc(sz, EJS_SCAN_TYPE_CLOSUREENV)

extern void _ejs_gc_add_root(ejsval* val);
extern void _ejs_gc_remove_root(ejsval* root);

extern void _ejs_gc_mark_conservative_range(void* low, void* high);

#define EJS_GC_MARK_THREAD_STACK_BOTTOM do {        \
        GCObjectPtr btm;                            \
        _ejs_gc_mark_thread_stack_bottom (&btm);    \
    } while(0)

extern void _ejs_gc_mark_thread_stack_bottom(GCObjectPtr* btm);

EJS_END_DECLS

#endif /* _ejs_gc_h */
