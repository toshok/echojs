/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include "ejs-map.h"
#include "ejs-gc.h"
#include "ejs-error.h"
#include "ejs-function.h"
#include "ejs-ops.h"

#define EJSVAL_IS_MAP(v)     (EJSVAL_IS_OBJECT(v) && (EJSVAL_TO_OBJECT(v)->ops == &_ejs_map_specops))
#define EJSVAL_TO_MAP(v)     ((EJSMap*)EJSVAL_TO_OBJECT(v))

typedef EJSBool (*ComparatorFunc)(ejsval, ejsval);

static EJSObject* _ejs_map_specop_allocate ();
static void    _ejs_map_specop_finalize (EJSObject* obj);
static void    _ejs_map_specop_scan (EJSObject* obj, EJSValueFunc scan_func);

EJS_DEFINE_CLASS(map, "Map",
                 OP_INHERIT, // get
                 OP_INHERIT, // get_own_property
                 OP_INHERIT, // get_property
                 OP_INHERIT, // put
                 OP_INHERIT, // can_put
                 OP_INHERIT, // has_property
                 OP_INHERIT, // delete
                 OP_INHERIT, // default_value
                 OP_INHERIT, // define_own_property
                 OP_INHERIT, // has_instance
                 _ejs_map_specop_allocate,
                 _ejs_map_specop_finalize,
                 _ejs_map_specop_scan
                 )

// ES6: 23.1.3.1
// Map.prototype.clear ()
ejsval
_ejs_Map_prototype_clear (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    // NOTE The existing [[MapData]] List is preserved because there
    // may be existing MapIterator objects that are suspended midway
    // through iterating over that List.

    // 1. Let M be the this value.
    ejsval M = _this;

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
_ejs_Map_prototype_delete (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    // NOTE The value empty is used as a specification device to
    // indicate that an entry has been deleted. Actual implementations
    // may take other actions such as physically removing the entry
    // from internal data structures.

    ejsval key = _ejs_undefined;
    if (argc > 0) key = args[0];

    // 1. Let M be the this value.
    ejsval M = _this;

    // 2. If Type(M) is not Object, then throw a TypeError exception.
    if (!EJSVAL_IS_OBJECT(M))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Map.prototype.delete called with non-object this.");

    // 3. If M does not have a [[MapData]] internal slot throw a TypeError exception.
    if (!EJSVAL_IS_MAP(M))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Map.prototype.delete called with non-Map this.");

    // 4. If M’s [[MapData]] internal slot is undefined, then throw a TypeError exception.
    // 5. If M’s [[MapComparator]] internal slot is undefined, then let same be the abstract operation SameValueZero.
    // 6. Else, let same be the abstract operation SameValue.
    // 7. Let entries be the List that is the value of M’s [[MapData]] internal slot.
    // 8. Repeat for each Record {[[key]], [[value]]} p that is an element of entries,
    //    a. If same(p.[[key]], key), then
    //       i. Set p.[[key]] to empty.
    //      ii. Set p.[[value]] to empty.
    //     iii. Return true.
    // 9. Return false.

    return _ejs_false;
}

// ES6: 23.1.3.4
// Map.prototype.entries ( )
ejsval
_ejs_Map_prototype_entries (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    // 1. Let M be the this value.
    ejsval M = _this;

    // 2. If Type(M) is not Object, then throw a TypeError exception.
    if (!EJSVAL_IS_OBJECT(M))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Map.prototype.get called with non-object this.");

    // 3. Return the result of calling the CreateMapIterator abstract operation with arguments M and "key+value".
    EJS_NOT_IMPLEMENTED();
}

// ES6: 23.1.3.5
// Map.prototype.forEach ( callbackfn , thisArg = undefined )
ejsval
_ejs_Map_prototype_forEach (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval callbackfn = _ejs_undefined;
    ejsval thisArg = _ejs_undefined;

    if (argc > 0) callbackfn = args[0];
    if (argc > 1) thisArg = args[1];

    // 1. Let M be the this value.
    ejsval M = _this;

    // 2. If Type(M) is not Object, then throw a TypeError exception.
    if (!EJSVAL_IS_OBJECT(M))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Map.prototype.forEach called with non-object this.");

    // 3. If M does not have a [[MapData]] internal slot throw a TypeError exception.
    if (!EJSVAL_IS_MAP(M))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Map.prototype.forEach called with non-Map this.");

    // 4. If M’s [[MapData]] internal slot is undefined, then throw a TypeError exception.

    // 5. If IsCallable(callbackfn) is false, throw a TypeError exception.
    if (!EJSVAL_IS_FUNCTION(callbackfn))
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
        _ejs_invoke_closure (callbackfn, T, 3, callback_args);
    }

    // 9. Return undefined.
    return _ejs_undefined;
}

// ES6: 23.1.3.6
// Map.prototype.get ( key )
ejsval
_ejs_Map_prototype_get (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval key = _ejs_undefined;
    if (argc > 0) key = args[0];

    // 1. Let M be the this value.
    ejsval M = _this;

    // 2. If Type(M) is not Object, then throw a TypeError exception.
    if (!EJSVAL_IS_OBJECT(M))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Map.prototype.get called with non-object this.");
        
    // 3. If M does not have a [[MapData]] internal slot throw a TypeError exception.
    if (!EJSVAL_IS_MAP(M))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Map.prototype.get called with non-Map this.");

    // 4. If M’s [[MapData]] internal slot is undefined, then throw a TypeError exception.


    EJSMap* _map = EJSVAL_TO_MAP(M);

    // 5. Let entries be the List that is the value of M’s [[MapData]] internal slot.
    EJSKeyValueEntry* entries = _map->head_insert;

    ComparatorFunc same;

    // 6. If M’s [[MapComparator]] internal slot is undefined, then let same be the abstract operation SameValueZero.
    if (EJSVAL_IS_UNDEFINED(_map->comparator))
        same = SameValueZero;
    // 7. Else, let same be the abstract operation SameValue.
    else
        same = SameValue;

    // 8. Repeat for each Record {[[key]], [[value]]} p that is an element of entries,
    for (EJSKeyValueEntry* p = entries; p; p = p->next_insert) {
        //    a. If same(p.[[key]], key), then return p.[[value]].
        if (same (p->key, key))
            return p->value;
    }

    // 9. Return undefined.
    return _ejs_undefined;
}

// ES6: 23.1.3.7
// Map.prototype.has ( key )
ejsval
_ejs_Map_prototype_has (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval key = _ejs_undefined;
    if (argc > 0) key = args[0];

    // 1. Let M be the this value.
    ejsval M = _this;

    // 2. If Type(M) is not Object, then throw a TypeError exception.
    if (!EJSVAL_IS_OBJECT(M))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Map.prototype.has called with non-object this.");

    // 3. If M does not have a [[MapData]] internal slot throw a TypeError exception.
    if (!EJSVAL_IS_MAP(M))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Map.prototype.has called with non-Map this.");

    // 4. If M’s [[MapData]] internal slot is undefined, then throw a TypeError exception.

    EJSMap* _map = EJSVAL_TO_MAP(M);

    // 5. Let entries be the List that is the value of M’s [[MapData]] internal slot.
    EJSKeyValueEntry* entries = _map->head_insert;

    ComparatorFunc same;

    // 6. If M’s [[MapComparator]] internal slot is undefined, then let same be the abstract operation SameValueZero.
    if (EJSVAL_IS_UNDEFINED(_map->comparator))
        same = SameValueZero;
    // 7. Else, let same be the abstract operation SameValue.
    else
        same = SameValue;

    // 8. Repeat for each Record {[[key]], [[value]]} p that is an element of entries,
    for (EJSKeyValueEntry* p = entries; p; p = p->next_insert) {
        //    a. If same(p.[[key]], key), then return true.
        if (same (p->key, key))
            return _ejs_true;
    }

    // 9. Return false.
    return _ejs_false;
}

// ES6: 23.1.3.8
// Map.prototype.keys ( )
ejsval
_ejs_Map_prototype_keys (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    // 1. Let M be the this value.
    ejsval M = _this;

    // 2. Return the result of calling the CreateMapIterator abstract operation with arguments M and "key".
    EJS_NOT_IMPLEMENTED();
}

// ES6: 23.1.3.9
// Map.prototype.set ( key , value )
ejsval
_ejs_Map_prototype_set (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval key = _ejs_undefined;
    ejsval value = _ejs_undefined;

    if (argc > 0) key = args[0];
    if (argc > 1) value = args[1];

    // 1. Let M be the this value.
    ejsval M = _this;

    // 2. If Type(M) is not Object, then throw a TypeError exception.
    if (!EJSVAL_IS_OBJECT(M))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Map.prototype.set called with non-object this.");

    // 3. If M does not have a [[MapData]] internal slot throw a TypeError exception.
    if (!EJSVAL_IS_MAP(M))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Map.prototype.set called with non-Map this.");

    // 4. If M’s [[MapData]] internal slot is undefined, then throw a TypeError exception.

    EJSMap* _map = EJSVAL_TO_MAP(M);

    // 5. Let entries be the List that is the value of M’s [[MapData]] internal slot.
    EJSKeyValueEntry* entries = _map->head_insert;

    ComparatorFunc same;

    // 6. If M’s [[MapComparator]] internal slot is undefined, then let same be the abstract operation SameValueZero.
    if (EJSVAL_IS_UNDEFINED(_map->comparator))
        same = SameValueZero;
    // 7. Else, let same be the abstract operation SameValue.
    else
        same = SameValue;
    
    EJSKeyValueEntry* p;
    // 8. Repeat for each Record {[[key]], [[value]]} p that is an element of entries,
    for (p = entries; p; p = p->next_insert) {
        //    a. If same(p.[[key]], key), then
        if (same (p->key, key)) {
            //       i. Set p.[[value]] to value.
            p->value = value;
            //       ii. Return M.
            return M;
        }
    }
    // 9. Let p be the Record {[[key]]: key, [[value]]: value}.
    p = calloc (1, sizeof (EJSKeyValueEntry));
    p->key = key;
    p->value = value;

    // 10. Append p as the last element of entries.
    if (!_map->head_insert)
        _map->head_insert = p;

    if (_map->tail_insert) {
        _map->tail_insert->next_insert = p;
        _map->tail_insert = p;
    }
    else {
        _map->tail_insert = p;
    }

    // 11. Return M.
    return M;
}

// ES6: 23.1.3.10
// get Map.prototype.size

// ES6: 23.1.3.11
// Map.prototype.values ( )
ejsval
_ejs_Map_prototype_values (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    // 1. Let M be the this value.
    // 2. Return the result of calling the CreateMapIterator abstract operation with arguments M and "value".
    EJS_NOT_IMPLEMENTED();
}

// ES6: 23.1.1.1
// Map (iterable = undefined , comparator = undefined )
static ejsval
_ejs_Map_impl (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval iterable = _ejs_undefined;
    ejsval comparator = _ejs_undefined;

    if (argc > 0) iterable = args[0];
    if (argc > 1) comparator = args[1];

    // 1. Let map be the this value.
    ejsval map = _this;

    if (EJSVAL_IS_UNDEFINED(map)) {
        EJSObject* obj = (EJSObject*)_ejs_gc_new(EJSMap);
        _ejs_init_object (obj, _ejs_Map_prototype, &_ejs_map_specops);
        map = OBJECT_TO_EJSVAL(obj);
    }

    // 2. If Type(map) is not Object then, throw a TypeError exception.
    if (!EJSVAL_IS_OBJECT(map))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Map constructor called with non-object this.");
    
    // 3. If map does not have a [[MapData]] internal slot, then throw a TypeError exception.
    if (!EJSVAL_IS_MAP(map))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Map constructor called with non-Map this.");

    // 4. If map’s [[MapData]] internal slot is not undefined, then throw a TypeError exception.

    ejsval iter;

    // 5. If iterable is not present, let iterable be undefined.
    // 6. If iterable is either undefined or null, then let iter be undefined.
    if (EJSVAL_IS_NULL_OR_UNDEFINED(iterable))
        iter = _ejs_undefined;
    // 7. Else,
    else {
        //    a. Let iter be the result of GetIterator(iterable).
        //    b. ReturnIfAbrupt(iter).
        //    c. Let adder be the result of Get(map, "set").
        //    d. ReturnIfAbrupt(adder).
        //    e. If IsCallable(adder) is false, throw a TypeError Exception.
    }

    // 8. If comparator is not undefined, then
    if (!EJSVAL_IS_UNDEFINED(comparator)) {
        //    a. If comparator is not "is", then throw a RangeError Exception.
        if (!EJSVAL_TO_BOOLEAN(_ejs_op_strict_eq (comparator, _ejs_atom_is)))
            _ejs_throw_nativeerror_utf8 (EJS_RANGE_ERROR, "Map constructor argument 'comparator' must be either undefined or the string 'is'");
            
    }
    EJSMap* _map = EJSVAL_TO_MAP(map);

    // 9. If the value of map’s [[MapData]] internal slot is not undefined, then throw a TypeError exception.
    // 10. Assert: map has not been reentrantly initialized.

    // 11. Set map’s [[MapData]] internal slot to a new empty List.
    _map->head_insert = NULL;
    _map->tail_insert = NULL;

    // 12. Set map’s [[MapComparator]] internal slot to comparator.
    _map->comparator = comparator;

    // 13. If iter is undefined, then return map.
    if (EJSVAL_IS_UNDEFINED(iter))
        return map;

    EJS_NOT_IMPLEMENTED(); // we don't support iterators at all yet
    // 14. Repeat
    //     a. Let next be the result of IteratorStep(iter).
    //     b. ReturnIfAbrupt(next).
    //     c. If next is false, then return NormalCompletion(map).
    //     d. Let nextItem be IteratorValue(next).
    //     e. ReturnIfAbrupt(nextItem).
    //     f. If Type(nextItem) is not Object, then throw a TypeError exception.
    //     g. Let k be the result of Get(nextItem, "0").
    //     h. ReturnIfAbrupt(k).
    //     i. Let v be the result of Get(nextItem, "1").
    //     j. ReturnIfAbrupt(v).
    //     k. Let status be the result of calling the [[Call]] internal method of adder with map as thisArgument and a List whose elements are k and v as argumentsList.
    //     l. ReturnIfAbrupt(status).
}

ejsval _ejs_Map EJSVAL_ALIGNMENT;
ejsval _ejs_Map_prototype EJSVAL_ALIGNMENT;

void
_ejs_map_init(ejsval global)
{
    _ejs_Map = _ejs_function_new_without_proto (_ejs_null, _ejs_atom_Map, (EJSClosureFunc)_ejs_Map_impl);
    _ejs_object_setprop (global, _ejs_atom_Map, _ejs_Map);

    _ejs_gc_add_root (&_ejs_Map_prototype);
    _ejs_Map_prototype = _ejs_object_new(_ejs_null, &_ejs_object_specops); // XXX should be a Map
    _ejs_object_setprop (_ejs_Map,       _ejs_atom_prototype,  _ejs_Map_prototype);

#define OBJ_METHOD(x) EJS_INSTALL_ATOM_FUNCTION(_ejs_Map, x, _ejs_Map_##x)
#define PROTO_METHOD(x) EJS_INSTALL_ATOM_FUNCTION_FLAGS(_ejs_Map_prototype, x, _ejs_Map_prototype_##x, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_WRITABLE | EJS_PROP_CONFIGURABLE)

    PROTO_METHOD(clear);
    // XXX (ES6 23.1.3.2) Map.prototype.constructor
    PROTO_METHOD(delete);
    PROTO_METHOD(entries);
    PROTO_METHOD(forEach);
    PROTO_METHOD(get);
    PROTO_METHOD(has);
    PROTO_METHOD(keys);
    PROTO_METHOD(set);

    // XXX (ES6 23.1.3.10) get Map.prototype.size
    PROTO_METHOD(values);
    // XXX (ES6 23.1.3.12) Map.prototype [ @@iterator ]( )
#undef OBJ_METHOD
#undef PROTO_METHOD
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

    _ejs_object_specops.finalize (obj);
}

static void
_ejs_map_specop_scan (EJSObject* obj, EJSValueFunc scan_func)
{
    EJSMap* map = (EJSMap*)obj;
    scan_func(map->comparator);

    for (EJSKeyValueEntry *s = map->head_insert; s; s = s->next_insert) {
        scan_func (s->key);
        scan_func (s->value);
    }

    _ejs_object_specops.scan (obj, scan_func);
}
