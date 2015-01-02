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
#include "ejs-arguments.h"
#include "ejs-boolean.h"
#include "ejs-proxy.h"
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

// ES6 7.3.1
// Get (O, P)
ejsval
Get (ejsval O, ejsval P)
{
    // 1. Assert: Type(O) is Object. 
    EJS_ASSERT(EJSVAL_IS_OBJECT(O));

    // 2. Assert: IsPropertyKey(P) is true. 

    // 3. Return the result of calling the [[Get]] internal method of O passing P and O as the arguments
    return OP(EJSVAL_TO_OBJECT(O),Get)(O, P, O);
}

// ECMA262: 7.3.2 Put (O, P, V, Throw) 
ejsval
Put (ejsval O, ejsval P, ejsval V, EJSBool Throw)
{
    // 1. Assert: Type(O) is Object. 
    EJS_ASSERT(EJSVAL_IS_OBJECT(O));

    // 2. Assert: IsPropertyKey(P) is true. 
    // 3. Assert: Type(Throw) is Boolean. 

    // 4. Let success be the result of calling the [[Set]] internal method of O passing P, V, and O as the arguments. 
    // 5. ReturnIfAbrupt(success). 
    EJSBool success = OP(EJSVAL_TO_OBJECT(O),Set)(O, P, V, O);
    // 6. If success is false and Throw is true, then throw a TypeError exception. 
    if (!success && Throw)
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "[[Set]] failed");
        
    // 7. Return success. 
    return BOOLEAN_TO_EJSVAL(success);
}

// ES6 7.3.2
// GetV (V, P)
ejsval
GetV (ejsval V, ejsval P)
{
    // 1. If V is undefined or null, then throw a TypeError exception.
    if (EJSVAL_IS_UNDEFINED(V) || EJSVAL_IS_NULL(V))
        return _ejs_undefined;

    // 2. If Type(V) is Object, then return Get(V, P).
    if (EJSVAL_IS_OBJECT(V))
        return Get(V, P);

    // 3. Let box be ToObject(V).
    // 4. ReturnIfAbrupt(box).
    ejsval box = ToObject(V);

    // 5. Return the result of calling the [[Get]] internal method of box passing P and V as the arguments.
    return OP(EJSVAL_TO_OBJECT(box),Get)(box, P, V);
}

// ES6 7.3.8
// GetMethod (O, P) 
ejsval
GetMethod (ejsval O, ejsval P)
{
    // 1. Assert: IsPropertyKey(P) is true.
    // 2. Let func be GetV(O, P).
    // 3. ReturnIfAbrupt(func).
    ejsval func = GetV(O, P);

    // 4. If func is either undefined or null, then return undefined.
    if (EJSVAL_IS_UNDEFINED(func) || EJSVAL_IS_NULL(func))
        return _ejs_undefined;

    // 5. If IsCallable(func) is false, then throw a TypeError exception.
    if (!EJSVAL_IS_CALLABLE(func))
        _ejs_throw_nativeerror (EJS_TYPE_ERROR, _ejs_string_concat(_ejs_atom_error_not_callable, ToString(P)));

    // 6. Return func.
    return func;
}

// ECMA262: 7.1.14
ejsval
ToPropertyKey(ejsval argument)
{
    // 1. ReturnIfAbrupt(argument). 

    // 2. If Type(argument) is Symbol, then 
    if (EJSVAL_IS_SYMBOL(argument))
        //    a. Return argument. 
        return argument;
    // 3. Return ToString(argument). 
    return ToString(argument);
}

ejsval
HasProperty(ejsval argument, ejsval property)
{
    // 1. Assert: Type(O) is Object.
    if (!EJSVAL_IS_OBJECT(argument))
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "O is not an Object");

    // 2. Assert: IsPropertyKey(P) is true.
    property = ToPropertyKey(property);

    // 3. Return the result of calling the [[HasProperty]] internal method of O with argument P.
    EJSBool retval = OP(EJSVAL_TO_OBJECT(argument),HasProperty)(argument, property);
    return BOOLEAN_TO_EJSVAL(retval);
}

ejsval
DeletePropertyOrThrow(ejsval argument, ejsval property)
{
    /* 1. Assert: Type(O) is Object. */
    if (!EJSVAL_IS_OBJECT(argument))
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "O is not an Object");

    /* 2. Assert: IsPropertyKey(P) is true. */
    argument = ToPropertyKey(argument);

    /* 3. Let success be the result of calling the [[Delete]] internal method of O passing P as the argument. */
    /* 4. ReturnIfAbrupt(success). */
    EJSBool success = OP(EJSVAL_TO_OBJECT(argument),Delete)(argument, property, EJS_TRUE);

    /* 5. If success is false, then throw a TypeError exception. */
   if (!success)
       _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "XXX");

   /* 6. Return success. */
   return BOOLEAN_TO_EJSVAL(success);
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

// ECMA262: 6.2.4.1
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

// ECMA262: 6.2.4.2
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

// ECMA262: 6.2.4.3
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

// ECMA262: 6.2.4.4
ejsval
FromPropertyDescriptor(EJSPropertyDesc* Desc)
{
    /* 1. If Desc is undefined, then return undefined. */
    if (!Desc)
        return _ejs_undefined;

    // 2. If Desc has an [[Origin]] field, then return Desc.[[Origin]]. 

    // 3. Let obj be ObjectCreate(%ObjectPrototype%). 
    ejsval obj = _ejs_object_new(_ejs_Object_prototype, &_ejs_Object_specops);
    EJSObject* obj_ = EJSVAL_TO_OBJECT(obj);

    // 4. Assert: obj is an extensible ordinary object with no own properties. 

    // 5. If Desc has a [[Value]] field, then 
    if (_ejs_property_desc_has_value(Desc)) {
        //    a. Call OrdinaryDefineOwnProperty with arguments obj, "value", and PropertyDescriptor{[[Value]]: Desc.[[Value]], [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: true} 
        EJSPropertyDesc value_desc = { .value= Desc->value, .flags = EJS_PROP_FLAGS_VALUE_SET | EJS_PROP_WRITABLE | EJS_PROP_ENUMERABLE | EJS_PROP_CONFIGURABLE };
        OP(obj_, DefineOwnProperty)(obj, _ejs_atom_value, &value_desc, EJS_FALSE);
    }

    // 6. If Desc has a [[Writable]] field, then 
    if (_ejs_property_desc_has_writable(Desc)) {
        // a. Call OrdinaryDefineOwnProperty with arguments obj, "writable", and PropertyDescriptor{[[Value]]: Desc.[[Writable]], [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: true}.
        EJSPropertyDesc writable_desc = { .value= BOOLEAN_TO_EJSVAL(_ejs_property_desc_is_writable(Desc)), .flags = EJS_PROP_FLAGS_VALUE_SET | EJS_PROP_WRITABLE | EJS_PROP_ENUMERABLE | EJS_PROP_CONFIGURABLE };
        OP(obj_, DefineOwnProperty)(obj, _ejs_atom_writable, &writable_desc, EJS_FALSE);
    }

    // 7. If Desc has a [[Get]] field, then
    if (_ejs_property_desc_has_getter(Desc)) {
        //    a. Call OrdinaryDefineOwnProperty with arguments obj, "get", and PropertyDescriptor{[[Value]]: Desc.[[Get]], [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: true}. 
        EJSPropertyDesc get_desc = { .value= _ejs_property_desc_get_getter(Desc), .flags = EJS_PROP_FLAGS_VALUE_SET | EJS_PROP_WRITABLE | EJS_PROP_ENUMERABLE | EJS_PROP_CONFIGURABLE };
        OP(obj_, DefineOwnProperty)(obj, _ejs_atom_get, &get_desc, EJS_FALSE);
    }

    // 8. If Desc has a [[Set]] field, then
    if (_ejs_property_desc_has_getter(Desc)) {
        //    a. Call OrdinaryDefineOwnProperty with arguments obj, "set", and PropertyDescriptor{[[Value]]: Desc.[[Set]], [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: true}. 
        EJSPropertyDesc set_desc = { .value= _ejs_property_desc_get_setter(Desc), .flags = EJS_PROP_FLAGS_VALUE_SET | EJS_PROP_WRITABLE | EJS_PROP_ENUMERABLE | EJS_PROP_CONFIGURABLE };
        OP(obj_, DefineOwnProperty)(obj, _ejs_atom_set, &set_desc, EJS_FALSE);
    }

    // 9. If Desc has an [[Enumerable]] field, then 
    if (_ejs_property_desc_has_enumerable(Desc)) {
        //    a. Call OrdinaryDefineOwnProperty with arguments obj, "enumerable", and PropertyDescriptor{[[Value]]: Desc.[[Enumerable]], [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: true}. 
        EJSPropertyDesc enumerable_desc = { .value= BOOLEAN_TO_EJSVAL(_ejs_property_desc_is_enumerable(Desc)), .flags = EJS_PROP_FLAGS_VALUE_SET | EJS_PROP_WRITABLE | EJS_PROP_ENUMERABLE | EJS_PROP_CONFIGURABLE };
        OP(obj_, DefineOwnProperty)(obj, _ejs_atom_enumerable, &enumerable_desc, EJS_FALSE);
    }

    // 10. If Desc has a [[Configurable]] field, then 
    if (_ejs_property_desc_has_configurable(Desc)) {
        //    a. Call OrdinaryDefineOwnProperty with arguments obj , "configurable", and PropertyDescriptor{[[Value]]: Desc.[[Configurable]], [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: true}. 
        EJSPropertyDesc configurable_desc = { .value= BOOLEAN_TO_EJSVAL(_ejs_property_desc_is_configurable(Desc)), .flags = EJS_PROP_FLAGS_VALUE_SET | EJS_PROP_WRITABLE | EJS_PROP_ENUMERABLE | EJS_PROP_CONFIGURABLE };
        OP(obj_, DefineOwnProperty)(obj, _ejs_atom_configurable, &configurable_desc, EJS_FALSE);
    }

    // 11. Return obj. 
    return obj;
}

// ECMA262: 6.2.4.5
void
ToPropertyDescriptor(ejsval Obj, EJSPropertyDesc *desc)
{
    // 1. ReturnIfAbrupt(Obj). 
    // 2. If Type(Obj) is not Object throw a TypeError exception. 
    if (!EJSVAL_IS_OBJECT(Obj)) {
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "ToPropertyDescriptor called on non-object");
    }
    EJSObject* obj = EJSVAL_TO_OBJECT(Obj);

    // 3. Let desc be a new Property Descriptor that initially has no fields. 
    memset (desc, 0, sizeof(EJSPropertyDesc));

    // 4. If HasProperty(Obj, "enumerable") is true, then 
    if (OP(obj,HasProperty)(Obj, _ejs_atom_enumerable)) {
        //    a. Let enum be Get(Obj, "enumerable"). 
        //    b. ReturnIfAbrupt(enum). 
        //    c. Set the [[Enumerable]] field of desc to ToBoolean(enum). 
        _ejs_property_desc_set_enumerable (desc, EJSVAL_TO_BOOLEAN(ToBoolean(OP(obj,Get)(Obj, _ejs_atom_enumerable, Obj))));
    }
    // 5. If HasProperty(Obj, "configurable") is true, then 
    if (OP(obj,HasProperty)(Obj, _ejs_atom_configurable)) {
        //    a. Let conf be Get(Obj, "configurable"). 
        //    b. ReturnIfAbrupt(conf). 
        //    c. Set the [[Configurable]] field of desc to ToBoolean(conf). 
        _ejs_property_desc_set_configurable (desc, EJSVAL_TO_BOOLEAN(ToBoolean(OP(obj,Get)(Obj, _ejs_atom_configurable, Obj))));
    }
    // 6. If HasProperty(Obj, "value") is true, then 
    if (OP(obj,HasProperty)(Obj, _ejs_atom_value)) {
        //    a. Let value be Get(Obj, "value"). 
        //    b. ReturnIfAbrupt(value). 
        //    c. Set the [[Value]] field of desc to value. 
        _ejs_property_desc_set_value (desc, OP(obj,Get)(Obj, _ejs_atom_value, Obj));
    }
    // 7. If HasProperty(Obj, "writable") is true, then 
    if (OP(obj,HasProperty)(Obj, _ejs_atom_writable)) {
        //    a. Let writable be Get(Obj, "writable"). 
        //    b. ReturnIfAbrupt(writable). 
        //    c. Set the [[Writable]] field of desc to ToBoolean(writable). 
        _ejs_property_desc_set_writable (desc, EJSVAL_TO_BOOLEAN(ToBoolean(OP(obj,Get)(Obj, _ejs_atom_writable, Obj))));
    }
    // 8. If HasProperty(Obj, "get") is true, then 
    if (OP(obj,HasProperty)(Obj, _ejs_atom_get)) {
        //    a. Let getter be Get(Obj, "get"). 
        //    b. ReturnIfAbrupt(getter). 
        ejsval getter = OP(obj,Get)(Obj, _ejs_atom_get, Obj);

        //    c. If IsCallable(getter) is false and getter is not undefined, then throw a TypeError exception. 
        if (!EJSVAL_IS_CALLABLE(getter) && !EJSVAL_IS_UNDEFINED(getter)) {
            _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Getter must be callable");
        }
        //    d. Set the [[Get]] field of desc to getter. 
        _ejs_property_desc_set_getter (desc, getter);
    }
    // 9. If HasProperty(Obj, "set") is true, then 
    if (OP(obj,HasProperty)(Obj, _ejs_atom_set)) {
        //    a. Let setter be Get(Obj, "set"). 
        //    b. ReturnIfAbrupt(setter). 
        ejsval setter = OP(obj,Get)(Obj, _ejs_atom_set, Obj);
        //    c. If IsCallable(setter) is false and setter is not undefined, then throw a TypeError exception. 
        if (!EJSVAL_IS_CALLABLE(setter) && !EJSVAL_IS_UNDEFINED(setter)) {
            _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Setter must be a function");
        }
        //    d. Set the [[Set]] field of desc to setter. 
        _ejs_property_desc_set_setter (desc, setter);
    }
    // 10. If either desc.[[Get]] or desc.[[Set]] are present, then 
    if (_ejs_property_desc_has_getter(desc) || _ejs_property_desc_has_setter(desc)) {
        /*    a. If either desc.[[Value]] or desc.[[Writable]] are present, then throw a TypeError exception. */
        if (_ejs_property_desc_has_value(desc) || _ejs_property_desc_has_writable(desc)) {
            _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Invalid property.  A property cannot both have accessors and be writable or have a value");
        }
    }
    // 11. Set the [[Origin]] field of desc to Obj. 
    // 12. Return desc. 
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
    5, 17, 31, 67, 131, 257, 521, 1031, 2053, 4099
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
        if (s->hash == hashcode && EJSVAL_TO_BOOLEAN(_ejs_op_strict_eq(s->name, name))) {
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
        if (s->hash != hashcode)
            continue;
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
        uint32_t hashcode = s->hash;
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
        if (s->hash != hashcode)
            continue;
        if (EJSVAL_TO_BOOLEAN(_ejs_op_strict_eq(s->name, name))) {
            _ejs_propertydesc_free (s->desc);
            s->desc = desc;
            return;
        }
    }
    _EJSPropertyMapEntry* new_s = malloc(sizeof(_EJSPropertyMapEntry));
    new_s->hash = hashcode;
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
    EJSPropertyIterator* iterator = (EJSPropertyIterator*)obj;
    free (iterator->keys);
    _ejs_Object_specops.Finalize (obj);
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

static EJS_DEFINE_CLASS(_EJSPropertyIterator,
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
                        _ejs_property_iterator_specop_allocate,
                        _ejs_property_iterator_specop_finalize,
                        _ejs_property_iterator_specop_scan)

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
    ejsval iter = _ejs_object_new (_ejs_null, &_ejs__EJSPropertyIterator_specops);
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
    // stop doing this inline
}


///

void
_ejs_init_object (EJSObject* obj, ejsval proto, EJSSpecOps *ops)
{
    obj->proto = proto;
    obj->ops = ops ? ops : &_ejs_Object_specops;
    obj->map = calloc (sizeof(EJSPropertyMap), 1);
    _ejs_propertymap_init (obj->map);
    //printf ("obj->map = %p\n", obj->map);
    EJS_OBJECT_SET_EXTENSIBLE(obj);
#if notyet
    ((GCObjectPtr)obj)->gc_data = 0x01; // HAS_FINALIZE
#endif
}

ejsval
_ejs_object_new (ejsval proto, EJSSpecOps *ops)
{
    EJSObject *obj = ops->Allocate();
    _ejs_init_object (obj, proto, ops);
    return OBJECT_TO_EJSVAL(obj);
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
    
    MAYBE_INHERIT_DISALLOW_NULL(GetPrototypeOf);
    MAYBE_INHERIT_DISALLOW_NULL(SetPrototypeOf);
    MAYBE_INHERIT_DISALLOW_NULL(IsExtensible);
    MAYBE_INHERIT_DISALLOW_NULL(PreventExtensions);
    MAYBE_INHERIT_DISALLOW_NULL(GetOwnProperty);
    MAYBE_INHERIT_DISALLOW_NULL(DefineOwnProperty);
    MAYBE_INHERIT_DISALLOW_NULL(HasProperty);
    MAYBE_INHERIT_DISALLOW_NULL(Get);
    MAYBE_INHERIT_DISALLOW_NULL(Set);
    MAYBE_INHERIT_DISALLOW_NULL(Delete);
    MAYBE_INHERIT_DISALLOW_NULL(Enumerate);
    MAYBE_INHERIT_DISALLOW_NULL(OwnPropertyKeys);
    MAYBE_INHERIT_DISALLOW_NULL(Allocate);
    MAYBE_INHERIT_DISALLOW_NULL(Finalize);
    MAYBE_INHERIT_DISALLOW_NULL(Scan);
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

    OP(EJSVAL_TO_OBJECT(val),Set)(val, key, value, val);

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

    return OP(EJSVAL_TO_OBJECT(obj),Get)(obj, key, obj);
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
    return OP(_obj,DefineOwnProperty)(obj, key, &desc, EJS_FALSE);
}

EJSBool
_ejs_object_define_accessor_property (ejsval obj, ejsval key, ejsval get, ejsval set, uint32_t flags)
{
    EJSObject *_obj = EJSVAL_TO_OBJECT(obj);
    EJSPropertyDesc desc = { .getter = get, .setter = set, .flags = flags | EJS_PROP_FLAGS_SETTER_SET | EJS_PROP_FLAGS_GETTER_SET };
    return OP(_obj,DefineOwnProperty)(obj, key, &desc, EJS_FALSE);
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

// ECMA262: 19.1.2.9 Object.getPrototypeOf ( O ) 
static ejsval
_ejs_Object_getPrototypeOf (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval O = _ejs_undefined;
    if (argc > 0)
        O = args[0];
    
    // 1. Let obj be ToObject(O). 
    // 2. ReturnIfAbrupt(obj). 
    ejsval obj = ToObject(O);

    // 3. Return the result of calling the [[GetPrototypeOf]] internal method of obj. 
    return OP(EJSVAL_TO_OBJECT(obj),GetPrototypeOf)(obj);
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
    // 6. ReturnIfAbrupt(status).
    EJSBool status = OP(EJSVAL_TO_OBJECT(O),SetPrototypeOf)(O,proto);
    // 7. If status is false, then throw a TypeError exception.
    if (!status) {
        _ejs_log ("throw TypeError\n");
        EJS_NOT_IMPLEMENTED();
    }
    // 8. Return O.
    return O;
}

// externally visible (for the compiler) wrapper around above native implementation.
ejsval
_ejs_object_set_prototype_of (ejsval obj, ejsval proto)
{
    ejsval args[] = { obj, proto };
    return _ejs_Object_setPrototypeOf(_ejs_undefined, _ejs_undefined, 2, args);
}

// ECMA262: 19.1.2.6 Object.getOwnPropertyDescriptor ( O, P ) 
static ejsval
_ejs_Object_getOwnPropertyDescriptor (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval O = _ejs_undefined;
    ejsval P = _ejs_undefined;

    if (argc > 0) O = args[0];
    if (argc > 1) P = args[1];

    // 1. Let obj be ToObject(O). 
    // 2. ReturnIfAbrupt(obj). 
    ejsval obj = ToObject(O);

    // 3. Let key be ToPropertyKey(P). 
    // 4. ReturnIfAbrupt(key). 
    ejsval key = ToPropertyKey(P);

    // 5. Let desc be the result of calling the [[GetOwnProperty]] internal method of obj with argument key. 
    // 6. ReturnIfAbrupt(desc). 
    EJSPropertyDesc* desc = OP(EJSVAL_TO_OBJECT(obj),GetOwnProperty)(obj, key, NULL);

    // 7. Return the result of calling FromPropertyDescriptor(desc). 
    return FromPropertyDescriptor(desc);
}

// ECMA262: 19.1.2.7 Object.getOwnPropertyNames ( O ) 
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

// ECMA262: 19.1.2.1
/* Object.assign ( target, ...source ) */
static ejsval
_ejs_Object_assign (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval target = _ejs_undefined;
    if (argc > 0) target = args[0];

    // 1. Let to be ToObject(target). 
    // 2. ReturnIfAbrupt(to). 
    ejsval to = ToObject(target);

    EJSObject* to_ = EJSVAL_TO_OBJECT(to);

    // 3. If fewer than two arguments were passed,then return to. 
    if (argc < 2)
        return to;

    // 4. Let sourceList be the List of argument vales starting with the second argument. 
    // 5. For each element nextSource of sourceList, in ascending index order, 
    for (int source_i = 1; source_i < argc; source_i ++) {
        ejsval nextSource = args[source_i];
        //    a. Let from be ToObject(nextSource). 
        //    b. ReturnIfAbrupt(from). 
        ejsval from = ToObject(nextSource);
        if (!EJSVAL_IS_OBJECT(from)) {
            _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "null/undefined passed to Object.assign");
        }

        EJSObject* from_ = EJSVAL_TO_OBJECT(from);

        //    c. Let keysArray be the result of calling the [[OwnPropertyKeys]] internal method of nextSource. 
        //    d. ReturnIfAbrupt(keysArray). 

        //    e. Let lenValue be Get(keysArray, "length"). 
        //    f. Let len be ToLength(lenValue). 
        //    g. ReturnIfAbrupt(len). 

        //    h. Let nextIndex be 0. 

        //    i. Let gotAllNames be false. 
        //EJSBool gotAllNames = EJS_FALSE;  XXX this is unused

        //    j. Let pendingException be undefined. 
        ejsval pendingException = _ejs_undefined;

        //    k. Repeat while nextIndex < len, 
        for (_EJSPropertyMapEntry* s = from_->map->head_insert; s; s = s->next_insert) {
            //       i. Let nextKey be Get(keysArray, ToString(nextIndex)). 
            //       ii. ReturnIfAbrupt(nextKey). 
            ejsval nextKey = s->name;

            //       iii. Let desc be the result of calling the [[GetOwnProperty]] internal method of from with argument nextKey. 
            EJSPropertyDesc* desc = s->desc;
            //       iv. If desc is an abrupt completion, then 
            //           1. If pendingException is undefined, then set pendingException to desc. 
            
            //       v. Else if desc is not undefined and desc.[[Enumerable]] is true, then 
            if (desc && _ejs_property_desc_is_enumerable(desc)) {
                //          1. Let propValue be Get(from, nextKey). 
                ejsval propValue = OP(from_,Get)(from, nextKey, from);
                //          2. If propValue is an abrupt completion, then 
                //             a. If pendingException is undefined, then set pendingException to propValue. 
                //          3. else 
                //             a. Let status be Put(to, nextKey, propValue, true); 
                /*ejsval status =*/ Put(to, nextKey, propValue, EJS_TRUE);
                //             b. If status is an abrupt completion, then 
                //                i. If pendingException is undefined, then set pendingException to status. 
            }
            //       vi. Increment nextIndex by 1. 
        }
        //    l. If pendingException is not undefined, then return pendingException. 
        if (!EJSVAL_IS_UNDEFINED(pendingException))
            return pendingException;
    }
    // 6. Return to.
    return to;
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

// a simple externally visible wrapper around the above native impl that only supplied the prototype argument
ejsval
_ejs_object_create (ejsval proto)
{
    ejsval args[] = { proto };
    return _ejs_Object_create(_ejs_undefined, _ejs_undefined, 1, args);
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
    OP(obj,DefineOwnProperty)(O, name, &desc, EJS_TRUE);

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
        ejsval descObj = OP(props_obj,Get)(props, P, props);

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
        OP(obj,DefineOwnProperty)(O, P, desc, EJS_TRUE);
    }

    free (names);
    free (descriptors);
    
    /* 7. Return O. */
    return O;
}

static EJSBool
DefinePropertyOrThrow (ejsval O, ejsval P, EJSPropertyDesc* desc, ejsval *exc)
{
    // 1. Assert: Type(O) is Object. 
    // 2. Assert: IsPropertyKey(P) is true. 
    // 3. Let success be the result of calling the [[DefineOwnProperty]] internal method of O passing P and desc as arguments. 
    // 4. ReturnIfAbrupt(success). 
    EJSBool success = OP(EJSVAL_TO_OBJECT(O),DefineOwnProperty)(O, P, desc, EJS_FALSE);
    // 5. If success is false, then throw a TypeError exception. 
    if (!success)
        *exc = _ejs_nativeerror_new_utf8 (EJS_TYPE_ERROR, "1"); // XXX
    // 6. Return success. 
    return success;
}

typedef enum {
    INTEGRITY_SEALED,
    INTEGRITY_FROZEN
} IntegrityLevel;

// ECMA262: 7.3.11 SetIntegrityLevel (O, level) 
static ejsval
SetIntegrityLevel (ejsval O, IntegrityLevel level)
{
    // 1. Assert: Type(O) is Object. 
    EJSObject* O_ = EJSVAL_TO_OBJECT(O);

    // 2. Assert: level is either "sealed" or "frozen". 
    // 3. Let keysArray be the result of calling the [[OwnPropertyKeys]] internal method of O. 
    ejsval keysArray = OP(O_,OwnPropertyKeys)(O);

    // 4. Let keys be CreateListFromArrayLike(keysArray). 
    // 5. ReturnIfAbrupt(keys). 
    ejsval* keys = NULL;

    ejsval len = Get(keysArray, _ejs_atom_length);
    uint32_t n = ToUint32(len);

    if (n > 0) {
        keys = alloca(sizeof(ejsval) * n);
        for (int i = 0; i < n; i ++) {
            keys[i] = Get(keysArray, ToString(NUMBER_TO_EJSVAL(i)));
        }
    }


    // 6. Let pendingException be undefined. 
    ejsval pendingException = _ejs_undefined;

    // 7. If level is "sealed", then 
    if (level == INTEGRITY_SEALED) {
        //    a. Repeat for each element k of keys, 
        for (int i = 0; i < n; i ++) {
            ejsval k = keys[i];
            EJSPropertyDesc desc = { EJS_PROP_FLAGS_CONFIGURABLE_SET };
            ejsval exc;
            //       i. Let status be DefinePropertyOrThrow(O, k, PropertyDescriptor{ [[Configurable]]: false}). 
            EJSBool status = DefinePropertyOrThrow(O, k, &desc, &exc);
            //       ii. If status is an abrupt completion, then 
            if (!status) {
                //           1. If pendingException is undefined, then set pendingException to status. 
                if (EJSVAL_IS_UNDEFINED(pendingException))
                    pendingException = exc;
            }
        }
    }
    // 8. Else level is "frozen", 
    else {
        //    a. Repeat for each element k of keys, 
        for (int i = 0; i < n; i ++) {
            ejsval k = keys[i];
            //       i. Let status be the result of calling the [[GetOwnProperty]] internal method of O with k. 
            ejsval exc;
            EJSPropertyDesc* currentDesc = OP(EJSVAL_TO_OBJECT(O),GetOwnProperty)(O, k, &exc);
            //       ii. If status is an abrupt completion, then 
            if (!currentDesc && !EJSVAL_IS_UNDEFINED(exc)) {
                //           1. If pendingException is undefined, then set pendingException to status. 
                if (EJSVAL_IS_UNDEFINED(pendingException))
                    pendingException = exc;
            }
            //       iii. Else, 
            else {
                //            1. Let currentDesc be status.[[value]]. 
                //            2. If currentDesc is not undefined, then 
                if (currentDesc) {
                    EJSPropertyDesc desc;
                    memset(&desc, 0, sizeof(desc));

                    //               a. If IsAccessorDescriptor(currentDesc) is true, then 
                    if (IsAccessorDescriptor(currentDesc)) {
                        //                  i. Let desc be the PropertyDescriptor{[[Configurable]]: false}. 
                        _ejs_property_desc_set_configurable(&desc, EJS_FALSE);
                    }
                    //               b. Else, 
                    else {
                        //                  i. Let desc be the PropertyDescriptor { [[Configurable]]: false, [[Writable]]: false }. 
                        _ejs_property_desc_set_configurable(&desc, EJS_FALSE);
                        _ejs_property_desc_set_writable(&desc, EJS_FALSE);
                    }
                    //               c. Let status be DefinePropertyOrThrow(O, k, desc). 
                    EJSBool status = DefinePropertyOrThrow(O, k, &desc, &exc);

                    //               d. If status is an abrupt completion, then
                    if (!status) {
                        //                  i. If pendingException is undefined, then set pendingException to status. 
                        if (EJSVAL_IS_UNDEFINED(pendingException))
                            pendingException = exc;
                    }
                }
            }
        }
    }
    // 9. If pendingException is not undefined, then return pendingException. 
    if (!EJSVAL_IS_UNDEFINED(pendingException))
        _ejs_throw(pendingException); // returning an abrupt completion = throwing an exception

    // 10. Return the result of calling the [[PreventExtensions]] internal method of O
    return BOOLEAN_TO_EJSVAL(OP(O_,PreventExtensions)(O));
}

// ECMA262: 7.3.12 TestIntegrityLevel (O, level) 
static ejsval
TestIntegrityLevel (ejsval O, IntegrityLevel level)
{
    // 1. Assert: Type(O) is Object. 
    // 2. Assert: level is either "sealed" or "frozen". 
    // 3. Let status be IsExtensible(O). 
    // 4. ReturnIfAbrupt(status). 
    EJSBool status = EJS_OBJECT_IS_EXTENSIBLE(EJSVAL_TO_OBJECT(O));
    // 5. If status is true, then return false 
    if (status)
        return _ejs_false;

    // 6. NOTE If the object is extensible, none of its properties are examined. 

    // 7. Let keysArray be the result of calling the [[OwnPropertyKeys]] internal method of O. 
    // 8. Let keys be CreateListFromArrayLike(keysArray). 
    // 9. ReturnIfAbrupt(keys). 
    
    // 20. Let pendingException be undefined. 
    ejsval pendingException = _ejs_undefined;

    // 11. Let configurable be false. 
    EJSBool configurable = EJS_FALSE;

    // 12. Let writable be false. 
    EJSBool writable = EJS_FALSE;

    // 13. Repeat for each element k of keys, 
    //     a. Let status be the result of calling the [[GetOwnProperty]] internal method of O with k. 
    //     b. If status is an abrupt completion, then 
    //        i. If pendingException is undefined, then set pendingException to status. 
    //        ii. Let configurable be true. 
    //     c. Else, 
    //        i. Let currentDesc be status.[[value]]. 
    //        ii. If currentDesc is not undefined, then 
    //            1. Set configurable to configurable logically ored with currentDesc.[[Configurable]]. 
    //            2. If IsDataDescriptor(currentDesc) is true, then 
    //               a. Set writable to writable logically ored with currentDesc.[[Writable]]. 
    // 14. If pendingException is not undefined, then return pendingException. 
    if (!EJSVAL_IS_UNDEFINED(pendingException))
        return pendingException;

    // 15. If level is "frozen" and writable is true, then return false. 
    if (level == INTEGRITY_FROZEN && writable)
        return _ejs_false;

    // 16. If configurable is true, then return false. 
    if (configurable)
        return _ejs_false;

    // 17. Return true.
    return _ejs_true;
}

// ECMA262: 19.1.2.17 Object.seal ( O ) 
static ejsval
_ejs_Object_seal (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval O = _ejs_undefined;
    if (argc > 0) O = args[0];

    // 1. If Type(O) is not Object, return O. 
    if (!EJSVAL_IS_OBJECT(O))
        return O;

    // 2. Let status be the result of SetIntegrityLevel( O, "sealed"). 
    // 3. ReturnIfAbrupt(status). 
    ejsval status = SetIntegrityLevel(O, INTEGRITY_SEALED);

    // 4. If status is false, throw a TypeError exception. 
    if (EJSVAL_EQ(status, _ejs_false))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Unable to seal object");

    // 5. Return O. 
    return O;
}

// ECMA262: 19.1.2.5 Object.freeze ( O ) 
static ejsval
_ejs_Object_freeze (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval O = _ejs_undefined;
    if (argc > 0) O = args[0];

    // 1. If Type(O) is not Object, return O. 
    if (!EJSVAL_IS_OBJECT(O))
        return O;

    // 2. Let status be the result of SetIntegrityLevel( O, "frozen"). 
    // 3. ReturnIfAbrupt(status). 
    ejsval status = SetIntegrityLevel(O, INTEGRITY_FROZEN);

    // 4. If status is false, throw a TypeError exception. 
    if (EJSVAL_EQ(status, _ejs_false))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Unable to freeze object");

    // 5. Return O. 
    return O;
}

// ECMA262: 19.1.2.15 Object.preventExtensions ( O ) 
static ejsval
_ejs_Object_preventExtensions (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval O = _ejs_undefined;
    if (argc > 0) O = args[0];

    // 1. If Type(O) is not Object, return O. 
    if (!EJSVAL_IS_OBJECT(O))
        return O;

    // 2. Let status be the result of calling the [[PreventExtensions]] internal method of O.
    // 3. ReturnIfAbrupt(status).
    EJSBool status = OP(EJSVAL_TO_OBJECT(O),PreventExtensions)(O);
    
    // 4. If status is false, throw a TypeError exception.
    if (!status)
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "[[PreventExtensions]] returned false");

    // 5. Return O.
    return O;
}

// ECMA262: 19.1.2.10 Object.is ( value1, value2 )
static ejsval
_ejs_Object_is (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval value1 = _ejs_undefined;
    ejsval value2 = _ejs_undefined;

    if (argc > 0) value1 = args[0];
    if (argc > 1) value2 = args[1];

    return BOOLEAN_TO_EJSVAL(SameValue(value1, value2));
}


// ECMA262: 19.1.2.13 Object.isSealed ( O ) 
static ejsval
_ejs_Object_isSealed (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval O = _ejs_undefined;
    if (argc > 0) O = args[0];

    // 1. If Type(O) is not Object, return true. 
    if (!EJSVAL_IS_OBJECT(O))
        return _ejs_true;

    // 2. Return TestIntegrityLevel(O, "sealed"). 
    return TestIntegrityLevel(O, INTEGRITY_SEALED);
}

// ECMA262: 19.1.2.12 Object.isFrozen ( O ) 
static ejsval
_ejs_Object_isFrozen (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval O = _ejs_undefined;
    if (argc > 0) O = args[0];

    // 1. If Type(O) is not Object, return true. 
    if (!EJSVAL_IS_OBJECT(O))
        return _ejs_true;

    // 2. Return TestIntegrityLevel(O, "frozen"). 
    return TestIntegrityLevel(O, INTEGRITY_FROZEN);
}

// ECMA262: 15.2.3.13
static ejsval
_ejs_Object_isExtensible (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval O = _ejs_undefined;
    if (argc > 0) O = args[0];

    // 1. If Type(O) is not Object, return false. 
    if (!EJSVAL_IS_OBJECT(O))
        return _ejs_false;
    
    // 2. Return the result of IsExtensible(O).
    return BOOLEAN_TO_EJSVAL(OP(EJSVAL_TO_OBJECT(O),IsExtensible)(O));
}

// ECMA262: 19.1.2.14
static ejsval
_ejs_Object_keys (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval O = _ejs_undefined;
    if (argc > 0) O = args[0];

    if (!EJSVAL_IS_OBJECT(O))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Object.keys called on non-object.");

    // toshok - is this really not identical to Object.getOwnPropertyNames?
    return _ejs_Object_getOwnPropertyNames(_ejs_undefined, _ejs_undefined, 1, &O);
}

// ECMA262: 19.1.3.6
ejsval
_ejs_Object_prototype_toString (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    // 1. If the this value is undefined, return "[object Undefined]". 
    if (EJSVAL_IS_UNDEFINED(_this))
        return _ejs_atom_undefined_toString;
    // 2. If the this value is null, return "[object Null]". 
    if (EJSVAL_IS_NULL(_this))
        return _ejs_atom_null_toString;

    ejsval builtinTag;

    // 3. Let O be the result of calling ToObject passing the this value as the argument. 
    ejsval O = ToObject(_this);
    EJSBool check_for_subclasses = EJS_TRUE;
    // 4. If O is an exotic Array object, then let builtinTag be "Array". 
    if (EJSVAL_IS_ARRAY(O)) {
        builtinTag = _ejs_atom_Array;
    }
    // 5. Else, if O is an exotic String object, then let builtinTag be "String". 
    else if (EJSVAL_IS_STRING_OBJECT(O)) {
        builtinTag = _ejs_atom_String;
    }
    // 6. Else, if O is an exotic Proxy object, then let builtinTag be "Proxy". 
    else if (EJSVAL_IS_PROXY(O)) {
        builtinTag = _ejs_atom_Proxy;
        check_for_subclasses = EJS_FALSE;
    }
    // 7. Else, if O is an exotic arguments object, then let builtinTag be "Arguments". 
    else if (EJSVAL_IS_ARGUMENTS(O)) {
        builtinTag = _ejs_atom_Arguments;
    }
    // 8. Else, if O is an ECMAScript function object, a built-in function object, or a bound function exotic object, then let builtinTag be "Function". 
    else if (EJSVAL_IS_FUNCTION(O)) {
        builtinTag = _ejs_atom_Function;
    }
    // 9. Else, if O has an [[ErrorData]] internal slot, then let builtinTag be "Error". 
    else if (EJSVAL_IS_ERROR(O)) {
        builtinTag = _ejs_atom_Error;
    }
    // 10. Else, if O has a [[BooleanData]] internal slot, then let builtinTag be "Boolean". 
    else if (EJSVAL_IS_BOOLEAN_OBJECT(O)) {
        builtinTag = _ejs_atom_Boolean;
    }
    // 11. Else, if O has a [[NumberData]] internal slot, then let builtinTag be "Number". 
    else if (EJSVAL_IS_NUMBER_OBJECT(O)) {
        builtinTag = _ejs_atom_Number;
    }
    // 12. Else, if O has a [[DateValue]] internal slot, then let builtinTag be "Date". 
    else if (EJSVAL_IS_DATE(O)) {
        builtinTag = _ejs_atom_Date;
    }
    // 13. Else, if O has a [[RegExpMatcher]] internal slot, then let builtinTag be "RegExp". 
    else if (EJSVAL_IS_REGEXP(O)) {
        builtinTag = _ejs_atom_RegExp;
    }
    // 14. Else, let builtinTag be "Object". 
    else {
        builtinTag = _ejs_atom_Object;
        check_for_subclasses = EJS_FALSE;
    }

    // 15. Let hasTag be the result of HasProperty(O, @@toStringTag). 
    // 16. ReturnIfAbrupt(hasTag). 
    EJSBool hasTag = OP(EJSVAL_TO_OBJECT(O),HasProperty)(O, _ejs_Symbol_toStringTag);

    ejsval tag;
    // 17. If hasTag is false, then let tag be builtinTag. 
    if (!hasTag) {
        tag = builtinTag;
    }
    // 18. Else, 
    else {
        //     a. Let tag be the result of Get(O, @@toStringTag). 
        //     b. If tag is an abrupt completion, let tag be NormalCompletion("???"). 
        //     c. Let tag be tag.[[value]]. 
        tag = OP(EJSVAL_TO_OBJECT(O),Get)(O, _ejs_Symbol_toStringTag, O);

        //     d. If Type(tag) is not String, let tag be "???". 
        if (!EJSVAL_IS_STRING(tag))
            tag = _ejs_atom_unknown_tag;
            
        //     e. If tag is any of "Arguments", "Array", "Boolean", "Date", "Error", "Function", "Number", "RegExp", or
        //        "String" and SameValue(tag, builtinTag) is false, then let tag be the string value "~" concatenated with
        //        the current value of tag. 
        if (check_for_subclasses && !SameValue(tag, builtinTag))
            tag = _ejs_string_concat(_ejs_atom_tilde, tag);
    }
    // 19. Return the String value that is the result of concatenating the three Strings "[object ", tag, and "]". 
    return _ejs_string_concatv(_ejs_atom_toString_prefix, tag, _ejs_atom_toString_suffix, _ejs_null);
}

// ECMA262: 15.2.4.3
static ejsval
_ejs_Object_prototype_toLocaleString (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    /* 1. Let O be the result of calling ToObject passing the this value as the argument. */
    ejsval O = ToObject(_this);
    EJSObject* O_ = EJSVAL_TO_OBJECT(O);

    /* 2. Let toString be the result of calling the [[Get]] internal method of O passing "toString" as the argument. */
    ejsval toString = OP(O_, Get)(O, _ejs_atom_toString, O);

    /* 3. If IsCallable(toString) is false, throw a TypeError exception. */
    if (!EJSVAL_IS_CALLABLE(toString))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "toString property is not callable");

    /* 4. Return the result of calling the [[Call]] internal method of toString passing O as the this value and no arguments. */
    return _ejs_invoke_closure (toString, O, 0, NULL);
}

// ECMA262: 19.1.3.7 Object.prototype.valueOf () 
static ejsval
_ejs_Object_prototype_valueOf (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    return ToObject(_this);
}

// ECMA262: 15.2.4.5
static ejsval
_ejs_Object_prototype_hasOwnProperty (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval needle = _ejs_undefined;
    if (EJS_UNLIKELY(argc > 0))
        needle = args[0];

    return BOOLEAN_TO_EJSVAL(OP(EJSVAL_TO_OBJECT(_this),GetOwnProperty)(_this, needle, NULL) != NULL);
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
        V = OP(EJSVAL_TO_OBJECT(V),GetPrototypeOf)(V);

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
    EJSPropertyDesc* desc = OP(O_, GetOwnProperty)(O, P, NULL);
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

    OBJ_METHOD(assign);
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
            ejsval nextp = OP(EJSVAL_TO_OBJECT(p),GetPrototypeOf)(p);
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

// ECMA262: 9.1.8 [[Get]] (P, Receiver) 
static ejsval
_ejs_object_specop_get (ejsval O, ejsval P, ejsval Receiver)
{
    // 1. Assert: IsPropertyKey(P) is true. 
    ejsval pname = ToPropertyKey(P); // XXX this shouldn't be necessary, but ejs passes numbers here

    if (EJSVAL_IS_STRING(pname) && !ucs2_strcmp(_ejs_ucs2___proto__, EJSVAL_TO_FLAT_STRING(pname)))
        return OP(EJSVAL_TO_OBJECT(O),GetPrototypeOf) (O);

    // 2. Let desc be the result of calling the [[GetOwnProperty]] internal method of O with argument P. 
    // 3. ReturnIfAbrupt(desc). 
    EJSPropertyDesc* desc = OP(EJSVAL_TO_OBJECT(O),GetOwnProperty) (O, P, NULL);

    // 4. If desc is undefined, then 
    if (desc == NULL) {
        //    a. Let parent be the result of calling the [[GetPrototypeOf]] internal method of O. 
        //    b. ReturnIfAbrupt(parent). 
        ejsval parent = OP(EJSVAL_TO_OBJECT(O),GetPrototypeOf)(O);
        //    c. If parent is null, then return undefined.
        if (EJSVAL_IS_NULL(parent)) {
            // _ejs_log ("property lookup on a %s object, propname %s => undefined\n", CLASSNAME(EJSVAL_TO_OBJECT(O)), EJSVAL_TO_FLAT_STRING(pname));
            return _ejs_undefined;
        }

        //    d. Return the result of calling the [[Get]] internal method of parent with arguments P and Receiver. 
        return OP(EJSVAL_TO_OBJECT(parent),Get)(parent, P, Receiver);
    }

    // 5. If IsDataDescriptor(desc) is true, return desc.[[Value]]. 
    if (IsDataDescriptor(desc)) {
        // if (EJSVAL_IS_UNDEFINED(desc->value))
        //     _ejs_log ("property lookup on a %s object, propname %s => undefined\n", CLASSNAME(EJSVAL_TO_OBJECT(O)), EJSVAL_TO_FLAT_STRING(pname));
        return desc->value;
    }

    // 6. Otherwise, IsAccessorDescriptor(desc) must be true so, let getter be desc.[[Get]]. 
    ejsval getter = _ejs_property_desc_get_getter(desc);

    // 7. If getter is undefined, return undefined. 
    if (EJSVAL_IS_UNDEFINED(getter)) {
        // _ejs_log ("property lookup on a %s object, propname %s => undefined getter\n", CLASSNAME(EJSVAL_TO_OBJECT(O)), EJSVAL_TO_FLAT_STRING(pname));
        return _ejs_undefined;
    }

    // 8. Return the result of calling the [[Call]] internal method of getter with Receiver as the thisArgument and an empty List as argumentsList. 
    return _ejs_invoke_closure (getter, Receiver, 0, NULL);
}

// ECMA262: 8.12.1
static EJSPropertyDesc*
_ejs_object_specop_get_own_property (ejsval obj, ejsval propertyName, ejsval* exc)
{
    ejsval property_str = ToPropertyKey(propertyName);
    EJSObject* obj_ = EJSVAL_TO_OBJECT(obj);

    return _ejs_propertymap_lookup (obj_->map, property_str);
}

// ECMA262: 9.1.9
static EJSBool
_ejs_object_specop_set (ejsval O, ejsval P, ejsval V, ejsval Receiver)
{
    EJSPropertyDesc undefined_desc = { .value = _ejs_undefined, .flags = EJS_PROP_FLAGS_VALUE_SET | EJS_PROP_WRITABLE | EJS_PROP_ENUMERABLE | EJS_PROP_CONFIGURABLE };

    // 1. Assert: IsPropertyKey(P) is true. 
    P = ToPropertyKey(P); // XXX this shouldn't be necessary, but ejs passes numbers here
    
    // 2. Let ownDesc be the result of calling the [[GetOwnProperty]] internal method of O with argument P. 
    // 3. ReturnIfAbrupt(ownDesc). 
    EJSPropertyDesc* ownDesc = OP(EJSVAL_TO_OBJECT(O),GetOwnProperty)(O, P, NULL);

    // 4. If ownDesc is undefined, then 
    if (!ownDesc) {
        //    a. Let parent be the result of calling the [[GetPrototypeOf]] internal method of O. 
        //    b. ReturnIfAbrupt(parent). 
        ejsval parent = OP(EJSVAL_TO_OBJECT(O),GetPrototypeOf)(O);
        //    c. If parent is not null, then 
        if (!EJSVAL_IS_NULL(parent)) {
            //       i. Return the result of calling the [[Set]] internal method of parent with arguments P, V, and Receiver. 
            return OP(EJSVAL_TO_OBJECT(parent),Set)(parent, P, V, Receiver);
        }
        //    d. Else, 
        else {
            //       i. Let ownDesc be the PropertyDescriptor{[[Value]]: undefined, [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: true}. 
            ownDesc = &undefined_desc;
        }
    }

    // 5. If IsDataDescriptor(ownDesc) is true, then 
    if (IsDataDescriptor(ownDesc)) {
        //    a. If ownDesc.[[Writable]] is false, return false. 
        if (!_ejs_property_desc_is_writable(ownDesc))
            return EJS_FALSE;

        //    b. If Type(Receiver) is not Object, return false.
        if (!EJSVAL_IS_OBJECT(Receiver))
            return EJS_FALSE;

        //    c. Let existingDescriptor be the result of calling the [[GetOwnProperty]] internal method of Receiver with argument P. 
        //    d. ReturnIfAbrupt(existingDescriptor). 
        EJSPropertyDesc* existingDescriptor = OP(EJSVAL_TO_OBJECT(Receiver),GetOwnProperty)(Receiver, P, NULL);

        //    e. If existingDescriptor is not undefined, then 
        if (existingDescriptor) {
            //       i. Let valueDesc be the PropertyDescriptor{[[Value]]: V}. 
            //       ii. Return the result of calling the [[DefineOwnProperty]] internal method of Receiver with arguments P and valueDesc. 
            EJSPropertyDesc valueDesc = { .value = V, .flags = EJS_PROP_FLAGS_VALUE_SET };
            return OP(EJSVAL_TO_OBJECT(Receiver),DefineOwnProperty)(Receiver, P, &valueDesc, EJS_FALSE); /* XXX not sure about last parameter here, it's an ES5-ism */
        }
        //    f. Else Receiver does not currently have a property P, 
        else {
            //       i. Return CreateDataProperty(Receiver, P, V). 
            return _ejs_object_define_value_property (Receiver, P, V, EJS_PROP_ENUMERABLE | EJS_PROP_CONFIGURABLE | EJS_PROP_WRITABLE);
        }
    }

    // 6. If IsAccessorDescriptor(ownDesc) is true, then 
    if (IsAccessorDescriptor(ownDesc)) {
        //    a. Let setter be ownDesc.[[Set]]. 
        ejsval setter = _ejs_property_desc_get_setter(ownDesc);
        //    b. If setter is undefined, return false. 
        if (EJSVAL_IS_UNDEFINED(setter))
            return EJS_FALSE;
        
        //    c. Let setterResult be the result of calling the [[Call]] internal method of setter providing Receiver as thisArgument and a new List containing V as argumentsList. 
        //    d. ReturnIfAbrupt(setterResult). 
        /* unused ejsval setterResult = */ _ejs_invoke_closure(setter, Receiver, 1, &V);
    }
    // e. Return true.
    return EJS_TRUE;
}


// ECMA262: 9.1.7 [[HasProperty]](P) 
static EJSBool
_ejs_object_specop_has_property (ejsval O, ejsval P)
{
    // 1. Assert: IsPropertyKey(P) is true. 

    // 2. Let hasOwn be the result of calling the [[GetOwnProperty]] internal method of O with argument P. 
    // 3. ReturnIfAbrupt(hasOwn). 
    EJSPropertyDesc* hasOwn = OP(EJSVAL_TO_OBJECT(O),GetOwnProperty)(O, P, NULL);

    // 4. If hasOwn is not undefined, then return true. 
    if (hasOwn)
        return EJS_TRUE;

    // 5. Let parent be the result of calling the [[GetPrototypeOf]] internal method of O. 
    // 6. ReturnIfAbrupt(parent). 
    ejsval parent = OP(EJSVAL_TO_OBJECT(O),GetPrototypeOf)(O);

    // 7. If parent is not null, then 
    if (!EJSVAL_IS_NULL(parent)) {
        //    a. Return the result of calling the [[HasProperty]] internal method of parent with argument P. 
        return OP(EJSVAL_TO_OBJECT(parent),HasProperty)(parent, P);
    }
    // 8. Return false. 
    return EJS_FALSE;
}

static EJSBool
_ejs_object_specop_delete (ejsval O, ejsval P, EJSBool Throw)
{
    EJSObject* obj = EJSVAL_TO_OBJECT(O);
    /* 1. Let desc be the result of calling the [[GetOwnProperty]] internal method of O with property name P. */
    EJSPropertyDesc* desc = OP(obj,GetOwnProperty)(O, P, NULL);
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
    EJSPropertyDesc* current = OP(obj, GetOwnProperty)(O, P, NULL);

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
    //printf ("_ejs_propertymap_free(obj->map = %p)\n", obj->map);
    _ejs_propertymap_free (obj->map);
    obj->map = NULL;
}

static void
scan_property (ejsval name, EJSPropertyDesc *desc, EJSValueFunc scan_func)
{
    scan_func (name);

    if (_ejs_property_desc_has_value (desc)) {
        scan_func (desc->value);
    }
    if (_ejs_property_desc_has_getter (desc)) {
        scan_func (desc->getter); 
    }
    if (_ejs_property_desc_has_setter (desc)) {
        scan_func (desc->setter);
    }
}

static void
_ejs_object_specop_scan (EJSObject* obj, EJSValueFunc scan_func)
{
    _ejs_propertymap_foreach_property (obj->map, (EJSPropertyDescFunc)scan_property, scan_func);
    scan_func (obj->proto);
}

// ECMA262: 9.1.3 [[IsExtensible]] ( ) 
static EJSBool
_ejs_object_specop_is_extensible(ejsval O)
{
    // 1. Return the value of the [[Extensible]] internal slot of O. 
    return EJS_OBJECT_IS_EXTENSIBLE(EJSVAL_TO_OBJECT(O));
}

// ECMA262: 9.1.4 [[PreventExtensions]] ( ) 
static EJSBool
_ejs_object_specop_preventextensions (ejsval O)
{
    // 1. Set the value of the [[Extensible]] internal slot of O to false. 
    EJS_OBJECT_CLEAR_EXTENSIBLE(EJSVAL_TO_OBJECT(O));

    // 2. Return true. 
    return EJS_TRUE;
}

// ECMA262: 9.1.11 [[Enumerate]] () 
static ejsval
_ejs_object_specop_enumerate (ejsval O)
{
    // 1. Let proto be the result of calling the [[GetPrototypeOf]] internal method of O with no arguments. 
    // 2. ReturnIfAbrupt(proto). 
    // 3. If proto is the value null, then 
    //    a. Let propList be a new empty List. 
    // 4. Else 
    //    a. Let propList be the result of calling the [[Enumerate]] internal method of proto. 
    // 5. ReturnIfAbrupt(propList). 
    // 6. For each name that is the property key of an own property of O 
    //    a. If Type(name) is String, then 
    //       i. Let desc be the result of calling the [[GetOwnProperty]] internal method of O with argument name. 
    //       ii. ReturnIfAbrupt(desc). 
    //       iii. If name is an element of propList, then remove name as an element of propList. 
    //       iv. If desc.[[Enumerable]] is true, then add name as an element of propList. 
    // 7. Order the elements of propList in an implementation defined order. 
    // 8. Return propList. 
    EJS_NOT_IMPLEMENTED();
}

// ECMA262: 9.1.12 [[OwnPropertyKeys]] ( ) 
static ejsval
_ejs_object_specop_own_property_keys (ejsval O)
{
    EJSObject* O_ = EJSVAL_TO_OBJECT(O);

    ejsval* numberkeys = malloc(sizeof(ejsval) *O_->map->inuse);
    int num_numberkeys = 0;
    ejsval* stringkeys = malloc(sizeof(ejsval) *O_->map->inuse);
    int num_stringkeys = 0;
    ejsval* symbolkeys = malloc(sizeof(ejsval) *O_->map->inuse);
    int num_symbolkeys = 0;
    // 1. Let keys be a new empty List. 
    for (_EJSPropertyMapEntry *s = O_->map->head_insert; s; s = s->next_insert) {
        if (EJSVAL_IS_STRING(s->name)) {
            ejsval idx_val = ToNumber(s->name);
            if (EJSVAL_IS_NUMBER(idx_val)) {
                double n = EJSVAL_TO_NUMBER(idx_val);
                if (floor(n) == n) {
                    // 2. For each own property key P of O that is an integer index, in ascending numeric index order 
                    //    a. Add P as the last element of keys. 
                    numberkeys[num_numberkeys++] = s->name;
                    continue;
                }
            }
            // 3. For each own property key P of O that is a String but is not an integer index, in property creation order 
            //    a. Add P as the last element of keys. 
            stringkeys[num_stringkeys++] = s->name;
        }
        else {
            // 4. For each own property key P of O that is a Symbol, in property creation order
            //    a. Add P as the last element of keys. 
            symbolkeys[num_symbolkeys++] = s->name;
        }
    }

    ejsval keys = _ejs_array_new (num_numberkeys + num_stringkeys + num_symbolkeys, EJS_FALSE);
    ejsval* elements = EJS_DENSE_ARRAY_ELEMENTS(keys);

    memmove (elements, numberkeys, num_numberkeys * sizeof(ejsval));
    memmove (elements + num_numberkeys * sizeof(ejsval), stringkeys, num_stringkeys * sizeof(ejsval));
    memmove (elements + (num_numberkeys + num_stringkeys) * sizeof(ejsval), symbolkeys, num_symbolkeys * sizeof(ejsval));

    free (numberkeys);
    free (stringkeys);
    free (symbolkeys);

    // 5. Return CreateListIteratorCreateArrayFromList(keys). 
    return keys;
}

EJS_DEFINE_CLASS(Object,
                 _ejs_object_specop_get_prototype_of,
                 _ejs_object_specop_set_prototype_of,
                 _ejs_object_specop_is_extensible,
                 _ejs_object_specop_preventextensions,
                 _ejs_object_specop_get_own_property,
                 _ejs_object_specop_define_own_property,
                 _ejs_object_specop_has_property,
                 _ejs_object_specop_get,
                 _ejs_object_specop_set,
                 _ejs_object_specop_delete,
                 _ejs_object_specop_enumerate,
                 _ejs_object_specop_own_property_keys,
                 _ejs_object_specop_allocate,
                 _ejs_object_specop_finalize,
                 _ejs_object_specop_scan
                 )
