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
  EJS_SCAN_TYPE_CLOSUREENV = 1 << 3,
  // a GC leaf: no child ejsvals, no self-interior pointers, no
  // finalizer — every collector dispatch correctly falls through
  EJS_SCAN_TYPE_BIGINT = 1 << 4
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

// bumped at every collection (ejs-gc-minor.c): identity hashes over heap
// pointers (Map/Set indexes) are only valid while this is unchanged
extern uint64_t _ejs_gc_move_epoch;

#define _ejs_gc_new(T) (T *)_ejs_gc_alloc(sizeof(T), EJS_SCAN_TYPE_OBJECT)
#define _ejs_gc_new_obj(T, sz) (T *)_ejs_gc_alloc(sz, EJS_SCAN_TYPE_OBJECT)
#define _ejs_gc_new_primstr(sz)                                                \
  (EJSPrimString *)_ejs_gc_alloc(sz, EJS_SCAN_TYPE_PRIMSTR)
#define _ejs_gc_new_primsym(sz)                                                \
  (EJSPrimSymbol *)_ejs_gc_alloc(sz, EJS_SCAN_TYPE_PRIMSYM)
#define _ejs_gc_new_closureenv(sz)                                             \
  (EJSClosureEnv *)_ejs_gc_alloc(sz, EJS_SCAN_TYPE_CLOSUREENV)

// ---- forwarding plumbing ---------------------------------------
//
// Consumed by the movers (minor evacuation / major compaction);
// EJS_GC_SELFTEST=1 exercises the helpers standalone.
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

// ---- the heap context + generational write barrier -------------
//
// ALL new collector state lives in the heap context (the Concurrency-II
// discipline: an isolate is "one more context", never "another pile of
// file statics").  The leading fields are THE emitted-code seam — the
// emitter reads bump/limit/nursery bounds through this struct's
// exported symbol, so their order and offsets are part of the emitter
// contract: append, never reorder.
//
// The nursery is one dedicated arena, so "is young" is a raw range
// check — cheap enough for the inline write barrier and the emitted
// fast paths.  With the nursery disabled (EJS_GC_NURSERY=off) the
// bounds are NULL and every check below degrades to a no-op / the
// old allocator path.

#define EJS_GC_NUM_SIZE_CLASSES 5 // ffs buckets: 16/32/64/128/256 cells

typedef struct {
    // -- emitted-code seam (offsets fixed; append only) --
    void* bump[EJS_GC_NUM_SIZE_CLASSES];  // current young page cursor, per class
    void* limit[EJS_GC_NUM_SIZE_CLASSES]; // current young page end, per class
    void* nursery_base;                   // [base, end) = the nursery arena
    void* nursery_end;
    // -- the dirty-OBJECT buffer (object-remembering): OLD objects
    //    whose owned storage received a YOUNG reference; deduped by the
    //    DIRTY header bit.  (The future concurrent-marking SATB log rides the same
    //    structure.) --
    void** remset;
    int32_t remset_count;
    int32_t remset_capacity;
    int32_t remset_overflowed; // fall back to a full old-gen scan this minor
    // the top of the CURRENT machine stack (main stack bottom, or the
    // running generator's stack end) — maintained by the generator
    // push/pop hooks so the barrier can reject transient stack slots
    void* current_stack_end;
    // -- runtime-private state (an opaque struct in ejs-gc.c) --
    void* priv;
    // -- head of the CURRENT stack's gc-frame chain (word 17
    //    of the emitted seam).  Emitted prologues link an EJSGCFrame
    //    here, epilogues unlink, catch handlers re-link their own frame
    //    (unwound callees' records die with their stack).  Each machine
    //    stack owns a disjoint chain: the generator push/pop hooks swap
    //    this head alongside current_stack_end, and suspended
    //    generators' chains are walked via their saved heads.  Minor
    //    collections process every chain slot PRECISELY (evacuate +
    //    rewrite) BEFORE the conservative pin pass — a frame-held young
    //    object therefore MOVES every minor, and the conservative
    //    scanner's stale copies of it skip via the forwarding check.
    void* gc_frame_head;
} EJSHeapContext;

// an emitted function's precise-root record, alloca'd in
// its own frame.  `slots` hold BOXED ejsvals only (raw f64/i1 values
// are invisible to GC by construction); the emitter initializes every
// slot to undefined at entry — a stale slot must still parse as a
// valid ejsval, never as stack garbage.
typedef struct _EJSGCFrame {
    struct _EJSGCFrame* prev;
    uintptr_t count;
    ejsval slots[1]; // really `count` of them
} EJSGCFrame;

extern EJSHeapContext _ejs_heap;

static inline EJSBool
_ejs_gc_is_young(void* p)
{
    return (char*)p >= (char*)_ejs_heap.nursery_base
        && (char*)p < (char*)_ejs_heap.nursery_end;
}

// The generational write barrier — OBJECT-REMEMBERING (the second
// design).  The first design recorded raw slot addresses; slots inside
// malloc'd satellites (element buffers, descriptors, map entries) kept
// dangling into freed memory — a structural hazard, not a bug tail.
// This design records the OWNING heap object instead: the minor rescans
// a dirty object through its Scan specop, which walks whatever storage
// the object owns AT SCAN TIME.  No captured interior pointers, no
// lifetime coupling.  Dedup is the DIRTY header bit; the buffer gets
// each old object at most once per cycle.
//
// Contract: after storing a traceable value anywhere in `owner`'s
// transitive OWNED storage (inline slots, element vector, property map,
// descriptors), call _ejs_gc_remember(owner_ptr, value).  Young owners
// and non-young values filter out.
#define EJS_GC_HEADER_DIRTY (1ULL << 60)

extern void _ejs_gc_remember_slow(void* owner);

static inline void
_ejs_gc_remember(void* owner, ejsval newval)
{
    if (!EJSVAL_IS_TRACEABLE_IMPL(newval)) return;
    void* target = (void*)EJSVAL_TO_GCTHING_IMPL(newval);
    if (!_ejs_gc_is_young(target)) return;
    if (_ejs_gc_is_young(owner)) return;
    GCObjectHeaderWord* h = (GCObjectHeaderWord*)owner;
    if (*h & EJS_GC_HEADER_DIRTY) return;
    _ejs_gc_remember_slow(owner);
}

// object-flavored convenience (most call sites hold the ejsval)
#define EJS_GC_REMEMBER(ownerval, v) \
    _ejs_gc_remember((void*)EJSVAL_TO_OBJECT_IMPL(ownerval), (v))

// object-flavored emitted entry (emit.ts passes the owner ejsval)
extern void _ejs_gc_remember_val(ejsval owner, ejsval val);

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
