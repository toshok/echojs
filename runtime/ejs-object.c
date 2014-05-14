/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#define DEBUG_PROPERTIES 0
#define DEBUG_GC 0

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "ejs-value.h"
#include "ejs-ops.h"
#include "ejs-object.h"
#include "ejs-number.h"
#include "ejs-string.h"
#include "ejs-regexp.h"
#include "ejs-date.h"
#include "ejs-array.h"
#include "ejs-map.h"
#include "ejs-set.h"
#include "ejs-typedarrays.h"
#include "ejs-function.h"
#include "ejs-proxy.h"
#include "ejs-symbol.h"
#include "ejs-error.h"
#include "ejs-xhr.h"

// ECMA262: 7.1.14
ejsval
ToPropertyKey(ejsval argument)
{
    if (EJSVAL_IS_SYMBOL(argument))
        return argument;
    return ToString(argument);
}

static uint32_t
PropertyKeyHash (ejsval argument)
{
    EJS_ASSERT(EJSVAL_IS_STRING(argument) || EJSVAL_IS_SYMBOL(argument));
    if (EJSVAL_IS_STRING(argument))
        return _ejs_string_hash(argument);
    else
        return _ejs_symbol_hash(argument);
}

// ECMA262: 8.10.1
EJSBool
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
EJSBool
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
void
ToPropertyDescriptor(ejsval O, EJSPropertyDesc *desc)
{
    /* 1. If Type(Obj) is not Object throw a TypeError exception. */
    if (!EJSVAL_IS_OBJECT(O)) {
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "ToPropertyDescriptor called on non-object");
    }
    EJSObject* obj = EJSVAL_TO_OBJECT(O);
    memset (desc, 0, sizeof(EJSPropertyDesc));

    /* 2. Let desc be the result of creating a new Property Descriptor that initially has no fields. */

    /* 3. If the result of calling the [[HasProperty]] internal method of Obj with argument "enumerable" is true, then */
    if (OP(obj,has_property)(O, _ejs_atom_enumerable)) {
        /*    a. Let enum be the result of calling the [[Get]] internal method of Obj with "enumerable". */
        /*    b. Set the [[Enumerable]] field of desc to ToBoolean(enum). */
        _ejs_property_desc_set_enumerable (desc, EJSVAL_TO_BOOLEAN(ToBoolean(OP(obj,get)(O, _ejs_atom_enumerable, O))));
    }
    /* 4. If the result of calling the [[HasProperty]] internal method of Obj with argument "configurable" is true, then */
    if (OP(obj,has_property)(O, _ejs_atom_configurable)) {
        /*    a. Let conf  be the result of calling the [[Get]] internal method of Obj with argument "configurable". */
        /*    b. Set the [[Configurable]] field of desc to ToBoolean(conf). */
        _ejs_property_desc_set_configurable (desc, EJSVAL_TO_BOOLEAN(ToBoolean(OP(obj,get)(O, _ejs_atom_configurable, O))));
    }
    /* 5. If the result of calling the [[HasProperty]] internal method of Obj with argument "value" is true, then */
    if (OP(obj,has_property)(O, _ejs_atom_value)) {
        /*    a. Let value be the result of calling the [[Get]] internal method of Obj with argument "value". */
        /*    b. Set the [[Value]] field of desc to value. */
        _ejs_property_desc_set_value (desc, OP(obj,get)(O, _ejs_atom_value, O));
    }
    /* 6. If the result of calling the [[HasProperty]] internal method of Obj with argument "writable" is true, then */
    if (OP(obj,has_property)(O, _ejs_atom_writable)) {
        /*    a. Let writable be the result of calling the [[Get]] internal method of Obj with argument "writable". */
        /*    b. Set the [[Writable]] field of desc to ToBoolean(writable). */
        _ejs_property_desc_set_writable (desc, EJSVAL_TO_BOOLEAN(ToBoolean(OP(obj,get)(O, _ejs_atom_writable, O))));
    }
    /* 7. If the result of calling the [[HasProperty]] internal method of Obj with argument "get" is true, then */
    if (OP(obj,has_property)(O, _ejs_atom_get)) {
        /*    a. Let getter be the result of calling the [[Get]] internal method of Obj with argument "get". */
        ejsval getter = OP(obj,get)(O, _ejs_atom_get, O);

        /*    b. If IsCallable(getter) is false and getter is not undefined, then throw a TypeError exception. */
        if (!EJSVAL_IS_CALLABLE(getter) && !EJSVAL_IS_UNDEFINED(getter)) {
            _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Getter must be a function");
        }

        /*    c. Set the [[Get]] field of desc to getter. */
        _ejs_property_desc_set_getter (desc, getter);
    }
    /* 8. If the result of calling the [[HasProperty]] internal method of Obj with argument "set" is true, then */
    if (OP(obj,has_property)(O, _ejs_atom_set)) {
        /*    a. Let setter be the result of calling the [[Get]] internal method of Obj with argument "set". */
        ejsval setter = OP(obj,get)(O, _ejs_atom_set, O);

        /*    b. If IsCallable(setter) is false and setter is not undefined, then throw a TypeError exception. */
        if (!EJSVAL_IS_CALLABLE(setter) && !EJSVAL_IS_UNDEFINED(setter)) {
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
    ejsval obj = _ejs_object_new(_ejs_Object_prototype, &_ejs_Object_specops);
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

EJSPropertyDesc*
_ejs_propertydesc_new ()
{
    return (EJSPropertyDesc*)calloc (sizeof(EJSPropertyDesc), 1);
}

void
_ejs_propertydesc_free (EJSPropertyDesc* desc)
{
    free (desc);
}

static int primes[] = {
    9, 17, 31, 67, 131, 257, 521, 1031, 2053, 4099
};
static int nprimes = sizeof(primes) / sizeof(primes[0]);

void
_ejs_propertymap_init (EJSPropertyMap *map)
{
    //_ejs_log ("%p: init\n", map);
    map->head_insert = map->tail_insert = NULL;
    map->buckets = NULL;
    map->nbuckets = 0;
    map->inuse = 0;
}

void
_ejs_propertymap_free (EJSPropertyMap *map)
{
    //_ejs_log ("%p: free\n", map);
    _EJSPropertyMapEntry* s = map->head_insert;
    while (s) {
        _EJSPropertyMapEntry* next = s->next_insert;
        free (s->desc);
        free (s);
        s = next;
    }
    free (map->buckets);
    free (map);
}

void
_ejs_propertymap_foreach_value (EJSPropertyMap* map, EJSValueFunc foreach_func)
{
    for (_EJSPropertyMapEntry *s = map->head_insert; s; s = s->next_insert) {
        if (_ejs_property_desc_has_value (s->desc))
            foreach_func(s->desc->value);
    }
}

void
_ejs_propertymap_foreach_property (EJSPropertyMap* map, EJSPropertyDescFunc foreach_func, void* data)
{
    for (_EJSPropertyMapEntry *s = map->head_insert; s; s = s->next_insert) {
        foreach_func (s->name, s->desc, data);
    }
}

void
_ejs_propertymap_remove (EJSPropertyMap *map, ejsval name)
{
    //_ejs_log ("%p: remove (%s)\n", map, ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(name)));
    if (map->inuse == 0) {
        //_ejs_log ("  map empty, returning early\n");
        return;
    }

    uint32_t hashcode = PropertyKeyHash(name);
    int bucket = (int)(hashcode % map->nbuckets);

    _EJSPropertyMapEntry* prev = NULL;
    _EJSPropertyMapEntry* s = map->buckets[bucket];
    while (s) {
        if (EJSVAL_TO_BOOLEAN(_ejs_op_strict_eq(s->name, name))) {
            //_ejs_log ("  found entry in bucket (hashcode %d, bucket %d)\n", hashcode, bucket);
            if (prev)
                prev->next_bucket = s->next_bucket;
            else
                map->buckets[bucket] = s->next_bucket;

            if (map->head_insert == s) {
                map->head_insert = s->next_insert;
                if (map->tail_insert == s)
                    map->tail_insert = NULL;
            }
            else {
                // FIXME do we need a doubly linked list for the insert-list instead of this second traversal?
                _EJSPropertyMapEntry *prev_insert;
                for (prev_insert = map->head_insert; prev_insert; prev_insert = prev_insert->next_insert) {
                    if (prev_insert->next_insert == s) {
                        //_ejs_log ("  found entry in insert list\n");
                        prev_insert->next_insert = s->next_insert;
                        if (map->tail_insert == s)
                            map->tail_insert = prev_insert;
                        break;
                    }
                }
            }
                

            free (s->desc);
            free (s);
            map->inuse --;
            return;
        }
        prev = s;
        s = s->next_bucket;
    }
}

EJSPropertyDesc*
_ejs_propertymap_lookup (EJSPropertyMap* map, ejsval name)
{
    if (map->inuse == 0)
        return NULL;

    uint32_t hashcode = PropertyKeyHash(name);
    int bucket = (int)(hashcode % map->nbuckets);

    for (_EJSPropertyMapEntry* s = map->buckets[bucket]; s; s = s->next_bucket) {
        if (EJSVAL_TO_BOOLEAN(_ejs_op_strict_eq(s->name, name)))
            return s->desc;
    }
    return NULL;
}

static void
_ejs_propertymap_rehash (EJSPropertyMap* map)
{
    // find the next prime up
    int new_size = -1;
    for (int i = 0; i < nprimes-1; i ++) {
        if (map->nbuckets == primes[i]) {
            new_size = primes[i+1];
            break;
        }
    }
    if (new_size == -1)
        abort();

    free (map->buckets);
    map->nbuckets = new_size;
    map->buckets = (_EJSPropertyMapEntry**)calloc (sizeof(_EJSPropertyMapEntry*), map->nbuckets);

    for (_EJSPropertyMapEntry *s = map->head_insert; s; s = s->next_insert) {
        uint32_t hashcode = PropertyKeyHash(s->name);
        int bucket = (int)(hashcode % map->nbuckets);

        s->next_bucket = map->buckets[bucket];
        map->buckets[bucket] = s;
    }
}

void
_ejs_propertymap_insert (EJSPropertyMap* map, ejsval name, EJSPropertyDesc* desc)
{
    //_ejs_log ("%p: insert (%s)\n", map, ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(name)));
    if (map->buckets == NULL) {
        map->nbuckets = primes[0];
        map->buckets = calloc (sizeof(_EJSPropertyMapEntry*), map->nbuckets);
    }

    uint32_t hashcode = PropertyKeyHash(name);
    int bucket = (int)(hashcode % map->nbuckets);

    for (_EJSPropertyMapEntry* s = map->buckets[bucket]; s; s = s->next_bucket) {
        if (EJSVAL_TO_BOOLEAN(_ejs_op_strict_eq(s->name, name))) {
            _ejs_propertydesc_free (s->desc);
            s->desc = desc;
            return;
        }
    }
    _EJSPropertyMapEntry* new_s = malloc(sizeof(_EJSPropertyMapEntry));
    new_s->name = name;
    new_s->desc = desc;
    new_s->next_bucket = map->buckets[bucket];
    new_s->next_insert = NULL;
    map->buckets[bucket] = new_s;
    map->inuse ++;

    if (!map->head_insert)
        map->head_insert = new_s;

    if (map->tail_insert)
        map->tail_insert->next_insert = new_s;
    map->tail_insert = new_s;

    if (map->inuse > map->nbuckets * 0.75) {
        _ejs_propertymap_rehash (map);
    }
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

    scan_func (iter->forObj);

    for (int i = 0; i < iter->num; i ++) {
        scan_func (iter->keys[i]);
    }
}

static EJSSpecOps _ejs_property_iterator_specops = {
    "<EJSPropertyIterator>",
    NULL, NULL, NULL,
    NULL, NULL, NULL,
    NULL, NULL, NULL,
    NULL, NULL, NULL,

    _ejs_property_iterator_specop_allocate,
    _ejs_property_iterator_specop_finalize,
    _ejs_property_iterator_specop_scan
};

static EJSBool
name_in_keys (ejsval name, ejsval *keys, int num)
{
    for (int i = 0; i < num; i ++) {
        if (!ucs2_strcmp(EJSVAL_TO_FLAT_STRING(name), EJSVAL_TO_FLAT_STRING(keys[i])))
            return EJS_TRUE;
    }
    return EJS_FALSE;
}

static void
collect_keys (ejsval objval, int *num, int *alloc, ejsval **keys)
{
    if (!EJSVAL_IS_OBJECT(objval))
        return;

    EJSObject *obj = EJSVAL_TO_OBJECT(objval);
    EJS_ASSERT(obj);

    for (_EJSPropertyMapEntry *s = obj->map->head_insert; s; s = s->next_insert) {
        if (_ejs_property_desc_is_enumerable (s->desc) && !name_in_keys (s->name, *keys, *num)) {
            if (*num == *alloc-1) {
                // we need to reallocate
                (*alloc) += 10;
                *keys = (ejsval*)realloc (*keys, (*alloc) * sizeof(ejsval));
            }
            (*keys)[(*num)++] = s->name;
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

    if (EJSVAL_IS_OBJECT(forVal)) {
        int num = 0;
        int alloc = 10;
        ejsval* keys;

        if (EJSVAL_IS_ARRAY(forVal))
            alloc += EJS_ARRAY_LEN(forVal);

        keys = (ejsval*)malloc (alloc * sizeof(ejsval));

        if (EJSVAL_IS_ARRAY(forVal)) {
            // iterate over array keys first then additional properties
            for (int i = 0; i < EJS_ARRAY_LEN(forVal); i ++) {
                // XXX skip holes
                keys[num++] = ToPropertyKey(NUMBER_TO_EJSVAL(i));
            }
        }

        collect_keys (forVal, &num, &alloc, &keys);

        iterator->forObj = forVal;
        iterator->num = num;
        if (num > 0) {
            iterator->keys = keys;
        }
        else {
            iterator->keys = NULL;
            free (keys);
        }

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
    obj->ops = ops ? ops : &_ejs_Object_specops;
    obj->map = calloc (sizeof(EJSPropertyMap), 1);
    _ejs_propertymap_init (obj->map);
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

    EJSSpecOps *ops = NULL;

    if      (EJSVAL_EQ(proto, _ejs_Object_prototype)) ops = &_ejs_Object_specops;
    else if (EJSVAL_EQ(proto, _ejs_Array_proto))      ops = &_ejs_Array_specops;
    else if (EJSVAL_EQ(proto, _ejs_String_prototype)) ops = &_ejs_String_specops;
    else if (EJSVAL_EQ(proto, _ejs_Map_prototype))    ops = &_ejs_Map_specops;
    else if (EJSVAL_EQ(proto, _ejs_Set_prototype))    ops = &_ejs_Set_specops;
    else if (EJSVAL_EQ(proto, _ejs_Proxy_prototype))  ops = &_ejs_Proxy_specops;
    else if (EJSVAL_EQ(proto, _ejs_Number_proto))     ops = &_ejs_Number_specops;
    else if (EJSVAL_EQ(proto, _ejs_RegExp_proto))     ops = &_ejs_RegExp_specops;
    else if (EJSVAL_EQ(proto, _ejs_Date_proto))       ops = &_ejs_Date_specops;
    else if (EJSVAL_EQ(proto, _ejs_ArrayBuffer_proto)) ops = &_ejs_ArrayBuffer_specops;
    else if (EJSVAL_EQ(proto, _ejs_DataView_proto)) ops = &_ejs_DataView_specops;
    else if (EJSVAL_EQ(proto, _ejs_EvalError_proto))  ops = &_ejs_Error_specops;
    else if (EJSVAL_EQ(proto, _ejs_RangeError_proto))  ops = &_ejs_Error_specops;
    else if (EJSVAL_EQ(proto, _ejs_ReferenceError_proto))  ops = &_ejs_Error_specops;
    else if (EJSVAL_EQ(proto, _ejs_SyntaxError_proto))  ops = &_ejs_Error_specops;
    else if (EJSVAL_EQ(proto, _ejs_TypeError_proto))  ops = &_ejs_Error_specops;
    else if (EJSVAL_EQ(proto, _ejs_URIError_proto))  ops = &_ejs_Error_specops;
    else if (EJSVAL_EQ(proto, _ejs_Error_proto))  ops = &_ejs_Error_specops;
    else if (EJSVAL_EQ(proto, _ejs_XMLHttpRequest_proto))  ops = &_ejs_XMLHttpRequest_specops;
    else {
        for (int i = 0; i < EJS_TYPEDARRAY_TYPE_COUNT; i ++) {
            if (EJSVAL_EQ(proto, _ejs_typed_array_protos[i]))
                ops = _ejs_typed_array_specops[i];
        }
    }
    
    if (!ops)
        ops = EJSVAL_TO_OBJECT(proto)->ops;
    
    ejsval objval = _ejs_object_new (proto, ops);
    //EJSObject* obj = EJSVAL_TO_OBJECT(objval);
    //_ejs_log ("ejs_object_create returned %p\n", obj);
    return objval;
}

void
_ejs_Class_initialize (EJSSpecOps *child, EJSSpecOps* parent)
{
#define MAYBE_INHERIT(p) EJS_MACRO_START                                \
        if ((void*)(child->p) == OP_INHERIT)                            \
            child->p = parent->p;                                       \
    EJS_ASSERT((void*)(child->p) != OP_INHERIT);                        \
    EJS_MACRO_END

#define MAYBE_INHERIT_DISALLOW_NULL(p) EJS_MACRO_START                  \
        MAYBE_INHERIT(p);                                               \
    EJS_ASSERT(child->p);                                               \
    EJS_MACRO_END
    
    MAYBE_INHERIT_DISALLOW_NULL(get_prototype_of);
    MAYBE_INHERIT_DISALLOW_NULL(set_prototype_of);
    MAYBE_INHERIT_DISALLOW_NULL(get);
    MAYBE_INHERIT_DISALLOW_NULL(get_own_property);
    MAYBE_INHERIT_DISALLOW_NULL(get_property);
    MAYBE_INHERIT_DISALLOW_NULL(put);
    MAYBE_INHERIT_DISALLOW_NULL(can_put);
    MAYBE_INHERIT_DISALLOW_NULL(has_property);
    MAYBE_INHERIT_DISALLOW_NULL(_delete);
    MAYBE_INHERIT_DISALLOW_NULL(default_value);
    MAYBE_INHERIT_DISALLOW_NULL(define_own_property);
    MAYBE_INHERIT(has_instance);
    MAYBE_INHERIT_DISALLOW_NULL(allocate);
    MAYBE_INHERIT_DISALLOW_NULL(finalize);
    MAYBE_INHERIT_DISALLOW_NULL(scan);
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
        _ejs_log ("setprop on primitive.  ignoring\n");
        EJS_NOT_IMPLEMENTED();
    }

    OP(EJSVAL_TO_OBJECT(val), put)(val, key, value, val, EJS_FALSE);

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
            _ejs_log ("last property lookup was for: %s\n", last_utf8);
            free (last_utf8);
        }
#endif
        char msg_buf[256];
        snprintf (msg_buf, sizeof(msg_buf)-1, "Cannot read property '%s' of %s", key_utf8, EJSVAL_IS_NULL(obj) ? "null" : "undefined");
        free (key_utf8);

        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, msg_buf);
    }

    if (EJSVAL_IS_PRIMITIVE(obj)) {
        if (EJSVAL_IS_STRING(obj)) {
            if (!ucs2_strcmp(EJSVAL_TO_FLAT_STRING(ToString(key)), _ejs_ucs2_length))
                return NUMBER_TO_EJSVAL(EJSVAL_TO_STRING(obj)->length);
        }
        obj = ToObject(obj);
    }

#if DEBUG_LAST_LOOKUP
    if (last_lookup) free (last_lookup);
    last_lookup = ucs2_strdup (EJSVAL_TO_FLAT_STRING(ToString(key)));
#endif

    return OP(EJSVAL_TO_OBJECT(obj),get)(obj, key, obj);
}

ejsval
_ejs_global_setprop (ejsval key, ejsval value)
{
    return _ejs_object_setprop(_ejs_global, key, value);
}

ejsval
_ejs_global_getprop (ejsval key)
{
    return _ejs_object_getprop(_ejs_global, key);
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
        _ejs_log ("throw ReferenceError\n");
        abort();
    }

    if (EJSVAL_IS_PRIMITIVE(val)) {
        _ejs_log ("setprop on primitive.  ignoring\n");
        return value;
    }

    return _ejs_object_setprop (val, _ejs_string_new_utf8(key), value);
}

ejsval
_ejs_object_getprop_utf8 (ejsval obj, const char *key)
{
    if (EJSVAL_IS_NULL(obj) || EJSVAL_IS_UNDEFINED(obj)) {
        _ejs_log ("throw TypeError, key is %s\n", key);
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
        _ejs_log ("undefined\n");
    }
    else if (EJSVAL_IS_NULL(val)) {
        _ejs_log ("null\n");
    }
    else if (EJSVAL_IS_NUMBER(val)) {
        _ejs_log ("number: " EJS_NUMBER_FORMAT "\n", EJSVAL_TO_NUMBER(val));
    }
    else if (EJSVAL_IS_BOOLEAN(val)) {
        _ejs_log ("boolean: %s\n", EJSVAL_TO_BOOLEAN(val) ? "true" : "false");
    }
    else if (EJSVAL_IS_STRING(val)) {
        char* val_utf8 = ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(val));
        _ejs_log ("string: '%s'\n", val_utf8);
        free (val_utf8);
    }
    else if (EJSVAL_IS_OBJECT(val)) {
        _ejs_log ("<object %s>\n", CLASSNAME(EJSVAL_TO_OBJECT(val)));
    }
}

ejsval _ejs_Object EJSVAL_ALIGNMENT;
ejsval _ejs_Object__proto__ EJSVAL_ALIGNMENT;
ejsval _ejs_Object_prototype EJSVAL_ALIGNMENT;

static ejsval
_ejs_Object_impl (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    if (EJSVAL_IS_UNDEFINED(_this)) {
        // ECMA262: 15.2.1.1
    
        _ejs_log ("called Object() as a function!\n");
        return _ejs_null;
    }
    else {
        // ECMA262: 15.2.2

        _ejs_log ("called Object() as a constructor!\n");
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
        _ejs_log ("throw TypeError, argument isn't an Object\n");
        EJS_NOT_IMPLEMENTED();
    }
    
    /* 2. Return the value of the [[Prototype]] internal property of O. */
    return OP(EJSVAL_TO_OBJECT(obj),get_prototype_of)(obj);
}

// ECMA262: 19.1.2.18
// Object.setPrototypeOf ( O, proto )
static ejsval
_ejs_Object_setPrototypeOf (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    // When the setPrototypeOf function is called with arguments O and proto, the following steps are taken:

    ejsval O = _ejs_undefined;
    ejsval proto = _ejs_undefined;
    if (argc > 0)
        O = args[0];
    if (argc > 1)
        proto = args[1];

    // 1. Let O be CheckObjectCoercible(O).
    // 2. ReturnIfAbrupt(O).
    if (!EJSVAL_IS_OBJECT(O) && !EJSVAL_IS_NULL(O)) {
        _ejs_log ("throw TypeError\n");
        EJS_NOT_IMPLEMENTED();
    }
    // 3. If Type(proto) is neither Object nor Null, then throw a TypeError exception.
    if (!EJSVAL_IS_OBJECT(proto) && !EJSVAL_IS_NULL(proto)) {
        _ejs_log ("throw TypeError\n");
        EJS_NOT_IMPLEMENTED();
    }

    // 4. If Type(O) is not Object, then return O.
    if (!EJSVAL_IS_OBJECT(O))
        return O;
    // 5. Let status be the result of calling the [[SetPrototypeOf]] internal method of O with argument proto.
    EJSBool status = OP(EJSVAL_TO_OBJECT(O),set_prototype_of)(O,proto);
    // 6. ReturnIfAbrupt(status).
    // 7. If status is false, then throw a TypeError exception.
    if (!status) {
        _ejs_log ("throw TypeError\n");
        EJS_NOT_IMPLEMENTED();
    }
    // 8. Return O.
    return O;
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
        _ejs_log ("throw TypeError, O isn't an Object\n");
        EJS_NOT_IMPLEMENTED();
    }
    EJSObject *obj = EJSVAL_TO_OBJECT(O);

    /* 2. Let name be ToString(P). */
    ejsval name = ToPropertyKey(P);

    /* 3. Let desc be the result of calling the [[GetOwnProperty]] internal method of O with argument name. */
    EJSPropertyDesc* desc = OP(obj, get_own_property)(O, name);

    /* 4. Return the result of calling FromPropertyDescriptor(desc) (8.10.4).  */
    return FromPropertyDescriptor(desc);
}

// ECMA262: 19.1.2.7
static ejsval
_ejs_Object_getOwnPropertyNames (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval O = _ejs_undefined;
    if (argc > 0) O = args[0];

    /* 1. If Type(O) is not Object throw a TypeError exception. */
    if (!EJSVAL_IS_OBJECT(O)) {
        _ejs_log ("throw TypeError, _this isn't an Object\n");
        EJS_NOT_IMPLEMENTED();
    }
    EJSObject* O_ = EJSVAL_TO_OBJECT(O);

    /* 2. Let array be the result of creating a new object as if by the expression new Array () where Array is the standard built-in constructor with that name. */
    ejsval arr = _ejs_array_new(0, EJS_FALSE);

    /* 3. Let n be 0. */

    /* 4. For each named own property P of O */
    for (_EJSPropertyMapEntry* s = O_->map->head_insert; s; s = s->next_insert) {
        if (!_ejs_property_desc_is_enumerable(s->desc))
            continue;

        /*    a. Let name be the String value that is the name of P. */
        ejsval name = s->name;

        if (!EJSVAL_IS_SYMBOL(name)) {
            /*    b. Call the [[DefineOwnProperty]] internal method of array with arguments ToString(n), the
                  PropertyDescriptor {[[Value]]: name, [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: 
                  true}, and false. */
            _ejs_array_push_dense(arr, 1, &name);
        }

        /*    c. Increment n by 1. */
    }
    /* 5. Return array. */

    return arr;
}

// ECMA262: 19.1.2.8
static ejsval
_ejs_Object_getOwnPropertySymbols (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval O = _ejs_undefined;
    if (argc > 0)
        O = args[0];

    /* 1. If Type(O) is not Object throw a TypeError exception. */
    if (!EJSVAL_IS_OBJECT(O)) {
        _ejs_log ("throw TypeError, _this isn't an Object\n");
        EJS_NOT_IMPLEMENTED();
    }
    EJSObject* O_ = EJSVAL_TO_OBJECT(O);

    /* 2. Let array be the result of creating a new object as if by the expression new Array () where Array is the 
          standard built-in constructor with that name. */
    ejsval arr = _ejs_array_new(0, EJS_FALSE);

    /* 3. Let n be 0. */

    /* 4. For each named own property P of O */
    for (_EJSPropertyMapEntry* s = O_->map->head_insert; s; s = s->next_insert) {
        if (!_ejs_property_desc_is_enumerable(s->desc))
            continue;

        /*    a. Let name be the String value that is the name of P. */
        ejsval name = s->name;

        if (EJSVAL_IS_SYMBOL(name)) {
            /*    b. Call the [[DefineOwnProperty]] internal method of array with arguments ToString(n), the
                  PropertyDescriptor {[[Value]]: name, [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: 
                  true}, and false. */
            _ejs_array_push_dense(arr, 1, &name);
        }

        /*    c. Increment n by 1. */
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
        _ejs_log ("throw TypeError, O isn't an Object or null\n");
        EJS_NOT_IMPLEMENTED();
    }

    /* 2. Let obj be the result of creating a new object as if by the expression new Object() where Object is the  */
    /*    standard built-in constructor with that name */
    ejsval obj = _ejs_object_new(_ejs_null, &_ejs_Object_specops);

    /* 3. Set the [[Prototype]] internal property of obj to O. */
    EJSVAL_TO_OBJECT(obj)->proto = O;

    /* 4. If the argument Properties is present and not undefined, add own properties to obj as if by calling the  */
    /*    standard built-in function Object.defineProperties with arguments obj and Properties. */
    if (!EJSVAL_IS_UNDEFINED(Properties)) {
        ejsval definePropertyArgs[] = { obj, Properties };
        _ejs_Object_defineProperties (env, _this, 2, definePropertyArgs);
    }

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
        char msg[200];
        ejsval name = ToString(P);
        char* utf8_name = ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(name));
        snprintf (msg, sizeof(msg)-1, "defineProperty(%s) called on non-object", utf8_name);
        free (utf8_name);
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, msg);
    }
    EJSObject *obj = EJSVAL_TO_OBJECT(O);

    /* 2. Let name be ToString(P). */
    // we skip this and handle it in object_specop_define_own_property
    //ejsval name = ToString(P);
    ejsval name = P;

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
        _ejs_log ("throw TypeError, _this isn't an Object\n");
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
    for (_EJSPropertyMapEntry *s = props_obj->map->head_insert; s; s = s->next_insert) {
        if (_ejs_property_desc_is_enumerable (s->desc))
            names_len ++;
    }

    if (names_len == 0) {
        /* no enumerable properties, bail early */
        return O;
    }

    ejsval* names = malloc(names_len * sizeof(ejsval));
    int n = 0;
    for (_EJSPropertyMapEntry *s = props_obj->map->head_insert; s; s = s->next_insert) {
        if (_ejs_property_desc_is_enumerable(s->desc))
            names[n++] = s->name;
    }

    /* 4. Let descriptors be an empty internal List. */
    DefinePropertiesPair *descriptors = malloc(sizeof(DefinePropertiesPair) * names_len);

    /* 5. For each element P of names in list order, */
    for (int n = 0; n < names_len; n ++) {
        ejsval P = names[n];

        /* a. Let descObj be the result of calling the [[Get]] internal method of props with P as the argument. */
        ejsval descObj = OP(props_obj,get)(props, P, props);

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
        _ejs_log ("throw TypeError, O isn't an Object\n");
        EJS_NOT_IMPLEMENTED();
    }
    EJSObject* obj = EJSVAL_TO_OBJECT(O);

    // XXX this needs special handling for arrays (and other objects?)

    /* 2. For each named own property name P of O, */
    for (_EJSPropertyMapEntry* s = obj->map->head_insert; s; s = s->next_insert) {

        /*    a. Let desc be the result of calling the [[GetOwnProperty]] internal method of O with P. */
        EJSPropertyDesc* desc = s->desc;

        /*    b. If desc.[[Configurable]] is true, set desc.[[Configurable]] to false. */
        if (_ejs_property_desc_is_configurable(desc))
            _ejs_property_desc_set_configurable(desc, EJS_FALSE);

        /*    c. Call the [[DefineOwnProperty]] internal method of O with P, desc, and true as arguments. */
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
        _ejs_log ("throw TypeError, O isn't an Object\n");
        EJS_NOT_IMPLEMENTED();
    }
    EJSObject* obj = EJSVAL_TO_OBJECT(O);

    // XXX this needs special handling for arrays (and other objects?)

    /* 2. For each named own property name P of O, */
    ejsval iter = _ejs_property_iterator_new(O);
    while (_ejs_property_iterator_next(iter, EJS_TRUE)) {
        ejsval P = _ejs_property_iterator_current(iter);

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
        _ejs_log ("throw TypeError, O isn't an Object\n");
        EJS_NOT_IMPLEMENTED();
    }
    EJSObject* obj = EJSVAL_TO_OBJECT(O);

    /* 2. Set the [[Extensible]] internal property of O to false. */
    EJS_OBJECT_CLEAR_EXTENSIBLE(obj);

    /* 3. Return O. */
    return O;
}

// ECMA262: 19.1.2.10
static ejsval
_ejs_Object_is (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval value1 = _ejs_undefined;
    ejsval value2 = _ejs_undefined;

    if (argc > 0) value1 = args[0];
    if (argc > 1) value2 = args[1];

    return SameValue(value1, value2) ? _ejs_true : _ejs_false;
}


// ECMA262: 15.2.3.11
static ejsval
_ejs_Object_isSealed (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval O = _ejs_undefined;
    if (argc > 0) O = args[0];

    /* 1. If Type(O) is not Object throw a TypeError exception. */
    if (!EJSVAL_IS_OBJECT(O)) {
        _ejs_log ("throw TypeError, O isn't an Object\n");
        EJS_NOT_IMPLEMENTED();
    }
    EJSObject* obj = EJSVAL_TO_OBJECT(O);

    // XXX this needs special handling for arrays (and other objects?)

    /* 2. For each named own property name P of O, */
    ejsval iter = _ejs_property_iterator_new(O);
    while (_ejs_property_iterator_next(iter, EJS_TRUE)) {
        ejsval P = _ejs_property_iterator_current(iter);

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
        _ejs_log ("throw TypeError, O isn't an Object\n");
        EJS_NOT_IMPLEMENTED();
    }
    EJSObject* obj = EJSVAL_TO_OBJECT(O);

    // XXX this needs special handling for arrays (and other objects?)

    /* 2. For each named own property name P of O, */
    ejsval iter = _ejs_property_iterator_new(O);
    while (_ejs_property_iterator_next(iter, EJS_TRUE)) {
        ejsval P = _ejs_property_iterator_current(iter);

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
        _ejs_log ("throw TypeError, O isn't an Object\n");
        EJS_NOT_IMPLEMENTED();
    }
    EJSObject* obj = EJSVAL_TO_OBJECT(O);

    /* 2. Return the Boolean value of the [[Extensible]] internal property of O */
    return BOOLEAN_TO_EJSVAL(EJS_OBJECT_IS_EXTENSIBLE(obj));
}

// ECMA262: 19.1.2.14
static ejsval
_ejs_Object_keys (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    // toshok - is this really not identical to Object.getOwnPropertyNames?
    return _ejs_Object_getOwnPropertyNames(env, _this, argc, args);
}

// ECMA262: 15.2.4.2
ejsval
_ejs_Object_prototype_toString (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    char buf[1024];
    const char *classname;
    if (EJSVAL_IS_NULL(_this))
        classname = "Null";
    else if (EJSVAL_IS_UNDEFINED(_this))
        classname = "Undefined";
    else
        classname = CLASSNAME(EJSVAL_TO_OBJECT(ToObject(_this)));
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
    ejsval toString = OP(O_, get)(O, _ejs_atom_toString, O);

    /* 3. If IsCallable(toString) is false, throw a TypeError exception. */
    if (!EJSVAL_IS_CALLABLE(toString))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "toString property is not callable");

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
        V = OP(EJSVAL_TO_OBJECT(V),get_prototype_of)(V);

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
    ejsval P = ToPropertyKey(V);

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
    _ejs_gc_add_root (&_ejs_Object__proto__);
    _ejs_gc_add_root (&_ejs_Object_prototype);

    EJSObject* prototype = _ejs_gc_new(EJSObject);
    _ejs_Object_prototype = OBJECT_TO_EJSVAL(prototype);
    _ejs_init_object (prototype, _ejs_null, &_ejs_Object_specops);

    EJSFunction* __proto__ = _ejs_gc_new(EJSFunction);
    _ejs_Object__proto__ = OBJECT_TO_EJSVAL(__proto__);
    _ejs_init_object ((EJSObject*)__proto__, _ejs_Object_prototype, &_ejs_Function_specops);
    __proto__->func = _ejs_Function_empty;
    __proto__->env = _ejs_null;

    _ejs_object_define_value_property (_ejs_Object__proto__, _ejs_atom_name, _ejs_atom_empty, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_CONFIGURABLE | EJS_PROP_NOT_WRITABLE);
}

void
_ejs_object_init (ejsval global)
{
    _ejs_Object = _ejs_function_new_without_proto (_ejs_null, _ejs_atom_Object, (EJSClosureFunc)_ejs_Object_impl);
    _ejs_object_setprop (global, _ejs_atom_Object, _ejs_Object);

    // ECMA262 15.2.3.1
    _ejs_object_define_value_property (_ejs_Object, _ejs_atom_prototype, _ejs_Object_prototype,
                                       EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_CONFIGURABLE | EJS_PROP_NOT_WRITABLE);
    // ECMA262: 15.2.4.1
    _ejs_object_define_value_property (_ejs_Object_prototype, _ejs_atom_constructor, _ejs_Object,
                                       EJS_PROP_NOT_ENUMERABLE | EJS_PROP_CONFIGURABLE | EJS_PROP_WRITABLE);

#define OBJ_METHOD(x) EJS_INSTALL_ATOM_FUNCTION_FLAGS(_ejs_Object, x, _ejs_Object_##x, EJS_PROP_NOT_ENUMERABLE)
#define PROTO_METHOD(x) EJS_INSTALL_ATOM_FUNCTION_FLAGS(_ejs_Object_prototype, x, _ejs_Object_prototype_##x, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_WRITABLE | EJS_PROP_CONFIGURABLE)

    OBJ_METHOD(getPrototypeOf);
    OBJ_METHOD(setPrototypeOf);
    OBJ_METHOD(getOwnPropertyDescriptor);
    OBJ_METHOD(getOwnPropertyNames);
    OBJ_METHOD(getOwnPropertySymbols);
    OBJ_METHOD(create);
    OBJ_METHOD(defineProperty);
    OBJ_METHOD(defineProperties);
    OBJ_METHOD(seal);
    OBJ_METHOD(freeze);
    OBJ_METHOD(preventExtensions);
    OBJ_METHOD(is);
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
}


// [[GetPrototypeOf]] ECMA262: 9.1.1
static ejsval
_ejs_object_specop_get_prototype_of (ejsval obj)
{
    return EJSVAL_TO_OBJECT(obj)->proto;
}

// [[SetPrototypeOf]] ECMA262: 9.1.1
static EJSBool
_ejs_object_specop_set_prototype_of (ejsval O, ejsval V)
{
    EJSObject* O_ = EJSVAL_TO_OBJECT(O);

    // When the [[SetPrototypeOf]] internal method of O is called with argument V the following steps are taken:

    // 1. Assert: Either Type(V) is Object or Type(V) is Null.
    if (!EJSVAL_IS_OBJECT(V) && !EJSVAL_IS_NULL(V)) {
        _ejs_log ("throw TypeError\n");
        EJS_NOT_IMPLEMENTED();
    }
        
    // 2. Let extensible be the value of the [[Extensible]] internal slot of O.
    EJSBool extensible = EJS_OBJECT_IS_EXTENSIBLE(O_);

    // 3. Let current be the value of the [[Prototype]] internal slot of O.
    ejsval current = O_->proto;

    // 4. If SameValue(V, current), then return true.
    if (SameValue(V, current))
        return EJS_TRUE;

    // 5. If extensible is false, then return false.
    if (!extensible)
        return EJS_FALSE;

    // 6. If V is not null, then
    if (!EJSVAL_IS_NULL(V)) {
        //    a. Let p be V.
        ejsval p = V;
        //    b. Repeat, while p is not null
        while (!EJSVAL_IS_NULL(p)) {
            //       i.   If SameValue(p, O) is true, then return false.
            if (SameValue(p, O))
                return EJS_FALSE;
            //       ii.  Let nextp be the result of calling the [[GetPrototypeOf]] internal method of p with no arguments.
            ejsval nextp = OP(EJSVAL_TO_OBJECT(p),get_prototype_of)(p);
            //       iii. ReturnIfAbrupt(nextp).
            //       iv.  Let p be nextp.
            p = nextp;
        }
    }

    // toshok -- why are re-doing this?  steps 4 and 5 above appear to already have taken care of these cases.

    // 7. Let extensible be the value of the [[Extensible]] internal slot of O.
    // 8. If extensible is false, then
    //    a. Let current2 be the value of the [[Prototype]] internal slot of O.
    //    b. If SameValue(V, current2) is true, then return true.
    //    c. Return false.


    // 9. Set the value of the [[Prototype]] internal slot of O to V.
    O_->proto = V;

    // 10. Return true.
    return EJS_TRUE;
}

// ECMA262: 8.12.3
static ejsval
_ejs_object_specop_get (ejsval obj_, ejsval propertyName, ejsval receiver)
{
    ejsval pname = ToPropertyKey(propertyName);

    if (EJSVAL_IS_STRING(pname) && !ucs2_strcmp(_ejs_ucs2___proto__, EJSVAL_TO_FLAT_STRING(pname)))
        return OP(EJSVAL_TO_OBJECT(obj_),get_prototype_of) (obj_);

    /* 1. Let desc be the result of calling the [[GetProperty]] internal method of O with property name P. */
    EJSPropertyDesc* desc = OP(EJSVAL_TO_OBJECT(obj_),get_property) (obj_, pname);
    /* 2. If desc is undefined, return undefined. */
    if (desc == NULL) {
        // _ejs_log ("property lookup on a %s object, propname %s => undefined\n", CLASSNAME(EJSVAL_TO_OBJECT(obj_)), EJSVAL_TO_FLAT_STRING(pname));
        return _ejs_undefined;
    }

    /* 3. If IsDataDescriptor(desc) is true, return desc.[[Value]]. */
    if (IsDataDescriptor(desc)) {
        // if (EJSVAL_IS_UNDEFINED(desc->value))
        //     _ejs_log ("property lookup on a %s object, propname %s => undefined\n", CLASSNAME(EJSVAL_TO_OBJECT(obj_)), EJSVAL_TO_FLAT_STRING(pname));
        return desc->value;
    }

    /* 4. Otherwise, IsAccessorDescriptor(desc) must be true so, let getter be desc.[[Get]]. */
    ejsval getter = _ejs_property_desc_get_getter(desc);

    /* 5. If getter is undefined, return undefined. */
    if (EJSVAL_IS_UNDEFINED(getter)) {
        // _ejs_log ("property lookup on a %s object, propname %s => undefined getter\n", CLASSNAME(EJSVAL_TO_OBJECT(obj_)), EJSVAL_TO_FLAT_STRING(pname));
        return _ejs_undefined;
    }

    /* 6. Return the result calling the [[Call]] internal method of getter providing O as the this value and providing no arguments */
    return _ejs_invoke_closure (getter, obj_, 0, NULL);
}

// ECMA262: 8.12.1
static EJSPropertyDesc*
_ejs_object_specop_get_own_property (ejsval obj, ejsval propertyName)
{
    ejsval property_str = ToPropertyKey(propertyName);
    EJSObject* obj_ = EJSVAL_TO_OBJECT(obj);

    return _ejs_propertymap_lookup (obj_->map, property_str);
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
    ejsval proto = OP(obj,get_prototype_of)(O);

    /* 4. If proto is null, return undefined. */
    if (EJSVAL_IS_NULL(proto))
        return NULL;

    EJSObject* proto_obj = EJSVAL_TO_OBJECT(proto);

    /* 5. Return the result of calling the [[GetProperty]] internal method of proto with argument P. */
    return OP(proto_obj, get_property)(proto, P);
}

// ECMA262: 8.12.5
static void
_ejs_object_specop_put (ejsval O, ejsval P, ejsval V, ejsval Receiver, EJSBool Throw)
{
    EJSObject* obj = EJSVAL_TO_OBJECT(O);

    if (!EJSVAL_IS_SYMBOL(P) && !EJSVAL_IS_STRING(P))
        P = ToPropertyKey(P);

    /* 1. If the result of calling the [[CanPut]] internal method of O with argument P is false, then */
    if (!OP(obj,can_put)(O, P)) {
        /*    a. If Throw is true, then throw a TypeError exception. */
        if (Throw) {
            _ejs_log ("throw TypeError\n");
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
        EJS_ASSERT (EJSVAL_IS_FUNCTION(setter));
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
    ejsval proto = OP(obj,get_prototype_of)(O);

    /* 4. If proto is null, then return the value of the [[Extensible]] internal property of O. */
    if (EJSVAL_IS_NULL(proto))
        return EJS_OBJECT_IS_EXTENSIBLE(obj);

    /* 5. Let inherited be the result of calling the [[GetProperty]] internal method of proto with property name P. */
    EJSPropertyDesc* inherited = OP(EJSVAL_TO_OBJECT(proto),get_property)(proto, P);

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
        _ejs_propertymap_remove (obj->map, P);
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
        EJSPropertyDesc *dest = _ejs_propertydesc_new();

        /*    a. If  IsGenericDescriptor(Desc) or IsDataDescriptor(Desc) is true, then */
        if (IsGenericDescriptor(Desc) || IsDataDescriptor(Desc)) {
            /*       i. Create an own data property named P of object O whose [[Value]], [[Writable]],  */
            /*          [[Enumerable]] and [[Configurable]] attribute values are described by Desc. If the value of */
            /*          an attribute field of Desc is absent, the attribute of the newly created property is set to its  */
            /*          default value. */
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
            if (_ejs_property_desc_has_getter (Desc))
                _ejs_property_desc_set_getter (dest, _ejs_property_desc_get_getter (Desc));
            if (_ejs_property_desc_has_setter (Desc))
                _ejs_property_desc_set_setter (dest, _ejs_property_desc_get_setter (Desc));
            if (_ejs_property_desc_has_configurable (Desc))
                _ejs_property_desc_set_configurable (dest, _ejs_property_desc_is_configurable (Desc));
            if (_ejs_property_desc_has_enumerable (Desc))
                _ejs_property_desc_set_enumerable (dest, _ejs_property_desc_is_enumerable (Desc));
        }
        _ejs_propertymap_insert (obj->map, P, dest);

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
    EJSPropertyDesc* dest = _ejs_propertymap_lookup (obj->map, P);

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
    _ejs_propertymap_free (obj->map);
}

static void
scan_property (ejsval name, EJSPropertyDesc *desc, EJSValueFunc scan_func)
{
#if DEBUG_GC
    _ejs_log ("scan property desc = %p, name = %s\n", desc, ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(name)));
#endif

    scan_func (name);

    if (_ejs_property_desc_has_value (desc)) {
#if DEBUG_GC
        if (EJSVAL_IS_OBJECT(desc->value)) {
        	_ejs_log ("   has_value %p\n", EJSVAL_TO_OBJECT(desc->value));
    	}
        else
        	_ejs_log ("   has_value\n");
#endif
        scan_func (desc->value);
    }
    if (_ejs_property_desc_has_getter (desc)) {
#if DEBUG_GC
        _ejs_log ("   has_getter\n");
#endif
        scan_func (desc->getter); 
    }
    if (_ejs_property_desc_has_setter (desc)) {
#if DEBUG_GC
        _ejs_log ("   has_setter\n");
#endif
        scan_func (desc->setter);
    }
}

static void
_ejs_object_specop_scan (EJSObject* obj, EJSValueFunc scan_func)
{
    _ejs_propertymap_foreach_property (obj->map, (EJSPropertyDescFunc)scan_property, scan_func);
    scan_func (obj->proto);
}

EJS_DEFINE_CLASS(Object,
                 _ejs_object_specop_get_prototype_of,
                 _ejs_object_specop_set_prototype_of,
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
                 )
