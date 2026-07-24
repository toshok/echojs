/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 *
 * Runtime shape tracking (shapes-plan P4.1).
 *
 * A shape is a transition edge (parent, name, repr) appended to a parent
 * shape; the global table is interned and append-only, mirroring maam's
 * type-aware hidden classes one-for-one (repr is part of shape identity).
 * In P4.1 shapes are pure bookkeeping: the property map remains the
 * store, ordinary objects just carry a shape index in the widened
 * GCObjectHeader, maintained on insert/delete/type-flip.  Anything the
 * shaped world can't express (deletes, non-default attributes, accessors,
 * symbol/index keys, cap overflow) drops the object to dictionary mode
 * (shape index 0) one-way, with the reason counted for the census.
 *
 * EJS_SHAPES=off disables tracking entirely; EJS_SHAPES_CENSUS=1 dumps
 * the shape census at exit.
 */

#ifndef _ejs_shapes_h_
#define _ejs_shapes_h_

#include "ejs.h"
#include "ejs-object.h"

EJS_BEGIN_DECLS

/* field representation, part of shape identity (mirrors maam's TypeSig
   abstraction; finer tags later if the census says they pay) */
typedef enum {
    EJS_SHAPE_REPR_BOXED = 0,
    EJS_SHAPE_REPR_F64 = 1,
} EJSShapeRepr;

/* shape index 0 = dictionary mode (or a class that never tracks);
   shape index 1 = the empty ordinary-object shape */
#define EJS_SHAPE_DICT 0
#define EJS_SHAPE_ROOT 1

/* the shape index lives in bits 32-55 of the 64-bit GCObjectHeader (bit
   56 is the P4.2 storage-mode bit; 57-63 belong to the GC) — see the
   layout comment in ejs-types.h */
#define EJS_GC_HEADER_SHAPE_SHIFT 32
#define EJS_GC_HEADER_SHAPE_MASK 0xFFFFFFULL

#define EJS_OBJECT_SHAPE(o)                                                    \
  ((uint32_t)((((EJSObject *)(o))->gc_header >> EJS_GC_HEADER_SHAPE_SHIFT) &   \
              EJS_GC_HEADER_SHAPE_MASK))
#define EJS_OBJECT_SET_SHAPE(o, s)                                             \
  (((EJSObject *)(o))->gc_header =                                             \
       (((EJSObject *)(o))->gc_header &                                        \
        ~(EJS_GC_HEADER_SHAPE_MASK << EJS_GC_HEADER_SHAPE_SHIFT)) |            \
       (((uint64_t)(s) & EJS_GC_HEADER_SHAPE_MASK)                             \
        << EJS_GC_HEADER_SHAPE_SHIFT))

/* dictionary-migration reasons, census-counted */
typedef enum {
    EJS_SHAPE_MIGRATE_DELETE = 0,   /* delete of a tracked field */
    EJS_SHAPE_MIGRATE_ATTRS,        /* non-default w/e/c attribute (incl. freeze/seal) */
    EJS_SHAPE_MIGRATE_ACCESSOR,     /* getter/setter definition or conversion */
    EJS_SHAPE_MIGRATE_SYMBOL_KEY,   /* symbol-keyed property */
    EJS_SHAPE_MIGRATE_INDEX_KEY,    /* numeric/index-looking key */
    EJS_SHAPE_MIGRATE_CAP,          /* per-object field-count cap overflow */
    EJS_SHAPE_MIGRATE_TABLE_FULL,   /* global shape-table pathology */
    EJS_SHAPE_MIGRATE_NUM_REASONS
} EJSShapeMigrateReason;

void _ejs_shapes_init(void);

/* the shape record and chunk table are exposed only so the hot-path
   inlines below can avoid a cross-TU call per property insert; everything
   else treats them as private to ejs-shapes.c */
typedef struct {
    uint32_t parent;      /* parent shape index (EJS_SHAPE_DICT for the root) */
    uint32_t field_count; /* own fields including this edge (root = 0) */
    ejsval name;          /* this edge's field name; gc-rooted (chunks are
                             address-stable) */
    uint8_t repr;         /* EJSShapeRepr, part of shape identity */
    uint32_t last_child;  /* memo of the most recent transition taken from
                             this shape; monomorphic construction sites hit
                             it every time and skip the hash entirely */
    uint32_t deaths;      /* census: objects finalized bearing this shape */
} EJSShape;

#define EJS_SHAPE_CHUNK_SHIFT 12
#define EJS_SHAPE_CHUNK_SIZE (1 << EJS_SHAPE_CHUNK_SHIFT)

extern EJSShape *_ejs_shape_chunks[];
extern EJSBool _ejs_shapes_tracking;
extern uint64_t _ejs_shape_stat_objects_born;
extern uint64_t _ejs_shape_stat_transitions;
extern uint64_t _ejs_shape_stat_cache_hits;
extern uint64_t _ejs_shape_stat_fast_hits;

static inline EJSShape *
_ejs_shape_get(uint32_t index)
{
    return &_ejs_shape_chunks[index >> EJS_SHAPE_CHUNK_SHIFT]
                             [index & (EJS_SHAPE_CHUNK_SIZE - 1)];
}

/* object-side hooks; every one is a cheap no-op when tracking is off or
   the object is untracked (shape index 0) */

/* an ordinary object was just initialized: give it the root shape */
static inline void
_ejs_shape_object_born(EJSObject *obj)
{
    if (!_ejs_shapes_tracking)
        return;
    EJS_OBJECT_SET_SHAPE(obj, EJS_SHAPE_ROOT);
    _ejs_shape_stat_objects_born++;
}

/* a new own data property with default attributes was inserted (the
   out-of-line path: key vetting, cap check, transition-cache lookup) */
void _ejs_shape_object_add(EJSObject *obj, ejsval name, ejsval value);

/* inline fast path for property adds: when the parent shape's transition
   memo matches (same name ejsval, same repr — the monomorphic
   construction sequence), the name was already vetted as a shapeable key
   when the memo's shape was interned and its field count already passed
   the cap, so every check collapses into one compare */
static inline void
_ejs_shape_object_add_fast(EJSObject *obj, ejsval name, ejsval value)
{
    uint32_t shape = EJS_OBJECT_SHAPE(obj);
    if (shape == EJS_SHAPE_DICT)
        return;
    uint32_t memo = _ejs_shape_get(shape)->last_child;
    if (memo != EJS_SHAPE_DICT) {
        EJSShape *m = _ejs_shape_get(memo);
        uint8_t repr = EJSVAL_IS_NUMBER(value) ? EJS_SHAPE_REPR_F64
                                               : EJS_SHAPE_REPR_BOXED;
        if (EJSVAL_EQ(m->name, name) && m->repr == repr) {
            EJS_OBJECT_SET_SHAPE(obj, memo);
            _ejs_shape_stat_transitions++;
            _ejs_shape_stat_fast_hits++;
            return;
        }
    }
    _ejs_shape_object_add(obj, name, value);
}

/* the value of an existing own data property was updated (repr-flip check) */
void _ejs_shape_object_set(EJSObject *obj, ejsval name, ejsval value);

/* something un-shapeable happened: one-way drop to dictionary mode */
void _ejs_shape_object_migrate(EJSObject *obj, EJSShapeMigrateReason reason);

/* finalizer hook, census only */
void _ejs_shape_object_died(EJSObject *obj);

EJS_END_DECLS

#endif /* _ejs_shapes_h_ */
