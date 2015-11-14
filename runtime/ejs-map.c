/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include "ejs-map.h"
#include "ejs-array.h"
#include "ejs-gc.h"
#include "ejs-generator.h"
#include "ejs-error.h"
#include "ejs-function.h"
#include "ejs-proxy.h"
#include "ejs-ops.h"
#include "ejs-symbol.h"

ejsval
_ejs_map_new ()
{
    EJSMap *map = _ejs_gc_new (EJSMap);
    _ejs_init_object ((EJSObject*)map, _ejs_Object_prototype, &_ejs_Map_specops);

    return OBJECT_TO_EJSVAL(map);
}

// ES6: 23.1.3.1
// Map.prototype.clear ()
static EJS_NATIVE_FUNC(_ejs_Map_prototype_clear) {
    // NOTE The existing [[MapData]] List is preserved because there
    // may be existing MapIterator objects that are suspended midway
    // through iterating over that List.

    // 1. Let M be the this value.
    ejsval M = *_this;

    // 2. If Type(M) is not Object, then throw a TypeError exception.
    if (!EJSVAL_IS_OBJECT(M))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Map.prototype.clear called with non-object this.");

    // 3. If M does not have a [[MapData]] internal slot throw a TypeError exception.
    // 4. If M’s [[MapData]] internal slot is undefined, then throw a TypeError exception.

    // 5. Let entries be the List that is the value of M’s [[MapData]] internal slot.
    EJSKeyValueEntry* entries = EJSVAL_TO_MAP(M)->head_insert;

    // 6. Repeat for each Record {[[key]], [[value]]} p that is an element of entries,
    for (EJSKeyValueEntry* p = entries; p; p = p->next_insert) {
        // 7. Set p.[[key]] to empty.
        p->key = MAGIC_TO_EJSVAL_IMPL(EJS_NO_ITER_VALUE);
        // 8. Set p.[[value]] to empty.
        p->value = MAGIC_TO_EJSVAL_IMPL(EJS_NO_ITER_VALUE);
    }

    // 9. Return undefined.
    return _ejs_undefined;
}

// ES6: 23.1.3.2
// Map.prototype.constructor

// ES6: 23.1.3.3
// Map.prototype.delete ( key )
ejsval
_ejs_map_delete (ejsval map, ejsval key)
{
    // our caller should have already validated and thrown appropriate TypeErrors
    EJS_ASSERT(EJSVAL_IS_MAP(map));

    // 4. Let entries be the List that is the value of M’s [[MapData]] internal slot.
    // 5. Repeat for each Record {[[key]], [[value]]} p that is an element of entries,
    // a. If p.[[key]] is not empty and SameValueZero(p.[[key]], key) is true, then
    // i. Set p.[[key]] to empty.
    // ii. Set p.[[value]] to empty.
    // iii. Return true.
    // 6. Return false.

    return _ejs_false;
}

static EJS_NATIVE_FUNC(_ejs_Map_prototype_delete) {
    // NOTE The value empty is used as a specification device to
    // indicate that an entry has been deleted. Actual implementations
    // may take other actions such as physically removing the entry
    // from internal data structures.

    ejsval key = _ejs_undefined;
    if (argc > 0) key = args[0];

    // 1. Let M be the this value.
    ejsval M = *_this;

    // 2. If Type(M) is not Object, then throw a TypeError exception.
    if (!EJSVAL_IS_OBJECT(M))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Map.prototype.delete called with non-object this.");

    // 3. If M does not have a [[MapData]] internal slot throw a TypeError exception.
    if (!EJSVAL_IS_MAP(M))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Map.prototype.delete called with non-Map this.");

    return _ejs_map_delete (M, key);
}

// ES6: 23.1.3.4
// Map.prototype.entries ( )
static EJS_NATIVE_FUNC(_ejs_Map_prototype_entries) {
    // 1. Let M be the this value.
    ejsval M = *_this;

    // 2. If Type(M) is not Object, then throw a TypeError exception.
    if (!EJSVAL_IS_OBJECT(M))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Map.prototype.get called with non-object this.");

    // 3. Return the result of calling the CreateMapIterator abstract operation with arguments M and "key+value".
    return _ejs_map_iterator_new (M, EJS_MAP_ITER_KIND_KEYVALUE);
}

// ES6: 23.1.3.5
// Map.prototype.forEach ( callbackfn , thisArg = undefined )
static EJS_NATIVE_FUNC(_ejs_Map_prototype_forEach) {
    ejsval callbackfn = _ejs_undefined;
    ejsval thisArg = _ejs_undefined;

    if (argc > 0) callbackfn = args[0];
    if (argc > 1) thisArg = args[1];

    // 1. Let M be the this value.
    ejsval M = *_this;

    // 2. If Type(M) is not Object, then throw a TypeError exception.
    if (!EJSVAL_IS_OBJECT(M))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Map.prototype.forEach called with non-object this.");

    // 3. If M does not have a [[MapData]] internal slot throw a TypeError exception.
    if (!EJSVAL_IS_MAP(M))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Map.prototype.forEach called with non-Map this.");

    // 4. If M’s [[MapData]] internal slot is undefined, then throw a TypeError exception.

    // 5. If IsCallable(callbackfn) is false, throw a TypeError exception.
    if (!IsCallable(callbackfn))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Map.prototype.forEach callbackfn isn't a function.");
    
    // 6. If thisArg was supplied, let T be thisArg; else let T be undefined.
    ejsval T = thisArg;

    EJSMap* map = EJSVAL_TO_MAP(M);

    // 7. Let entries be the List that is the value of M’s [[MapData]] internal slot.
    // 8. Repeat for each Record {[[key]], [[value]]} e that is an element of entries, in original key insertion order
    //    a. If e.[[key]] is not empty, then
    //       i. Let funcResult be the result of calling the [[Call]] internal method of callbackfn with T as thisArgument and a List containing e.[[value]], e.[[key]], and M as argumentsList.
    //       ii. ReturnIfAbrupt(funcResult).
    for (EJSKeyValueEntry *s = map->head_insert; s; s = s->next_insert) {
        if (EJSVAL_IS_NO_ITER_VALUE_MAGIC(s->key))
            continue;
        ejsval callback_args[3];
        callback_args[0] = s->value;
        callback_args[1] = s->key;
        callback_args[2] = M;
        _ejs_invoke_closure (callbackfn, &T, 3, callback_args, _ejs_undefined);
    }

    // 9. Return undefined.
    return _ejs_undefined;
}

ejsval
_ejs_map_get (ejsval map, ejsval key)
{
    // our caller should have already validated and thrown appropriate TypeErrors
    EJS_ASSERT(EJSVAL_IS_MAP(map));

    EJSMap* _map = EJSVAL_TO_MAP(map);

    // 4. Let entries be the List that is the value of M’s [[MapData]] internal slot.
    EJSKeyValueEntry* entries = _map->head_insert;

    // 5. Repeat for each Record {[[key]], [[value]]} p that is an element of entries,
    for (EJSKeyValueEntry* p = entries; p; p = p->next_insert) {
        // a. If p.[[key]] is not empty and SameValueZero(p.[[key]], key) is true, return p.[[value]].
        if (!EJSVAL_IS_NO_ITER_VALUE_MAGIC(p->key) && SameValueZero (p->key, key))
            return p->value;
    }
    // 6. Return undefined.
    return _ejs_undefined;
}

// ES6: 23.1.3.6
// Map.prototype.get ( key )
static EJS_NATIVE_FUNC(_ejs_Map_prototype_get) {
    ejsval key = _ejs_undefined;
    if (argc > 0) key = args[0];

    // 1. Let M be the this value.
    ejsval M = *_this;

    // 2. If Type(M) is not Object, then throw a TypeError exception.
    if (!EJSVAL_IS_OBJECT(M))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Map.prototype.get called with non-object this.");
        
    // 3. If M does not have a [[MapData]] internal slot throw a TypeError exception.
    if (!EJSVAL_IS_MAP(M))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Map.prototype.get called with non-Map this.");

    return _ejs_map_get(M, key);
}

ejsval
_ejs_map_has (ejsval map, ejsval key)
{
    // our caller should have already validated and thrown appropriate TypeErrors
    EJS_ASSERT(EJSVAL_IS_MAP(map));

    EJSMap* _map = EJSVAL_TO_MAP(map);

    // 4. Let entries be the List that is the value of M’s [[MapData]] internal slot.
    EJSKeyValueEntry* entries = _map->head_insert;

    // 5. Repeat for each Record {[[key]], [[value]]} p that is an element of entries,
    for (EJSKeyValueEntry* p = entries; p; p = p->next_insert) {
        // a. If p.[[key]] is not empty and SameValueZero(p.[[key]], key) is true, return true.
        if (!EJSVAL_IS_NO_ITER_VALUE_MAGIC(p->key) && SameValueZero (p->key, key))
            return _ejs_true;
    }

    // 6. Return false.
    return _ejs_false;
}

// ES6: 23.1.3.7
// Map.prototype.has ( key )
static EJS_NATIVE_FUNC(_ejs_Map_prototype_has) {
    ejsval key = _ejs_undefined;
    if (argc > 0) key = args[0];

    // 1. Let M be the this value.
    ejsval M = *_this;

    // 2. If Type(M) is not Object, then throw a TypeError exception.
    if (!EJSVAL_IS_OBJECT(M))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Map.prototype.has called with non-object this.");

    // 3. If M does not have a [[MapData]] internal slot throw a TypeError exception.
    if (!EJSVAL_IS_MAP(M))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Map.prototype.has called with non-Map this.");

    return _ejs_map_has(M, key);
}

// ES6: 23.1.3.8
// Map.prototype.keys ( )
static EJS_NATIVE_FUNC(_ejs_Map_prototype_keys) {
    // 1. Let M be the this value.
    ejsval M = *_this;

    // 2. Return the result of calling the CreateMapIterator abstract operation with arguments M and "key".
    return _ejs_map_iterator_new (M, EJS_MAP_ITER_KIND_KEY);
}

ejsval
_ejs_map_set (ejsval map, ejsval key, ejsval value)
{
    // our caller should have already validated and thrown appropriate TypeErrors
    EJS_ASSERT(EJSVAL_IS_MAP(map));

    EJSMap* _map = EJSVAL_TO_MAP(map);

    // 4. Let entries be the List that is the value of M’s [[MapData]] internal slot.
    EJSKeyValueEntry* entries = _map->head_insert;

    // 5. Repeat for each Record {[[key]], [[value]]} p that is an element of entries,
    EJSKeyValueEntry* p;
    for (p = entries; p; p = p->next_insert) {
        // a. If p.[[key]] is not empty and SameValueZero(p.[[key]], key) is true, then
        if (!EJSVAL_IS_NO_ITER_VALUE_MAGIC(p->key) && SameValueZero (p->key, key)) {
            // i. Set p.[[value]] to value.
            p->value = value;
            // ii. Return M.
            return map;
        }
    }
    // 6. If key is −0, let key be +0.

    // XXX

    // 7. Let p be the Record {[[key]]: key, [[value]]: value}.
    p = calloc (1, sizeof (EJSKeyValueEntry));
    p->key = key;
    p->value = value;

    // 8. Append p as the last element of entries.
    if (!_map->head_insert)
        _map->head_insert = p;

    if (_map->tail_insert) {
        _map->tail_insert->next_insert = p;
        _map->tail_insert = p;
    }
    else {
        _map->tail_insert = p;
    }

    // 9. Return M.
    return map;
}

// ES6: 23.1.3.9
// Map.prototype.set ( key , value )
static EJS_NATIVE_FUNC(_ejs_Map_prototype_set) {
    ejsval key = _ejs_undefined;
    ejsval value = _ejs_undefined;

    if (argc > 0) key = args[0];
    if (argc > 1) value = args[1];

    // 1. Let M be the this value.
    ejsval M = *_this;

    // 2. If Type(M) is not Object, then throw a TypeError exception.
    if (!EJSVAL_IS_OBJECT(M))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Map.prototype.set called with non-object this.");

    // 3. If M does not have a [[MapData]] internal slot throw a TypeError exception.
    if (!EJSVAL_IS_MAP(M))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Map.prototype.set called with non-Map this.");

    return _ejs_map_set (M, key, value);
}

// ES6: 23.1.3.10
// get Map.prototype.size
static EJS_NATIVE_FUNC(_ejs_Map_prototype_get_size) {
    EJSMap* _map = EJSVAL_TO_MAP(*_this);
    uint32_t size = 0;

    EJSKeyValueEntry* entry = _map->head_insert;
    while (entry) {
        if (!EJSVAL_IS_NO_ITER_VALUE_MAGIC(entry->key))
            size++;
        entry = entry->next_insert;
    }
    return NUMBER_TO_EJSVAL(size);
}

// ES6: 23.1.3.11
// Map.prototype.values ( )
static EJS_NATIVE_FUNC(_ejs_Map_prototype_values) {
    // 1. Let M be the this value.
    ejsval M = *_this;

    // 2. Return the result of calling the CreateMapIterator abstract operation with arguments M and "value".
    return _ejs_map_iterator_new (M, EJS_MAP_ITER_KIND_VALUE);
}

// ES2015, June 2015
// 23.1.1.1 Map ( [ iterable ] )
static EJS_NATIVE_FUNC(_ejs_Map_impl) {
    ejsval iterable = _ejs_undefined;
    if (argc > 0) iterable = args[0];

    // 1. If NewTarget is undefined, throw a TypeError exception.
    if (EJSVAL_IS_UNDEFINED(newTarget))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Map constructor must be called with new");

    // 2. Let map be OrdinaryCreateFromConstructor(NewTarget, "%MapPrototype%", «[[MapData]]» ).
    // 3. ReturnIfAbrupt(map).
    // 4. Set map’s [[MapData]] internal slot to a new empty List.
    ejsval map = OrdinaryCreateFromConstructor(newTarget, _ejs_Map_prototype, &_ejs_Map_specops);
    *_this = map;

    // 5. If iterable is not present, let iterable be undefined.
    // 6. If iterable is either undefined or null, let iter be undefined.
    ejsval iter;
    ejsval adder;
    if (EJSVAL_IS_NULL_OR_UNDEFINED(iterable)) {
        iter = _ejs_undefined;
    }
    // 7. Else,
    else {
        // a. Let adder be Get(map, "set").
        // b. ReturnIfAbrupt(adder).
        adder = Get (map, _ejs_atom_set);
        // c. If IsCallable(adder) is false, throw a TypeError exception.
        if (!IsCallable(adder))
            _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Map.prototype.set is not a function");
        // d. Let iter be GetIterator(iterable).
        // e. ReturnIfAbrupt(iter).
        iter = GetIterator(iterable, _ejs_undefined);
    }
    // 8. If iter is undefined, return map.
    if (EJSVAL_IS_UNDEFINED(iter))
        return map;
    // 9. Repeat
    for (;;) {
        // a. Let next be IteratorStep(iter).
        // b. ReturnIfAbrupt(next).
        ejsval next = IteratorStep (iter);

        // c. If next is false, return map.
        if (!EJSVAL_TO_BOOLEAN(next))
            return map;

        // d. Let nextItem be IteratorValue(next).
        // e. ReturnIfAbrupt(nextItem).
        ejsval nextItem = IteratorValue (next);

        // f. If Type(nextItem) is not Object,
        if (!EJSVAL_IS_OBJECT(nextItem)) {
            // i. Let error be Completion{[[type]]: throw, [[value]]: a newly created TypeError object, [[target]]:empty}.
            ejsval error = _ejs_nativeerror_new_utf8(EJS_TYPE_ERROR, "non-object in iterable for Map constructor");

            // ii. Return IteratorClose(iter, error).
            return IteratorClose(iter, error, EJS_TRUE);
        }

        // g. Let k be Get(nextItem, "0").
        // h. If k is an abrupt completion, return IteratorClose(iter, k).
        ejsval k = Get(nextItem, _ejs_atom_0); // XXX call IteratorClose here on exception
        
        // i. Let v be Get(nextItem, "1").
        // j. If v is an abrupt completion, return IteratorClose(iter, v).
        ejsval v = Get(nextItem, _ejs_atom_1);  // XXX call IteratorClose here on exception

        // k. Let status be Call(adder, map, «k.[[value]], v.[[value]]»).
        // XXX _ejs_invoke_closure won't call proxy methods
        ejsval rv;

        ejsval adder_args[2];
        adder_args[0] = k;
        adder_args[1] = v;
        EJSBool status = _ejs_invoke_closure_catch (&rv, adder, &map, 2, adder_args, _ejs_undefined);

        // l. If status is an abrupt completion, return IteratorClose(iter, status).
        if (!status)
            return IteratorClose(iter, rv, EJS_TRUE);
    }
}

static EJS_NATIVE_FUNC(_ejs_Map_get_species) {
    return _ejs_Map;
}

ejsval _ejs_Map EJSVAL_ALIGNMENT;
ejsval _ejs_Map_prototype EJSVAL_ALIGNMENT;

ejsval
_ejs_map_iterator_new (ejsval map, EJSMapIteratorKind kind)
{
    /* 1. If Type(map) is not Object, throw a TypeError exception. */
    if (!EJSVAL_IS_OBJECT(map))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "XXX");

    /* 2. If map does not have a [[MapData]] internal slot throw a TypeError exception. */
    if (!EJSVAL_IS_MAP(map))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "XXX");

    /* 3. If the value of map’s [[MapData]] internal slot is undefined, then throw a TypeError exception. */

    /* 4. Let iterator be the result of ObjectCreate(%MapIteratorPrototype%,
     * ([[Map]], [[MapNextIndex]], [[MapIterationKind]])). */
    EJSMapIterator *iterator = _ejs_gc_new (EJSMapIterator);
    _ejs_init_object ((EJSObject*) iterator, _ejs_MapIterator_prototype, &_ejs_MapIterator_specops);

    /* 5. Set iterator’s [[Map]] internal slot to map. */
    iterator->iterated = map;

    /* 6. Set iterator’s [[MapNextIndex]] internal slot to 0. */
    iterator->next_index = 0;

    /* 7. Set iterator’s [[MapIterationKind]] internal slot to kind. */
    iterator->kind = kind;

    return OBJECT_TO_EJSVAL(iterator);
}

ejsval _ejs_MapIterator EJSVAL_ALIGNMENT;
ejsval _ejs_MapIterator_prototype EJSVAL_ALIGNMENT;

static EJS_NATIVE_FUNC(_ejs_MapIterator_impl) {
    return *_this;
}

static EJS_NATIVE_FUNC(_ejs_MapIterator_prototype_next) {
    /* 1. 0 Let O be the this value. */
    ejsval O = *_this;

    /* 2. If Type(O) is not Object, throw a TypeError exception. */
    if (!EJSVAL_IS_OBJECT(O))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "XXX");

    /* 3. If O does not have all of the internal slots of a Map Iterator Instance (23.1.5.3),
     * throw a TypeError exception. */
    if (!EJSVAL_IS_MAPITERATOR(O))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "XXX");

    EJSMapIterator *OObj = (EJSMapIterator*)EJSVAL_TO_OBJECT(O);

    /* 4. Let m be the value of the [[Map]] internal slot of O. */
    ejsval m = OObj->iterated;

    /* 5. Let index be the value of the [[MapNextIndex]] internal slot of O. */
    uint32_t index = OObj->next_index;

    /* 6. Let itemKind be the value of the [[MapIterationKind]] internal slot of O. */
    EJSMapIteratorKind itemKind = OObj->kind;

    /* 7. If m is undefined, then return CreateIterResultObject(undefined, true) */
    if (EJSVAL_IS_UNDEFINED(m))
        return _ejs_create_iter_result (_ejs_undefined, _ejs_true);

    /* 8. Assert: m has a [[MapData]] internal slot and m has been initialized so the value of
     * [[MapData]] is not undefined. */

    /* 9. Let entries be the List that is the value of the [[MapData]] internal slot of m. */
    EJSKeyValueEntry* entries = EJSVAL_TO_MAP(m)->head_insert;

    /* 10. Repeat while index is less than the total number of elements of entries. The number of elements must
     * be redetermined each time this method is evaluated. */
    uint32_t i = 0;
    for (EJSKeyValueEntry *entry = entries; entry; entry = entry->next_insert) {

        /* Ignore if this entry is marked as empty */
        if (EJSVAL_IS_NO_ITER_VALUE_MAGIC(entry->key))
            continue;

        /* Ignore this item if we haven't reached the initial needed point/index */
        if (index > i++)
            continue;

        /* a. Let e be the Record {[[key]], [[value]]} that is the value of entries[index]. */
        EJSKeyValueEntry *e = entry;

        /* b. Set index to index+1; */
        index = index + 1;

        /* c. Set the [[MapNextIndex]] internal slot of O to index. */
        OObj->next_index = index;

        /* d. If e.[[key]] is not empty, then */
        /* (see EJSVAL_IS_NO_ITER_VALUE_MAGIC check at the beginning of the loop) */
        ejsval result;

        /*  i. If itemKind is "key" then, let result be e.[[key]]. */
        if (itemKind == EJS_MAP_ITER_KIND_KEY)
            result = e->key;
        /*  ii. Else if itemKind is "value" then, let result be e.[[value]]. */
        else if (itemKind == EJS_MAP_ITER_KIND_VALUE)
            result = e->value;
        /*  iii. Else, */
        else {
            /* 1. Assert: itemKind is "key+value". */
            /* 2. Let result be the result of performing ArrayCreate(2). */
            result = _ejs_array_new (2, EJS_FALSE);

            /* 3. Assert: result is a new, well-formed Array object so the following operations will never fail. */
            /* 4. Call CreateDataProperty(result, "0", e.[[key]]) . */
            _ejs_object_setprop (result, NUMBER_TO_EJSVAL(0), e->key);

            /* 5. Call CreateDataProperty(result, "1", e.[[value]]). */
            _ejs_object_setprop (result, NUMBER_TO_EJSVAL(1), e->value);
        }

        /*  iv. Return CreateIterResultObject(result, false). */
        return _ejs_create_iter_result (result, _ejs_false);
    }

    /* 11. Set the [[Map]] internal slot of O to undefined. */
    OObj->iterated = _ejs_undefined;

    /* 12. Return CreateIterResultObject(undefined, true). */
    return _ejs_create_iter_result (_ejs_undefined, _ejs_true);
}

void
_ejs_map_init(ejsval global)
{
    _ejs_Map = _ejs_function_new_without_proto (_ejs_null, _ejs_atom_Map, (EJSClosureFunc)_ejs_Map_impl);
    _ejs_object_setprop (global, _ejs_atom_Map, _ejs_Map);

    _ejs_gc_add_root (&_ejs_Map_prototype);
    _ejs_Map_prototype = _ejs_map_new ();
    _ejs_object_setprop (_ejs_Map,       _ejs_atom_prototype,  _ejs_Map_prototype);

#define OBJ_METHOD(x) EJS_INSTALL_ATOM_FUNCTION(_ejs_Map, x, _ejs_Map_##x)
#define PROTO_METHOD(x) EJS_INSTALL_ATOM_FUNCTION_FLAGS(_ejs_Map_prototype, x, _ejs_Map_prototype_##x, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_WRITABLE | EJS_PROP_CONFIGURABLE)
#define PROTO_GETTER(x) EJS_INSTALL_ATOM_GETTER(_ejs_Map_prototype, x, _ejs_Map_prototype_get_##x)

    PROTO_METHOD(clear);
    // XXX (ES6 23.1.3.2) Map.prototype.constructor
    PROTO_METHOD(delete);
    PROTO_METHOD(forEach);
    PROTO_METHOD(get);
    PROTO_METHOD(has);
    PROTO_METHOD(keys);
    PROTO_METHOD(values);
    PROTO_METHOD(set);

    // XXX (ES6 23.1.3.10) get Map.prototype.size
    PROTO_GETTER(size);

    // expand PROTO_METHOD(entries) here so we can install the function for @@iterator below
    ejsval _entries = _ejs_function_new_native (_ejs_null, _ejs_atom_entries,  (EJSClosureFunc)_ejs_Map_prototype_entries);
    _ejs_object_define_value_property (_ejs_Map_prototype, _ejs_atom_entries, _entries, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_FLAGS_WRITABLE | EJS_PROP_FLAGS_CONFIGURABLE);

    _ejs_object_define_value_property (_ejs_Map_prototype, _ejs_Symbol_iterator, _entries, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_FLAGS_WRITABLE | EJS_PROP_FLAGS_CONFIGURABLE);
    _ejs_object_define_value_property (_ejs_Map_prototype, _ejs_Symbol_toStringTag, _ejs_atom_Map, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_WRITABLE | EJS_PROP_CONFIGURABLE);

    EJS_INSTALL_SYMBOL_GETTER(_ejs_Map, species, _ejs_Map_get_species);

#undef OBJ_METHOD
#undef PROTO_METHOD

    _ejs_MapIterator = _ejs_function_new_without_proto (_ejs_null, _ejs_atom_Map, (EJSClosureFunc)_ejs_MapIterator_impl);

    _ejs_gc_add_root (&_ejs_MapIterator_prototype);
    _ejs_MapIterator_prototype = _ejs_map_iterator_new (_ejs_Map_prototype, EJS_MAP_ITER_KIND_VALUE);
    EJSVAL_TO_OBJECT(_ejs_MapIterator_prototype)->proto = _ejs_Iterator_prototype;
    _ejs_object_define_value_property (_ejs_MapIterator, _ejs_atom_prototype, _ejs_MapIterator_prototype,
            EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_CONFIGURABLE | EJS_PROP_NOT_WRITABLE);
    _ejs_object_define_value_property (_ejs_MapIterator_prototype, _ejs_atom_constructor, _ejs_MapIterator,
            EJS_PROP_NOT_ENUMERABLE | EJS_PROP_CONFIGURABLE | EJS_PROP_WRITABLE);

#define PROTO_ITER_METHOD(x) EJS_INSTALL_ATOM_FUNCTION_FLAGS (_ejs_MapIterator_prototype, x, _ejs_MapIterator_prototype_##x, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_WRITABLE | EJS_PROP_CONFIGURABLE)
    PROTO_ITER_METHOD(next);
#undef PROTO_ITER_METHOD

}

static EJSObject*
_ejs_map_specop_allocate ()
{
    return (EJSObject*)_ejs_gc_new (EJSMap);
}

static void
_ejs_map_specop_finalize (EJSObject* obj)
{
    EJSMap* map = (EJSMap*)obj;

    EJSKeyValueEntry* s = map->head_insert;
    while (s) {
        EJSKeyValueEntry* next = s->next_insert;
        free (s);
        s = next;
    }

    _ejs_Object_specops.Finalize (obj);
}

static void
_ejs_map_specop_scan (EJSObject* obj, EJSValueFunc scan_func)
{
    EJSMap* map = (EJSMap*)obj;

    for (EJSKeyValueEntry *s = map->head_insert; s; s = s->next_insert) {
        scan_func (s->key);
        scan_func (s->value);
    }

    _ejs_Object_specops.Scan (obj, scan_func);
}

EJS_DEFINE_CLASS(Map,
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
                 _ejs_map_specop_allocate,
                 _ejs_map_specop_finalize,
                 _ejs_map_specop_scan
                 )

static void
_ejs_map_iterator_specop_scan (EJSObject* obj, EJSValueFunc scan_func)
{
    EJSMapIterator* iter = (EJSMapIterator*)obj;
    scan_func(iter->iterated);
    _ejs_Object_specops.Scan (obj, scan_func);
}

EJS_DEFINE_CLASS(MapIterator,
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
                 _ejs_map_iterator_specop_scan
                 )

