/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include "ejs-weakmap.h"
#include "ejs-map.h"
#include "ejs-array.h"
#include "ejs-gc.h"
#include "ejs-error.h"
#include "ejs-function.h"
#include "ejs-proxy.h"
#include "ejs-ops.h"
#include "ejs-symbol.h"


#define EJSVAL_IS_WEAKMAP(v)     (EJSVAL_IS_OBJECT(v) && (EJSVAL_TO_OBJECT(v)->ops == &_ejs_WeakMap_specops))
#define EJSVAL_TO_WEAKMAP(v)     ((EJSMap*)EJSVAL_TO_OBJECT(v))

static ejsval _ejs_WeakMapData_symbol EJSVAL_ALIGNMENT;

ejsval
_ejs_weakmap_new ()
{
    EJSObject *map = _ejs_gc_new (EJSObject);
    _ejs_init_object (map, _ejs_WeakMap_prototype, &_ejs_WeakMap_specops);

    return OBJECT_TO_EJSVAL(map);
}


// ES6: 23.3.3.2
// WeakMap.prototype.delete ( key )
static EJS_NATIVE_FUNC(_ejs_WeakMap_prototype_delete) {
    ejsval key = _ejs_undefined;
    if (argc > 0) key = args[0];

    // 1. Let M be the this value.
    ejsval M = *_this;

    // 2. If Type(M) is not Object, then throw a TypeError exception.
    if (!EJSVAL_IS_OBJECT(M))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "delete called with non-object this.");

    // 3. If M does not have a [[WeakMapData]] internal slot throw a TypeError exception.
    if (!EJSVAL_IS_WEAKMAP(M))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "delete called with non-WeakMap this.");

    // 4. Let entries be the List that is the value of M’s [[WeakMapData]] internal slot.
    // 5. If entries is undefined, then throw a TypeError exception.
    // 6. If Type(key) is not Object, then return false.
    if (!EJSVAL_IS_OBJECT(key))
        return _ejs_false;

#if WEAK_COLLECTIONS_USE_INVERTED_REP
    ejsval imap = _ejs_object_getprop(key, _ejs_WeakMapData_symbol);
    if (EJSVAL_IS_NULL_OR_UNDEFINED(imap))
        return _ejs_false;

    if (!EJSVAL_IS_MAP(imap))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "[[WeakMapData]] internal error");

    return _ejs_map_delete (imap, M);
#else    
    // 7. Repeat for each Record {[[key]], [[value]]} p that is an element of entries,
    //    a. If p.[[key]] is not empty and SameValue(p.[[key]], key) is true, then
    //       i. Set p.[[key]] to empty.
    //       ii. Set p.[[value]] to empty.
    //       iii. Return true.
    // 8 Return false.
    return _ejs_false;
#endif
}

// ES6: 23.3.3.3
// WeakMap.prototype.get ( key )
static EJS_NATIVE_FUNC(_ejs_WeakMap_prototype_get) {
    ejsval key = _ejs_undefined;
    if (argc > 0) key = args[0];

    // 1. Let M be the this value.
    ejsval M = *_this;

    // 2. If Type(M) is not Object, then throw a TypeError exception.
    if (!EJSVAL_IS_OBJECT(M))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "get called with non-object this.");

    // 3. If M does not have a [[WeakMapData]] internal slot throw a TypeError exception.
    if (!EJSVAL_IS_WEAKMAP(M))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "get called with non-WeakMap this.");

    // 4. Let entries be the List that is the value of M’s [[WeakMapData]] internal slot.
    // 5. If entries is undefined, then throw a TypeError exception.
    // 6. If Type(key) is not Object, then return undefined.
    if (!EJSVAL_IS_OBJECT(key))
        return _ejs_undefined;

#if WEAK_COLLECTIONS_USE_INVERTED_REP
    ejsval imap = _ejs_object_getprop(key, _ejs_WeakMapData_symbol);
    if (EJSVAL_IS_NULL_OR_UNDEFINED(imap))
        return _ejs_undefined;

    if (!EJSVAL_IS_MAP(imap))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "[[WeakMapData]] internal error");

    return _ejs_map_get (imap, M);
#else    
    // 7. Repeat for each Record {[[key]], [[value]]} p that is an element of entries,
    //    a. If p.[[key]] is not empty and SameValue(p.[[key]], key) is true, then return p.[[value]].
    // 8. Return undefined.
#endif
}

// ES6: 23.3.3.4
// WeakMap.prototype.has ( key )
static EJS_NATIVE_FUNC(_ejs_WeakMap_prototype_has) {
    ejsval key = _ejs_undefined;
    if (argc > 0) key = args[0];

    // 1. Let M be the this value.
    ejsval M = *_this;

    // 2. If Type(M) is not Object, then throw a TypeError exception.
    if (!EJSVAL_IS_OBJECT(M))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "has called with non-object this.");

    // 3. If M does not have a [[WeakMapData]] internal slot throw a TypeError exception.
    if (!EJSVAL_IS_WEAKMAP(M))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "has called with non-WeakMap this.");

    // 4. Let entries be the List that is the value of M’s [[WeakMapData]] internal slot.
    // 5. If entries is undefined, then throw a TypeError exception.
    // 6. If Type(key) is not Object, then return false.
    if (!EJSVAL_IS_OBJECT(key))
        return _ejs_false;

#if WEAK_COLLECTIONS_USE_INVERTED_REP
    ejsval imap = _ejs_object_getprop(key, _ejs_WeakMapData_symbol);
    if (EJSVAL_IS_NULL_OR_UNDEFINED(imap))
        return _ejs_false;

    if (!EJSVAL_IS_MAP(imap))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "[[WeakMapData]] internal error");

    return _ejs_map_has (imap, M);
#else    
    // 7. Repeat for each Record {[[key]], [[value]]} p that is an element of entries,
    //    a. If p.[[key]] is not empty and SameValue(p.[[key]], key) is true, then return p.[[value]].
    // 8. Return false.
#endif
}

// ES6: 23.3.3.4
// WeakMap.prototype.set ( key, value )
static EJS_NATIVE_FUNC(_ejs_WeakMap_prototype_set) {
    ejsval key = _ejs_undefined;
    ejsval value = _ejs_undefined;

    if (argc > 0) key = args[0];
    if (argc > 1) value = args[1];

    // 1. Let M be the this value.
    ejsval M = *_this;

    // 2. If Type(M) is not Object, then throw a TypeError exception.
    if (!EJSVAL_IS_OBJECT(M))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "set called with non-object this.");

    // 3. If M does not have a [[WeakMapData]] internal slot throw a TypeError exception.
    if (!EJSVAL_IS_WEAKMAP(M))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "set called with non-WeakMap this.");

    // 4. Let entries be the List that is the value of M’s [[WeakMapData]] internal slot.
    // 5. If entries is undefined, then throw a TypeError exception.
    // 6. If Type(key) is not Object, then return false.
    if (!EJSVAL_IS_OBJECT(key))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "set called with non-Object key.");


#if WEAK_COLLECTIONS_USE_INVERTED_REP
    ejsval imap = _ejs_object_getprop(key, _ejs_WeakMapData_symbol);
    if (EJSVAL_IS_NULL_OR_UNDEFINED(imap)) {
        imap = _ejs_map_new();
        _ejs_object_setprop(key, _ejs_WeakMapData_symbol, imap);
    }

    if (!EJSVAL_IS_MAP(imap))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "[[WeakMapData]] internal error");

    _ejs_map_set (imap, M, value);
    return M;
#else
    // 7. Repeat for each Record {[[key]], [[value]]} p that is an element of entries,
    //    a. If p.[[key]] is not empty and SameValue(p.[[key]], key) is true, then
    //       i. Set p.[[value]] to value.
    //       ii. Return M.
    // 8. Let p be the Record {[[key]]: key, [[value]]: value}.
    // 9. Append p as the last element of entries.
    // 10. Return M.
#endif
}

// ES2015, June 2015
// 23.3.1.1 WeakMap ( [ iterable ] )
static EJS_NATIVE_FUNC(_ejs_WeakMap_impl) {
    ejsval iterable = _ejs_undefined;
    if (argc > 0) iterable = args[0];

    // 1. If NewTarget is undefined, throw a TypeError exception.
    if (EJSVAL_IS_UNDEFINED(newTarget))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "WeakMap constructor must be called with new");

    // 2. Let map be OrdinaryCreateFromConstructor(NewTarget, "%WeakMapPrototype%", «[[WeakMapData]]» ).
    // 3. ReturnIfAbrupt(map).
    // 4. Set map’s [[WeakMapData]] internal slot to a new empty List.
    ejsval map = OrdinaryCreateFromConstructor(newTarget, _ejs_WeakMap_prototype, &_ejs_WeakMap_specops);
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
            _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "WeakMap.prototype.set is not a function");
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

    EJS_NOT_REACHED();
}

ejsval _ejs_WeakMap EJSVAL_ALIGNMENT;
ejsval _ejs_WeakMap_prototype EJSVAL_ALIGNMENT;

void
_ejs_weakmap_init(ejsval global)
{
    _ejs_gc_add_root (&_ejs_WeakMapData_symbol);
    _ejs_WeakMapData_symbol = _ejs_symbol_new(_ejs_atom_WeakMapData);

    _ejs_WeakMap = _ejs_function_new_without_proto (_ejs_null, _ejs_atom_WeakMap, (EJSClosureFunc)_ejs_WeakMap_impl);
    _ejs_object_setprop (global, _ejs_atom_WeakMap, _ejs_WeakMap);

    _ejs_gc_add_root (&_ejs_WeakMap_prototype);
    _ejs_WeakMap_prototype = _ejs_object_new (_ejs_Object_prototype, &_ejs_Object_specops);
    _ejs_object_setprop (_ejs_WeakMap,       _ejs_atom_prototype,  _ejs_WeakMap_prototype);

#define OBJ_METHOD(x) EJS_INSTALL_ATOM_FUNCTION(_ejs_WeakMap, x, _ejs_WeakMap_##x)
#define PROTO_METHOD(x) EJS_INSTALL_ATOM_FUNCTION_FLAGS(_ejs_WeakMap_prototype, x, _ejs_WeakMap_prototype_##x, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_WRITABLE | EJS_PROP_CONFIGURABLE)
#define PROTO_GETTER(x) EJS_INSTALL_ATOM_GETTER(_ejs_WeakMap_prototype, x, _ejs_WeakMap_prototype_get_##x)

    // XXX (ES6 23.3.3.1) WeakMap.prototype.constructor
    PROTO_METHOD(delete);
    PROTO_METHOD(get);
    PROTO_METHOD(has);
    PROTO_METHOD(set);

    _ejs_object_define_value_property (_ejs_WeakMap_prototype, _ejs_Symbol_toStringTag, _ejs_atom_WeakMap, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_WRITABLE | EJS_PROP_CONFIGURABLE);

#undef OBJ_METHOD
#undef PROTO_METHOD
}


static EJSObject*
_ejs_weakmap_specop_allocate()
{
    return (EJSObject*)_ejs_gc_new (EJSWeakMap);
}

EJS_DEFINE_CLASS(WeakMap,
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
                 _ejs_weakmap_specop_allocate,
                 OP_INHERIT, // [[Finalize]]
                 OP_INHERIT  // [[Scan]] XXX?
                 )
