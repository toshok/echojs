/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include "ejs-map.h"
#include "ejs-gc.h"
#include "ejs-error.h"
#include "ejs-function.h"

static ejsval  _ejs_map_specop_get (ejsval obj, ejsval propertyName);
static EJSPropertyDesc* _ejs_map_specop_get_own_property (ejsval obj, ejsval propertyName);
static EJSPropertyDesc* _ejs_map_specop_get_property (ejsval obj, ejsval propertyName);
static void    _ejs_map_specop_put (ejsval obj, ejsval propertyName, ejsval val, EJSBool flag);
static EJSBool _ejs_map_specop_can_put (ejsval obj, ejsval propertyName);
static EJSBool _ejs_map_specop_has_property (ejsval obj, ejsval propertyName);
static EJSBool _ejs_map_specop_delete (ejsval obj, ejsval propertyName, EJSBool flag);
static ejsval  _ejs_map_specop_default_value (ejsval obj, const char *hint);
static EJSBool _ejs_map_specop_define_own_property (ejsval obj, ejsval propertyName, EJSPropertyDesc* propertyDescriptor, EJSBool flag);
static EJSObject* _ejs_map_specop_allocate ();
static void    _ejs_map_specop_finalize (EJSObject* obj);
static void    _ejs_map_specop_scan (EJSObject* obj, EJSValueFunc scan_func);

EJSSpecOps _ejs_map_specops = {
    "Map",
    _ejs_map_specop_get,
    _ejs_map_specop_get_own_property,
    _ejs_map_specop_get_property,
    _ejs_map_specop_put,
    _ejs_map_specop_can_put,
    _ejs_map_specop_has_property,
    _ejs_map_specop_delete,
    _ejs_map_specop_default_value,
    _ejs_map_specop_define_own_property,
    NULL, /* [[HasInstance]] */

    _ejs_map_specop_allocate,
    _ejs_map_specop_finalize,
    _ejs_map_specop_scan
};

// ES6: 23.1.3.1
ejsval
_ejs_Map_prototype_clear (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    // NOTE The existing [[MapData]] List is preserved because there
    // may be existing MapIterator objects that are suspended midway
    // through iterating over that List.

    // 1. Let M be the this value.
    // 2. If Type(M) is not Object, then throw a TypeError exception.
    // 3. If M does not have a [[MapData]] internal slot throw a TypeError exception.
    // 4. If M’s [[MapData]] internal slot is undefined, then throw a TypeError exception.
    // 5. Let entries be the List that is the value of M’s [[MapData]] internal slot.
    // 6. Repeat for each Record {[[key]], [[value]]} p that is an element of entries,
    // 7. Set p.[[key]] to empty.
    // 8. Set p.[[value]] to empty.
    // 9. Return undefined.

    return _ejs_undefined;
}

// ES6: 23.1.3.3
ejsval
_ejs_Map_prototype_delete (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    // NOTE The value empty is used as a specification device to
    // indicate that an entry has been deleted. Actual implementations
    // may take other actions such as physically removing the entry
    // from internal data structures.

    // 1. Let M be the this value.
    // 2. If Type(M) is not Object, then throw a TypeError exception.
    // 3. If M does not have a [[MapData]] internal slot throw a TypeError exception.
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
ejsval
_ejs_Map_prototype_entries (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    // 1. Let M be the this value.
    // 2. If Type(M) is not Object, then throw a TypeError exception.
    // 3. Return the result of calling the CreateMapIterator abstract operation with arguments M and "key+value".
    EJS_NOT_IMPLEMENTED();
}

// ES6: 23.1.3.5
ejsval
_ejs_Map_prototype_forEach (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    EJS_NOT_IMPLEMENTED();
}

// ES6: 23.1.3.6
ejsval
_ejs_Map_prototype_get (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    // 1. Let M be the this value.
    // 2. If Type(M) is not Object, then throw a TypeError exception.
    // 3. If M does not have a [[MapData]] internal slot throw a TypeError exception.
    // 4. If M’s [[MapData]] internal slot is undefined, then throw a TypeError exception.
    // 5. Let entries be the List that is the value of M’s [[MapData]] internal slot.
    // 6. If M’s [[MapComparator]] internal slot is undefined, then let same be the abstract operation SameValueZero.
    // 7. Else, let same be the abstract operation SameValue.
    // 8. Repeat for each Record {[[key]], [[value]]} p that is an element of entries,
    //    a. If same(p.[[key]], key), then return p.[[value]].
    // 9. Return undefined.

    return _ejs_undefined;
}

// ES6: 23.1.3.7
ejsval
_ejs_Map_prototype_has (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    // 1. Let M be the this value.
    // 2. If Type(M) is not Object, then throw a TypeError exception.
    // 3. If M does not have a [[MapData]] internal slot throw a TypeError exception.
    // 4. If M’s [[MapData]] internal slot is undefined, then throw a TypeError exception.
    // 5. Let entries be the List that is the value of M’s [[MapData]] internal slot.
    // 6. If M’s [[MapComparator]] internal slot is undefined, then let same be the abstract operation SameValueZero.
    // 7. Else, let same be the abstract operation SameValue.
    // 8. Repeat for each Record {[[key]], [[value]]} p that is an element of entries,
    //    a. If same(p.[[key]], key), then return true.
    // 9. Return false.
    return _ejs_false;
}

// ES6: 23.1.3.8
ejsval
_ejs_Map_prototype_keys (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    // 1. Let M be the this value.
    // 2. Return the result of calling the CreateMapIterator abstract operation with arguments M and "key".
    EJS_NOT_IMPLEMENTED();
}

// ES6: 23.1.3.9
ejsval
_ejs_Map_prototype_set (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    // 1. Let M be the this value.
    // 2. If Type(M) is not Object, then throw a TypeError exception.
    // 3. If M does not have a [[MapData]] internal slot throw a TypeError exception.
    // 4. If M’s [[MapData]] internal slot is undefined, then throw a TypeError exception.
    // 5. Let entries be the List that is the value of M’s [[MapData]] internal slot.
    // 6. If M’s [[MapComparator]] internal slot is undefined, then let same be the abstract operation SameValueZero.
    // 7. Else, let same be the abstract operation SameValue.
    // 8. Repeat for each Record {[[key]], [[value]]} p that is an element of entries,
    //    a. If same(p.[[key]], key), then
    // i. Set p.[[value]] to value.
    // ii. Return M.
    // 9. Let p be the Record {[[key]]: key, [[value]]: value}.
    // 10. Append p as the last element of entries.
    // 11. Return M.
    EJS_NOT_IMPLEMENTED();
}

// ES6: 23.1.3.11
ejsval
_ejs_Map_prototype_values (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    // 1. Let M be the this value.
    // 2. Return the result of calling the CreateMapIterator abstract operation with arguments M and "value".
    EJS_NOT_IMPLEMENTED();
}

void
_ejs_map_init(ejsval global)
{
    ejsval _ejs_Map = _ejs_object_new (_ejs_null, &_ejs_object_specops);
    _ejs_object_setprop (global, _ejs_atom_Map, _ejs_Map);

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


static ejsval
_ejs_map_specop_get (ejsval obj, ejsval propertyName)
{
    EJS_NOT_IMPLEMENTED();
}

static EJSPropertyDesc*
_ejs_map_specop_get_own_property (ejsval obj, ejsval propertyName)
{
    EJS_NOT_IMPLEMENTED();
}

static EJSPropertyDesc*
_ejs_map_specop_get_property (ejsval obj, ejsval propertyName)
{
    EJS_NOT_IMPLEMENTED();
}

static void
_ejs_map_specop_put (ejsval obj, ejsval propertyName, ejsval val, EJSBool flag)
{
    EJS_NOT_IMPLEMENTED();
}

static EJSBool
_ejs_map_specop_can_put (ejsval obj, ejsval propertyName)
{
    EJS_NOT_IMPLEMENTED();
}

static EJSBool
_ejs_map_specop_has_property (ejsval obj, ejsval propertyName)
{
    EJS_NOT_IMPLEMENTED();
}

static EJSBool
_ejs_map_specop_delete (ejsval obj, ejsval propertyName, EJSBool flag)
{
    EJS_NOT_IMPLEMENTED();
}

static ejsval
_ejs_map_specop_default_value (ejsval obj, const char *hint)
{
    EJS_NOT_IMPLEMENTED();
}

static EJSBool
_ejs_map_specop_define_own_property (ejsval obj, ejsval propertyName, EJSPropertyDesc* propertyDescriptor, EJSBool flag)
{
    EJS_NOT_IMPLEMENTED();
}

static EJSObject*
_ejs_map_specop_allocate ()
{
    EJS_NOT_IMPLEMENTED();
}

static void
_ejs_map_specop_finalize (EJSObject* obj)
{
    EJS_NOT_IMPLEMENTED();
}

static void
_ejs_map_specop_scan (EJSObject* obj, EJSValueFunc scan_func)
{
    EJS_NOT_IMPLEMENTED();
}
