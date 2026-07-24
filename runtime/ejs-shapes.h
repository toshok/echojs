/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 *
 * Runtime shape tracking (shapes-plan P4.1/P4.2).
 *
 * A shape is a transition edge (parent, name, repr) appended to a parent
 * shape; the global table is interned and append-only, mirroring maam's
 * type-aware hidden classes one-for-one (repr is part of shape identity).
 * Since P4.2 the shape IS the property structure for shaped-mode ordinary
 * objects: their values live in a slot array at shape-determined indices
 * (the storage engine is in ejs-object.c; this module owns the shape
 * table and answers name->slot / transition queries).  Anything the
 * shaped world can't express (deletes, non-default attributes, accessors,
 * symbol/index keys, cap overflow) drops the object to dictionary mode
 * (shape index 0) one-way — the map path — with the reason counted for
 * the census.
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

/* hard ceiling on shaped field count (and EJS_SHAPE_CAP): a full slot
   array must fit the page allocator's largest cell — 128 bytes: despite
   the "= 256" comment on OBJECT_SIZE_HIGH_LIMIT_BITS, `ffs(256) = 9 > 8`
   sends 256-byte allocations to the LOS, whose linear per-reference
   lookup makes marking quadratic on big heaps (the stage2 self-compile
   went from minutes to hours before this cap).  16-byte EJSClosureEnv
   header + 14 * 8-byte slots = 128.  Objects with more fields drop to
   dictionary mode — the pre-P4.2 map world.  Revisit when the gc plan
   gives the LOS an O(log n) lookup or a 256-byte size class. */
#define EJS_SHAPE_FIELD_CAP_MAX 14

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

/* object-side hooks */

/* an ordinary object was just initialized: give it the root shape */
static inline void
_ejs_shape_object_born(EJSObject *obj)
{
    if (!_ejs_shapes_tracking)
        return;
    EJS_OBJECT_SET_SHAPE(obj, EJS_SHAPE_ROOT);
    _ejs_shape_stat_objects_born++;
}

/* something un-shapeable happened: one-way drop to dictionary mode.
   Only flips the header index and counts the reason — materializing the
   map from slot storage is the object layer's job
   (_ejs_object_to_dictionary in ejs-object.c) */
void _ejs_shape_object_migrate(EJSObject *obj, EJSShapeMigrateReason reason);

/* finalizer hook, census only */
void _ejs_shape_object_died(EJSObject *obj);

/* shape-table queries for the object layer's storage engine (P4.2).
   None of these touch any object. */

/* number of own fields of `shape` */
static inline uint32_t
_ejs_shape_field_count(uint32_t shape)
{
    return _ejs_shape_get(shape)->field_count;
}

/* find string key `name` among shape's fields; on hit returns EJS_TRUE
   with *slot = the field's insertion-ordered index */
EJSBool _ejs_shape_lookup(uint32_t shape, ejsval name, uint32_t *slot);

/* fill names[0 .. field_count) with the field names in insertion
   (root->leaf) order; names must have room for field_count entries */
void _ejs_shape_fields(uint32_t shape, ejsval *names);

/* transition for inserting a new own data property `name` (a string,
   caller-checked) with default attributes and initial value `value`.
   Returns the child shape index, or EJS_SHAPE_DICT with *reason set when
   the add can't stay shaped (index-looking key, field cap, table full). */
uint32_t _ejs_shape_transition_add(uint32_t shape, ejsval name, ejsval value,
                                   EJSShapeMigrateReason *reason);

/* inline fast path for property adds: when the parent shape's transition
   memo matches (same name ejsval, same repr — the monomorphic
   construction sequence), the name was already vetted as a shapeable key
   when the memo's shape was interned and its field count already passed
   the cap, so every check collapses into one compare */
static inline uint32_t
_ejs_shape_transition_add_fast(uint32_t shape, ejsval name, ejsval value,
                               EJSShapeMigrateReason *reason)
{
    uint32_t memo = _ejs_shape_get(shape)->last_child;
    if (memo != EJS_SHAPE_DICT) {
        EJSShape *m = _ejs_shape_get(memo);
        uint8_t repr = EJSVAL_IS_NUMBER(value) ? EJS_SHAPE_REPR_F64
                                               : EJS_SHAPE_REPR_BOXED;
        if (EJSVAL_EQ(m->name, name) && m->repr == repr) {
            _ejs_shape_stat_transitions++;
            _ejs_shape_stat_fast_hits++;
            return memo;
        }
    }
    return _ejs_shape_transition_add(shape, name, value, reason);
}

/* transition for storing `value` into the existing field at
   `slot_index`: returns `shape` when the repr is unchanged, the
   repr-flipped sibling shape otherwise, or EJS_SHAPE_DICT on shape-table
   overflow */
uint32_t _ejs_shape_transition_set(uint32_t shape, uint32_t slot_index,
                                   ejsval value);

EJS_END_DECLS

#endif /* _ejs_shapes_h_ */
