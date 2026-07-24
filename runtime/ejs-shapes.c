/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 *
 * Runtime shape tracking (shapes-plan P4.1).  Pure bookkeeping in this
 * phase: the property map remains the store; ordinary objects carry a
 * shape index maintained by the hooks below, and the census (dumped at
 * exit under EJS_SHAPES_CENSUS) records what real programs do with it.
 */

#include <stdlib.h>
#include <string.h>

#include "ejs.h"
#include "ejs-shapes.h"
#include "ejs-gc.h"
#include "ejs-ops.h"
#include "ejs-string.h"
#include "ejs-log.h"

/* chunked, append-only shape storage: chunk addresses never move, so
   &shape->name can be handed to _ejs_gc_add_root (the EJSShape record and
   the chunk table itself live in ejs-shapes.h for the hot-path inlines) */
#define SHAPE_MAX_SHAPES (1 << 24) /* the header gives us 24 bits */
#define SHAPE_NUM_CHUNKS (SHAPE_MAX_SHAPES >> EJS_SHAPE_CHUNK_SHIFT)

EJSShape *_ejs_shape_chunks[SHAPE_NUM_CHUNKS];
static uint32_t shape_count; /* next unallocated index; starts at 2 (0 =
                                dictionary, 1 = root) */

EJSBool _ejs_shapes_tracking = EJS_FALSE;
static EJSBool census_enabled = EJS_FALSE;
static uint32_t shape_field_cap = 64; /* runtime twin of maam's shapeCap;
                                         EJS_SHAPE_CAP overrides */

/* transition cache: open-addressed (parent, name, repr) -> child.
   child == 0 marks an empty slot (shape 0 is never a transition target) */
typedef struct {
    uint32_t parent;
    uint32_t child;
    uint32_t name_hash; /* rejects probe collisions without touching names */
} TransitionEntry;

static TransitionEntry *transitions;
static uint32_t transition_capacity; /* power of two */
static uint32_t transition_count;

/* census counters (the first three are bumped by the header inlines) */
uint64_t _ejs_shape_stat_objects_born;
uint64_t _ejs_shape_stat_transitions;
uint64_t _ejs_shape_stat_cache_hits;
uint64_t _ejs_shape_stat_fast_hits;
static uint64_t stat_repr_flips;
static uint64_t stat_deaths_shaped;
static uint64_t stat_migrations[EJS_SHAPE_MIGRATE_NUM_REASONS];
static uint32_t stat_max_depth;

#define shape_get _ejs_shape_get

/* returns the new shape's index, or EJS_SHAPE_DICT if the table is full */
static uint32_t
shape_alloc(uint32_t parent, ejsval name, uint8_t repr, uint32_t field_count)
{
    if (shape_count >= SHAPE_MAX_SHAPES)
        return EJS_SHAPE_DICT;

    uint32_t index = shape_count++;
    uint32_t chunk = index >> EJS_SHAPE_CHUNK_SHIFT;
    if (_ejs_shape_chunks[chunk] == NULL)
        _ejs_shape_chunks[chunk] = (EJSShape *)calloc(EJS_SHAPE_CHUNK_SIZE, sizeof(EJSShape));

    EJSShape *shape = shape_get(index);
    shape->parent = parent;
    shape->field_count = field_count;
    shape->name = name;
    shape->repr = repr;

    /* keep the field name alive: shapes are process-global and never freed */
    if (EJSVAL_IS_STRING(name))
        _ejs_gc_add_root(&shape->name);

    return index;
}

static uint32_t
transition_hash(uint32_t parent, uint32_t name_hash, uint8_t repr)
{
    uint32_t h = parent * 0x9e3779b9u;
    h ^= name_hash + 0x9e3779b9u + (h << 6) + (h >> 2);
    h ^= (uint32_t)repr + 0x9e3779b9u + (h << 6) + (h >> 2);
    return h;
}

static uint32_t
shape_name_hash(ejsval name)
{
    return _ejs_string_hash(name);
}

/* property keys at a given site are almost always the same interned atom,
   so raw ejsval equality catches nearly every cache hit; fall back to
   content comparison for equal strings from different allocations */
static EJSBool
shape_name_eq(ejsval a, ejsval b)
{
    if (EJSVAL_EQ(a, b))
        return EJS_TRUE;
    return EJSVAL_TO_BOOLEAN(_ejs_op_strict_eq(a, b));
}

static void transition_insert(uint32_t parent, uint32_t name_hash, uint32_t child);

static void
transition_grow(void)
{
    TransitionEntry *old = transitions;
    uint32_t old_capacity = transition_capacity;

    transition_capacity = old_capacity ? old_capacity * 2 : 256;
    transitions = (TransitionEntry *)calloc(transition_capacity, sizeof(TransitionEntry));
    transition_count = 0;

    for (uint32_t i = 0; i < old_capacity; i++) {
        if (old[i].child == 0)
            continue;
        EJSShape *child_shape = shape_get(old[i].child);
        transition_insert(old[i].parent, shape_name_hash(child_shape->name),
                          old[i].child);
    }
    free(old);
}

static void
transition_insert(uint32_t parent, uint32_t name_hash, uint32_t child)
{
    if (transition_count + 1 > transition_capacity - (transition_capacity >> 2))
        transition_grow();

    uint32_t mask = transition_capacity - 1;
    uint32_t slot = transition_hash(parent, name_hash, shape_get(child)->repr) & mask;
    while (transitions[slot].child != 0)
        slot = (slot + 1) & mask;
    transitions[slot].parent = parent;
    transitions[slot].child = child;
    transitions[slot].name_hash = name_hash;
    transition_count++;
}

/* the one hash hit per property add: find the (parent, name, repr) edge,
   interning a new shape on first use.  returns EJS_SHAPE_DICT only when
   the global table is full. */
static uint32_t
transition_find_or_add(uint32_t parent, ejsval name, uint8_t repr)
{
    EJSShape *parent_shape = shape_get(parent);

    /* the memo hit: same construction sequence as last time */
    uint32_t memo = parent_shape->last_child;
    if (memo != EJS_SHAPE_DICT) {
        EJSShape *m = shape_get(memo);
        if (m->repr == repr && EJSVAL_EQ(m->name, name)) {
            _ejs_shape_stat_cache_hits++;
            return memo;
        }
    }

    if (transition_capacity == 0)
        transition_grow();

    uint32_t name_hash = shape_name_hash(name);
    uint32_t mask = transition_capacity - 1;
    uint32_t slot = transition_hash(parent, name_hash, repr) & mask;

    while (transitions[slot].child != 0) {
        if (transitions[slot].parent == parent &&
            transitions[slot].name_hash == name_hash) {
            EJSShape *cand = shape_get(transitions[slot].child);
            if (cand->repr == repr && shape_name_eq(cand->name, name)) {
                _ejs_shape_stat_cache_hits++;
                parent_shape->last_child = transitions[slot].child;
                return transitions[slot].child;
            }
        }
        slot = (slot + 1) & mask;
    }

    uint32_t child = shape_alloc(parent, name, repr,
                                 parent_shape->field_count + 1);
    if (child == EJS_SHAPE_DICT)
        return EJS_SHAPE_DICT;

    transition_insert(parent, name_hash, child);
    shape_get(parent)->last_child = child; /* shape_alloc may have grown chunks */
    return child;
}

static uint8_t
classify_repr(ejsval value)
{
    return EJSVAL_IS_NUMBER(value) ? EJS_SHAPE_REPR_F64 : EJS_SHAPE_REPR_BOXED;
}

void
_ejs_shape_object_migrate(EJSObject *obj, EJSShapeMigrateReason reason)
{
    if (EJS_OBJECT_SHAPE(obj) == EJS_SHAPE_DICT)
        return;
    EJS_OBJECT_SET_SHAPE(obj, EJS_SHAPE_DICT);
    stat_migrations[reason]++;
}

void
_ejs_shape_object_add(EJSObject *obj, ejsval name, ejsval value)
{
    uint32_t shape = EJS_OBJECT_SHAPE(obj);
    if (shape == EJS_SHAPE_DICT)
        return;

    if (!EJSVAL_IS_STRING(name)) {
        _ejs_shape_object_migrate(obj, EJS_SHAPE_MIGRATE_SYMBOL_KEY);
        return;
    }

    /* numeric/index-looking keys stay in the map (arrays own indexed
       storage; indexed access on plain objects is rare enough to eat it) */
    EJSPrimString *namestr = EJSVAL_TO_STRING(name);
    if (namestr->length > 0) {
        jschar c0 = EJS_PRIMSTR_GET_TYPE(namestr) == EJS_STRING_FLAT
                        ? namestr->data.flat[0]
                        : _ejs_string_ucs2_at(namestr, 0);
        if (c0 >= '0' && c0 <= '9') {
            _ejs_shape_object_migrate(obj, EJS_SHAPE_MIGRATE_INDEX_KEY);
            return;
        }
    }

    EJSShape *cur = shape_get(shape);
    if (cur->field_count >= shape_field_cap) {
        _ejs_shape_object_migrate(obj, EJS_SHAPE_MIGRATE_CAP);
        return;
    }

    uint32_t child = transition_find_or_add(shape, name, classify_repr(value));
    if (child == EJS_SHAPE_DICT) {
        _ejs_shape_object_migrate(obj, EJS_SHAPE_MIGRATE_TABLE_FULL);
        return;
    }

    EJS_OBJECT_SET_SHAPE(obj, child);
    _ejs_shape_stat_transitions++;
    uint32_t depth = shape_get(child)->field_count;
    if (depth > stat_max_depth)
        stat_max_depth = depth;
}

/* rebuild the chain with `field_index`'s repr changed: the sibling shape a
   type-flipping store transitions to.  returns EJS_SHAPE_DICT on table
   overflow. */
static uint32_t
shape_flip_repr(uint32_t shape, uint32_t field_index, uint8_t new_repr)
{
    /* collect edges leaf->root; depth is capped by shape_field_cap */
    ejsval names[256];
    uint8_t reprs[256];
    uint32_t depth = shape_get(shape)->field_count;
    EJS_ASSERT(depth <= 256);

    uint32_t s = shape;
    for (uint32_t i = depth; i > 0; i--) {
        EJSShape *cur = shape_get(s);
        names[i - 1] = cur->name;
        reprs[i - 1] = cur->repr;
        s = cur->parent;
    }
    reprs[field_index] = new_repr;

    uint32_t rebuilt = EJS_SHAPE_ROOT;
    for (uint32_t i = 0; i < depth; i++) {
        rebuilt = transition_find_or_add(rebuilt, names[i], reprs[i]);
        if (rebuilt == EJS_SHAPE_DICT)
            return EJS_SHAPE_DICT;
    }
    return rebuilt;
}

void
_ejs_shape_object_set(EJSObject *obj, ejsval name, ejsval value)
{
    uint32_t shape = EJS_OBJECT_SHAPE(obj);
    if (shape == EJS_SHAPE_DICT)
        return;

    if (!EJSVAL_IS_STRING(name))
        return; /* symbol props never enter a shape; nothing to flip */

    /* find the field on the chain (leaf->root; index counts from the root) */
    uint32_t s = shape;
    int32_t field_index = -1;
    uint8_t old_repr = EJS_SHAPE_REPR_BOXED;
    while (s != EJS_SHAPE_DICT) {
        EJSShape *cur = shape_get(s);
        if (cur->field_count == 0)
            break;
        if (shape_name_eq(cur->name, name)) {
            field_index = (int32_t)cur->field_count - 1;
            old_repr = cur->repr;
            break;
        }
        s = cur->parent;
    }
    if (field_index < 0)
        return; /* not a tracked field (e.g. index-looking key kept in the map) */

    uint8_t new_repr = classify_repr(value);
    if (new_repr == old_repr)
        return;

    uint32_t flipped = shape_flip_repr(shape, (uint32_t)field_index, new_repr);
    if (flipped == EJS_SHAPE_DICT) {
        _ejs_shape_object_migrate(obj, EJS_SHAPE_MIGRATE_TABLE_FULL);
        return;
    }
    EJS_OBJECT_SET_SHAPE(obj, flipped);
    stat_repr_flips++;
}

void
_ejs_shape_object_died(EJSObject *obj)
{
    if (!census_enabled)
        return;
    uint32_t shape = EJS_OBJECT_SHAPE(obj);
    if (shape == EJS_SHAPE_DICT)
        return;
    shape_get(shape)->deaths++;
    stat_deaths_shaped++;
}

/* ------------------------------------------------------------------ */
/* census                                                             */

static void
census_print_shape_fields(uint32_t shape)
{
    ejsval names[256];
    uint8_t reprs[256];
    uint32_t depth = shape_get(shape)->field_count;

    uint32_t s = shape;
    for (uint32_t i = depth; i > 0; i--) {
        EJSShape *cur = shape_get(s);
        names[i - 1] = cur->name;
        reprs[i - 1] = cur->repr;
        s = cur->parent;
    }

    _ejs_logstr("{");
    for (uint32_t i = 0; i < depth; i++) {
        if (i > 0)
            _ejs_logstr(", ");
        char *utf8 = ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(names[i]));
        _ejs_logstr(utf8);
        free(utf8);
        if (reprs[i] == EJS_SHAPE_REPR_F64)
            _ejs_logstr(":f64");
    }
    _ejs_logstr("}");
}

static void
census_dump(void)
{
    static const char *reason_names[EJS_SHAPE_MIGRATE_NUM_REASONS] = {
        "delete", "attrs", "accessor", "symbol-key",
        "index-key", "cap", "table-full",
    };

    uint64_t migrations_total = 0;
    for (int i = 0; i < EJS_SHAPE_MIGRATE_NUM_REASONS; i++)
        migrations_total += stat_migrations[i];

    _ejs_log("=== ejs shape census ===\n");
    _ejs_log("objects born tracked:  %llu\n",
             (unsigned long long)_ejs_shape_stat_objects_born);
    _ejs_log("shapes interned:       %u (max depth %u)\n",
             shape_count > 2 ? shape_count - 2 : 0, stat_max_depth);
    _ejs_log("transitions:           %llu (fast hits %llu, cache hits %llu)\n",
             (unsigned long long)_ejs_shape_stat_transitions,
             (unsigned long long)_ejs_shape_stat_fast_hits,
             (unsigned long long)_ejs_shape_stat_cache_hits);
    _ejs_log("repr flips:            %llu\n",
             (unsigned long long)stat_repr_flips);
    _ejs_log("dictionary migrations: %llu\n",
             (unsigned long long)migrations_total);
    for (int i = 0; i < EJS_SHAPE_MIGRATE_NUM_REASONS; i++)
        if (stat_migrations[i])
            _ejs_log("    %-11s %llu\n", reason_names[i],
                     (unsigned long long)stat_migrations[i]);
    _ejs_log("deaths while shaped:   %llu\n",
             (unsigned long long)stat_deaths_shaped);

    /* top shapes by death count */
    uint32_t top[16];
    uint32_t ntop = 0;
    for (uint32_t i = 2; i < shape_count; i++) {
        if (shape_get(i)->deaths == 0)
            continue;
        uint32_t pos = ntop < 16 ? ntop : 15;
        if (ntop == 16 && shape_get(i)->deaths <= shape_get(top[15])->deaths)
            continue;
        while (pos > 0 && shape_get(top[pos - 1])->deaths < shape_get(i)->deaths) {
            top[pos] = top[pos - 1];
            pos--;
        }
        top[pos] = i;
        if (ntop < 16)
            ntop++;
    }
    if (ntop > 0) {
        _ejs_log("top shapes at death:\n");
        for (uint32_t i = 0; i < ntop; i++) {
            _ejs_log("    %8u  ", shape_get(top[i])->deaths);
            census_print_shape_fields(top[i]);
            _ejs_logstr("\n");
        }
    }
}

void
_ejs_shapes_init(void)
{
    const char *shapes_env = getenv("EJS_SHAPES");
    _ejs_shapes_tracking =
        !(shapes_env && (!strcmp(shapes_env, "off") || !strcmp(shapes_env, "0")));
    if (!_ejs_shapes_tracking)
        return;

    const char *cap_env = getenv("EJS_SHAPE_CAP");
    if (cap_env) {
        int cap = atoi(cap_env);
        if (cap > 0 && cap <= 256)
            shape_field_cap = (uint32_t)cap;
    }

    /* index 0 is dictionary mode; index 1 is the empty root shape */
    shape_count = 1;
    shape_alloc(EJS_SHAPE_DICT, _ejs_undefined, EJS_SHAPE_REPR_BOXED, 0);

    if (getenv("EJS_SHAPES_CENSUS")) {
        census_enabled = EJS_TRUE;
        atexit(census_dump);
    }
}
