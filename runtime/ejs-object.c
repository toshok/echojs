/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#define DEBUG_PROPERTIES 0

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
#include "ejs-function.h"

static ejsval  _ejs_object_specop_get (ejsval obj, ejsval propertyName, EJSBool isCStr);
static EJSPropertyDesc* _ejs_object_specop_get_own_property (ejsval obj, ejsval propertyName);
static EJSPropertyDesc* _ejs_object_specop_get_property (ejsval obj, ejsval propertyName);
static void    _ejs_object_specop_put (ejsval obj, ejsval propertyName, ejsval val, EJSBool flag);
static EJSBool _ejs_object_specop_can_put (ejsval obj, ejsval propertyName);
static EJSBool _ejs_object_specop_has_property (ejsval obj, ejsval propertyName);
static EJSBool _ejs_object_specop_delete (ejsval obj, ejsval propertyName, EJSBool flag);
static ejsval  _ejs_object_specop_default_value (ejsval obj, const char *hint);
static EJSBool _ejs_object_specop_define_own_property (ejsval obj, ejsval propertyName, EJSPropertyDesc* propertyDescriptor, EJSBool flag);
static void    _ejs_object_specop_finalize (EJSObject* obj);
static void    _ejs_object_specop_scan (EJSObject* obj, EJSValueFunc scan_func);

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
    if (EJSVAL_IS_UNDEFINED(Desc->get) && EJSVAL_IS_UNDEFINED(Desc->set))
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
    if (EJSVAL_IS_UNDEFINED(Desc->value) // XXX this is wrong, no?  you can assign props = undefined
        && !Desc->writable) 
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
        printf ("throw TypeError, O isn't an Object\n");
        EJS_NOT_IMPLEMENTED();
    }
    EJSObject* obj = EJSVAL_TO_OBJECT(O);

    /* 2. Let desc be the result of creating a new Property Descriptor that initially has no fields. */

    /* 3. If the result of calling the [[HasProperty]] internal method of Obj with argument "enumerable" is true, then */
    if (OP(obj,has_property)(O, _ejs_atom_enumerable)) {
        /*    a. Let enum be the result of calling the [[Get]] internal method of Obj with "enumerable". */
        /*    b. Set the [[Enumerable]] field of desc to ToBoolean(enum). */
        desc->enumerable = EJSVAL_TO_BOOLEAN(ToBoolean(OP(obj,get)(O, _ejs_atom_enumerable, EJS_FALSE)));
    }
    /* 4. If the result of calling the [[HasProperty]] internal method of Obj with argument "configurable" is true, then */
    if (OP(obj,has_property)(O, _ejs_atom_configurable)) {
        /*    a. Let conf  be the result of calling the [[Get]] internal method of Obj with argument "configurable". */
        /*    b. Set the [[Configurable]] field of desc to ToBoolean(conf). */
        desc->configurable = EJSVAL_TO_BOOLEAN(ToBoolean(OP(obj,get)(O, _ejs_atom_configurable, EJS_FALSE)));
    }
    /* 5. If the result of calling the [[HasProperty]] internal method of Obj with argument "value" is true, then */
    if (OP(obj,has_property)(O, _ejs_atom_value)) {
        /*    a. Let value be the result of calling the [[Get]] internal method of Obj with argument "value". */
        /*    b. Set the [[Value]] field of desc to value. */
        desc->value = OP(obj,get)(O, _ejs_atom_value, EJS_FALSE);
    }
    /* 6. If the result of calling the [[HasProperty]] internal method of Obj with argument "writable" is true, then */
    if (OP(obj,has_property)(O, _ejs_atom_writable)) {
        /*    a. Let writable be the result of calling the [[Get]] internal method of Obj with argument "writable". */
        /*    b. Set the [[Writable]] field of desc to ToBoolean(writable). */
        desc->writable = EJSVAL_TO_BOOLEAN(ToBoolean(OP(obj,get)(O, _ejs_atom_writable, EJS_FALSE)));
    }
    /* 7. If the result of calling the [[HasProperty]] internal method of Obj with argument "get" is true, then */
    if (OP(obj,has_property)(O, _ejs_atom_get)) {
        /*    a. Let getter be the result of calling the [[Get]] internal method of Obj with argument "get". */
        /*    b. If IsCallable(getter) is false and getter is not undefined, then throw a TypeError exception. */
        /*    c. Set the [[Get]] field of desc to getter. */
        desc->get = OP(obj,get)(O, _ejs_atom_get, EJS_FALSE);
        // XXX missing IsCallableCheck
    }
    /* 8. If the result of calling the [[HasProperty]] internal method of Obj with argument "set" is true, then */
    if (OP(obj,has_property)(O, _ejs_atom_set)) {
        /*    a. Let setter be the result of calling the [[Get]] internal method of Obj with argument "set". */
        /*    b. If IsCallable(setter) is false and setter is not undefined, then throw a TypeError exception. */
        /*    c. Set the [[Set]] field of desc to setter. */
        desc->set = OP(obj,get)(O, _ejs_atom_set, EJS_FALSE);
    }
    /* 9. If either desc.[[Get]] or desc.[[Set]] are present, then */
    /*    a. If either desc.[[Value]] or desc.[[Writable]] are present, then throw a TypeError exception. */
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
    ejsval obj = _ejs_object_new(_ejs_Object_proto);
    EJSObject* obj_ = EJSVAL_TO_OBJECT(obj);

    /* 3. If IsDataDescriptor(Desc) is true, then */
    if (IsDataDescriptor(Desc)) {
        /*    a. Call the [[DefineOwnProperty]] internal method of obj with arguments "value", Property Descriptor  */
        /*       {[[Value]]: Desc.[[Value]], [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: true}, and false. */
        EJSPropertyDesc value_desc = { .value= Desc->value, .writable= EJS_TRUE, .enumerable= EJS_TRUE, .configurable= EJS_TRUE };

        OP(obj_, define_own_property)(obj, _ejs_atom_value, &value_desc, EJS_FALSE);

        /*    b. Call the [[DefineOwnProperty]] internal method of obj with arguments "writable", Property Descriptor  */
        /*       {[[Value]]: Desc.[[Writable]], [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: true}, and false. */
        EJSPropertyDesc writable_desc = { .value= BOOLEAN_TO_EJSVAL(Desc->writable), .writable= EJS_TRUE, .enumerable= EJS_TRUE, .configurable= EJS_TRUE };

        OP(obj_, define_own_property)(obj, _ejs_atom_writable, &writable_desc, EJS_FALSE);
    }
    else {
        /* 4. Else, IsAccessorDescriptor(Desc) must be true, so */
        /*    a. Call the [[DefineOwnProperty]] internal method of obj with arguments "get", Property Descriptor */
        /*       {[[Value]]: Desc.[[Get]], [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: true}, and false. */
        EJSPropertyDesc get_desc = { .value= Desc->get, .writable= EJS_TRUE, .enumerable= EJS_TRUE, .configurable= EJS_TRUE };

        OP(obj_, define_own_property)(obj, _ejs_atom_get, &get_desc, EJS_FALSE);

        /*    b. Call the [[DefineOwnProperty]] internal method of obj with arguments "set", Property Descriptor */
        /*       {[[Value]]: Desc.[[Set]], [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: true}, and false. */
        EJSPropertyDesc set_desc = { .value= Desc->set, .writable= EJS_TRUE, .enumerable= EJS_TRUE, .configurable= EJS_TRUE };

        OP(obj_, define_own_property)(obj, _ejs_atom_set, &set_desc, EJS_FALSE);
    }
    /* 5. Call the [[DefineOwnProperty]] internal method of obj with arguments "enumerable", Property Descriptor */
    /*    {[[Value]]: Desc.[[Enumerable]], [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: true}, and false. */
    EJSPropertyDesc enumerable_desc = { .value= BOOLEAN_TO_EJSVAL(Desc->enumerable), .writable= EJS_TRUE, .enumerable= EJS_TRUE, .configurable= EJS_TRUE };
    OP(obj_, define_own_property)(obj, _ejs_atom_enumerable, &enumerable_desc, EJS_FALSE);

    /* 6. Call the [[DefineOwnProperty]] internal method of obj with arguments "configurable", Property Descriptor */
    /*    {[[Value]]: Desc.[[Configurable]], [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: true}, and false. */
    EJSPropertyDesc configurable_desc = { .value= BOOLEAN_TO_EJSVAL(Desc->configurable), .writable= EJS_TRUE, .enumerable= EJS_TRUE, .configurable= EJS_TRUE };
    OP(obj_, define_own_property)(obj, _ejs_atom_configurable, &configurable_desc, EJS_FALSE);

    /* 7. Return obj. */
    return obj;
}

EJSPropertyMap*
_ejs_propertymap_new (int initial_allocation)
{
    EJSPropertyMap* rv = (EJSPropertyMap*)malloc(sizeof (EJSPropertyMap));
    if (initial_allocation) {
        rv->names = (char**)malloc(sizeof(char*) * initial_allocation);
        rv->properties = (EJSPropertyDesc*)malloc(sizeof (EJSPropertyDesc) * initial_allocation);

        for (int i = 0; i < initial_allocation; i ++) {
            rv->properties[i].configurable =
                rv->properties[i].writable =
                rv->properties[i].enumerable = EJS_TRUE;
            rv->properties[i].value = 
                rv->properties[i].get =
                rv->properties[i].set = _ejs_undefined;
        }
    }
    else {
        rv->names = NULL;
        rv->properties = NULL;
    }
    rv->num = 0;
    rv->allocated = initial_allocation;
    return rv;
}

void
_ejs_propertymap_free (EJSPropertyMap *map)
{
    for (int i = 0; i < map->num; i ++) {
        free (map->names[i]);
    }
    free (map->names);
    free (map->properties);
    free(map);
}

void
_ejs_propertymap_foreach_value (EJSPropertyMap* map, EJSValueFunc foreach_func)
{
    for (int i = 0; i < map->num; i ++) {
        foreach_func (map->properties[i].value);
    }
}

int
_ejs_propertymap_lookup (EJSPropertyMap *map, const char *name, EJSBool add_if_not_found)
{
    int i;
    for (i = 0; i < map->num; i ++) {
        if (!strcmp (map->names[i], name))
            return i;
    }
  
    int idx = -1;
    if (add_if_not_found) {
        idx = map->num++;
        if (map->allocated == 0 || idx == map->allocated - 1) {
            int new_allocated = map->allocated + 10;

            char **new_names = (char**)malloc(sizeof(char*) * new_allocated);
            EJSPropertyDesc* new_properties = (EJSPropertyDesc*)malloc (sizeof(EJSPropertyDesc) * new_allocated);

            memmove (new_names, map->names, map->allocated * sizeof(char*));
            memmove (new_properties, map->properties, map->allocated * sizeof(EJSPropertyDesc));

            for (i = map->allocated; i < new_allocated; i ++) {
                new_properties[i].configurable =
                    new_properties[i].writable =
                    new_properties[i].enumerable = EJS_TRUE;
                new_properties[i].value =
                    new_properties[i].get =
                    new_properties[i].set = _ejs_undefined;
            }

            if (map->names)
                free (map->names);
            if (map->properties)
                free (map->properties);

            map->names = new_names;
            map->properties = new_properties;

            map->allocated = new_allocated;
        }
        map->names[idx] = strdup (name);

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
    ejsval forObj;
    char **names;
    int num;
    int current;
};

EJSPropertyIterator*
_ejs_property_iterator_new (ejsval forVal)
{
    if (EJSVAL_IS_PRIMITIVE(forVal))
        EJS_NOT_IMPLEMENTED();

    // totally broken, only iterator over forObj's property map, not prototype properties
    EJSPropertyMap *map = EJSVAL_TO_OBJECT(forVal)->map;

    // the iterator-using code for for..in should handle a null iterator return value,
    // which would save us this allocation.
    EJSPropertyIterator* iterator = (EJSPropertyIterator*)calloc(1, sizeof (EJSPropertyIterator));

    if (map) {
        int i;

        iterator->forObj = forVal;
        iterator->num = map->num;
        iterator->names = (char**)malloc(sizeof(char*) * iterator->num);
        for (i = 0; i < iterator->num; i ++)
            iterator->names[i] = strdup(map->names[i]);
        return iterator;
    }
    else {
        return NULL;
    }
}

char *
_ejs_property_iterator_current (EJSPropertyIterator* iterator)
{
    return iterator->current < iterator->num ? iterator->names[iterator->current] : NULL;
}

void
_ejs_property_iterator_next (EJSPropertyIterator* iterator)
{
    iterator->current ++;
}

void
_ejs_property_iterator_free (EJSPropertyIterator *iterator)
{
    int i;
    for (i = 0; i < iterator->num; i ++)
        free(iterator->names[i]);
    free (iterator->names);
    free (iterator);
}


///

void
_ejs_init_object (EJSObject* obj, ejsval proto, EJSSpecOps *ops)
{
    obj->proto = proto;
    obj->ops = ops ? ops : &_ejs_object_specops;
    obj->map = _ejs_propertymap_new (obj->ops == &_ejs_object_specops ? 5 : 0);
    obj->extensible = EJS_TRUE;
#if notyet
    ((GCObjectPtr)obj)->gc_data = 0x01; // HAS_FINALIZE
#endif
}

ejsval
_ejs_object_new (ejsval proto)
{
    if (EJSVAL_IS_NULL(proto)) proto = _ejs_Object_proto;

    EJSObject *obj;
    EJSSpecOps *ops;

    if (EJSVAL_EQ(proto, _ejs_Object_proto)) {
        obj = _ejs_object_alloc_instance();
        ops = &_ejs_object_specops;
    }
    else if (EJSVAL_EQ(proto, _ejs_Array_proto)) {
        obj = _ejs_array_alloc_instance();
        ops = &_ejs_array_specops;
    }
    else if (EJSVAL_EQ(proto, _ejs_String_proto)) {
        obj = _ejs_string_alloc_instance();
        ops = &_ejs_string_specops;
    }
    else if (EJSVAL_EQ(proto, _ejs_Number_proto)) {
        obj = _ejs_number_alloc_instance();
        ops = &_ejs_number_specops;
    }
    else if (EJSVAL_EQ(proto, _ejs_Regexp_proto)) {
        obj = _ejs_regexp_alloc_instance();
        ops = &_ejs_regexp_specops;
    }
    else if (EJSVAL_EQ(proto, _ejs_Date_proto)) {
        obj = _ejs_date_alloc_instance();
        ops = &_ejs_date_specops;
    }
    else {
        obj = _ejs_object_alloc_instance();
        ops = &_ejs_object_specops;
    }

    _ejs_init_object (obj, proto, ops);
    return OBJECT_TO_EJSVAL(obj);
}

EJSObject* _ejs_object_alloc_instance()
{
    EJSObject* rv = _ejs_gc_new(EJSObject);
    return rv;
}

#define offsetof(t,f) (int)(long)(&((t*)NULL)->f)

ejsval
_ejs_number_new (double value)
{
    return NUMBER_TO_EJSVAL(value);
}

ejsval
_ejs_string_new_utf8 (const char* str)
{
    int str_len = strlen(str);
    size_t value_size = sizeof(EJSPrimString) + str_len + 1;

    EJSPrimString* rv = (EJSPrimString*)_ejs_gc_alloc(value_size, EJS_FALSE);
    rv->type = EJS_STRING_FLAT;
    rv->length = str_len;
    rv->data.flat = (char*)rv + sizeof(EJSPrimString);
    strcpy (rv->data.flat, str);
    return STRING_TO_EJSVAL(rv);
}

ejsval
_ejs_string_new_utf8_len (const char* str, int len)
{
    size_t value_size = sizeof(EJSPrimString) + len + 1;

    EJSPrimString* rv = (EJSPrimString*)_ejs_gc_alloc(value_size, EJS_FALSE);
    rv->type = EJS_STRING_FLAT;
    rv->length = len;
    rv->data.flat = (char*)rv + sizeof(EJSPrimString);
    strncpy (rv->data.flat, str, len);
    return STRING_TO_EJSVAL(rv);
}

ejsval
_ejs_string_concat (ejsval left, ejsval right)
{
    EJSPrimString* lhs = EJSVAL_TO_STRING(left);
    EJSPrimString* rhs = EJSVAL_TO_STRING(right);
    
    EJSPrimString* rv = (EJSPrimString*)_ejs_gc_alloc(sizeof(EJSPrimString), EJS_FALSE/*XXX*/);
    rv->type = EJS_STRING_ROPE;
    rv->length = lhs->length + rhs->length;
    rv->data.rope.left = lhs;
    rv->data.rope.right = rhs;

    return STRING_TO_EJSVAL(rv);
}

static void
flatten_rope (char **p, EJSPrimString *n)
{
    switch (n->type) {
    case EJS_STRING_FLAT:
        strncpy(*p, n->data.flat, n->length);
        *p += n->length;
        break;
    case EJS_STRING_ROPE:
        flatten_rope(p, n->data.rope.left);
        flatten_rope(p, n->data.rope.right);
        break;
    default:
        EJS_NOT_IMPLEMENTED();
    }
}

EJSPrimString*
_ejs_string_flatten (ejsval str)
{
    EJSPrimString* primstr = EJSVAL_TO_STRING_IMPL(str);
    switch (primstr->type) {
    case EJS_STRING_FLAT:
        return primstr;
    case EJS_STRING_ROPE: {
        char *buffer = (char*)malloc(primstr->length + 1);
        char *p = buffer;
        flatten_rope (&p, primstr);
        buffer[primstr->length] = 0;
        // switch the type of the string to flat
        primstr->type = EJS_STRING_FLAT;
        primstr->data.flat = buffer;
        return primstr;
    }
    default:
        EJS_NOT_IMPLEMENTED();
    }
}

ejsval
_ejs_object_setprop (ejsval val, ejsval key, ejsval value)
{
    if (EJSVAL_IS_PRIMITIVE(val)) {
        printf ("setprop on primitive.  ignoring\n" );
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
            if (EJS_ARRAY_LEN(val) >= EJS_ARRAY_ALLOC(val))
                abort();
            return value;
        }
        // if we fail there, we fall back to the object impl below
    }

    if (!EJSVAL_IS_STRING(key)) {
        if (EJSVAL_IS_NUMBER(key)) {
            char buf[128];
            snprintf(buf, sizeof(buf), EJS_NUMBER_FORMAT, EJSVAL_TO_NUMBER(key));
            key = _ejs_string_new_utf8(buf);
        }
        else {
            printf ("key isn't a string\n");
            return _ejs_null;
        }
    }

    EJSObject *obj = EJSVAL_TO_OBJECT(val);
    int prop_index = _ejs_propertymap_lookup (obj->map, EJSVAL_TO_FLAT_STRING(key), EJS_TRUE);
    obj->map->properties[prop_index].value = value;

    return value;
}

ejsval
_ejs_object_getprop (ejsval obj, ejsval key)
{
    if (EJSVAL_IS_NULL(obj) || EJSVAL_IS_UNDEFINED(obj)) {
        printf ("throw TypeError, key is %s\n", EJSVAL_TO_FLAT_STRING(ToString(key)));
        EJS_NOT_IMPLEMENTED();
    }

    if (EJSVAL_IS_PRIMITIVE(obj)) {
        obj = ToObject(obj);
    }

    return OP(EJSVAL_TO_OBJECT(obj),get)(obj, key, EJS_FALSE);
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

    EJSObject *obj = EJSVAL_TO_OBJECT(val);
    int prop_index = _ejs_propertymap_lookup (obj->map, key, EJS_TRUE);
    obj->map->properties[prop_index].value = value;

    return value;
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

    char keybuf[256+8];
    char *kp;
    if ((((unsigned long long)key) & 0x1) != 0) {
        kp = (char*)(((unsigned long long)keybuf + 0x7) & ~0x7);

        int keylen = strlen(key);
        if (keylen >= 255) {
            return _ejs_object_getprop (obj, _ejs_string_new_utf8(key));
        }

        strncpy (kp, key, keylen);
        kp[keylen] = 0;
    }
    else {
        kp = (char*)key;
    }
    
    return OP(EJSVAL_TO_OBJECT(obj),get)(obj, PRIVATE_PTR_TO_EJSVAL_IMPL(kp), EJS_TRUE);
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
        printf ("string: '%s'\n", EJSVAL_TO_FLAT_STRING(val));
    }
    else if (EJSVAL_IS_OBJECT(val)) {
        printf ("<object %s>\n", CLASSNAME(EJSVAL_TO_OBJECT(val)));
    }
}

ejsval _ejs_Object;
ejsval _ejs_Object_proto = {0};

static ejsval
_ejs_Object_impl (ejsval env, ejsval _this, int argc, ejsval *args)
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
_ejs_Object_getPrototypeOf (ejsval env, ejsval _this, int argc, ejsval *args)
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
_ejs_Object_getOwnPropertyDescriptor (ejsval env, ejsval _this, int argc, ejsval *args)
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
_ejs_Object_getOwnPropertyNames (ejsval env, ejsval _this, int argc, ejsval *args)
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
    ejsval arr = _ejs_array_new(O->map->num);
    EJSArray* array = (EJSArray*)EJSVAL_TO_OBJECT(arr);
    /* 3. Let n be 0. */
    int n = 0;
    /* 4. For each named own property P of O */
    while (n < O->map->num) {
        /*    a. Let name be the String value that is the name of P. */
        char* name = O->map->names[n];
        ejsval propName = _ejs_string_new_utf8(name);
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

static ejsval _ejs_Object_defineProperties (ejsval env, ejsval _this, int argc, ejsval *args);

// ECMA262: 15.2.3.5
/* Object.create ( O [, Properties] ) */
static ejsval
_ejs_Object_create (ejsval env, ejsval _this, int argc, ejsval *args)
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
    ejsval obj = _ejs_object_new(_ejs_null);

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
_ejs_Object_defineProperty (ejsval env, ejsval _this, int argc, ejsval *args)
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
    EJSPropertyDesc desc = { .value= _ejs_undefined, .writable= EJS_FALSE, .enumerable= EJS_FALSE, .configurable= EJS_FALSE, .get= _ejs_undefined, .set =_ejs_undefined };

    ToPropertyDescriptor(Attributes, &desc);

    /* 4. Call the [[DefineOwnProperty]] internal method of O with arguments name, desc, and true. */
    OP(obj,define_own_property)(O, name, &desc, EJS_TRUE);

    /* 5. Return O. */
    return O;
}

// ECMA262: 15.2.3.7
/* Object.defineProperties ( O, Properties ) */
static ejsval
_ejs_Object_defineProperties (ejsval env, ejsval _this, int argc, ejsval *args)
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

    /* 2. Let props be ToObject(Properties). */
    ejsval props = ToObject(Properties);

    /* 3. Let names be an internal list containing the names of each enumerable own property of props. */
    EJS_NOT_IMPLEMENTED();
    /* 4. Let descriptors be an empty internal List. */
    /* 5. For each element P of names in list order, */
    /* a. Let descObj be the result of calling the [[Get]] internal method of props with P as the argument. */
    /* b. Let desc be the result of calling ToPropertyDescriptor with descObj as the argument. */
    /* c. Append the pair (a two element List) consisting of P and desc to the end of descriptors. */
    /* 6. For  each pair from descriptors in list order, */
    /*    a. Let P be the first element of pair. */
    /*    b. Let desc be the second element of pair. */
    /*    c. Call the [[DefineOwnProperty]] internal method of O with arguments P, desc, and true. */
    /* 7. Return O. */
    return O;
}

// ECMA262: 15.2.3.8
static ejsval
_ejs_Object_seal (ejsval env, ejsval _this, int argc, ejsval *args)
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
    for (int n = 0; n < obj->map->num; n++) {
        ejsval P = _ejs_string_new_utf8(obj->map->names[n]);

        /*    a. Let desc be the result of calling the [[GetOwnProperty]] internal method of O with P. */
        EJSPropertyDesc* desc = OP(obj,get_own_property)(O, P);

        /*    b. If desc.[[Configurable]] is true, set desc.[[Configurable]] to false. */
        EJS_NOT_IMPLEMENTED();

        /*    c. Call the [[DefineOwnProperty]] internal method of O with P, desc, and true as arguments. */
        OP(obj,define_own_property)(O, P, desc, EJS_TRUE);
    }

    /* 3. Set the [[Extensible]] internal property of O to false. */
    obj->extensible = EJS_FALSE;

    /* 4. Return O. */
    return O;
}

// FIXME ECMA262: 15.2.3.9
static ejsval
_ejs_Object_freeze (ejsval env, ejsval _this, int argc, ejsval *args)
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
    for (int n = 0; n < obj->map->num; n++) {
        ejsval P = _ejs_string_new_utf8(obj->map->names[n]);

        /*    a. Let desc be the result of calling the [[GetOwnProperty]] internal method of O with P. */
        EJSPropertyDesc* desc = OP(obj,get_own_property)(O, P);

        /*    b. If IsDataDescriptor(desc) is true, then */
        if (IsDataDescriptor(desc)) {
            /*       i. If desc.[[Writable]] is true, set desc.[[Writable]] to false. */
            desc->writable = EJS_FALSE;
            /*    c. If desc.[[Configurable]] is true, set desc.[[Configurable]] to false. */
            desc->configurable = EJS_FALSE;
        }

        /*    d. Call the [[DefineOwnProperty]] internal method of O with P, desc, and true as arguments. */
        OP(obj,define_own_property)(O, P, desc, EJS_TRUE);
    }

    /* 3. Set the [[Extensible]] internal property of O to false. */
    obj->extensible = EJS_FALSE;

    /* 4. Return O. */
    return O;
}

// ECMA262: 15.2.3.10
static ejsval
_ejs_Object_preventExtensions (ejsval env, ejsval _this, int argc, ejsval *args)
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
    obj->extensible = EJS_FALSE;

    /* 3. Return O. */
    return O;
}

// ECMA262: 15.2.3.11
static ejsval
_ejs_Object_isSealed (ejsval env, ejsval _this, int argc, ejsval *args)
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
    for (int n = 0; n < obj->map->num; n++) {
        ejsval P = _ejs_string_new_utf8(obj->map->names[n]);

        /*    a. Let desc be the result of calling the [[GetOwnProperty]] internal method of O with P. */
        EJSPropertyDesc* desc = OP(obj,get_own_property)(O, P);

        /*    b. If desc.[[Configurable]] is true, then return false. */
        desc->configurable = EJS_FALSE;
    }

    /* 3. If the [[Extensible]] internal property of O is false, then return true. */
    if (!obj->extensible)
        return _ejs_true;

    /* 4. Otherwise, return false. */
    return _ejs_false;
}

// ECMA262: 15.2.3.12
static ejsval
_ejs_Object_isFrozen (ejsval env, ejsval _this, int argc, ejsval *args)
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
    for (int n = 0; n < obj->map->num; n++) {
        ejsval P = _ejs_string_new_utf8(obj->map->names[n]);

        /*    a. Let desc be the result of calling the [[GetOwnProperty]] internal method of O with P. */
        EJSPropertyDesc* desc = OP(obj,get_own_property)(O, P);

        /*    b. If IsDataDescriptor(desc) is true then */
        if (IsDataDescriptor(desc)) {
            /*       i. If desc.[[Writable]] is true, return false. */
            desc->writable = EJS_FALSE;
            /*    c. If desc.[[Configurable]] is true, then return false. */
            desc->configurable = EJS_FALSE;
        }
    }

    /* 3. If the [[Extensible]] internal property of O is false, then return true. */
    if (!obj->extensible)
        return _ejs_true;
    
    /* 4. Otherwise, return false. */
    return _ejs_false;
}

// ECMA262: 15.2.3.13
static ejsval
_ejs_Object_isExtensible (ejsval env, ejsval _this, int argc, ejsval *args)
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
    return BOOLEAN_TO_EJSVAL(obj->extensible);
}

// ECMA262: 15.2.3.14
static ejsval
_ejs_Object_keys (ejsval env, ejsval _this, int argc, ejsval *args)
{
    EJS_NOT_IMPLEMENTED();
}

// ECMA262: 15.2.4.2
static ejsval
_ejs_Object_prototype_toString (ejsval env, ejsval _this, int argc, ejsval *args)
{
    char buf[1024];
    ejsval thisObj = ToObject(_this);
    snprintf (buf, sizeof(buf), "[object %s]", CLASSNAME(EJSVAL_TO_OBJECT(thisObj)));
    return _ejs_string_new_utf8 (buf);
}

// ECMA262: 15.2.4.3
static ejsval
_ejs_Object_prototype_toLocaleString (ejsval env, ejsval _this, int argc, ejsval *args)
{
    EJS_NOT_IMPLEMENTED();
}

// ECMA262: 15.2.4.4
static ejsval
_ejs_Object_prototype_valueOf (ejsval env, ejsval _this, int argc, ejsval *args)
{
    EJS_NOT_IMPLEMENTED();
}

// ECMA262: 15.2.4.5
static ejsval
_ejs_Object_prototype_hasOwnProperty (ejsval env, ejsval _this, int argc, ejsval *args)
{
    ejsval needle = _ejs_undefined;
    if (EJS_UNLIKELY(argc > 0))
        needle = args[0];

    return BOOLEAN_TO_EJSVAL(OP(EJSVAL_TO_OBJECT(_this),get_own_property)(_this, needle) != NULL);
}

// ECMA262: 15.2.4.6
static ejsval
_ejs_Object_prototype_isPrototypeOf (ejsval env, ejsval _this, int argc, ejsval *args)
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
_ejs_Object_prototype_propertyIsEnumerable (ejsval env, ejsval _this, int argc, ejsval *args)
{
    EJS_NOT_IMPLEMENTED();
}

void
_ejs_object_init (ejsval global)
{
    START_SHADOW_STACK_FRAME;

    // FIXME ECMA262 15.2.4
    _ejs_gc_add_named_root (_ejs_Object_proto);
    _ejs_Object_proto = _ejs_null;
    _ejs_Object_proto = _ejs_object_new(_ejs_null); // XXX circular initialization going on here..

    ADD_STACK_ROOT(ejsval, tmpobj, _ejs_function_new (_ejs_null, _ejs_atom_Object, (EJSClosureFunc)_ejs_Object_impl));
    _ejs_Object = tmpobj;

    // ECMA262 15.2.3.1
    _ejs_object_setprop (_ejs_Object,       _ejs_atom_prototype,    _ejs_Object_proto); // FIXME: {[[Writable]]: false, [[Enumerable]]: false, [[Configurable]]: false }
    // ECMA262: 15.2.4.1
    _ejs_object_setprop (_ejs_Object_proto, _ejs_atom_constructor,  _ejs_Object);

#define OBJ_METHOD(x) EJS_INSTALL_FUNCTION(_ejs_Object, EJS_STRINGIFY(x), _ejs_Object_##x)
#define PROTO_METHOD(x) EJS_INSTALL_FUNCTION(_ejs_Object_proto, EJS_STRINGIFY(x), _ejs_Object_prototype_##x)

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
_ejs_object_specop_get (ejsval obj_, ejsval propertyName, EJSBool isCStr)
{
#if 1
    EJSObject* obj = EJSVAL_TO_OBJECT(obj_);

    char *pname;
    if (isCStr)
        pname = (char*)EJSVAL_TO_PRIVATE_PTR_IMPL(propertyName);
    else {
        propertyName = ToString(propertyName);
        pname = EJSVAL_TO_FLAT_STRING(propertyName);
    }

    int prop_index = _ejs_propertymap_lookup (obj->map, pname, EJS_FALSE);

    if (prop_index == -1) {
        if (!strcmp("prototype", pname))
            return obj->proto;

        if (EJSVAL_IS_NULL(obj->proto)) {
#if DEBUG_PROPERTIES
            printf ("failed to find property %s, returning undefined\n", pname);
#endif
            return _ejs_undefined;
        }
        else {
#if DEBUG_PROPERTIES
            printf ("walking up prototype chain for property %s\n", pname);
#endif
            return OP(EJSVAL_TO_OBJECT(obj->proto),get) (obj->proto, propertyName, isCStr);
        }
    }
    else {
        return obj->map->properties[prop_index].value;
    }
#else
    /* 1. Let desc be the result of calling the [[GetProperty]] internal method of O with property name P. */
    /* 2. If desc is undefined, return undefined. */
    /* 3. If IsDataDescriptor(desc) is true, return desc.[[Value]]. */
    /* 4. Otherwise, IsAccessorDescriptor(desc) must be true so, let getter be desc.[[Get]]. */
    /* 5. If getter is undefined, return undefined. */
    /* 6. Return the result calling the [[Call]] internal method of getter providing O as the this value and providing no arguments */
#endif
}

// ECMA262: 8.12.1
static EJSPropertyDesc*
_ejs_object_specop_get_own_property (ejsval obj, ejsval propertyName)
{
    ejsval property_str = ToString(propertyName);
    EJSObject* obj_ = EJSVAL_TO_OBJECT(obj);

    int idx = _ejs_propertymap_lookup (obj_->map, EJSVAL_TO_FLAT_STRING(property_str), EJS_FALSE);
    if (idx == -1)
        return NULL;
    return &obj_->map->properties[idx];
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
_ejs_object_specop_put (ejsval obj, ejsval propertyName, ejsval val, EJSBool flag)
{
    /* 1. If the result of calling the [[CanPut]] internal method of O with argument P is false, then */
    /*    a. If Throw is true, then throw a TypeError exception. */
    /*    b. Else return. */
    /* 2. Let ownDesc be the result of calling the [[GetOwnProperty]] internal method of O with argument P. */
    /* 3. If IsDataDescriptor(ownDesc) is true, then */
    /*    a. Let valueDesc be the Property Descriptor {[[Value]]: V}. */
    /*    b. Call the [[DefineOwnProperty]] internal method of O passing P, valueDesc, and Throw as arguments. */
    /*    c. Return. */
    /* 4. Let desc be the result of calling the [[GetProperty]] internal method of O with argument P. This may be */
    /*    either an own or inherited accessor property descriptor or an inherited data property descriptor. */
    /* 5. If IsAccessorDescriptor(desc) is true, then */
    /*    a. Let setter be desc.[[Set]] which cannot be undefined. */
    /*    b. Call the [[Call]] internal method of setter providing O as the this value and providing V as the sole argument. */
    /* 6. Else, create a named data property named P on object O as follows */
    /*    a. Let newDesc be the Property Descriptor */
    /*       {[[Value]]: V, [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: true}. */
    /*    b. Call the [[DefineOwnProperty]] internal method of O passing P, newDesc, and Throw as arguments. */
    /* 7. Return. */
}

static EJSBool
_ejs_object_specop_can_put (ejsval obj, ejsval propertyName)
{
    EJS_NOT_IMPLEMENTED();
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
    EJS_NOT_IMPLEMENTED();
    /*    a. Remove the own property with name P from O. */
    /*    b. Return true. */
    /* 4. Else if Throw, then throw a TypeError exception.*/
    /* 5. Return false. */
    return EJS_FALSE;
}

static ejsval
_ejs_object_specop_default_value (ejsval obj, const char *hint)
{
    EJS_NOT_IMPLEMENTED();
}

// ECMA262: 8.12.9
static EJSBool
_ejs_object_specop_define_own_property (ejsval O, ejsval P, EJSPropertyDesc* Desc, EJSBool Throw)
{
#define REJECT()                                                        \
    EJS_MACRO_START                                                     \
        if (Throw) {                                                    \
            printf ("throw a TypeError, [[DefineOwnProperty]] Reject\n"); \
            EJS_NOT_IMPLEMENTED();                                          \
        }                                                               \
        return EJS_FALSE;                                               \
    EJS_MACRO_END

    EJSObject* obj = EJSVAL_TO_OBJECT(O);
    /* 1. Let current be the result of calling the [[GetOwnProperty]] internal method of O with property name P. */
    EJSPropertyDesc* current = OP(obj, get_own_property)(O, P);

    /* 2. Let extensible be the value of the [[Extensible]] internal property of O. */
    EJSBool extensible = obj->extensible;

    /* 3. If current is undefined and extensible is false, then Reject. */
    if (!current && !extensible)
        REJECT();

    /* 4. If current is undefined and extensible is true, then */
    if (!current && extensible) {
        /*    a. If  IsGenericDescriptor(Desc) or IsDataDescriptor(Desc) is true, then */
        if (IsGenericDescriptor(Desc) || IsDataDescriptor(Desc)) {
            /*       i. Create an own data property named P of object O whose [[Value]], [[Writable]],  */
            /*          [[Enumerable]] and [[Configurable]] attribute values are described by Desc. If the value of */
            /*          an attribute field of Desc is absent, the attribute of the newly created property is set to its  */
            /*          default value. */
            int idx = _ejs_propertymap_lookup (obj->map, EJSVAL_TO_FLAT_STRING(P), EJS_TRUE);
            obj->map->properties[idx].get = Desc->get;
            obj->map->properties[idx].set = Desc->set;
            obj->map->properties[idx].configurable = Desc->configurable;
            obj->map->properties[idx].enumerable = Desc->enumerable;
            obj->map->properties[idx].writable = Desc->writable;
            obj->map->properties[idx].value = Desc->value;
        }
        /*    b. Else, Desc must be an accessor Property Descriptor so, */
        else {
            EJS_NOT_IMPLEMENTED();
            /*       i. Create an own accessor property named P of object O whose [[Get]], [[Set]],  */
            /*          [[Enumerable]] and [[Configurable]] attribute values are described by Desc. If the value of  */
            /*          an attribute field of Desc is absent, the attribute of the newly created property is set to its  */
            /*          default value. */
        }
        /*    c. Return true. */
        return EJS_TRUE;
    }
    /* 5. Return true, if every field in Desc is absent. */
    if (EJSVAL_IS_UNDEFINED(Desc->value) &&
        EJSVAL_IS_UNDEFINED(Desc->get) &&
        EJSVAL_IS_UNDEFINED(Desc->set)
        // XXX the EJSBool fields can be absent too
        ) {
        return EJS_TRUE;
    }

    /* 6. Return true, if every field in Desc also occurs in current and the value of every field in Desc is the same  */
    /*    value as the corresponding field in current when compared using the SameValue algorithm (9.12). */
    if (EJSVAL_EQ(Desc->value, current->value) &&
        EJSVAL_EQ(Desc->get, current->get) &&
        EJSVAL_EQ(Desc->set, current->set) &&
        Desc->writable == current->writable &&
        Desc->configurable == current->configurable &&
        Desc->enumerable == current->enumerable) {
        return EJS_TRUE;
    }

    /* 7. If the [[Configurable]] field of current is false then */
    if (!current->configurable) {
        /*    a. Reject, if the [[Configurable]] field of Desc is true. */
        if (Desc->configurable) REJECT();

        /*    b. Reject, if the [[Enumerable]] field of Desc is present and the [[Enumerable]] fields of current and  */
        /*       Desc are the Boolean negation of each other. */
        if (Desc->enumerable != current->enumerable) REJECT();
    }

    /* 8. If IsGenericDescriptor(Desc) is true, then no further validation is required. */
    if (IsGenericDescriptor(Desc)) {
    }
    /* 9. Else, if IsDataDescriptor(current) and IsDataDescriptor(Desc) have different results, then */
    else if (IsDataDescriptor(current) != IsDataDescriptor(Desc)) {
        /*    a. Reject, if the [[Configurable]] field of current is false.  */
        if (!current->configurable) REJECT();
        /*    b. If IsDataDescriptor(current) is true, then */
        if (IsDataDescriptor(current)) {
            /*       i. Convert the property named P of object O from a data property to an accessor property.  */
            /*          Preserve the existing values of the converted property‘s [[Configurable]] and  */
            /*          [[Enumerable]] attributes and set the rest of the property‘s attributes to their default values. */
        }
        /*    c. Else, */
        else {
            /*       i. Convert the property named P of object O from an accessor property to a data property.  */
            /*          Preserve the existing values of the converted property‘s [[Configurable]] and */
            /*          [[Enumerable]] attributes and set the rest of the property‘s attributes to their default values. */
        }
    }
    /* 10. Else, if IsDataDescriptor(current) and IsDataDescriptor(Desc) are both true, then */
    else if (IsDataDescriptor(current) && IsDataDescriptor(Desc)) {
        /*     a. If the [[Configurable]] field of current is false, then */
        /*        i. Reject, if the [[Writable]] field of current is false and the [[Writable]] field of Desc is true. */
        /*        ii. If the [[Writable]] field of current is false, then */
        /*            1. Reject, if the [[Value]] field of Desc is present and SameValue(Desc.[[Value]],  */
        /*               current.[[Value]]) is false.  */
        /*     b. else, the [[Configurable]] field of current is true, so any change is acceptable. */
    }
    /* 11. Else, IsAccessorDescriptor(current) and IsAccessorDescriptor(Desc) are both true so, */
    else /* IsAccessorDescriptor(current) && IsAccessorDescriptor(Desc) */ {
        /*     a. If the [[Configurable]] field of current is false, then */
        if (!current->configurable) {
            /*        i. Reject, if the [[Set]] field of Desc is present and SameValue(Desc.[[Set]], current.[[Set]]) is  */
            /*           false. */
            if (!EJSVAL_IS_UNDEFINED(Desc->set) && !EJSVAL_EQ(Desc->set, current->set)) REJECT();
            /*        ii. Reject, if the [[Get]] field of Desc is present and SameValue(Desc.[[Get]], current.[[Get]]) */
            /*            is false. */
            if (!EJSVAL_IS_UNDEFINED(Desc->get) && !EJSVAL_EQ(Desc->get, current->get)) REJECT();
        }
    }

    /* 12. For each attribute field of Desc that is present, set the correspondingly named attribute of the property  */
    /*     named P of object O to the value of the field. */
    int idx = _ejs_propertymap_lookup (obj->map, EJSVAL_TO_FLAT_STRING(P), EJS_TRUE);
    // XXX this is wrong - we need to only set the values that are specified in desc.
    obj->map->properties[idx].get = Desc->get;
    obj->map->properties[idx].set = Desc->set;
    obj->map->properties[idx].configurable = Desc->configurable;
    obj->map->properties[idx].enumerable = Desc->enumerable;
    obj->map->properties[idx].writable = Desc->writable;
    obj->map->properties[idx].value = Desc->value;
    

    /* 13. Return true. */
    return EJS_TRUE;
}

void 
_ejs_object_specop_finalize(EJSObject* obj)
{
    _ejs_propertymap_free (obj->map);
    obj->proto = _ejs_null;
    obj->map = NULL;
    obj->ops = NULL;
}

static void
_ejs_object_specop_scan (EJSObject* obj, EJSValueFunc scan_func)
{
    _ejs_propertymap_foreach_value (obj->map, scan_func);
    scan_func (obj->proto);
}
