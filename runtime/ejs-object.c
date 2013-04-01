/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#define DEBUG_PROPERTIES 0
#define DEBUG_GC 0

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "ejs-value.h"
#include "ejs-ops.h"
#include "ejs-object.h"
#include "ejs-number.h"
#include "ejs-string.h"
#include "ejs-regexp.h"
#include "ejs-date.h"
#include "ejs-array.h"
#include "ejs-typedarrays.h"
#include "ejs-function.h"
#include "ejs-error.h"

static ejsval           _ejs_object_specop_get (ejsval obj, ejsval propertyName);
static EJSPropertyDesc* _ejs_object_specop_get_own_property (ejsval obj, ejsval propertyName);
static EJSPropertyDesc* _ejs_object_specop_get_property (ejsval obj, ejsval propertyName);
static void             _ejs_object_specop_put (ejsval obj, ejsval propertyName, ejsval val, EJSBool flag);
static EJSBool          _ejs_object_specop_can_put (ejsval obj, ejsval propertyName);
static EJSBool          _ejs_object_specop_has_property (ejsval obj, ejsval propertyName);
static EJSBool          _ejs_object_specop_delete (ejsval obj, ejsval propertyName, EJSBool flag);
static ejsval           _ejs_object_specop_default_value (ejsval obj, const char *hint);
static EJSBool          _ejs_object_specop_define_own_property (ejsval obj, ejsval propertyName, EJSPropertyDesc* propertyDescriptor, EJSBool flag);
static EJSBool          _ejs_object_specop_enumerate_properties (ejsval obj, ejsval propertyName, EJSPropertyDesc* propertyDescriptor, EJSBool flag);
static EJSObject*       _ejs_object_specop_allocate ();
static void             _ejs_object_specop_finalize (EJSObject* obj);
static void             _ejs_object_specop_scan (EJSObject* obj, EJSValueFunc scan_func);

EJSSpecOps _ejs_object_specops = {
    "Object",
    _ejs_object_specop_get,
    _ejs_object_specop_get_own_property,
    _ejs_object_specop_get_property,
    _ejs_object_specop_put,
    _ejs_object_specop_can_put,
    _ejs_object_specop_has_property,
    _ejs_object_specop_delete,
    _ejs_object_specop_default_value,
    _ejs_object_specop_define_own_property,
    NULL, /* [[HasInstance]] */

    _ejs_object_specop_allocate,
    _ejs_object_specop_finalize,
    _ejs_object_specop_scan
};


// ECMA262: 8.10.1
static EJSBool
IsAccessorDescriptor(EJSPropertyDesc* Desc)
{
    /* 1. If Desc is undefined, then return false. */
    if (!Desc)
        return EJS_FALSE;

    /* 2. If both Desc.[[Get]] and Desc.[[Set]] are absent, then return false. */
    if (!_ejs_property_desc_has_getter(Desc) && !_ejs_property_desc_has_setter(Desc))
        return EJS_FALSE;

    /* 3. Return true. */
    return EJS_TRUE;
}

// ECMA262: 8.10.2
static EJSBool
IsDataDescriptor(EJSPropertyDesc* Desc)
{
    /* 1. If Desc is undefined, then return false. */
    if (!Desc)
        return EJS_FALSE;

    /* 2. If both Desc.[[Value]] and Desc.[[Writable]] are absent, then return false. */
    if (!_ejs_property_desc_has_value(Desc) && !_ejs_property_desc_has_writable(Desc))
        return EJS_FALSE;

    /* 3. Return true. */
    return EJS_TRUE;
}

// ECMA262: 8.10.3
static EJSBool
IsGenericDescriptor(EJSPropertyDesc* Desc)
{
    /* 1. If Desc is undefined, then return false. */
    if (!Desc)
        return EJS_FALSE;

    /* 2. If IsAccessorDescriptor(Desc) and IsDataDescriptor(Desc) are both false, then return true */
    if (!IsAccessorDescriptor(Desc) && !IsDataDescriptor(Desc))
        return EJS_TRUE;

    /* 3. Return false. */
    return EJS_FALSE;
}

// ECMA262: 8.10.5
static void
ToPropertyDescriptor(ejsval O, EJSPropertyDesc *desc)
{
    /* 1. If Type(Obj) is not Object throw a TypeError exception. */
    if (!EJSVAL_IS_OBJECT(O)) {
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Object.defineProperty called on non-object");
    }
    EJSObject* obj = EJSVAL_TO_OBJECT(O);
    memset (desc, 0, sizeof(EJSPropertyDesc));

    /* 2. Let desc be the result of creating a new Property Descriptor that initially has no fields. */

    /* 3. If the result of calling the [[HasProperty]] internal method of Obj with argument "enumerable" is true, then */
    if (OP(obj,has_property)(O, _ejs_atom_enumerable)) {
        /*    a. Let enum be the result of calling the [[Get]] internal method of Obj with "enumerable". */
        /*    b. Set the [[Enumerable]] field of desc to ToBoolean(enum). */
        _ejs_property_desc_set_enumerable (desc, EJSVAL_TO_BOOLEAN(ToBoolean(OP(obj,get)(O, _ejs_atom_enumerable))));
    }
    /* 4. If the result of calling the [[HasProperty]] internal method of Obj with argument "configurable" is true, then */
    if (OP(obj,has_property)(O, _ejs_atom_configurable)) {
        /*    a. Let conf  be the result of calling the [[Get]] internal method of Obj with argument "configurable". */
        /*    b. Set the [[Configurable]] field of desc to ToBoolean(conf). */
        _ejs_property_desc_set_configurable (desc, EJSVAL_TO_BOOLEAN(ToBoolean(OP(obj,get)(O, _ejs_atom_configurable))));
    }
    /* 5. If the result of calling the [[HasProperty]] internal method of Obj with argument "value" is true, then */
    if (OP(obj,has_property)(O, _ejs_atom_value)) {
        /*    a. Let value be the result of calling the [[Get]] internal method of Obj with argument "value". */
        /*    b. Set the [[Value]] field of desc to value. */
        _ejs_property_desc_set_value (desc, OP(obj,get)(O, _ejs_atom_value));
    }
    /* 6. If the result of calling the [[HasProperty]] internal method of Obj with argument "writable" is true, then */
    if (OP(obj,has_property)(O, _ejs_atom_writable)) {
        /*    a. Let writable be the result of calling the [[Get]] internal method of Obj with argument "writable". */
        /*    b. Set the [[Writable]] field of desc to ToBoolean(writable). */
        _ejs_property_desc_set_writable (desc, EJSVAL_TO_BOOLEAN(ToBoolean(OP(obj,get)(O, _ejs_atom_writable))));
    }
    /* 7. If the result of calling the [[HasProperty]] internal method of Obj with argument "get" is true, then */
    if (OP(obj,has_property)(O, _ejs_atom_get)) {
        /*    a. Let getter be the result of calling the [[Get]] internal method of Obj with argument "get". */
        ejsval getter = OP(obj,get)(O, _ejs_atom_get);

        /*    b. If IsCallable(getter) is false and getter is not undefined, then throw a TypeError exception. */
        if (!EJSVAL_IS_FUNCTION(getter) && !EJSVAL_IS_UNDEFINED(getter)) {
            _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Getter must be a function");
        }

        /*    c. Set the [[Get]] field of desc to getter. */
        _ejs_property_desc_set_getter (desc, getter);
    }
    /* 8. If the result of calling the [[HasProperty]] internal method of Obj with argument "set" is true, then */
    if (OP(obj,has_property)(O, _ejs_atom_set)) {
        /*    a. Let setter be the result of calling the [[Get]] internal method of Obj with argument "set". */
        ejsval setter = OP(obj,get)(O, _ejs_atom_set);

        /*    b. If IsCallable(setter) is false and setter is not undefined, then throw a TypeError exception. */
        if (!EJSVAL_IS_FUNCTION(setter) && !EJSVAL_IS_UNDEFINED(setter)) {
            _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Setter must be a function");
        }

        /*    c. Set the [[Set]] field of desc to setter. */
        _ejs_property_desc_set_setter (desc, setter);
    }
    /* 9. If either desc.[[Get]] or desc.[[Set]] are present, then */
    if (_ejs_property_desc_has_getter(desc) || _ejs_property_desc_has_setter(desc)) {
        /*    a. If either desc.[[Value]] or desc.[[Writable]] are present, then throw a TypeError exception. */
        if (_ejs_property_desc_has_value(desc) || _ejs_property_desc_has_writable(desc)) {
            _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Invalid property.  A property cannot both have accessors and be writable or have a value");
        }
    }

    /* 10. Return desc. */
}

// ECMA262: 8.10.4
static ejsval
FromPropertyDescriptor(EJSPropertyDesc* Desc)
{
    /* 1. If Desc is undefined, then return undefined. */
    if (!Desc)
        return _ejs_undefined;

    /* 2. Let obj be the result of creating  a new object as if by the expression new Object() where Object  is the standard  */
    /*    built-in constructor with that name. */
    ejsval obj = _ejs_object_new(_ejs_Object_prototype, &_ejs_object_specops);
    EJSObject* obj_ = EJSVAL_TO_OBJECT(obj);

    /* 3. If IsDataDescriptor(Desc) is true, then */
    if (IsDataDescriptor(Desc)) {
        /*    a. Call the [[DefineOwnProperty]] internal method of obj with arguments "value", Property Descriptor  */
        /*       {[[Value]]: Desc.[[Value]], [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: true}, and false. */
        EJSPropertyDesc value_desc = { .value= Desc->value, .flags = EJS_PROP_FLAGS_VALUE_SET | EJS_PROP_WRITABLE | EJS_PROP_ENUMERABLE | EJS_PROP_CONFIGURABLE };

        OP(obj_, define_own_property)(obj, _ejs_atom_value, &value_desc, EJS_FALSE);

        /*    b. Call the [[DefineOwnProperty]] internal method of obj with arguments "writable", Property Descriptor  */
        /*       {[[Value]]: Desc.[[Writable]], [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: true}, and false. */
        EJSPropertyDesc writable_desc = { .value= BOOLEAN_TO_EJSVAL(_ejs_property_desc_is_writable(Desc)), .flags = EJS_PROP_FLAGS_VALUE_SET | EJS_PROP_WRITABLE | EJS_PROP_ENUMERABLE | EJS_PROP_CONFIGURABLE };

        OP(obj_, define_own_property)(obj, _ejs_atom_writable, &writable_desc, EJS_FALSE);
    }
    else {
        /* 4. Else, IsAccessorDescriptor(Desc) must be true, so */
        /*    a. Call the [[DefineOwnProperty]] internal method of obj with arguments "get", Property Descriptor */
        /*       {[[Value]]: Desc.[[Get]], [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: true}, and false. */
        EJSPropertyDesc get_desc = { .value= _ejs_property_desc_get_getter(Desc), .flags = EJS_PROP_FLAGS_VALUE_SET | EJS_PROP_WRITABLE | EJS_PROP_ENUMERABLE | EJS_PROP_CONFIGURABLE };

        OP(obj_, define_own_property)(obj, _ejs_atom_get, &get_desc, EJS_FALSE);

        /*    b. Call the [[DefineOwnProperty]] internal method of obj with arguments "set", Property Descriptor */
        /*       {[[Value]]: Desc.[[Set]], [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: true}, and false. */
        EJSPropertyDesc set_desc = { .value= _ejs_property_desc_get_setter(Desc), .flags = EJS_PROP_FLAGS_VALUE_SET | EJS_PROP_WRITABLE | EJS_PROP_ENUMERABLE | EJS_PROP_CONFIGURABLE };

        OP(obj_, define_own_property)(obj, _ejs_atom_set, &set_desc, EJS_FALSE);
    }
    /* 5. Call the [[DefineOwnProperty]] internal method of obj with arguments "enumerable", Property Descriptor */
    /*    {[[Value]]: Desc.[[Enumerable]], [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: true}, and false. */
    EJSPropertyDesc enumerable_desc = { .value= BOOLEAN_TO_EJSVAL(_ejs_property_desc_is_enumerable(Desc)), .flags = EJS_PROP_FLAGS_VALUE_SET | EJS_PROP_WRITABLE | EJS_PROP_ENUMERABLE | EJS_PROP_CONFIGURABLE };
    OP(obj_, define_own_property)(obj, _ejs_atom_enumerable, &enumerable_desc, EJS_FALSE);

    /* 6. Call the [[DefineOwnProperty]] internal method of obj with arguments "configurable", Property Descriptor */
    /*    {[[Value]]: Desc.[[Configurable]], [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: true}, and false. */
    EJSPropertyDesc configurable_desc = { .value= BOOLEAN_TO_EJSVAL(_ejs_property_desc_is_configurable(Desc)), .flags = EJS_PROP_FLAGS_VALUE_SET | EJS_PROP_WRITABLE | EJS_PROP_ENUMERABLE | EJS_PROP_CONFIGURABLE };
    OP(obj_, define_own_property)(obj, _ejs_atom_configurable, &configurable_desc, EJS_FALSE);

    /* 7. Return obj. */
    return obj;
}

void
_ejs_propertymap_init (EJSPropertyMap *map, int initial_allocation)
{
    if (initial_allocation) {
        map->names = (jschar**)malloc(sizeof(jschar*) * initial_allocation);
        map->properties = (EJSPropertyDesc*)calloc(sizeof (EJSPropertyDesc), initial_allocation);
    }
    else {
        map->names = NULL;
        map->properties = NULL;
    }
    map->num = 0;
    map->allocated = initial_allocation;
}

void
_ejs_propertymap_free (EJSPropertyMap *map)
{
    for (int i = 0; i < map->num; i ++) {
        free (map->names[i]);
    }
    free (map->names);
    free (map->properties);
}

void
_ejs_propertymap_foreach_value (EJSPropertyMap* map, EJSValueFunc foreach_func)
{
    for (int i = 0; i < map->num; i ++) {
        EJSPropertyDesc *desc = &map->properties[i];
        if (!_ejs_property_desc_has_value (desc))
            continue;
        foreach_func (desc->value);
    }
}

void
_ejs_propertymap_foreach_property (EJSPropertyMap* map, EJSPropertyDescFunc foreach_func, void* data)
{
    for (int i = 0; i < map->num; i ++) {
        jschar* name = map->names[i];
        EJSPropertyDesc *desc = &map->properties[i];

        foreach_func (name, desc, data);
    }
}

void
_ejs_propertymap_delete_named (EJSPropertyMap *map, const char *name)
{
    EJS_NOT_IMPLEMENTED();
}

void
_ejs_propertymap_delete_desc (EJSPropertyMap *map, EJSPropertyDesc *desc)
{
    int i;
    for (i = 0; i < map->num; i ++) {
        if (desc == &map->properties[i]) {
            if (i < map->num - 1) {
                memmove (&map->names[i], &map->names[i+1], (map->num - (i+1)) * sizeof(char*));
                memmove (&map->properties[i], &map->properties[i+1], (map->num - (i + 1)) * sizeof(EJSPropertyDesc*));
            }
            map->num--;
            return;
        }
    }
    EJS_NOT_IMPLEMENTED();
}

int
_ejs_propertymap_lookup (EJSPropertyMap *map, const jschar *name, EJSBool add_if_not_found)
{
    int i;
    for (i = 0; i < map->num; i ++) {
        if (!ucs2_strcmp (map->names[i], name))
            return i;
    }
  
    int idx = -1;
    if (add_if_not_found) {
        idx = map->num++;
        if (map->allocated == 0 || idx == map->allocated - 1) {
            int new_allocated = map->allocated + 10;

            jschar **new_names = (jschar**)malloc(sizeof(jschar*) * new_allocated);
            EJSPropertyDesc* new_properties = (EJSPropertyDesc*)calloc (sizeof(EJSPropertyDesc), new_allocated);

            memmove (new_names, map->names, map->allocated * sizeof(jschar*));
            memmove (new_properties, map->properties, map->allocated * sizeof(EJSPropertyDesc));

            if (map->names)
                free (map->names);
            if (map->properties)
                free (map->properties);

            map->names = new_names;
            map->properties = new_properties;

            map->allocated = new_allocated;
        }
        map->names[idx] = ucs2_strdup (name);

#if DEBUG_PROPMAP
        printf ("after inserting item, map %p is %d long:\n", map, map->num);
        for (i = 0; i < map->num; i ++) {
            printf ("  [%d]: %p %s\n", i, map->names[i], map->names[i]);
        }
#endif
    }

    return idx;
}

/* property iterators */
struct _EJSPropertyIterator {
    EJSObject obj;

    ejsval forObj;
    ejsval *keys;
    int num;
    int current;
};

static EJSObject*
_ejs_property_iterator_specop_allocate ()
{
    return (EJSObject*)_ejs_gc_new(EJSPropertyIterator);
}

static void
_ejs_property_iterator_specop_finalize (EJSObject* obj)
{
    // nothing to do here, we've already been destroyed inline by the generated code.
}

static void
_ejs_property_iterator_specop_scan (EJSObject* obj, EJSValueFunc scan_func)
{
    EJSPropertyIterator *iter = (EJSPropertyIterator*)obj;

    for (int i = 0; i < iter->num; i ++) {
        scan_func (iter->keys[i]);
    }
}

static EJSSpecOps _ejs_property_iterator_specops = {
    "<EJSPropertyIterator>",
    NULL, NULL, NULL,
    NULL, NULL, NULL,
    NULL, NULL, NULL,
    NULL,

    _ejs_property_iterator_specop_allocate,
    _ejs_property_iterator_specop_finalize,
    _ejs_property_iterator_specop_scan
};

static EJSBool
name_in_keys (jschar* name, jschar **keys, int num)
{
    for (int i = 0; i < num; i ++) {
        if (!ucs2_strcmp(name, keys[i]))
            return EJS_TRUE;
    }
    return EJS_FALSE;
}

static void
collect_keys (ejsval objval, int *num, int *alloc, jschar ***keys)
{
    if (!EJSVAL_IS_OBJECT(objval))
        return;

    EJSObject *obj = EJSVAL_TO_OBJECT(objval);
    EJSPropertyMap *map = &obj->map;

    if (map) {
        for (int i = 0; i < map->num; i ++) {
            if (_ejs_property_desc_is_enumerable (&map->properties[i]) && !name_in_keys (map->names[i], *keys, *num)) {
                if (*num == *alloc-1) {
                    // we need to reallocate
                    (*alloc) += 10;
                    *keys = (jschar**)realloc (*keys, (*alloc) * sizeof(jschar*));
                }
                (*keys)[(*num)++] = map->names[i]; // XXX do we need to dup here?
            }
        }
    }

    collect_keys (obj->proto, num, alloc, keys);
}

ejsval
_ejs_property_iterator_new (ejsval forVal)
{
    // the iterator-using code for for..in should handle a null iterator return value,
    // which would save us this allocation.
    ejsval iter = _ejs_object_new (_ejs_null, &_ejs_property_iterator_specops);
    EJSPropertyIterator* iterator = (EJSPropertyIterator*)EJSVAL_TO_OBJECT(iter);

    iterator->current = -1;

    if (EJSVAL_IS_PRIMITIVE(forVal) || EJSVAL_IS_NULL(forVal) || EJSVAL_IS_UNDEFINED(forVal)) {
        iterator->num = 0;
        return OBJECT_TO_EJSVAL(iterator);
    }

    if (EJSVAL_IS_ARRAY(forVal)) {
        int num_keys = EJS_ARRAY_LEN(forVal);

        // iterate over array keys first then additional properties
        iterator->forObj = forVal;
        iterator->num = 0;
        iterator->keys = (ejsval*)malloc(sizeof(ejsval) * num_keys);
        for (int i = 0; i < num_keys; i ++) {
            iterator->keys[iterator->num++] = _ejs_number_new(i);
        }
        return iter;
    }
    else if (EJSVAL_IS_OBJECT(forVal)) {

        int num, alloc;
        jschar** keys;

        num = 0;
        alloc = 10;
        keys = (jschar**)malloc (alloc * sizeof(jschar*));

        collect_keys (forVal, &num, &alloc, &keys);

        iterator->forObj = forVal;
        iterator->num = 0;
        iterator->keys = (ejsval*)malloc(sizeof(ejsval) * num);
        for (int i = 0; i < num; i ++) {
            iterator->keys[iterator->num++] = _ejs_string_new_ucs2(keys[i]);
        }
        free (keys);

        return iter;
    }
    else {
        EJS_NOT_IMPLEMENTED();
    }
}

ejsval
_ejs_property_iterator_current (ejsval iter)
{
    EJSPropertyIterator* iterator = (EJSPropertyIterator*)EJSVAL_TO_OBJECT(iter);
    if (iterator->current == -1) {
        printf ("_ejs_property_iterator_current called before _ejs_property_iterator_next\n");
        abort();
    }

    if (iterator->current >= iterator->num) {
        // FIXME runtime error
        EJS_NOT_IMPLEMENTED();
    }
    return iterator->keys[iterator->current];
}

EJSBool
_ejs_property_iterator_next (ejsval iter, EJSBool free_on_end)
{
    EJSPropertyIterator* iterator = (EJSPropertyIterator*)EJSVAL_TO_OBJECT(iter);
    iterator->current ++;
    if (iterator->current == iterator->num) {
        if (free_on_end)
            _ejs_property_iterator_free (iter);
        return EJS_FALSE;
    }
    return EJS_TRUE;
}

void
_ejs_property_iterator_free (ejsval iter)
{
    EJSPropertyIterator* iterator = (EJSPropertyIterator*)EJSVAL_TO_OBJECT(iter);
    free (iterator->keys);
}


///

void
_ejs_init_object (EJSObject* obj, ejsval proto, EJSSpecOps *ops)
{
    obj->proto = proto;
    obj->ops = ops ? ops : &_ejs_object_specops;
    _ejs_propertymap_init (&obj->map, obj->ops == &_ejs_object_specops ? 5 : 0);
    EJS_OBJECT_SET_EXTENSIBLE(obj);
#if notyet
    ((GCObjectPtr)obj)->gc_data = 0x01; // HAS_FINALIZE
#endif
}

ejsval
_ejs_object_new (ejsval proto, EJSSpecOps *ops)
{
    EJSObject *obj = ops->allocate();
    _ejs_init_object (obj, proto, ops);
    return OBJECT_TO_EJSVAL(obj);
}

ejsval
_ejs_object_create (ejsval proto)
{
    if (EJSVAL_IS_NULL(proto)) proto = _ejs_Object_prototype;

    EJSSpecOps *ops;

    if      (EJSVAL_EQ(proto, _ejs_Object_prototype)) ops = &_ejs_object_specops;
    else if (EJSVAL_EQ(proto, _ejs_Array_proto))      ops = &_ejs_array_specops;
    else if (EJSVAL_EQ(proto, _ejs_String_prototype)) ops = &_ejs_string_specops;
    else if (EJSVAL_EQ(proto, _ejs_Number_proto))     ops = &_ejs_number_specops;
    else if (EJSVAL_EQ(proto, _ejs_RegExp_proto))     ops = &_ejs_regexp_specops;
    else if (EJSVAL_EQ(proto, _ejs_Date_proto))       ops = &_ejs_date_specops;
    else if (EJSVAL_EQ(proto, _ejs_ArrayBuffer_proto)) ops = &_ejs_arraybuffer_specops;
    else if (EJSVAL_EQ(proto, _ejs_EvalError_proto))  ops = &_ejs_error_specops;
    else if (EJSVAL_EQ(proto, _ejs_RangeError_proto))  ops = &_ejs_error_specops;
    else if (EJSVAL_EQ(proto, _ejs_ReferenceError_proto))  ops = &_ejs_error_specops;
    else if (EJSVAL_EQ(proto, _ejs_SyntaxError_proto))  ops = &_ejs_error_specops;
    else if (EJSVAL_EQ(proto, _ejs_TypeError_proto))  ops = &_ejs_error_specops;
    else if (EJSVAL_EQ(proto, _ejs_URIError_proto))  ops = &_ejs_error_specops;
    else if (EJSVAL_EQ(proto, _ejs_Error_proto))  ops = &_ejs_error_specops;
    else                                              ops = EJSVAL_TO_OBJECT(proto)->ops;

    return _ejs_object_new (proto, ops);
}

ejsval
_ejs_number_new (double value)
{
    return NUMBER_TO_EJSVAL(value);
}

ejsval
_ejs_object_setprop (ejsval val, ejsval key, ejsval value)
{
    if (EJSVAL_IS_PRIMITIVE(val)) {
        LOG ("setprop on primitive.  ignoring\n");
        EJS_NOT_IMPLEMENTED();
    }

    if (EJSVAL_IS_ARRAY(val)) {
        // check if key is a uint32, or a string that we can convert to an uint32
        int idx = -1;
        if (EJSVAL_IS_NUMBER(key)) {
            double n = EJSVAL_TO_NUMBER(key);
            if (floor(n) == n) {
                idx = (int)n;
            }
        }

        if (idx != -1) {
            if (idx >= EJS_ARRAY_ALLOC(val)) {
                int new_alloc = idx + 10;
                ejsval* new_elements = (ejsval*)malloc (sizeof(ejsval*) * new_alloc);
                memmove (new_elements, EJS_ARRAY_ELEMENTS(val), EJS_ARRAY_ALLOC(val) * sizeof(ejsval));
                free (EJS_ARRAY_ELEMENTS(val));
                EJS_ARRAY_ELEMENTS(val) = new_elements;
                EJS_ARRAY_ALLOC(val) = new_alloc;
            }
            EJS_ARRAY_ELEMENTS(val)[idx] = value;
            EJS_ARRAY_LEN(val) = idx + 1;
            return value;
        }
        // if we fail there, we fall back to the object impl below
    }

    ejsval real_key = ToString(key);

    // this should be:
    OP(EJSVAL_TO_OBJECT(val), put)(val, real_key, value, EJS_FALSE);


    return value;
}

#if DEBUG_LAST_LOOKUP
jschar* last_lookup = NULL;
#endif

ejsval
_ejs_object_getprop (ejsval obj, ejsval key)
{
    if (EJSVAL_IS_NULL(obj) || EJSVAL_IS_UNDEFINED(obj)) {
        char* key_utf8 = ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(ToString(key)));
#if DEBUG_LAST_LOOKUP
        if (last_lookup) {
            char *last_utf8 = ucs2_to_utf8(last_lookup);
            fprintf (stderr, "last property lookup was for: %s\n", last_utf8);
            free (last_utf8);
        }
#endif
        char msg_buf[256];
        snprintf (msg_buf, sizeof(msg_buf)-1, "Cannot read property '%s' of %s", key_utf8, EJSVAL_IS_NULL(obj) ? "null" : "undefined");
        free (key_utf8);

        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, msg_buf);
    }

    if (EJSVAL_IS_PRIMITIVE(obj)) {
        obj = ToObject(obj);
    }

#if DEBUG_LAST_LOOKUP
    if (last_lookup) free (last_lookup);
    last_lookup = ucs2_strdup (EJSVAL_TO_FLAT_STRING(ToString(key)));
#endif

    return OP(EJSVAL_TO_OBJECT(obj),get)(obj, key);
}

EJSBool
_ejs_object_define_value_property (ejsval obj, ejsval key, ejsval value, uint32_t flags)
{
    EJSObject *_obj = EJSVAL_TO_OBJECT(obj);
    EJSPropertyDesc desc = { .value = value, .flags = flags | EJS_PROP_FLAGS_VALUE_SET };
    return OP(_obj,define_own_property)(obj, key, &desc, EJS_FALSE);
}

EJSBool
_ejs_object_define_accessor_property (ejsval obj, ejsval key, ejsval get, ejsval set, uint32_t flags)
{
    EJSObject *_obj = EJSVAL_TO_OBJECT(obj);
    EJSPropertyDesc desc = { .value = _ejs_undefined, .getter = get, .setter = set, .flags = flags | EJS_PROP_FLAGS_SETTER_SET | EJS_PROP_FLAGS_GETTER_SET };
    return OP(_obj,define_own_property)(obj, key, &desc, EJS_FALSE);
}


ejsval
_ejs_object_setprop_utf8 (ejsval val, const char *key, ejsval value)
{
    if (EJSVAL_IS_NULL(val) || EJSVAL_IS_UNDEFINED(val)) {
        printf ("throw ReferenceError\n");
        abort();
    }

    if (EJSVAL_IS_PRIMITIVE(val)) {
        printf ("setprop on primitive.  ignoring\n");
        return value;
    }

    return _ejs_object_setprop (val, _ejs_string_new_utf8(key), value);
}

ejsval
_ejs_object_getprop_utf8 (ejsval obj, const char *key)
{
    if (EJSVAL_IS_NULL(obj) || EJSVAL_IS_UNDEFINED(obj)) {
        printf ("throw TypeError, key is %s\n", key);
        EJS_NOT_IMPLEMENTED();
    }

    if (EJSVAL_IS_PRIMITIVE(obj)) {
        obj = ToObject(obj);
    }

    return _ejs_object_getprop (obj, _ejs_string_new_utf8(key));
}

void
_ejs_dump_value (ejsval val)
{
    if (EJSVAL_IS_UNDEFINED(val)) {
        printf ("undefined\n");
    }
    else if (EJSVAL_IS_NUMBER(val)) {
        printf ("number: " EJS_NUMBER_FORMAT "\n", EJSVAL_TO_NUMBER(val));
    }
    else if (EJSVAL_IS_BOOLEAN(val)) {
        printf ("boolean: %s\n", EJSVAL_TO_BOOLEAN(val) ? "true" : "false");
    }
    else if (EJSVAL_IS_STRING(val)) {
        char* val_utf8 = ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(val));
        printf ("string: '%s'\n", val_utf8);
        free (val_utf8);
    }
    else if (EJSVAL_IS_OBJECT(val)) {
        printf ("<object %s>\n", CLASSNAME(EJSVAL_TO_OBJECT(val)));
    }
}

ejsval _ejs_Object;
ejsval _ejs_Object__proto__;
ejsval _ejs_Object_prototype;

static ejsval
_ejs_Object_impl (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    if (EJSVAL_IS_UNDEFINED(_this)) {
        // ECMA262: 15.2.1.1
    
        printf ("called Object() as a function!\n");
        return _ejs_null;
    }
    else {
        // ECMA262: 15.2.2

        printf ("called Object() as a constructor!\n");
        return _ejs_null;
    }
}

// ECMA262: 15.2.3.2
static ejsval
_ejs_Object_getPrototypeOf (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval obj = _ejs_undefined;
    if (argc > 0)
        obj = args[0];
    
    /* 1. If Type(O) is not Object throw a TypeError exception. */
    if (!EJSVAL_IS_OBJECT(obj)) {
        printf ("throw TypeError, _this isn't an Object\n");
        EJS_NOT_IMPLEMENTED();
    }
    
    /* 2. Return the value of the [[Prototype]] internal property of O. */
    return EJSVAL_TO_OBJECT(obj)->proto;
}

// ECMA262: 15.2.3.3
static ejsval
_ejs_Object_getOwnPropertyDescriptor (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval O = _ejs_undefined;
    ejsval P = _ejs_undefined;

    if (argc > 0) O = args[0];
    if (argc > 1) P = args[1];

    /* 1. If Type(O) is not Object throw a TypeError exception. */
    if (!EJSVAL_IS_OBJECT(O)) {
        printf ("throw TypeError, O isn't an Object\n");
        EJS_NOT_IMPLEMENTED();
    }
    EJSObject *obj = EJSVAL_TO_OBJECT(O);

    /* 2. Let name be ToString(P). */
    ejsval name = ToString(P);

    /* 3. Let desc be the result of calling the [[GetOwnProperty]] internal method of O with argument name. */
    EJSPropertyDesc* desc = OP(obj, get_own_property)(O, name);

    /* 4. Return the result of calling FromPropertyDescriptor(desc) (8.10.4).  */
    return FromPropertyDescriptor(desc);
}

// ECMA262: 15.2.3.4
static ejsval
_ejs_Object_getOwnPropertyNames (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval obj = _ejs_undefined;
    if (argc > 0)
        obj = args[0];

    /* 1. If Type(O) is not Object throw a TypeError exception. */
    if (!EJSVAL_IS_OBJECT(obj)) {
        printf ("throw TypeError, _this isn't an Object\n");
        EJS_NOT_IMPLEMENTED();
    }
    EJSObject* O = EJSVAL_TO_OBJECT(obj);
    /* 2. Let array be the result of creating a new object as if by the expression new Array () where Array is the 
          standard built-in constructor with that name. */
    ejsval arr = _ejs_array_new(O->map.num);
    EJSArray* array = (EJSArray*)EJSVAL_TO_OBJECT(arr);
    /* 3. Let n be 0. */
    int n = 0;
    /* 4. For each named own property P of O */
    while (n < O->map.num) {
        /*    a. Let name be the String value that is the name of P. */
        jschar* name = O->map.names[n];
        ejsval propName = _ejs_string_new_ucs2(name);
        /*    b. Call the [[DefineOwnProperty]] internal method of array with arguments ToString(n), the
                 PropertyDescriptor {[[Value]]: name, [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: 
                 true}, and false. */
        EJSARRAY_ELEMENTS(array)[n] = propName;
        /*    c. Increment n by 1. */
        ++n;
    }
    /* 5. Return array. */

    return arr;
}

static ejsval _ejs_Object_defineProperties (ejsval env, ejsval _this, uint32_t argc, ejsval *args);

// ECMA262: 15.2.3.5
/* Object.create ( O [, Properties] ) */
static ejsval
_ejs_Object_create (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval O = _ejs_undefined;
    ejsval Properties = _ejs_undefined;
    if (argc > 0) O = args[0];
    if (argc > 1) Properties = args[1];

    /* 1. If Type(O) is not Object or Null throw a TypeError exception. */
    if (!EJSVAL_IS_OBJECT_OR_NULL(O)) {
        printf ("throw TypeError, O isn't an Object or null\n");
        EJS_NOT_IMPLEMENTED();
    }

    /* 2. Let obj be the result of creating a new object as if by the expression new Object() where Object is the  */
    /*    standard built-in constructor with that name */
    ejsval obj = _ejs_object_new(_ejs_null, &_ejs_object_specops);

    /* 3. Set the [[Prototype]] internal property of obj to O. */
    EJSVAL_TO_OBJECT(obj)->proto = O;

    /* 4. If the argument Properties is present and not undefined, add own properties to obj as if by calling the  */
    /*    standard built-in function Object.defineProperties with arguments obj and Properties. */
    ejsval definePropertyArgs[] = { obj, Properties };
    _ejs_Object_defineProperties (env, _this, 2, definePropertyArgs);

    /* 5. Return obj. */
    return obj;
}

// ECMA262: 15.2.3.6
static ejsval
_ejs_Object_defineProperty (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval O = _ejs_undefined;
    ejsval P = _ejs_undefined;
    ejsval Attributes = _ejs_undefined;
    if (argc > 0) O = args[0];
    if (argc > 1) P = args[1];
    if (argc > 2) Attributes = args[2];

    /* 1. If Type(O) is not Object throw a TypeError exception. */
    if (!EJSVAL_IS_OBJECT(O)) {
        printf ("throw TypeError, _this isn't an Object\n");
        EJS_NOT_IMPLEMENTED();
    }
    EJSObject *obj = EJSVAL_TO_OBJECT(O);

    /* 2. Let name be ToString(P). */
    ejsval name = ToString(P);

    /* 3. Let desc be the result of calling ToPropertyDescriptor with Attributes as the argument. */
    EJSPropertyDesc desc;
    ToPropertyDescriptor(Attributes, &desc);

    /* 4. Call the [[DefineOwnProperty]] internal method of O with arguments name, desc, and true. */
    OP(obj,define_own_property)(O, name, &desc, EJS_TRUE);

    /* 5. Return O. */
    return O;
}

// ECMA262: 15.2.3.7

typedef struct {
    ejsval P;
    EJSPropertyDesc desc;
} DefinePropertiesPair;

/* Object.defineProperties ( O, Properties ) */
static ejsval
_ejs_Object_defineProperties (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval O = _ejs_undefined;
    ejsval Properties = _ejs_undefined;
    if (argc > 0) O = args[0];
    if (argc > 1) Properties = args[1];

    /* 1. If Type(O) is not Object throw a TypeError exception. */
    if (!EJSVAL_IS_OBJECT(O)) {
        printf ("throw TypeError, _this isn't an Object\n");
        EJS_NOT_IMPLEMENTED();
    }
    EJSObject *obj = EJSVAL_TO_OBJECT(O);

    if (EJSVAL_IS_UNDEFINED(Properties) || EJSVAL_IS_NULL(Properties))
        return O;

    /* 2. Let props be ToObject(Properties). */
    ejsval props = ToObject(Properties);
    EJSObject* props_obj = EJSVAL_TO_OBJECT(props);

    /* 3. Let names be an internal list containing the names of each enumerable own property of props. */
    int names_len = 0;
    for (int p = 0; p < props_obj->map.num; p ++) {
        if (_ejs_property_desc_is_enumerable (&props_obj->map.properties[p]))
            names_len ++;
    }

    if (names_len == 0) {
        /* no enumerable properties, bail early */
        return O;
    }

    ejsval* names = malloc(names_len * sizeof(ejsval));
    int n = 0;
    for (int p = 0; p < props_obj->map.num; p ++) {
        if (_ejs_property_desc_is_enumerable(&props_obj->map.properties[p]))
            names[n++] = _ejs_string_new_ucs2(props_obj->map.names[p]);
    }

    /* 4. Let descriptors be an empty internal List. */
    DefinePropertiesPair *descriptors = malloc(sizeof(DefinePropertiesPair) * names_len);

    /* 5. For each element P of names in list order, */
    for (int n = 0; n < names_len; n ++) {
        ejsval P = names[n];

        /* a. Let descObj be the result of calling the [[Get]] internal method of props with P as the argument. */
        ejsval descObj = OP(props_obj,get)(props, P);

        DefinePropertiesPair *pair = &descriptors[n];
        /* b. Let desc be the result of calling ToPropertyDescriptor with descObj as the argument. */
        ToPropertyDescriptor (descObj, &pair->desc);
        /* c. Append the pair (a two element List) consisting of P and desc to the end of descriptors. */
        pair->P = P;
    }

    /* 6. For  each pair from descriptors in list order, */
    for (int d = 0; d < names_len; d++) {
        /*    a. Let P be the first element of pair. */
        ejsval P = descriptors[d].P;

        /*    b. Let desc be the second element of pair. */
        EJSPropertyDesc* desc = &descriptors[d].desc;

        /*    c. Call the [[DefineOwnProperty]] internal method of O with arguments P, desc, and true. */
        OP(obj,define_own_property)(O, P, desc, EJS_TRUE);
    }

    free (names);
    free (descriptors);
    
    /* 7. Return O. */
    return O;
}

// ECMA262: 15.2.3.8
static ejsval
_ejs_Object_seal (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval O = _ejs_undefined;
    if (argc > 0) O = args[0];

    /* 1. If Type(O) is not Object throw a TypeError exception. */
    if (!EJSVAL_IS_OBJECT(O)) {
        printf ("throw TypeError, O isn't an Object\n");
        EJS_NOT_IMPLEMENTED();
    }
    EJSObject* obj = EJSVAL_TO_OBJECT(O);

    // XXX this needs special handling for arrays (and other objects?)

    /* 2. For each named own property name P of O, */
    for (int n = 0; n < obj->map.num; n++) {
        ejsval P = _ejs_string_new_ucs2(obj->map.names[n]);

        /*    a. Let desc be the result of calling the [[GetOwnProperty]] internal method of O with P. */
        EJSPropertyDesc* desc = OP(obj,get_own_property)(O, P);

        /*    b. If desc.[[Configurable]] is true, set desc.[[Configurable]] to false. */
        EJS_NOT_IMPLEMENTED();

        /*    c. Call the [[DefineOwnProperty]] internal method of O with P, desc, and true as arguments. */
        OP(obj,define_own_property)(O, P, desc, EJS_TRUE);
    }

    /* 3. Set the [[Extensible]] internal property of O to false. */
    EJS_OBJECT_CLEAR_EXTENSIBLE(obj);

    /* 4. Return O. */
    return O;
}

// FIXME ECMA262: 15.2.3.9
static ejsval
_ejs_Object_freeze (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval O = _ejs_undefined;
    if (argc > 0) O = args[0];

    /* 1. If Type(O) is not Object throw a TypeError exception. */
    if (!EJSVAL_IS_OBJECT(O)) {
        printf ("throw TypeError, O isn't an Object\n");
        EJS_NOT_IMPLEMENTED();
    }
    EJSObject* obj = EJSVAL_TO_OBJECT(O);

    // XXX this needs special handling for arrays (and other objects?)

    /* 2. For each named own property name P of O, */
    for (int n = 0; n < obj->map.num; n++) {
        ejsval P = _ejs_string_new_ucs2(obj->map.names[n]);

        /*    a. Let desc be the result of calling the [[GetOwnProperty]] internal method of O with P. */
        EJSPropertyDesc* desc = OP(obj,get_own_property)(O, P);

        /*    b. If IsDataDescriptor(desc) is true, then */
        if (IsDataDescriptor(desc)) {
            /*       i. If desc.[[Writable]] is true, set desc.[[Writable]] to false. */
            if (_ejs_property_desc_is_writable(desc))
                _ejs_property_desc_set_writable(desc, EJS_FALSE);
            /*    c. If desc.[[Configurable]] is true, set desc.[[Configurable]] to false. */
            if (_ejs_property_desc_is_configurable(desc))
                _ejs_property_desc_set_configurable(desc, EJS_FALSE);
        }

        /*    d. Call the [[DefineOwnProperty]] internal method of O with P, desc, and true as arguments. */
        OP(obj,define_own_property)(O, P, desc, EJS_TRUE);
    }

    /* 3. Set the [[Extensible]] internal property of O to false. */
    EJS_OBJECT_CLEAR_EXTENSIBLE(obj);

    /* 4. Return O. */
    return O;
}

// ECMA262: 15.2.3.10
static ejsval
_ejs_Object_preventExtensions (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval O = _ejs_undefined;
    if (argc > 0) O = args[0];

    /* 1. If Type(O) is not Object throw a TypeError exception. */
    if (!EJSVAL_IS_OBJECT(O)) {
        printf ("throw TypeError, O isn't an Object\n");
        EJS_NOT_IMPLEMENTED();
    }
    EJSObject* obj = EJSVAL_TO_OBJECT(O);

    /* 2. Set the [[Extensible]] internal property of O to false. */
    EJS_OBJECT_CLEAR_EXTENSIBLE(obj);

    /* 3. Return O. */
    return O;
}

// ECMA262: 15.2.3.11
static ejsval
_ejs_Object_isSealed (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval O = _ejs_undefined;
    if (argc > 0) O = args[0];

    /* 1. If Type(O) is not Object throw a TypeError exception. */
    if (!EJSVAL_IS_OBJECT(O)) {
        printf ("throw TypeError, O isn't an Object\n");
        EJS_NOT_IMPLEMENTED();
    }
    EJSObject* obj = EJSVAL_TO_OBJECT(O);

    // XXX this needs special handling for arrays (and other objects?)

    /* 2. For each named own property name P of O, */
    for (int n = 0; n < obj->map.num; n++) {
        ejsval P = _ejs_string_new_ucs2(obj->map.names[n]);

        /*    a. Let desc be the result of calling the [[GetOwnProperty]] internal method of O with P. */
        EJSPropertyDesc* desc = OP(obj,get_own_property)(O, P);

        /*    b. If desc.[[Configurable]] is true, then return false. */
        if (_ejs_property_desc_is_configurable(desc))
            return _ejs_false;
    }

    /* 3. If the [[Extensible]] internal property of O is false, then return true. */
    if (!EJS_OBJECT_IS_EXTENSIBLE(obj))
        return _ejs_true;

    /* 4. Otherwise, return false. */
    return _ejs_false;
}

// ECMA262: 15.2.3.12
static ejsval
_ejs_Object_isFrozen (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval O = _ejs_undefined;
    if (argc > 0) O = args[0];

    /* 1. If Type(O) is not Object throw a TypeError exception. */
    if (!EJSVAL_IS_OBJECT(O)) {
        printf ("throw TypeError, O isn't an Object\n");
        EJS_NOT_IMPLEMENTED();
    }
    EJSObject* obj = EJSVAL_TO_OBJECT(O);

    // XXX this needs special handling for arrays (and other objects?)

    /* 2. For each named own property name P of O, */
    for (int n = 0; n < obj->map.num; n++) {
        ejsval P = _ejs_string_new_ucs2(obj->map.names[n]);

        /*    a. Let desc be the result of calling the [[GetOwnProperty]] internal method of O with P. */
        EJSPropertyDesc* desc = OP(obj,get_own_property)(O, P);

        /*    b. If IsDataDescriptor(desc) is true then */
        if (IsDataDescriptor(desc)) {
            /*       i. If desc.[[Writable]] is true, return false. */
            if (_ejs_property_desc_is_writable(desc))
                return _ejs_false;
            /*    c. If desc.[[Configurable]] is true, then return false. */
            if (_ejs_property_desc_is_configurable(desc))
                return _ejs_false;
        }
    }

    /* 3. If the [[Extensible]] internal property of O is false, then return true. */
    if (!EJS_OBJECT_IS_EXTENSIBLE(obj))
        return _ejs_true;
    
    /* 4. Otherwise, return false. */
    return _ejs_false;
}

// ECMA262: 15.2.3.13
static ejsval
_ejs_Object_isExtensible (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval O = _ejs_undefined;
    if (argc > 0) O = args[0];

    /* 1. If Type(O) is not Object throw a TypeError exception. */
    if (!EJSVAL_IS_OBJECT(O)) {
        printf ("throw TypeError, O isn't an Object\n");
        EJS_NOT_IMPLEMENTED();
    }
    EJSObject* obj = EJSVAL_TO_OBJECT(O);

    /* 2. Return the Boolean value of the [[Extensible]] internal property of O */
    return BOOLEAN_TO_EJSVAL(EJS_OBJECT_IS_EXTENSIBLE(obj));
}

// ECMA262: 15.2.3.14
static ejsval
_ejs_Object_keys (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    EJS_NOT_IMPLEMENTED();
}

// ECMA262: 15.2.4.2
ejsval
_ejs_Object_prototype_toString (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    char buf[1024];
    ejsval thisObj = ToObject(_this);
    const char *classname;
    if (EJSVAL_IS_NULL(thisObj))
        classname = "Null";
    else if (EJSVAL_IS_UNDEFINED(thisObj))
        classname = "Undefined";
    else
        classname = CLASSNAME(EJSVAL_TO_OBJECT(thisObj));
    snprintf (buf, sizeof(buf), "[object %s]", classname);
    return _ejs_string_new_utf8 (buf);
}

// ECMA262: 15.2.4.3
static ejsval
_ejs_Object_prototype_toLocaleString (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    /* 1. Let O be the result of calling ToObject passing the this value as the argument. */
    ejsval O = ToObject(_this);
    EJSObject* O_ = EJSVAL_TO_OBJECT(O);

    /* 2. Let toString be the result of calling the [[Get]] internal method of O passing "toString" as the argument. */
    ejsval toString = OP(O_, get)(O, _ejs_atom_toString);

    /* 3. If IsCallable(toString) is false, throw a TypeError exception. */
    // XXX

    /* 4. Return the result of calling the [[Call]] internal method of toString passing O as the this value and no arguments. */
    return _ejs_invoke_closure (toString, O, 0, NULL);
}

// ECMA262: 15.2.4.4
static ejsval
_ejs_Object_prototype_valueOf (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    EJS_NOT_IMPLEMENTED();
}

// ECMA262: 15.2.4.5
static ejsval
_ejs_Object_prototype_hasOwnProperty (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval needle = _ejs_undefined;
    if (EJS_UNLIKELY(argc > 0))
        needle = args[0];

    return BOOLEAN_TO_EJSVAL(OP(EJSVAL_TO_OBJECT(_this),get_own_property)(_this, needle) != NULL);
}

// ECMA262: 15.2.4.6
static ejsval
_ejs_Object_prototype_isPrototypeOf (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval V = _ejs_undefined;
    if (argc > 0) V = args[0];

    /* 1. If V is not an object, return false. */
    if (!EJSVAL_IS_OBJECT(V))
        return _ejs_false;

    /* 2. Let O be the result of calling ToObject passing the this value as the argument. */
    ejsval O = ToObject(_this);

    /* 3. Repeat */
    while (EJS_TRUE) {
        /*    a. Let V be the value of the [[Prototype]] internal property of V. */
        V = EJSVAL_TO_OBJECT(V)->proto;

        /*    b. if V is null, return false */
        if (EJSVAL_IS_NULL(V))
            return _ejs_false;

        /* c. If O and V refer to the same object, return true. */
        if (EJSVAL_EQ(O, V))
            return _ejs_true;
    }
}

// ECMA262: 15.2.4.7
static ejsval
_ejs_Object_prototype_propertyIsEnumerable (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval V = _ejs_undefined;
    if (argc > 0) V = args[0];

    /* 1. Let P be ToString(V). */
    ejsval P = ToString(V);

    /* 2. Let O be the result of calling ToObject passing the this value as the argument. */
    ejsval O = ToObject(_this);
    EJSObject* O_ = EJSVAL_TO_OBJECT(O);

    /* 3. Let desc be the result of calling the [[GetOwnProperty]] internal method of O passing P as the argument. */
    EJSPropertyDesc* desc = OP(O_, get_own_property)(O, P);
    /* 4. If desc is undefined, return false. */
    if (!desc)
        return _ejs_false;

    /* 5. Return the value of desc.[[Enumerable]]. */
    return BOOLEAN_TO_EJSVAL(_ejs_property_desc_is_enumerable(desc));
}

void
_ejs_object_init_proto()
{
    _ejs_gc_add_named_root (_ejs_Object__proto__);
    _ejs_gc_add_named_root (_ejs_Object_prototype);

    EJSFunction* __proto__ = _ejs_gc_new(EJSFunction);

    __proto__->func = _ejs_Function_empty;
    __proto__->env = _ejs_null;

    EJSObject* prototype = _ejs_gc_new(EJSObject);

    _ejs_Object__proto__ = OBJECT_TO_EJSVAL(__proto__);
    _ejs_Object_prototype = OBJECT_TO_EJSVAL(prototype);

    _ejs_init_object (prototype, _ejs_null, &_ejs_object_specops);
    _ejs_init_object ((EJSObject*)__proto__, _ejs_Object_prototype, &_ejs_function_specops);

    _ejs_object_define_value_property (_ejs_Object__proto__, _ejs_atom_name, _ejs_atom_empty, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_CONFIGURABLE | EJS_PROP_NOT_WRITABLE);
}

void
_ejs_object_init (ejsval global)
{
    START_SHADOW_STACK_FRAME;

    ADD_STACK_ROOT(ejsval, tmpobj, _ejs_function_new (_ejs_null, _ejs_atom_Object, (EJSClosureFunc)_ejs_Object_impl));
    _ejs_Object = tmpobj;

    // ECMA262 15.2.3.1
    _ejs_object_define_value_property (_ejs_Object, _ejs_atom_prototype, _ejs_Object_prototype, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_CONFIGURABLE | EJS_PROP_NOT_WRITABLE);
    // ECMA262: 15.2.4.1
    _ejs_object_define_value_property (_ejs_Object_prototype, _ejs_atom_constructor, _ejs_Object, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_CONFIGURABLE | EJS_PROP_WRITABLE);

#define OBJ_METHOD(x) EJS_INSTALL_ATOM_FUNCTION_FLAGS(_ejs_Object, x, _ejs_Object_##x, EJS_PROP_NOT_ENUMERABLE)
#define PROTO_METHOD(x) EJS_INSTALL_ATOM_FUNCTION_FLAGS(_ejs_Object_prototype, x, _ejs_Object_prototype_##x, EJS_PROP_NOT_ENUMERABLE)

    OBJ_METHOD(getPrototypeOf);
    OBJ_METHOD(getOwnPropertyDescriptor);
    OBJ_METHOD(getOwnPropertyNames);
    OBJ_METHOD(create);
    OBJ_METHOD(defineProperty);
    OBJ_METHOD(defineProperties);
    OBJ_METHOD(seal);
    OBJ_METHOD(freeze);
    OBJ_METHOD(preventExtensions);
    OBJ_METHOD(isSealed);
    OBJ_METHOD(isFrozen);
    OBJ_METHOD(isExtensible);
    OBJ_METHOD(keys);

    PROTO_METHOD(toString);
    PROTO_METHOD(toLocaleString);
    PROTO_METHOD(valueOf);
    PROTO_METHOD(hasOwnProperty);
    PROTO_METHOD(isPrototypeOf);
    PROTO_METHOD(propertyIsEnumerable);

#undef PROTO_METHOD
#undef OBJ_METHOD

    _ejs_object_setprop (global, _ejs_atom_Object, _ejs_Object);

    END_SHADOW_STACK_FRAME;
}



// ECMA262: 8.12.3
static ejsval
_ejs_object_specop_get (ejsval obj_, ejsval propertyName)
{
    ejsval pname;

    pname = ToString(propertyName);

    if (!ucs2_strcmp(_ejs_ucs2___proto__, EJSVAL_TO_FLAT_STRING(pname)))
        return EJSVAL_TO_OBJECT(obj_)->proto;

    /* 1. Let desc be the result of calling the [[GetProperty]] internal method of O with property name P. */
    EJSPropertyDesc* desc = OP(EJSVAL_TO_OBJECT(obj_),get_property) (obj_, pname);
    /* 2. If desc is undefined, return undefined. */
    if (desc == NULL) {
        // printf ("property lookup on a %s object, propname %s => undefined\n", CLASSNAME(EJSVAL_TO_OBJECT(obj_)), EJSVAL_TO_FLAT_STRING(pname));
        return _ejs_undefined;
    }

    /* 3. If IsDataDescriptor(desc) is true, return desc.[[Value]]. */
    if (IsDataDescriptor(desc)) {
        // if (EJSVAL_IS_UNDEFINED(desc->value))
        //     printf ("property lookup on a %s object, propname %s => undefined\n", CLASSNAME(EJSVAL_TO_OBJECT(obj_)), EJSVAL_TO_FLAT_STRING(pname));
        return desc->value;
    }

    /* 4. Otherwise, IsAccessorDescriptor(desc) must be true so, let getter be desc.[[Get]]. */
    ejsval getter = _ejs_property_desc_get_getter(desc);

    /* 5. If getter is undefined, return undefined. */
    if (EJSVAL_IS_UNDEFINED(getter)) {
        // printf ("property lookup on a %s object, propname %s => undefined getter\n", CLASSNAME(EJSVAL_TO_OBJECT(obj_)), EJSVAL_TO_FLAT_STRING(pname));
        return _ejs_undefined;
    }

    /* 6. Return the result calling the [[Call]] internal method of getter providing O as the this value and providing no arguments */
    return _ejs_invoke_closure (getter, obj_, 0, NULL);
}

// ECMA262: 8.12.1
static EJSPropertyDesc*
_ejs_object_specop_get_own_property (ejsval obj, ejsval propertyName)
{
    ejsval property_str = ToString(propertyName);
    EJSObject* obj_ = EJSVAL_TO_OBJECT(obj);

    int idx = _ejs_propertymap_lookup (&obj_->map, EJSVAL_TO_FLAT_STRING(property_str), EJS_FALSE);
    if (idx == -1)
        return NULL;
    return &obj_->map.properties[idx];
}

// ECMA262: 8.12.2
static EJSPropertyDesc*
_ejs_object_specop_get_property (ejsval O, ejsval P)
{
    EJSObject* obj = EJSVAL_TO_OBJECT(O);
    /* 1. Let prop be the result of calling the [[GetOwnProperty]] internal method of O with property name P. */
    EJSPropertyDesc* prop = OP(obj,get_own_property)(O, P);

    /* 2. If prop is not undefined, return prop. */
    if (prop)
        return prop;

    /* 3. Let proto be the value of the [[Prototype]] internal property of O. */
    ejsval proto = obj->proto;

    /* 4. If proto is null, return undefined. */
    if (EJSVAL_IS_NULL(proto))
        return NULL;

    EJSObject* proto_obj = EJSVAL_TO_OBJECT(proto);

    /* 5. Return the result of calling the [[GetProperty]] internal method of proto with argument P. */
    return OP(proto_obj, get_property)(proto, P);
}

// ECMA262: 8.12.5
static void
_ejs_object_specop_put (ejsval O, ejsval P, ejsval V, EJSBool Throw)
{
    EJSObject* obj = EJSVAL_TO_OBJECT(O);
    /* 1. If the result of calling the [[CanPut]] internal method of O with argument P is false, then */
    if (!OP(obj,can_put)(O, P)) {
        /*    a. If Throw is true, then throw a TypeError exception. */
        if (Throw) {
            printf ("throw TypeError\n");
            EJS_NOT_IMPLEMENTED();
        }
        else {
            return;
        }
    }
    /* 2. Let ownDesc be the result of calling the [[GetOwnProperty]] internal method of O with argument P. */
    EJSPropertyDesc* ownDesc = OP(obj,get_own_property)(O, P);
    
    /* 3. If IsDataDescriptor(ownDesc) is true, then */
    if (IsDataDescriptor(ownDesc)) {
        /*    a. Let valueDesc be the Property Descriptor {[[Value]]: V}. */
        EJSPropertyDesc valueDesc = { .value = V, .flags = EJS_PROP_FLAGS_VALUE_SET };
        /*    b. Call the [[DefineOwnProperty]] internal method of O passing P, valueDesc, and Throw as arguments. */
        OP(obj,define_own_property)(O, P, &valueDesc, Throw);
        /*    c. Return. */
        return;
    }
    /* 4. Let desc be the result of calling the [[GetProperty]] internal method of O with argument P. This may be */
    /*    either an own or inherited accessor property descriptor or an inherited data property descriptor. */
    EJSPropertyDesc* desc = OP(obj,get_property)(O, P);

    /* 5. If IsAccessorDescriptor(desc) is true, then */
    if (IsAccessorDescriptor(desc)) {
        /*    a. Let setter be desc.[[Set]] which cannot be undefined. */
        ejsval setter = _ejs_property_desc_get_setter(desc);
        assert (EJSVAL_IS_FUNCTION(setter));
        /*    b. Call the [[Call]] internal method of setter providing O as the this value and providing V as the sole argument. */
        _ejs_invoke_closure (setter, O, 1, &V);
    }
    else {
        /* 6. Else, create a named data property named P on object O as follows */
        /*    a. Let newDesc be the Property Descriptor */
        /*       {[[Value]]: V, [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: true}. */
        EJSPropertyDesc newDesc = { .value = V, .flags = EJS_PROP_FLAGS_VALUE_SET | EJS_PROP_WRITABLE | EJS_PROP_ENUMERABLE | EJS_PROP_CONFIGURABLE };

        /*    b. Call the [[DefineOwnProperty]] internal method of O passing P, newDesc, and Throw as arguments. */
        OP(obj,define_own_property)(O, P, &newDesc, Throw);
    }
    /* 7. Return. */
}

// ECMA262: 8.12.4
static EJSBool
_ejs_object_specop_can_put (ejsval O, ejsval P)
{
    EJSObject* obj = EJSVAL_TO_OBJECT(O);

    /* 1. Let desc be the result of calling the [[GetProperty]] internal method of O with property name P. */
    EJSPropertyDesc* desc = OP(obj,get_own_property)(O, P);

    /* 2. If desc is not undefined, then */
    if (desc) {
        /* a. If IsAccessorDescriptor(desc) is true, then */
        if (IsAccessorDescriptor(desc)) {
            /*    i. If desc.[[Set]] is undefined, then return false. */
            if (EJSVAL_IS_UNDEFINED(_ejs_property_desc_get_setter(desc)))
                return EJS_FALSE;

            /*    ii. Else return true. */
            return EJS_TRUE;
        }
        /* b. Else, desc must be a DataDescriptor so return the value of desc.[[Writable]]. */
        return _ejs_property_desc_is_writable(desc);
    }
    /* 3. Let proto be the [[Prototype]] internal property of O. */
    ejsval proto = obj->proto;

    /* 4. If proto is null, then return the value of the [[Extensible]] internal property of O. */
    if (EJSVAL_IS_NULL(proto))
        return EJS_OBJECT_IS_EXTENSIBLE(obj);

    /* 5. Let inherited be the result of calling the [[GetProperty]] internal method of proto with property name P. */
    EJSPropertyDesc* inherited = OP(obj,get_property)(proto, P);

    /* 6. If inherited is undefined, return the value of the [[Extensible]] internal property of O. */
    if (!inherited)
        return EJS_OBJECT_IS_EXTENSIBLE(obj);

    /* 7. If IsAccessorDescriptor(inherited) is true, then */
    if (IsAccessorDescriptor(inherited)) {
        /* a. If inherited.[[Set]] is undefined, then return false.*/
        if (EJSVAL_IS_UNDEFINED(_ejs_property_desc_get_setter(inherited)))
            return EJS_FALSE;
        /* b. Else return true. */
        return EJS_TRUE;
    }
    /* 8. Else, inherited must be a DataDescriptor*/
    else {
        /* a. If the [[Extensible]] internal property of O is false, return false. */
        if (!EJS_OBJECT_IS_EXTENSIBLE(obj))
            return EJS_FALSE;

        /* b. Else return the value of inherited.[[Writable]]. */
        return _ejs_property_desc_is_writable (inherited);
    }
}

static EJSBool
_ejs_object_specop_has_property (ejsval O, ejsval P)
{
    EJSObject* obj = EJSVAL_TO_OBJECT(O);

    /* 1. Let desc be the result of calling the [[GetProperty]] internal method of O with property name P. */
    EJSPropertyDesc* desc = OP(obj,get_own_property)(O, P);
    
    /* 2. If desc is undefined, then return false. */
    if (!desc)
        return EJS_FALSE;

    /* 3. Else return true. */
    return EJS_TRUE;
}

static EJSBool
_ejs_object_specop_delete (ejsval O, ejsval P, EJSBool Throw)
{
    EJSObject* obj = EJSVAL_TO_OBJECT(O);
    /* 1. Let desc be the result of calling the [[GetOwnProperty]] internal method of O with property name P. */
    EJSPropertyDesc* desc = OP(obj,get_own_property)(O, P);
    /* 2. If desc is undefined, then return true. */
    if (!desc)
        return EJS_TRUE;
    /* 3. If desc.[[Configurable]] is true, then */
    if (_ejs_property_desc_is_configurable(desc)) {
        /*    a. Remove the own property with name P from O. */
        _ejs_propertymap_delete_desc (&obj->map, desc);
        /*    b. Return true. */
        return EJS_TRUE;
    }
    /* 4. Else if Throw, then throw a TypeError exception.*/
    else if (Throw) {
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "property is not configurable");
    }
    /* 5. Return false. */
    return EJS_FALSE;
}

static ejsval
_ejs_object_specop_default_value (ejsval obj, const char *hint)
{
    if (!strcmp (hint, "PreferredType") || !strcmp(hint, "String")) {
        // this should look up ToString and call it
        return ToString(obj);
    }
    else if (!strcmp (hint, "String")) {
        EJS_NOT_IMPLEMENTED();
    }

    EJS_NOT_IMPLEMENTED();
}

// ECMA262: 8.12.9
static EJSBool
_ejs_object_specop_define_own_property (ejsval O, ejsval P, EJSPropertyDesc* Desc, EJSBool Throw)
{
#define REJECT(reason)                                                  \
    EJS_MACRO_START                                                     \
        if (Throw)                                                      \
            _ejs_throw_nativeerror (EJS_TYPE_ERROR, _ejs_string_concat (P, _ejs_string_new_utf8(reason))); \
        return EJS_FALSE;                                               \
    EJS_MACRO_END

    EJSObject* obj = EJSVAL_TO_OBJECT(O);
    /* 1. Let current be the result of calling the [[GetOwnProperty]] internal method of O with property name P. */
    EJSPropertyDesc* current = OP(obj, get_own_property)(O, P);

    /* 2. Let extensible be the value of the [[Extensible]] internal property of O. */
    EJSBool extensible = EJS_OBJECT_IS_EXTENSIBLE(obj);

    /* 3. If current is undefined and extensible is false, then Reject. */
    if (!current && !extensible)
        REJECT(": property is currently unset and object is not extensible");

    /* 4. If current is undefined and extensible is true, then */
    if (!current && extensible) {
        /*    a. If  IsGenericDescriptor(Desc) or IsDataDescriptor(Desc) is true, then */
        if (IsGenericDescriptor(Desc) || IsDataDescriptor(Desc)) {
            /*       i. Create an own data property named P of object O whose [[Value]], [[Writable]],  */
            /*          [[Enumerable]] and [[Configurable]] attribute values are described by Desc. If the value of */
            /*          an attribute field of Desc is absent, the attribute of the newly created property is set to its  */
            /*          default value. */
            int idx = _ejs_propertymap_lookup (&obj->map, EJSVAL_TO_FLAT_STRING(P), EJS_TRUE);
            EJSPropertyDesc *dest = &obj->map.properties[idx];

            if (_ejs_property_desc_has_value (Desc))
                _ejs_property_desc_set_value (dest, _ejs_property_desc_get_value (Desc));
            if (_ejs_property_desc_has_configurable (Desc))
                _ejs_property_desc_set_configurable (dest, _ejs_property_desc_is_configurable (Desc));
            if (_ejs_property_desc_has_enumerable (Desc))
                _ejs_property_desc_set_enumerable (dest, _ejs_property_desc_is_enumerable (Desc));
            if (_ejs_property_desc_has_writable (Desc))
                _ejs_property_desc_set_writable (dest, _ejs_property_desc_is_writable (Desc));
        }
        /*    b. Else, Desc must be an accessor Property Descriptor so, */
        else {
            /*       i. Create an own accessor property named P of object O whose [[Get]], [[Set]],  */
            /*          [[Enumerable]] and [[Configurable]] attribute values are described by Desc. If the value of  */
            /*          an attribute field of Desc is absent, the attribute of the newly created property is set to its  */
            /*          default value. */
            int idx = _ejs_propertymap_lookup (&obj->map, EJSVAL_TO_FLAT_STRING(P), EJS_TRUE);
            EJSPropertyDesc *dest = &obj->map.properties[idx];

            if (_ejs_property_desc_has_getter (Desc))
                _ejs_property_desc_set_getter (dest, _ejs_property_desc_get_getter (Desc));
            if (_ejs_property_desc_has_setter (Desc))
                _ejs_property_desc_set_setter (dest, _ejs_property_desc_get_setter (Desc));
            if (_ejs_property_desc_has_configurable (Desc))
                _ejs_property_desc_set_configurable (dest, _ejs_property_desc_is_configurable (Desc));
            if (_ejs_property_desc_has_enumerable (Desc))
                _ejs_property_desc_set_enumerable (dest, _ejs_property_desc_is_enumerable (Desc));
        }
        /*    c. Return true. */
        return EJS_TRUE;
    }
    /* 5. Return true, if every field in Desc is absent. */
    if ((Desc->flags & EJS_PROP_FLAGS_SET_MASK) == 0) {
        return EJS_TRUE;
    }

    /* 6. Return true, if every field in Desc also occurs in current and the value of every field in Desc is the same  */
    /*    value as the corresponding field in current when compared using the SameValue algorithm (9.12). */
    if ((Desc->flags & EJS_PROP_FLAGS_SET_MASK) == (current->flags & EJS_PROP_FLAGS_SET_MASK)) {
        EJSBool match = EJS_TRUE;

        if (match) match = !_ejs_property_desc_has_enumerable(Desc) || (_ejs_property_desc_is_enumerable(Desc) == _ejs_property_desc_is_enumerable(current));
        if (match) match = !_ejs_property_desc_has_configurable(Desc) || (_ejs_property_desc_is_configurable(Desc) == _ejs_property_desc_is_configurable(current));
        if (match) match = !_ejs_property_desc_has_writable(Desc) || (_ejs_property_desc_is_writable(Desc) == _ejs_property_desc_is_writable(current));
        if (match) match = !_ejs_property_desc_has_value(Desc) || (EJSVAL_EQ(_ejs_property_desc_get_value(Desc), _ejs_property_desc_get_value(current)));
        if (match) match = !_ejs_property_desc_has_getter(Desc) || (EJSVAL_EQ(_ejs_property_desc_get_getter(Desc), _ejs_property_desc_get_getter(current)));
        if (match) match = !_ejs_property_desc_has_setter(Desc) || (EJSVAL_EQ(_ejs_property_desc_get_setter(Desc), _ejs_property_desc_get_setter(current)));

        if (match)
            return EJS_TRUE;
    }

    /* 7. If the [[Configurable]] field of current is false then */
    if (!_ejs_property_desc_is_configurable(current)) {
        /*    a. Reject, if the [[Configurable]] field of Desc is true. */
        if (_ejs_property_desc_is_configurable(Desc)) REJECT("_ejs_property_desc_is_configurable(Desc)");

        /*    b. Reject, if the [[Enumerable]] field of Desc is present and the [[Enumerable]] fields of current and  */
        /*       Desc are the Boolean negation of each other. */
        if (_ejs_property_desc_has_enumerable (Desc) &&
            _ejs_property_desc_is_enumerable(Desc) != _ejs_property_desc_is_enumerable(current))
            REJECT("_ejs_property_desc_has_enumerable (Desc) && _ejs_property_desc_is_enumerable(Desc) != _ejs_property_desc_is_enumerable(current)");
    }

    /* 8. If IsGenericDescriptor(Desc) is true, then no further validation is required. */
    if (IsGenericDescriptor(Desc)) {
    }
    /* 9. Else, if IsDataDescriptor(current) and IsDataDescriptor(Desc) have different results, then */
    else if (IsDataDescriptor(current) != IsDataDescriptor(Desc)) {
        /*    a. Reject, if the [[Configurable]] field of current is false.  */
        if (!_ejs_property_desc_is_configurable(current)) REJECT("!_ejs_property_desc_is_configurable(current)");
        /*    b. If IsDataDescriptor(current) is true, then */
        if (IsDataDescriptor(current)) {
            /*       i. Convert the property named P of object O from a data property to an accessor property.  */
            /*          Preserve the existing values of the converted property‘s [[Configurable]] and  */
            /*          [[Enumerable]] attributes and set the rest of the property‘s attributes to their default values. */
            _ejs_property_desc_clear_value (current);
            _ejs_property_desc_clear_writable (current);
            // XXX set the rest to default values
        }
        /*    c. Else, */
        else {
            /*       i. Convert the property named P of object O from an accessor property to a data property.  */
            /*          Preserve the existing values of the converted property‘s [[Configurable]] and */
            /*          [[Enumerable]] attributes and set the rest of the property‘s attributes to their default values. */
            _ejs_property_desc_clear_getter (current);
            _ejs_property_desc_clear_setter (current);
        }
    }
    /* 10. Else, if IsDataDescriptor(current) and IsDataDescriptor(Desc) are both true, then */
    else if (IsDataDescriptor(current) && IsDataDescriptor(Desc)) {
        /*     a. If the [[Configurable]] field of current is false, then */
        if (!_ejs_property_desc_is_configurable (current)) {
            /*        i. Reject, if the [[Writable]] field of current is false and the [[Writable]] field of Desc is true. */
            if (!_ejs_property_desc_is_writable(current) && _ejs_property_desc_is_writable(Desc)) REJECT("1");
            /*        ii. If the [[Writable]] field of current is false, then */
            if (!_ejs_property_desc_is_writable(current)) {
                /*            1. Reject, if the [[Value]] field of Desc is present and SameValue(Desc.[[Value]],  */
                /*               current.[[Value]]) is false.  */
                if (_ejs_property_desc_has_value(Desc) && !EJSVAL_EQ(_ejs_property_desc_get_value(Desc),
                                                                     _ejs_property_desc_get_value(current)))
                    REJECT(": property is not writable and also not configurable");
            }
        }
        /*     b. else, the [[Configurable]] field of current is true, so any change is acceptable. */
    }
    /* 11. Else, IsAccessorDescriptor(current) and IsAccessorDescriptor(Desc) are both true so, */
    else /* IsAccessorDescriptor(current) && IsAccessorDescriptor(Desc) */ {
        /*     a. If the [[Configurable]] field of current is false, then */
        if (!_ejs_property_desc_is_configurable(current)) {
            /*        i. Reject, if the [[Set]] field of Desc is present and SameValue(Desc.[[Set]], current.[[Set]]) is  */
            /*           false. */
            if (_ejs_property_desc_has_setter (Desc)
                && !EJSVAL_EQ(_ejs_property_desc_get_setter(Desc),
                              _ejs_property_desc_get_setter(current)))
                REJECT("3");

            /*        ii. Reject, if the [[Get]] field of Desc is present and SameValue(Desc.[[Get]], current.[[Get]]) */
            /*            is false. */
            if (_ejs_property_desc_has_getter (Desc)
                && !EJSVAL_EQ(_ejs_property_desc_get_getter(Desc),
                              _ejs_property_desc_get_getter(current)))
                REJECT("4");
        }
    }

    /* 12. For each attribute field of Desc that is present, set the correspondingly named attribute of the property  */
    /*     named P of object O to the value of the field. */
    int idx = _ejs_propertymap_lookup (&obj->map, EJSVAL_TO_FLAT_STRING(P), EJS_TRUE);
    EJSPropertyDesc *dest = &obj->map.properties[idx];

    if (_ejs_property_desc_has_getter (Desc))
        _ejs_property_desc_set_getter (dest, _ejs_property_desc_get_getter (Desc));
    if (_ejs_property_desc_has_setter (Desc))
        _ejs_property_desc_set_setter (dest, _ejs_property_desc_get_setter (Desc));
    if (_ejs_property_desc_has_value (Desc))
        _ejs_property_desc_set_value (dest, _ejs_property_desc_get_value (Desc));
    if (_ejs_property_desc_has_configurable (Desc))
        _ejs_property_desc_set_configurable (dest, _ejs_property_desc_is_configurable (Desc));
    if (_ejs_property_desc_has_enumerable (Desc))
        _ejs_property_desc_set_enumerable (dest, _ejs_property_desc_is_enumerable (Desc));
    if (_ejs_property_desc_has_writable (Desc))
        _ejs_property_desc_set_writable (dest, _ejs_property_desc_is_writable (Desc));

    /* 13. Return true. */
    return EJS_TRUE;
}

EJSObject*
_ejs_object_specop_allocate ()
{
    return _ejs_gc_new(EJSObject);
}

void 
_ejs_object_specop_finalize(EJSObject* obj)
{
    _ejs_propertymap_free (&obj->map);
    obj->proto = _ejs_null;
    obj->ops = NULL;
}

static void
scan_property (jschar* name, EJSPropertyDesc *desc, EJSValueFunc scan_func)
{
#if DEBUG_GC
    printf ("scan property desc = %p, name = %s\n", desc, ucs2_to_utf8(name));
#endif

    if (_ejs_property_desc_has_value (desc)) {
#if DEBUG_GC
        if (EJSVAL_IS_OBJECT(desc->value)) {
        	printf ("   has_value %p\n", EJSVAL_TO_OBJECT(desc->value));
    	}
        else
        	printf ("   has_value\n");
#endif
        scan_func (desc->value);
    }
    if (_ejs_property_desc_has_getter (desc)) {
#if DEBUG_GC
        printf ("   has_getter\n");
#endif
        scan_func (desc->getter); 
    }
    if (_ejs_property_desc_has_setter (desc)) {
#if DEBUG_GC
        printf ("   has_setter\n");
#endif
        scan_func (desc->setter);
    }
}

static void
_ejs_object_specop_scan (EJSObject* obj, EJSValueFunc scan_func)
{
    _ejs_propertymap_foreach_property (&obj->map, (EJSPropertyDescFunc)scan_property, scan_func);
    scan_func (obj->proto);
}
