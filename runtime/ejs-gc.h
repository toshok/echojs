/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#ifndef _ejs_gc_h_
#define _ejs_gc_h_

#include "ejs.h"

EJS_BEGIN_DECLS

#define CONSERVATIVE_STACKWALK 1

typedef enum {
  EJS_SCAN_TYPE_PRIMSTR = 1 << 0,
  EJS_SCAN_TYPE_PRIMSYM = 1 << 1,
  EJS_SCAN_TYPE_OBJECT = 1 << 2,
  EJS_SCAN_TYPE_CLOSUREENV = 1 << 3
} EJSScanType;

#define EJS_GC_INTERNAL_FLAGS_MASK 0x0000ffff
#define EJS_GC_USER_FLAGS_SHIFT 24
#define EJS_GC_USER_FLAGS_MASK 0xffff0000

typedef void *GCObjectPtr;

extern void _ejs_GC_init(ejsval global);
extern void _ejs_gc_init();
extern void _ejs_gc_allocate_oom_exceptions();
extern void _ejs_gc_shutdown();
extern void _ejs_gc_collect(const char *reason);

extern GCObjectPtr _ejs_gc_alloc(size_t size, EJSScanType scan_type);

#define _ejs_gc_new(T) (T *)_ejs_gc_alloc(sizeof(T), EJS_SCAN_TYPE_OBJECT)
#define _ejs_gc_new_obj(T, sz) (T *)_ejs_gc_alloc(sz, EJS_SCAN_TYPE_OBJECT)
#define _ejs_gc_new_primstr(sz)                                                \
  (EJSPrimString *)_ejs_gc_alloc(sz, EJS_SCAN_TYPE_PRIMSTR)
#define _ejs_gc_new_primsym(sz)                                                \
  (EJSPrimSymbol *)_ejs_gc_alloc(sz, EJS_SCAN_TYPE_PRIMSYM)
#define _ejs_gc_new_closureenv(sz)                                             \
  (EJSClosureEnv *)_ejs_gc_alloc(sz, EJS_SCAN_TYPE_CLOSUREENV)

// ---- gc-plan P1: forwarding plumbing ---------------------------------------
//
// Inert until the mover (gc-P2 evacuation / gc-P4 compaction) consumes it;
// landed now so the header bit inventory is complete and the helpers are
// exercised (EJS_GC_SELFTEST=1) with the old collector still active.
//
// Forwarding uses the classic first-word overwrite: once an object has been
// evacuated its old header is dead (the copy carries the real one), so the
// old slot's header word becomes the forwarding record — the target address
// in the low bits (heap addresses live below 2^47 by the NaN-boxing rule)
// plus a discriminator bit chosen ABOVE the address range from the header's
// gc-reserved bits (57-63; see ejs-types.h).  A live header can never be
// mistaken for a forwarding record (bit 59 is written by nothing else), and
// a forwarding record can never be mistaken for a live header of any scan
// type worth trusting — readers must check _ejs_gc_is_forwarded first, as
// the evacuation loop will.
#define EJS_GC_HEADER_FORWARDED (1ULL << 59)
#define EJS_GC_FORWARD_ADDR_MASK ((1ULL << 47) - 1)

typedef uint64_t GCObjectHeaderWord; // matches GCObjectHeader (ejs-types.h)

static inline EJSBool
_ejs_gc_is_forwarded(GCObjectPtr p)
{
    return (*(GCObjectHeaderWord*)p & EJS_GC_HEADER_FORWARDED) != 0;
}

static inline GCObjectPtr
_ejs_gc_forwarding_addr(GCObjectPtr p)
{
    return (GCObjectPtr)(uintptr_t)(*(GCObjectHeaderWord*)p & EJS_GC_FORWARD_ADDR_MASK);
}

// overwrite `from`'s header with a forwarding record pointing at `to`.
// `to` must be 8-aligned and below 2^47 (both invariants of the allocator).
static inline void
_ejs_gc_forward(GCObjectPtr from, GCObjectPtr to)
{
    *(GCObjectHeaderWord*)from =
        ((GCObjectHeaderWord)(uintptr_t)to & EJS_GC_FORWARD_ADDR_MASK)
        | EJS_GC_HEADER_FORWARDED;
}

extern void _ejs_gc_add_root(ejsval *val);
extern void _ejs_gc_remove_root(ejsval *root);

extern void _ejs_gc_mark_conservative_range(void *low, void *high);

#define EJS_GC_MARK_THREAD_STACK_BOTTOM                                        \
  do {                                                                         \
    GCObjectPtr btm;                                                           \
    _ejs_gc_mark_thread_stack_bottom(&btm);                                    \
  } while (0)

extern void _ejs_gc_mark_thread_stack_bottom(GCObjectPtr *btm);

EJS_END_DECLS

#endif /* _ejs_gc_h */
