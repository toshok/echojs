/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include <math.h>

#include "ejs-set.h"
#include "ejs-array.h"
#include "ejs-gc.h"
#include "ejs-generator.h"
#include "ejs-error.h"
#include "ejs-function.h"
#include "ejs-proxy.h"
#include "ejs-ops.h"
#include "ejs-symbol.h"

ejsval
_ejs_set_new ()
{
    EJSSet *set = _ejs_gc_new (EJSSet);
    _ejs_init_object ((EJSObject*)set, _ejs_Set_prototype, &_ejs_Set_specops);

    return OBJECT_TO_EJSVAL(set);
}

// ---- svz-hash index over the insertion list -------------------------
// mirrors ejs-map.c: the [[SetData]] List survives for iterators; the
// index makes add/has/delete O(1) instead of a SameValueZero walk.
// Object/symbol values hash via the header identity-hash bits
// (ejs-gc.h), stable across collections — rebuilds are growth-only.

struct _EJSSetIndexSlot {
    EJSSetValueEntry* entry; // NULL = empty, SET_INDEX_TOMB = deleted
};
#define SET_INDEX_TOMB ((EJSSetValueEntry*)1)

static void
set_index_insert_raw (struct _EJSSetIndexSlot* slots, uint32_t capacity, EJSSetValueEntry* e)
{
    uint32_t mask = capacity - 1;
    uint32_t i = _ejs_svz_hash(e->value) & mask;
    while (slots[i].entry && slots[i].entry != SET_INDEX_TOMB)
        i = (i + 1) & mask;
    slots[i].entry = e;
}

static void
set_index_rebuild (EJSSet* set)
{
    uint32_t live = 0;
    for (EJSSetValueEntry* e = set->head_insert; e; e = e->next_insert)
        if (!EJSVAL_IS_NO_ITER_VALUE_MAGIC(e->value))
            live++;

    uint32_t capacity = 16;
    while (capacity < live * 2)
        capacity <<= 1;

    free (set->index);
    set->index = (struct _EJSSetIndexSlot*)calloc (capacity, sizeof(struct _EJSSetIndexSlot));
    set->index_capacity = capacity;
    set->index_used = live;

    for (EJSSetValueEntry* e = set->head_insert; e; e = e->next_insert)
        if (!EJSVAL_IS_NO_ITER_VALUE_MAGIC(e->value))
            set_index_insert_raw (set->index, set->index_capacity, e);
}

static void
set_index_ensure (EJSSet* set)
{
    if (!set->index
        || set->index_used + 1 > set->index_capacity - (set->index_capacity >> 2))
        set_index_rebuild (set);
}

static struct _EJSSetIndexSlot*
set_index_find_slot (EJSSet* set, ejsval value)
{
    set_index_ensure (set);
    uint32_t mask = set->index_capacity - 1;
    uint32_t i = _ejs_svz_hash(value) & mask;
    while (set->index[i].entry) {
        EJSSetValueEntry* e = set->index[i].entry;
        if (e != SET_INDEX_TOMB
            && !EJSVAL_IS_NO_ITER_VALUE_MAGIC(e->value)
            && SameValueZero (e->value, value))
            return &set->index[i];
        i = (i + 1) & mask;
    }
    return NULL;
}

static void
set_index_add (EJSSet* set, EJSSetValueEntry* e)
{
    set_index_ensure (set);
    set_index_insert_raw (set->index, set->index_capacity, e);
    set->index_used++;
}

// ES6: 23.1.3.1
// Map.prototype.clear ()
static EJS_NATIVE_FUNC(_ejs_Set_prototype_clear) {
    // 1. Let S be this value. 
    ejsval S = *_this;

    // 2. If Type(S) is not Object, then throw a TypeError exception. 
    if (!EJSVAL_IS_OBJECT(S))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Set.prototype.clear called with non-object this.");

    // 3. If S does not have a [[SetData]] internal slot throw a TypeError exception. 
    if (!EJSVAL_IS_SET(S))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Set.prototype.clear called with non-Set this.");

    // 4. If S’s [[SetData]] internal slot is undefined, then throw a TypeError exception. 

    // 5. Let entries be the List that is the value of S’s [[SetData]] internal slot. 
    EJSSetValueEntry* entries = EJSVAL_TO_SET(S)->head_insert;
    // 6. Repeat for each e that is an element of entries,
    for (EJSSetValueEntry* e = entries; e; e = e->next_insert) {
        //    a. Replace the element of entries whose value is e with an element whose value is empty.
        e->value = MAGIC_TO_EJSVAL_IMPL(EJS_NO_ITER_VALUE);
    }
    free (EJSVAL_TO_SET(S)->index);
    EJSVAL_TO_SET(S)->index = NULL;
    // 7. Return undefined.
    return _ejs_undefined;
}

ejsval
_ejs_set_delete(ejsval S, ejsval value)
{
    // our caller should have already validated and thrown appropriate TypeErrors
    EJS_ASSERT(EJSVAL_IS_SET(S));

    // index probe replaces the spec's [[SetData]] walk (the List
    // survives for suspended iterators; the entry empties in place)
    struct _EJSSetIndexSlot* slot = set_index_find_slot (EJSVAL_TO_SET(S), value);
    if (slot) {
        slot->entry->value = MAGIC_TO_EJSVAL_IMPL(EJS_NO_ITER_VALUE);
        slot->entry = SET_INDEX_TOMB;
        return _ejs_true;
    }
    // 7. Return false.
    return _ejs_false;
}

// 23.2.3.4 Set.prototype.delete ( value ) 
static EJS_NATIVE_FUNC(_ejs_Set_prototype_delete) {
    // NOTE The value empty is used as a specification device to
    // indicate that an entry has been deleted. Actual implementations
    // may take other actions such as physically removing the entry
    // from internal data structures.

    ejsval value = _ejs_undefined;
    if (argc > 0) value = args[0];

    // 1. Let S be the this value. 
    ejsval S = *_this;

    // 2. If Type(S) is not Object, then throw a TypeError exception. 
    if (!EJSVAL_IS_OBJECT(S))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Set.prototype.delete called with non-object this.");

    // 3. If S does not have a [[SetData]] internal slot throw a TypeError exception. 
    if (!EJSVAL_IS_SET(S))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Set.prototype.delete called with non-Set this.");

    // 4. If S’s [[SetData]] internal slot is undefined, then throw a TypeError exception. 

    return _ejs_set_delete(S, value);
}

// ES6: 23.2.3.5
// Set.prototype.entries ( )
static EJS_NATIVE_FUNC(_ejs_Set_prototype_entries) {
    // 1. Let S be the this value.
    ejsval S = *_this;

    // 2. Return the result of calling the CreateSetIterator abstract operation with arguments S and "key+value".
    return _ejs_set_iterator_new (S, EJS_SET_ITER_KIND_KEYVALUE);
}


// ES6: 23.2.3.6 Set.prototype.forEach ( callbackfn , thisArg = undefined )
static EJS_NATIVE_FUNC(_ejs_Set_prototype_forEach) {
    ejsval callbackfn = _ejs_undefined;
    ejsval thisArg = _ejs_undefined;

    if (argc > 0) callbackfn = args[0];
    if (argc > 1) thisArg = args[1];

    // 1. Let S be the this value. 
    ejsval S = *_this;

    // 2. If Type(S) is not Object, then throw a TypeError exception. 
    if (!EJSVAL_IS_OBJECT(S))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Set.prototype.forEach called with non-object this.");

    // 3. If S does not have a [[SetData]] internal slot throw a TypeError exception. 
    if (!EJSVAL_IS_SET(S))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Set.prototype.forEach called with non-Set this.");

    // 4. If S’s [[SetData]] internal slot is undefined, then throw a TypeError exception. 

    // 5. If IsCallable(callbackfn) is false, throw a TypeError exception. 
    if (!IsCallable(callbackfn))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Set.prototype.forEach callbackfn isn't a function.");

    // 6. If thisArg was supplied, let T be thisArg; else let T be undefined. 
    ejsval T = thisArg;

    EJSSet* set = EJSVAL_TO_SET(S);

    // 7. Let entries be the List that is the value of S’s [[SetData]] internal slot. 
    EJSSetValueEntry* entries = set->head_insert;

    // 8. Repeat for each e that is an element of entries, in original insertion order 
    for (EJSSetValueEntry *e = entries; e; e = e->next_insert) {
        //    a. If e is not empty, then 
        if (EJSVAL_IS_NO_ITER_VALUE_MAGIC(e->value))
            continue;

        //       i. Let funcResult be the result of calling the [[Call]] internal method of callbackfn with T as thisArgument and a List containing e, e, and S as argumentsList. 
        //       ii. ReturnIfAbrupt(funcResult). 
        ejsval callback_args[3];
        callback_args[0] = e->value;
        callback_args[1] = e->value;
        callback_args[2] = S;
        _ejs_invoke_closure (callbackfn, &T, 3, callback_args, _ejs_undefined);

    }

    // 9. Return undefined. 
    return _ejs_undefined;
}

ejsval
_ejs_set_has(ejsval S, ejsval value)
{
    // our caller should have already validated and thrown appropriate TypeErrors
    EJS_ASSERT(EJSVAL_IS_SET(S));


    EJSSet* _set = EJSVAL_TO_SET(S);

    // index probe replaces the spec's [[SetData]] walk
    return set_index_find_slot (_set, value) ? _ejs_true : _ejs_false;
}

// ES6: 23.2.3.7
// Set.prototype.has ( value )
static EJS_NATIVE_FUNC(_ejs_Set_prototype_has) {
    ejsval value = _ejs_undefined;
    if (argc > 0) value = args[0];

    // 1. Let S be the this value. 
    ejsval S = *_this;

    // 2. If Type(S) is not Object, then throw a TypeError exception. 
    if (!EJSVAL_IS_OBJECT(S))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Set.prototype.has called with non-object this.");

    // 3. If S does not have a [[SetData]] internal slot throw a TypeError exception. 
    if (!EJSVAL_IS_SET(S))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Set.prototype.has called with non-Set this.");

    // 4. If S’s [[SetData]] internal slot is undefined, then throw a TypeError exception. 

    return _ejs_set_has(S, value);
}

ejsval
_ejs_set_add(ejsval S, ejsval value)
{
    // our caller should have already validated and thrown appropriate TypeErrors
    EJS_ASSERT(EJSVAL_IS_SET(S));

    // 4. If S’s [[SetData]] internal slot is undefined, then throw a TypeError exception. 
    EJSSet* _set = EJSVAL_TO_SET(S);

    // 6. index probe replaces the spec's [[SetData]] walk
    if (set_index_find_slot (_set, value))
        return S;

    // 7. If value is −0, then let value be +0.
    if (EJSVAL_IS_NUMBER(value) && EJSDOUBLE_IS_NEGZERO(EJSVAL_TO_NUMBER(value)))
        value = NUMBER_TO_EJSVAL(0);
    // 8. Append value as the last element of entries.
    EJSSetValueEntry* e = calloc (1, sizeof (EJSSetValueEntry));
    e->value = value;
    _ejs_gc_remember(_set, e->value);

    if (!_set->head_insert)
        _set->head_insert = e;

    if (_set->tail_insert) {
        _set->tail_insert->next_insert = e;
        _set->tail_insert = e;
    }
    else {
        _set->tail_insert = e;
    }

    set_index_add (_set, e);

    // 9. Return S.
    return S;
}

// ES6: 23.2.3.1 Set.prototype.add ( value )
static EJS_NATIVE_FUNC(_ejs_Set_prototype_add) {
    ejsval value = _ejs_undefined;
    if (argc > 0) value = args[0];

    // 1. Let S be the this value. 
    ejsval S = *_this;

    // 2. If Type(S) is not Object, then throw a TypeError exception. 
    if (!EJSVAL_IS_OBJECT(S)) {
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Set.prototype.add called with non-object this.");
    }

    // 3. If S does not have a [[SetData]] internal slot throw a TypeError exception. 
    if (!EJSVAL_IS_SET(S))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Set.prototype.set called with non-Set this.");

    return _ejs_set_add(S, value);
}

// ES6: 23.2.3.9
// get Set.prototype.size
static EJS_NATIVE_FUNC(_ejs_Set_prototype_get_size) {
    // 1. Let S be the this value.
    ejsval S = *_this;

    // 2. If Type(S) is not Object, then throw a TypeError exception.
    if (!EJSVAL_IS_OBJECT(S)) {
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Set size getter called with non-object this.");
    }

    // 3. If S does not have a [[SetData]] internal slot throw a TypeError exception.
    if (!EJSVAL_IS_SET(S)) {
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Set size getter called with non-Set this.");
    }

    EJSSet* _set = EJSVAL_TO_SET(S);

    // 4. If S’s [[SetData]] internal slot is undefined, then throw a TypeError exception.

    // 5. Let entries be the List that is the value of S’s [[SetData]] internal slot.
    EJSSetValueEntry* entries = _set->head_insert;

    // 6. Let count be 0.
    uint32_t count = 0;
    // 7. For each e that is an element of entries
    for (EJSSetValueEntry* e = entries; e; e = e->next_insert) {
        //   a. If e is not empty then
        if (!EJSVAL_IS_NO_ITER_VALUE_MAGIC(e->value))
            //      i. Set count to count+1.
            count ++;
    }
    // 8. Return count.
    return NUMBER_TO_EJSVAL(count);
}

// ES6: 23.2.3.10
// Set.prototype.values ( )
static EJS_NATIVE_FUNC(_ejs_Set_prototype_values) {
    // 1. Let S be the this value. 
    ejsval S = *_this;

    // 2. Return the result of calling the CreateSetIterator abstract operation with argument S and "value".
    return _ejs_set_iterator_new (S, EJS_SET_ITER_KIND_VALUE);
}

// the [[Set]]/[[Size]]/[[Has]]/[[Keys]] fields of a Set Record
// (ES2025 24.2.1.1).  size is a double so +Infinity survives the
// size comparisons.
typedef struct {
    ejsval set;
    double size;
    ejsval has;
    ejsval keys;
} EJSSetRecord;

// counts the non-empty entries of S's [[SetData]]
static double
set_data_size (ejsval S)
{
    double count = 0;
    for (EJSSetValueEntry* e = EJSVAL_TO_SET(S)->head_insert; e; e = e->next_insert) {
        if (!EJSVAL_IS_NO_ITER_VALUE_MAGIC(e->value))
            count++;
    }
    return count;
}

// ES2025 24.2.1.2 GetSetRecord ( obj )
static EJSSetRecord
GetSetRecord (ejsval obj)
{
    // 1. If obj is not an Object, throw a TypeError exception.
    if (!EJSVAL_IS_OBJECT(obj))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "argument must be an object");

    // 2. Let rawSize be ? Get(obj, "size").
    ejsval rawSize = Get (obj, _ejs_atom_size);

    // 3. Let numSize be ? ToNumber(rawSize).
    double numSize = ToDouble (ToNumber (rawSize));

    // 4. If numSize is NaN, throw a TypeError exception.
    if (isnan(numSize))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "size is NaN");

    // 5. Let intSize be ! ToIntegerOrInfinity(numSize).
    double intSize = trunc(numSize);

    // 6. If intSize < 0, throw a RangeError exception.
    if (intSize < 0)
        _ejs_throw_nativeerror_utf8 (EJS_RANGE_ERROR, "size is negative");

    // 7. Let has be ? Get(obj, "has").
    ejsval has = Get (obj, _ejs_atom_has);

    // 8. If IsCallable(has) is false, throw a TypeError exception.
    if (!IsCallable(has))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "has is not callable");

    // 9. Let keys be ? Get(obj, "keys").
    ejsval keys = Get (obj, _ejs_atom_keys);

    // 10. If IsCallable(keys) is false, throw a TypeError exception.
    if (!IsCallable(keys))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "keys is not callable");

    // 11. Return a new Set Record.
    EJSSetRecord rec;
    rec.set = obj;
    rec.size = intSize;
    rec.has = has;
    rec.keys = keys;
    return rec;
}

// ES2025 24.2.1.6 GetKeysIterator ( setRec ), inlined GetIteratorDirect:
// the iterator's next method is looked up per step by IteratorStep
static ejsval
GetKeysIterator (EJSSetRecord* setRec)
{
    // 1. Let keysIter be ? Call(setRec.[[Keys]], setRec.[[Set]]).
    ejsval thisArg = setRec->set;
    ejsval keysIter = _ejs_invoke_closure (setRec->keys, &thisArg, 0, NULL, _ejs_undefined);

    // 2. If keysIter is not an Object, throw a TypeError exception.
    if (!EJSVAL_IS_OBJECT(keysIter))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "keys() did not return an object");

    return keysIter;
}

// calls setRec.[[Has]] on setRec.[[Set]] with value, returning
// ToBoolean of the result
static EJSBool
set_record_has (EJSSetRecord* setRec, ejsval value)
{
    ejsval thisArg = setRec->set;
    ejsval rv = _ejs_invoke_closure (setRec->has, &thisArg, 1, &value, _ejs_undefined);
    return EJSVAL_TO_BOOLEAN(ToBoolean(rv));
}

// a new Set whose [[SetData]] is a copy of S's
static ejsval
set_copy (ejsval S)
{
    ejsval rv = _ejs_set_new();
    for (EJSSetValueEntry* e = EJSVAL_TO_SET(S)->head_insert; e; e = e->next_insert) {
        if (!EJSVAL_IS_NO_ITER_VALUE_MAGIC(e->value))
            _ejs_set_add (rv, e->value);
    }
    return rv;
}

static void
validate_set_this (ejsval S, const char* method)
{
    if (!EJSVAL_IS_OBJECT(S))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, method);
    if (!EJSVAL_IS_SET(S))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, method);
}

// ES2025 24.2.4.16 Set.prototype.union ( other )
static EJS_NATIVE_FUNC(_ejs_Set_prototype_union) {
    ejsval other = _ejs_undefined;
    if (argc > 0) other = args[0];

    // 1-2. Let O be the this value; perform RequireInternalSlot(O, [[SetData]]).
    ejsval S = *_this;
    validate_set_this (S, "Set.prototype.union called with non-Set this");

    // 3. Let otherRec be ? GetSetRecord(other).
    EJSSetRecord otherRec = GetSetRecord (other);

    // 4. Let keysIter be ? GetKeysIterator(otherRec).
    ejsval keysIter = GetKeysIterator (&otherRec);

    // 5. Let resultSetData be a copy of O.[[SetData]].
    ejsval result = set_copy (S);

    // 6. Repeat over the keys iterator,
    for (;;) {
        ejsval next = IteratorStep (keysIter);
        if (!EJSVAL_TO_BOOLEAN(next))
            break;
        ejsval nextValue = IteratorValue (next);
        // b.ii-iv. canonicalize -0 and append if not already present
        // (_ejs_set_add does both)
        _ejs_set_add (result, nextValue);
    }

    // 7-9. Return a new Set whose [[SetData]] is resultSetData.
    return result;
}

// ES2025 24.2.4.9 Set.prototype.intersection ( other )
static EJS_NATIVE_FUNC(_ejs_Set_prototype_intersection) {
    ejsval other = _ejs_undefined;
    if (argc > 0) other = args[0];

    ejsval S = *_this;
    validate_set_this (S, "Set.prototype.intersection called with non-Set this");

    // 3. Let otherRec be ? GetSetRecord(other).
    EJSSetRecord otherRec = GetSetRecord (other);

    // 4. Let resultSetData be a new empty List.
    ejsval result = _ejs_set_new();

    // 5. If SetDataSize(O.[[SetData]]) <= otherRec.[[Size]], then
    if (set_data_size(S) <= otherRec.size) {
        // a-b. for each element e of O.[[SetData]]: entries appended
        // by the has calls are visited too, matching the spec's
        // index-based walk of a growing list
        for (EJSSetValueEntry* e = EJSVAL_TO_SET(S)->head_insert; e; e = e->next_insert) {
            ejsval v = e->value;
            if (EJSVAL_IS_NO_ITER_VALUE_MAGIC(v))
                continue;
            // i. Let inOther be ToBoolean(? Call(otherRec.[[Has]], otherRec.[[Set]], « e »)).
            if (set_record_has (&otherRec, v))
                // ii. append e to resultSetData if not already present
                _ejs_set_add (result, v);
        }
    }
    // 6. Else,
    else {
        // a. Let keysIter be ? GetKeysIterator(otherRec).
        ejsval keysIter = GetKeysIterator (&otherRec);
        // b. Repeat over the keys iterator,
        for (;;) {
            ejsval next = IteratorStep (keysIter);
            if (!EJSVAL_TO_BOOLEAN(next))
                break;
            ejsval nextValue = IteratorValue (next);
            // iv. If SetDataHas(O.[[SetData]], nextValue), append to
            // resultSetData if not already present (SameValueZero
            // lookups make the -0 canonicalization observationally
            // moot; _ejs_set_add canonicalizes on append)
            if (EJSVAL_TO_BOOLEAN(_ejs_set_has (S, nextValue)))
                _ejs_set_add (result, nextValue);
        }
    }

    // 7-9. Return a new Set whose [[SetData]] is resultSetData.
    return result;
}

// ES2025 24.2.4.5 Set.prototype.difference ( other )
static EJS_NATIVE_FUNC(_ejs_Set_prototype_difference) {
    ejsval other = _ejs_undefined;
    if (argc > 0) other = args[0];

    ejsval S = *_this;
    validate_set_this (S, "Set.prototype.difference called with non-Set this");

    // 3. Let otherRec be ? GetSetRecord(other).
    EJSSetRecord otherRec = GetSetRecord (other);

    // 4. Let resultSetData be a copy of O.[[SetData]].
    ejsval result = set_copy (S);

    // 5. If SetDataSize(O.[[SetData]]) <= otherRec.[[Size]], then
    if (set_data_size(S) <= otherRec.size) {
        // a. For each element e of resultSetData: the copy is not
        // observable by the has calls, so removal is a simple
        // empty-out of the entry
        for (EJSSetValueEntry* e = EJSVAL_TO_SET(result)->head_insert; e; e = e->next_insert) {
            ejsval v = e->value;
            if (EJSVAL_IS_NO_ITER_VALUE_MAGIC(v))
                continue;
            if (set_record_has (&otherRec, v))
                e->value = MAGIC_TO_EJSVAL_IMPL(EJS_NO_ITER_VALUE);
        }
    }
    // 6. Else,
    else {
        // a. Let keysIter be ? GetKeysIterator(otherRec).
        ejsval keysIter = GetKeysIterator (&otherRec);
        // b. Repeat over the keys iterator, removing each yielded
        // value from resultSetData (SameValueZero handles -0)
        for (;;) {
            ejsval next = IteratorStep (keysIter);
            if (!EJSVAL_TO_BOOLEAN(next))
                break;
            ejsval nextValue = IteratorValue (next);
            _ejs_set_delete (result, nextValue);
        }
    }

    // 7-9. Return a new Set whose [[SetData]] is resultSetData.
    return result;
}

// ES2025 24.2.4.15 Set.prototype.symmetricDifference ( other )
static EJS_NATIVE_FUNC(_ejs_Set_prototype_symmetricDifference) {
    ejsval other = _ejs_undefined;
    if (argc > 0) other = args[0];

    ejsval S = *_this;
    validate_set_this (S, "Set.prototype.symmetricDifference called with non-Set this");

    // 3. Let otherRec be ? GetSetRecord(other).
    EJSSetRecord otherRec = GetSetRecord (other);

    // 4. Let keysIter be ? GetKeysIterator(otherRec).
    ejsval keysIter = GetKeysIterator (&otherRec);

    // 5. Let resultSetData be a copy of O.[[SetData]].
    ejsval result = set_copy (S);

    // 6. Repeat over the keys iterator,
    for (;;) {
        ejsval next = IteratorStep (keysIter);
        if (!EJSVAL_TO_BOOLEAN(next))
            break;
        ejsval nextValue = IteratorValue (next);
        // c. Let inThis be SetDataHas(O.[[SetData]], nextValue):
        // checked against the live O, not the copy, so values the
        // iterator added to O are seen
        if (EJSVAL_TO_BOOLEAN(_ejs_set_has (S, nextValue)))
            // d. remove nextValue from resultSetData if present
            _ejs_set_delete (result, nextValue);
        else
            // e. append nextValue to resultSetData if not present
            _ejs_set_add (result, nextValue);
    }

    // 7-9. Return a new Set whose [[SetData]] is resultSetData.
    return result;
}

// ES2025 24.2.4.10 Set.prototype.isSubsetOf ( other )
static EJS_NATIVE_FUNC(_ejs_Set_prototype_isSubsetOf) {
    ejsval other = _ejs_undefined;
    if (argc > 0) other = args[0];

    ejsval S = *_this;
    validate_set_this (S, "Set.prototype.isSubsetOf called with non-Set this");

    // 3. Let otherRec be ? GetSetRecord(other).
    EJSSetRecord otherRec = GetSetRecord (other);

    // 4. If SetDataSize(O.[[SetData]]) > otherRec.[[Size]], return false.
    if (set_data_size(S) > otherRec.size)
        return _ejs_false;

    // 5. For each element e of O.[[SetData]],
    for (EJSSetValueEntry* e = EJSVAL_TO_SET(S)->head_insert; e; e = e->next_insert) {
        ejsval v = e->value;
        if (EJSVAL_IS_NO_ITER_VALUE_MAGIC(v))
            continue;
        // b. If inOther is false, return false.
        if (!set_record_has (&otherRec, v))
            return _ejs_false;
    }
    // 6. Return true.
    return _ejs_true;
}

// ES2025 24.2.4.11 Set.prototype.isSupersetOf ( other )
static EJS_NATIVE_FUNC(_ejs_Set_prototype_isSupersetOf) {
    ejsval other = _ejs_undefined;
    if (argc > 0) other = args[0];

    ejsval S = *_this;
    validate_set_this (S, "Set.prototype.isSupersetOf called with non-Set this");

    // 3. Let otherRec be ? GetSetRecord(other).
    EJSSetRecord otherRec = GetSetRecord (other);

    // 4. If SetDataSize(O.[[SetData]]) < otherRec.[[Size]], return false.
    if (set_data_size(S) < otherRec.size)
        return _ejs_false;

    // 5. Let keysIter be ? GetKeysIterator(otherRec).
    ejsval keysIter = GetKeysIterator (&otherRec);

    // 6. Repeat over the keys iterator,
    for (;;) {
        ejsval next = IteratorStep (keysIter);
        if (!EJSVAL_TO_BOOLEAN(next))
            break;
        ejsval nextValue = IteratorValue (next);
        // c. If SetDataHas(O.[[SetData]], nextValue) is false,
        if (!EJSVAL_TO_BOOLEAN(_ejs_set_has (S, nextValue))) {
            // i. Perform ? IteratorClose(keysIter, NormalCompletion(unused)).
            IteratorClose (keysIter, _ejs_false, EJS_FALSE);
            // ii. Return false.
            return _ejs_false;
        }
    }
    // 7. Return true.
    return _ejs_true;
}

// ES2025 24.2.4.12 Set.prototype.isDisjointFrom ( other )
static EJS_NATIVE_FUNC(_ejs_Set_prototype_isDisjointFrom) {
    ejsval other = _ejs_undefined;
    if (argc > 0) other = args[0];

    ejsval S = *_this;
    validate_set_this (S, "Set.prototype.isDisjointFrom called with non-Set this");

    // 3. Let otherRec be ? GetSetRecord(other).
    EJSSetRecord otherRec = GetSetRecord (other);

    // 4. If SetDataSize(O.[[SetData]]) <= otherRec.[[Size]], then
    if (set_data_size(S) <= otherRec.size) {
        // a. For each element e of O.[[SetData]],
        for (EJSSetValueEntry* e = EJSVAL_TO_SET(S)->head_insert; e; e = e->next_insert) {
            ejsval v = e->value;
            if (EJSVAL_IS_NO_ITER_VALUE_MAGIC(v))
                continue;
            // ii. If inOther is true, return false.
            if (set_record_has (&otherRec, v))
                return _ejs_false;
        }
    }
    // 5. Else,
    else {
        // a. Let keysIter be ? GetKeysIterator(otherRec).
        ejsval keysIter = GetKeysIterator (&otherRec);
        // b. Repeat over the keys iterator,
        for (;;) {
            ejsval next = IteratorStep (keysIter);
            if (!EJSVAL_TO_BOOLEAN(next))
                break;
            ejsval nextValue = IteratorValue (next);
            // c. If SetDataHas(O.[[SetData]], nextValue) is true,
            if (EJSVAL_TO_BOOLEAN(_ejs_set_has (S, nextValue))) {
                // i-ii. close the iterator and return false
                IteratorClose (keysIter, _ejs_false, EJS_FALSE);
                return _ejs_false;
            }
        }
    }
    // 6. Return true.
    return _ejs_true;
}

// ES2015, June 2015
// 23.2.1.1 Set ( [ iterable ] )
static EJS_NATIVE_FUNC(_ejs_Set_impl) {
    ejsval iterable = _ejs_undefined;
    if (argc > 0)
        iterable = args[0];

    // 1. If NewTarget is undefined, throw a TypeError exception.
    if (EJSVAL_IS_UNDEFINED(newTarget))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Set constructor must be called with new");

    // 2. Let set be OrdinaryCreateFromConstructor(NewTarget, "%SetPrototype%", «[[SetData]]» ).
    // 3. ReturnIfAbrupt(set).
    // 4. Set set’s [[SetData]] internal slot to a new empty List.
    ejsval set = OrdinaryCreateFromConstructor(newTarget, _ejs_Set_prototype, &_ejs_Set_specops);
    *_this = set;

    // 5. If iterable is not present, let iterable be undefined.
    // 6. If iterable is either undefined or null, let iter be undefined.
    ejsval iter;
    ejsval adder;
    if (EJSVAL_IS_NULL_OR_UNDEFINED(iterable)) {
        iter = _ejs_undefined;
    }
    // 7. Else,
    else {
        // a. Let adder be Get(set, "add").
        // b. ReturnIfAbrupt(adder).
        adder = Get (set, _ejs_atom_add);

        // c. If IsCallable(adder) is false, throw a TypeError exception.
        if (!IsCallable(adder))
            _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "WeakSet.prototype.add is not a function");

        // d. Let iter be GetIterator(iterable).
        // e. ReturnIfAbrupt(iter).
        iter = GetIterator(iterable, _ejs_undefined);
    }

    // 8. If iter is undefined, return set.
    if (EJSVAL_IS_UNDEFINED(iter))
        return set;

    // 9. Repeat
    for (;;) {
        // a. Let next be IteratorStep(iter).
        // b. ReturnIfAbrupt(next).
        ejsval next = IteratorStep (iter);

        // c. If next is false, return set.
        if (!EJSVAL_TO_BOOLEAN(next))
            return set;

        // d. Let nextValue be IteratorValue(next).
        // e. ReturnIfAbrupt(nextValue).
        ejsval nextValue = IteratorValue (next);

        // f. Let status be Call(adder, set, «nextValue.[[value]]»).
        // XXX _ejs_invoke_closure won't call proxy methods
        ejsval rv;

        EJSBool status = _ejs_invoke_closure_catch (&rv, adder, &set, 1, &nextValue, _ejs_undefined);

        // g. If status is an abrupt completion, return IteratorClose(iter, status).
        if (!status)
            return IteratorClose(iter, rv, EJS_TRUE);
    }
}

static EJS_NATIVE_FUNC(_ejs_Set_get_species) {
    return _ejs_Set;
}

ejsval _ejs_Set EJSVAL_ALIGNMENT;
ejsval _ejs_Set_prototype EJSVAL_ALIGNMENT;

ejsval
_ejs_set_iterator_new (ejsval set, EJSSetIteratorKind kind)
{
    /* 1. If Type(set) is not Object, throw a TypeError exception. */
    if (!EJSVAL_IS_OBJECT(set))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "set is not a Object");

    /* 2. If set does not have a [[SetData]] internal slot throw a TypeError exception. */
    if (!EJSVAL_IS_SET(set))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "value is not a Set");

    /* 3. If set’s [[SetData]] internal slot is undefined, then throw a TypeError exception. */

    /* 4. Let iterator be the result of ObjectCreate(%SetIteratorPrototype%,
     * ([[IteratedSet]], [[SetNextIndex]], [[SetIterationKind]])). */
    EJSSetIterator *iter = _ejs_gc_new (EJSSetIterator);
    _ejs_init_object ((EJSObject*) iter, _ejs_SetIterator_prototype, &_ejs_SetIterator_specops);

    /* 5. Set iterator’s [[IteratedSet]] internal slot to set. */
    iter->iterated = set;

    /* 6. Set iterator’s [[SetNextIndex]] internal slot to 0. */
    iter->next_index = 0;

    /* 7. Set iterator’s [[SetIterationKind]] internal slot to kind. */
    iter->kind = kind;

    /* 8. Return iterator */
    return OBJECT_TO_EJSVAL(iter);
}

ejsval _ejs_SetIterator EJSVAL_ALIGNMENT;
ejsval _ejs_SetIterator_prototype EJSVAL_ALIGNMENT;

static EJS_NATIVE_FUNC(_ejs_SetIterator_impl) {
    return *_this;
}

static EJS_NATIVE_FUNC(_ejs_SetIterator_prototype_next) {
    /* 1. Let O be the this value. */
    ejsval O = *_this;

    /* 2. If Type(O) is not Object, throw a TypeError exception. */
    if (!EJSVAL_IS_OBJECT(O))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, ".next called on non-object");

    /* 3. If O does not have all of the internal slots of a Set Iterator Instance (23.2.5.3),
     * throw a TypeError exception. */
    if (!EJSVAL_IS_SETITERATOR(O))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, ".next called on non-SetIterator instance");

    EJSSetIterator *OObj = (EJSSetIterator*)EJSVAL_TO_OBJECT(O);

    /* 4. Let s be the value of the [[IteratedSet]] internal slot of O. */
    ejsval s = OObj->iterated;

    /* 5. Let index be the value of the [[SetNextIndex]] internal slot of O. */
    uint32_t index = OObj->next_index;

    /* 6. Let itemKind be the value of the [[SetIterationKind]] internal slot of O. */
    EJSSetIteratorKind itemKind = OObj->kind;

    /* 7. If s is undefined, then return CreateIterResultObject(undefined, true). */
    if (EJSVAL_IS_UNDEFINED(s))
        return _ejs_create_iter_result (_ejs_undefined, _ejs_true);

    /* 8. Assert: s has a [[SetData]] internal slot and s has been initialized so the value of
     * [[SetData]] is not undefined. */

    /* 9. Let entries be the List that is the value of the [[SetData]] internal slot of s. */
    EJSSetValueEntry *entries = EJSVAL_TO_SET(s)->head_insert;

    /* 10. Repeat while index is less than the total number of elements of entries. The number of elements must
     * be redetermined each time this method is evaluated. */
    uint32_t i = 0;
    for (EJSSetValueEntry *entry = entries; entry; entry = entry->next_insert) {

        /* Ignore this item if is marked as empty */
        if (EJSVAL_IS_NO_ITER_VALUE_MAGIC(entry->value))
            continue;

        /* Ignore this item if we haven't reached the initial needed point/index */
        if (index > i++)
            continue;

        /* a. Let e be entries[index]. */
        ejsval e = entry->value;

        /* b. Set index to index+1; */
        index = index + 1;

        /* c. Set the [[SetNextIndex]] internal slot of O to index. */
        OObj->next_index = index;

        /* d. If e is not empty, then */
        /*  (see EJSVAL_IS_NO_ITER_VALUE_MAGIC check at the beginning of the loop */

        /*      i. If itemKind is "key+value" then, */
        if (itemKind == EJS_SET_ITER_KIND_KEYVALUE) {
            /* 1. Let result be the result of performing ArrayCreate(2). */
            /* 2. Assert: result is a new, well-formed Array object so the following operations will never fail. */
            ejsval result = _ejs_array_new (2, EJS_FALSE);

            /* 3. Call CreateDataProperty(result, "0", e) . */
            _ejs_object_setprop (result, NUMBER_TO_EJSVAL(0), e);

            /* 4. Call CreateDataProperty(result, "1", e) . */
            _ejs_object_setprop (result, NUMBER_TO_EJSVAL(1), e);

            return _ejs_create_iter_result (result, _ejs_false);
        }

        /*      ii. Return CreateIterResultObject(e, false). */
        return _ejs_create_iter_result (e, _ejs_false);
    }

    /* 11. Set the [[IteratedSet]] internal slot of O to undefined. */
    OObj->iterated = _ejs_undefined;

    /* 12. Return CreateIterResultObject(undefined, true). */
    return _ejs_create_iter_result (_ejs_undefined, _ejs_true);
}

void
_ejs_set_init(ejsval global)
{
    _ejs_gc_add_root (&_ejs_Set);
    _ejs_Set = _ejs_function_new_without_proto (_ejs_null, _ejs_atom_Set, _ejs_Set_impl);
    _ejs_object_setprop (global, _ejs_atom_Set, _ejs_Set);

    _ejs_gc_add_root (&_ejs_Set_prototype);
    _ejs_Set_prototype = _ejs_object_new(_ejs_null, &_ejs_Object_specops);
    _ejs_object_define_value_property (_ejs_Set, _ejs_atom_prototype, _ejs_Set_prototype, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_CONFIGURABLE | EJS_PROP_NOT_WRITABLE);
    _ejs_object_define_value_property (_ejs_Set_prototype, _ejs_atom_constructor, _ejs_Set, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_CONFIGURABLE | EJS_PROP_WRITABLE);

#define OBJ_METHOD(x) EJS_INSTALL_ATOM_FUNCTION(_ejs_Set, x, _ejs_Set_##x)
#define PROTO_METHOD(x) EJS_INSTALL_ATOM_FUNCTION_FLAGS(_ejs_Set_prototype, x, _ejs_Set_prototype_##x, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_WRITABLE | EJS_PROP_CONFIGURABLE)
#define PROTO_GETTER(x) EJS_INSTALL_ATOM_GETTER(_ejs_Set_prototype, x, _ejs_Set_prototype_get_##x)

    PROTO_METHOD(add);
    PROTO_METHOD(clear);
    PROTO_METHOD(delete);
    PROTO_METHOD(entries);
    PROTO_METHOD(forEach);
    PROTO_METHOD(has);
    PROTO_GETTER(size);

    // ES2025 set methods
    PROTO_METHOD(union);
    PROTO_METHOD(intersection);
    PROTO_METHOD(difference);
    PROTO_METHOD(symmetricDifference);
    PROTO_METHOD(isSubsetOf);
    PROTO_METHOD(isSupersetOf);
    PROTO_METHOD(isDisjointFrom);

    // expand PROTO_METHOD(values) here so that we can install the function for both keys and @@iterator below
    ejsval _values = _ejs_function_new_native (_ejs_null, _ejs_atom_values, _ejs_Set_prototype_values);
    _ejs_object_define_value_property (_ejs_Set_prototype, _ejs_atom_values, _values, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_FLAGS_WRITABLE | EJS_PROP_CONFIGURABLE);
    _ejs_object_define_value_property (_ejs_Set_prototype, _ejs_atom_keys, _values, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_WRITABLE | EJS_PROP_CONFIGURABLE);

    _ejs_object_define_value_property (_ejs_Set_prototype, _ejs_Symbol_iterator, _values, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_WRITABLE | EJS_PROP_CONFIGURABLE);
    _ejs_object_define_value_property (_ejs_Set_prototype, _ejs_Symbol_toStringTag, _ejs_atom_Set, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_WRITABLE | EJS_PROP_CONFIGURABLE);

    EJS_INSTALL_SYMBOL_GETTER(_ejs_Set, species, _ejs_Set_get_species);

#undef OBJ_METHOD
#undef PROTO_METHOD

    _ejs_SetIterator = _ejs_function_new_without_proto (_ejs_null, _ejs_atom_Set, _ejs_SetIterator_impl);

    _ejs_gc_add_root (&_ejs_SetIterator_prototype);
    _ejs_SetIterator_prototype = _ejs_object_new(_ejs_Iterator_prototype, &_ejs_Object_specops);
    _ejs_object_define_value_property (_ejs_SetIterator, _ejs_atom_prototype, _ejs_SetIterator_prototype,
                                        EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_CONFIGURABLE | EJS_PROP_NOT_WRITABLE);
    _ejs_object_define_value_property (_ejs_SetIterator_prototype, _ejs_atom_constructor, _ejs_SetIterator,
                                        EJS_PROP_NOT_ENUMERABLE | EJS_PROP_CONFIGURABLE | EJS_PROP_WRITABLE);

#define PROTO_ITER_METHOD(x) EJS_INSTALL_ATOM_FUNCTION_FLAGS (_ejs_SetIterator_prototype, x, _ejs_SetIterator_prototype_##x, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_WRITABLE | EJS_PROP_CONFIGURABLE)
    PROTO_ITER_METHOD(next);
#undef PROTO_ITER_METHOD

}

static EJSObject*
_ejs_set_specop_allocate ()
{
    return (EJSObject*)_ejs_gc_new (EJSSet);
}

static void
_ejs_set_specop_finalize (EJSObject* obj)
{
    EJSSet* set = (EJSSet*)obj;

    EJSSetValueEntry* s = set->head_insert;
    while (s) {
        EJSSetValueEntry* next = s->next_insert;
        free (s);
        s = next;
    }
    free (set->index);

    _ejs_Object_specops.Finalize (obj);
}

static void
_ejs_set_specop_scan (EJSObject* obj, EJSValueFunc scan_func)
{
    EJSSet* set = (EJSSet*)obj;

    for (EJSSetValueEntry *s = set->head_insert; s; s = s->next_insert)
        scan_func (&(s->value));

    _ejs_Object_specops.Scan (obj, scan_func);
}

EJS_DEFINE_CLASS(Set,
                 OP_INHERIT, // [[GetPrototypeOf]]
                 OP_INHERIT, // [[SetPrototypeOf]]
                 OP_INHERIT, // [[IsExtensible]]
                 OP_INHERIT, // [[PreventExtensions]]
                 OP_INHERIT, // [[GetOwnProperty]]
                 OP_INHERIT, // [[DefineOwnProperty]]
                 OP_INHERIT, // [[HasProperty]]
                 OP_INHERIT, // [[Get]]
                 OP_INHERIT, // [[Set]]
                 OP_INHERIT, // [[Delete]]
                 OP_INHERIT, // [[Enumerate]]
                 OP_INHERIT, // [[OwnPropertyKeys]]
                 OP_INHERIT, // [[Call]]
                 OP_INHERIT, // [[Construct]]
                 _ejs_set_specop_allocate,
                 _ejs_set_specop_finalize,
                 _ejs_set_specop_scan
                 )

static void
_ejs_set_iterator_specop_scan (EJSObject* obj, EJSValueFunc scan_func)
{
    EJSSetIterator* iter = (EJSSetIterator*)obj;
    scan_func(&(iter->iterated));
    _ejs_Object_specops.Scan (obj, scan_func);
}

EJS_DEFINE_CLASS(SetIterator,
                 OP_INHERIT, // [[GetPrototypeOf]]
                 OP_INHERIT, // [[SetPrototypeOf]]
                 OP_INHERIT, // [[IsExtensible]]
                 OP_INHERIT, // [[PreventExtensions]]
                 OP_INHERIT, // [[GetOwnProperty]]
                 OP_INHERIT, // [[DefineOwnProperty]]
                 OP_INHERIT, // [[HasProperty]]
                 OP_INHERIT, // [[Get]]
                 OP_INHERIT, // [[Set]]
                 OP_INHERIT, // [[Delete]]
                 OP_INHERIT, // [[Enumerate]]
                 OP_INHERIT, // [[OwnPropertyKeys]]
                 OP_INHERIT, // [[Call]]
                 OP_INHERIT, // [[Construct]]
                 OP_INHERIT, // allocate.  shouldn't ever be used
                 OP_INHERIT, // finalize.  also shouldn't ever be used
                 _ejs_set_iterator_specop_scan
                 )

