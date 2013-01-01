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

static ejsval _ejs_object_specop_get (ejsval obj, ejsval propertyName, EJSBool isCStr);
static ejsval _ejs_object_specop_get_own_property (ejsval obj, ejsval propertyName);
static ejsval _ejs_object_specop_get_property (ejsval obj, ejsval propertyName);
static void      _ejs_object_specop_put (ejsval obj, ejsval propertyName, ejsval val, EJSBool flag);
static EJSBool   _ejs_object_specop_can_put (ejsval obj, ejsval propertyName);
static EJSBool   _ejs_object_specop_has_property (ejsval obj, ejsval propertyName);
static EJSBool   _ejs_object_specop_delete (ejsval obj, ejsval propertyName, EJSBool flag);
static ejsval _ejs_object_specop_default_value (ejsval obj, const char *hint);
static void      _ejs_object_specop_define_own_property (ejsval obj, ejsval propertyName, ejsval propertyDescriptor, EJSBool flag);
static void      _ejs_object_specop_finalize (EJSObject* obj);
static void      _ejs_object_specop_scan (EJSObject* obj, EJSValueFunc scan_func);

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


EJSPropertyMap*
_ejs_propertymap_new (int initial_allocation)
{
    EJSPropertyMap* rv = (EJSPropertyMap*)malloc(sizeof (EJSPropertyMap));
    if (initial_allocation) {
        rv->names = (char**)malloc(sizeof(char*) * initial_allocation);
        rv->fields = (ejsval*)malloc(sizeof (ejsval) * initial_allocation);
    }
    else {
        rv->names = NULL;
        rv->fields = NULL;
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
    free (map->fields);
    free(map);
}

void
_ejs_propertymap_foreach_value (EJSPropertyMap* map, EJSValueFunc foreach_func)
{
    for (int i = 0; i < map->num; i ++)
        foreach_func (map->fields[i]);
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
            ejsval* new_fields = (ejsval*)malloc (sizeof(ejsval) * new_allocated);

            memmove (new_names, map->names, map->allocated * sizeof(char*));
            memmove (new_fields, map->fields, map->allocated * sizeof(ejsval));

            if (map->names)
                free (map->names);
            if (map->fields)
                free (map->fields);

            map->names = new_names;
            map->fields = new_fields;

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
        NOT_IMPLEMENTED();

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
        NOT_IMPLEMENTED();
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
        NOT_IMPLEMENTED();
    }
}

ejsval
_ejs_object_setprop (ejsval val, ejsval key, ejsval value)
{
    if (EJSVAL_IS_PRIMITIVE(val)) {
        printf ("setprop on primitive.  ignoring\n" );
        NOT_IMPLEMENTED();
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
    obj->map->fields[prop_index] = value;

    return value;
}

ejsval
_ejs_object_getprop (ejsval obj, ejsval key)
{
    if (EJSVAL_IS_NULL(obj) || EJSVAL_IS_UNDEFINED(obj)) {
        printf ("throw TypeError, key is %s\n", EJSVAL_TO_FLAT_STRING(ToString(key)));
        NOT_IMPLEMENTED();
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
    obj->map->fields[prop_index] = value;

    return value;
}

ejsval
_ejs_object_getprop_utf8 (ejsval obj, const char *key)
{
    if (EJSVAL_IS_NULL(obj) || EJSVAL_IS_UNDEFINED(obj)) {
        printf ("throw TypeError, key is %s\n", key);
        NOT_IMPLEMENTED();
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
    NOT_IMPLEMENTED();
}

// ECMA262: 15.2.3.3
static ejsval
_ejs_Object_getOwnPropertyDescriptor (ejsval env, ejsval _this, int argc, ejsval *args)
{
    NOT_IMPLEMENTED();
}

// ECMA262: 15.2.3.4
static ejsval
_ejs_Object_getOwnPropertyNames (ejsval env, ejsval _this, int argc, ejsval *args)
{
    NOT_IMPLEMENTED();
}

// FIXME ECMA262: 15.2.3.5
static ejsval
_ejs_Object_create (ejsval env, ejsval _this, int argc, ejsval *args)
{
    /* Object.create ( O [, Properties] ) */
    /* 1. If Type(O) is not Object or Null throw a TypeError exception. */
    /* 2. Let obj be the result of creating a new object as if by the expression new Object() where Object is the  */
    /* standard built-in constructor with that name */
    ejsval obj = _ejs_object_new(_ejs_null);
    /* 3. Set the [[Prototype]] internal property of obj to O. */
    EJSVAL_TO_OBJECT(obj)->proto = args[0];
    /* 4. If the argument Properties is present and not undefined, add own properties to obj as if by calling the  */
    /* standard built-in function Object.defineProperties with arguments obj and Properties. */
    /* 5. Return obj. */
    return obj;
}

// ECMA262: 15.2.3.6
static ejsval
_ejs_Object_defineProperty (ejsval env, ejsval _this, int argc, ejsval *args)
{
    NOT_IMPLEMENTED();
}

// ECMA262: 15.2.3.7
static ejsval
_ejs_Object_defineProperties (ejsval env, ejsval _this, int argc, ejsval *args)
{
    NOT_IMPLEMENTED();
}

// ECMA262: 15.2.3.8
static ejsval
_ejs_Object_seal (ejsval env, ejsval _this, int argc, ejsval *args)
{
    return args[0];
}

// FIXME ECMA262: 15.2.3.9
static ejsval
_ejs_Object_freeze (ejsval env, ejsval _this, int argc, ejsval *args)
{
    return args[0];
}

// ECMA262: 15.2.3.10
static ejsval
_ejs_Object_preventExtensions (ejsval env, ejsval _this, int argc, ejsval *args)
{
    NOT_IMPLEMENTED();
}

// ECMA262: 15.2.3.11
static ejsval
_ejs_Object_isSealed (ejsval env, ejsval _this, int argc, ejsval *args)
{
    NOT_IMPLEMENTED();
}

// ECMA262: 15.2.3.12
static ejsval
_ejs_Object_isFrozen (ejsval env, ejsval _this, int argc, ejsval *args)
{
    NOT_IMPLEMENTED();
}

// ECMA262: 15.2.3.13
static ejsval
_ejs_Object_isExtensible (ejsval env, ejsval _this, int argc, ejsval *args)
{
    NOT_IMPLEMENTED();
}

// ECMA262: 15.2.3.14
static ejsval
_ejs_Object_keys (ejsval env, ejsval _this, int argc, ejsval *args)
{
    NOT_IMPLEMENTED();
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
    NOT_IMPLEMENTED();
}

// ECMA262: 15.2.4.4
static ejsval
_ejs_Object_prototype_valueOf (ejsval env, ejsval _this, int argc, ejsval *args)
{
    NOT_IMPLEMENTED();
}

// ECMA262: 15.2.4.5
static ejsval
_ejs_Object_prototype_hasOwnProperty (ejsval env, ejsval _this, int argc, ejsval *args)
{
    if (argc == 0)
        return BOOLEAN_TO_EJSVAL(EJS_FALSE);

    ejsval needle = args[0];
    if (EJSVAL_IS_NUMBER(needle)) {
        // if the oject is an array, we look up the index.
    }

    ejsval needle_str = ToString(needle);

    int idx = _ejs_propertymap_lookup (EJSVAL_TO_OBJECT(_this)->map, EJSVAL_TO_FLAT_STRING(needle_str), EJS_FALSE);
    return BOOLEAN_TO_EJSVAL (idx != -1);
}

// ECMA262: 15.2.4.6
static ejsval
_ejs_Object_prototype_isPrototypeOf (ejsval env, ejsval _this, int argc, ejsval *args)
{
    NOT_IMPLEMENTED();
}

// ECMA262: 15.2.4.7
static ejsval
_ejs_Object_prototype_propertyIsEnumerable (ejsval env, ejsval _this, int argc, ejsval *args)
{
    NOT_IMPLEMENTED();
}

void
_ejs_object_init (ejsval global)
{
    START_SHADOW_STACK_FRAME;

    // FIXME ECMA262 15.2.4
    _ejs_gc_add_named_root (_ejs_Object_proto);
    _ejs_Object_proto = _ejs_null;
    _ejs_Object_proto = _ejs_object_new(_ejs_null); // XXX circular initialization going on here..

    ADD_STACK_ROOT(ejsval, tmpobj, _ejs_function_new_utf8 (_ejs_null, "Object", (EJSClosureFunc)_ejs_Object_impl));
    _ejs_Object = tmpobj;

    // ECMA262 15.2.3.1
    _ejs_object_setprop (_ejs_Object,       _ejs_atom_prototype,    _ejs_Object_proto); // FIXME: {[[Writable]]: false, [[Enumerable]]: false, [[Configurable]]: false }
    // ECMA262: 15.2.4.1
    _ejs_object_setprop_utf8 (_ejs_Object_proto, "constructor",  _ejs_Object);

#define OBJ_METHOD(x) EJS_MACRO_START ADD_STACK_ROOT(ejsval, funcname, _ejs_string_new_utf8(#x)); ADD_STACK_ROOT(ejsval, tmpfunc, _ejs_function_new (_ejs_null, funcname, (EJSClosureFunc)_ejs_Object_##x)); _ejs_object_setprop (_ejs_Object, funcname, tmpfunc); EJS_MACRO_END
#define PROTO_METHOD(x) EJS_MACRO_START ADD_STACK_ROOT(ejsval, funcname, _ejs_string_new_utf8(#x)); ADD_STACK_ROOT(ejsval, tmpfunc, _ejs_function_new (_ejs_null, funcname, (EJSClosureFunc)_ejs_Object_prototype_##x)); _ejs_object_setprop (_ejs_Object_proto, funcname, tmpfunc); EJS_MACRO_END

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

#undef PROTOTYPE_METHOD
#undef OBJ_METHOD

    _ejs_object_setprop_utf8 (global, "Object", _ejs_Object);

    END_SHADOW_STACK_FRAME;
}




static ejsval
_ejs_object_specop_get (ejsval obj_, ejsval propertyName, EJSBool isCStr)
{
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
        return obj->map->fields[prop_index];
    }
}

static ejsval
_ejs_object_specop_get_own_property (ejsval obj, ejsval propertyName)
{
    NOT_IMPLEMENTED();
}

static ejsval
_ejs_object_specop_get_property (ejsval obj, ejsval propertyName)
{
    NOT_IMPLEMENTED();
}

static void
_ejs_object_specop_put (ejsval obj, ejsval propertyName, ejsval val, EJSBool flag)
{
    NOT_IMPLEMENTED();
}

static EJSBool
_ejs_object_specop_can_put (ejsval obj, ejsval propertyName)
{
    NOT_IMPLEMENTED();
}

static EJSBool
_ejs_object_specop_has_property (ejsval obj, ejsval propertyName)
{
    NOT_IMPLEMENTED();
}

static EJSBool
_ejs_object_specop_delete (ejsval obj, ejsval propertyName, EJSBool flag)
{
    NOT_IMPLEMENTED();
}

static ejsval
_ejs_object_specop_default_value (ejsval obj, const char *hint)
{
    NOT_IMPLEMENTED();
}

static void
_ejs_object_specop_define_own_property (ejsval obj, ejsval propertyName, ejsval propertyDescriptor, EJSBool flag)
{
    NOT_IMPLEMENTED();
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
