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
ejsval
_ejs_WeakMap_prototype_delete(ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval key = _ejs_undefined;
    if (argc > 0) key = args[0];

    // 1. Let M be the this value.
    ejsval M = _this;

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
ejsval
_ejs_WeakMap_prototype_get(ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval key = _ejs_undefined;
    if (argc > 0) key = args[0];

    // 1. Let M be the this value.
    ejsval M = _this;

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
ejsval
_ejs_WeakMap_prototype_has(ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval key = _ejs_undefined;
    if (argc > 0) key = args[0];

    // 1. Let M be the this value.
    ejsval M = _this;

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
ejsval
_ejs_WeakMap_prototype_set(ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval key = _ejs_undefined;
    ejsval value = _ejs_undefined;

    if (argc > 0) key = args[0];
    if (argc > 1) value = args[1];

    // 1. Let M be the this value.
    ejsval M = _this;

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

// ES6: 23.1.1.1
// Map (iterable = undefined , comparator = undefined )
static ejsval
_ejs_WeakMap_impl (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    // 1. Let map be the this value.
    ejsval map = _this;

    if (EJSVAL_IS_UNDEFINED(map)) {
        EJSObject* obj = (EJSObject*)_ejs_gc_new(EJSWeakMap);
        _ejs_init_object (obj, _ejs_WeakMap_prototype, &_ejs_WeakMap_specops);
        map = OBJECT_TO_EJSVAL(obj);
    }

    // 2. If Type(map) is not Object then, throw a TypeError exception.
    if (!EJSVAL_IS_OBJECT(map))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "WeakMap constructor called with non-object this.");
    
    // 3. If map does not have a [[MapData]] internal slot, then throw a TypeError exception.
    if (!EJSVAL_IS_WEAKMAP(map))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "WeakMap constructor called with non-Map this.");

    return map;
}

static ejsval
_ejs_WeakMap_create (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    // 1. Let F be the this value. 
    ejsval F = _this;

    if (!EJSVAL_IS_CONSTRUCTOR(F)) 
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "'this' in WeakMap[Symbol.create] is not a constructor");

    EJSObject* F_ = EJSVAL_TO_OBJECT(F);

    // 2. Let obj be the result of calling OrdinaryCreateFromConstructor(F, "%WeakMapPrototype%", ([[WeakMapData]]) ). 
    ejsval proto = OP(F_,Get)(F, _ejs_atom_prototype, F);
    if (EJSVAL_IS_UNDEFINED(proto))
        proto = _ejs_WeakMap_prototype;

    EJSObject* obj = (EJSObject*)_ejs_gc_new (EJSWeakMap);
    _ejs_init_object (obj, proto, &_ejs_WeakMap_specops);
    return OBJECT_TO_EJSVAL(obj);
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

    EJS_INSTALL_SYMBOL_FUNCTION_FLAGS (_ejs_WeakMap, create, _ejs_WeakMap_create, EJS_PROP_NOT_ENUMERABLE);

#undef OBJ_METHOD
#undef PROTO_METHOD
}

EJS_DEFINE_INHERIT_ALL_CLASS(WeakMap)
