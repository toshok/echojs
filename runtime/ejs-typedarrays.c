/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include <assert.h>
#include <string.h>
#include <math.h>

#include "ejs.h"
#include "ejs-ops.h"
#include "ejs-typedarrays.h"
#include "ejs-function.h"
#include "ejs-string.h"
#include "ejs-array.h"
#include "ejs-error.h"

static ejsval  _ejs_arraybuffer_specop_get (ejsval obj, ejsval propertyName, EJSBool isCStr);
static EJSPropertyDesc* _ejs_arraybuffer_specop_get_own_property (ejsval obj, ejsval propertyName);
static EJSPropertyDesc* _ejs_arraybuffer_specop_get_property (ejsval obj, ejsval propertyName);
static void    _ejs_arraybuffer_specop_put (ejsval obj, ejsval propertyName, ejsval val, EJSBool flag);
static EJSBool _ejs_arraybuffer_specop_can_put (ejsval obj, ejsval propertyName);
static EJSBool _ejs_arraybuffer_specop_has_property (ejsval obj, ejsval propertyName);
static EJSBool _ejs_arraybuffer_specop_delete (ejsval obj, ejsval propertyName, EJSBool flag);
static ejsval  _ejs_arraybuffer_specop_default_value (ejsval obj, const char *hint);
static EJSBool _ejs_arraybuffer_specop_define_own_property (ejsval obj, ejsval propertyName, EJSPropertyDesc* propertyDescriptor, EJSBool flag);
static EJSObject* _ejs_arraybuffer_specop_allocate ();
static void    _ejs_arraybuffer_specop_finalize (EJSObject* obj);
static void    _ejs_arraybuffer_specop_scan (EJSObject* obj, EJSValueFunc scan_func);

EJSSpecOps _ejs_arraybuffer_specops = {
    "ArrayBuffer",
    _ejs_arraybuffer_specop_get,
    _ejs_arraybuffer_specop_get_own_property,
    _ejs_arraybuffer_specop_get_property,
    _ejs_arraybuffer_specop_put,
    _ejs_arraybuffer_specop_can_put,
    _ejs_arraybuffer_specop_has_property,
    _ejs_arraybuffer_specop_delete,
    _ejs_arraybuffer_specop_default_value,
    _ejs_arraybuffer_specop_define_own_property,
    NULL, /* [[HasInstance]] */

    _ejs_arraybuffer_specop_allocate,
    _ejs_arraybuffer_specop_finalize,
    _ejs_arraybuffer_specop_scan
};

static EJSObject* _ejs_typedarray_specop_allocate ();
static void    _ejs_typedarray_specop_finalize (EJSObject* obj);
static void    _ejs_typedarray_specop_scan (EJSObject* obj, EJSValueFunc scan_func);

/* specops that are shared by all the typed array types with overrides for particular methods */
static EJSSpecOps _ejs_typedarray_specops = {
    "TypedArray",

    NULL, NULL, NULL,
    NULL, NULL, NULL,
    NULL, NULL, NULL,
    NULL,

    _ejs_typedarray_specop_allocate,
    _ejs_typedarray_specop_finalize,
    _ejs_typedarray_specop_scan
};


#define EJSVAL_IS_TYPEDARRAY(v) (EJSVAL_IS_OBJECT(v) &&         \
                                 (EJSVAL_TO_OBJECT(v)->ops == &_ejs_int8array_specops))

#define EJSVAL_IS_ARRAYBUFFER(v) (EJSVAL_IS_OBJECT(v) && (EJSVAL_TO_OBJECT(v)->ops == &_ejs_arraybuffer_specops))

#define _EJS_ARRAY_LEN(arrobj)      (((EJSArray*)arrobj)->array_length)
#define _EJS_ARRAY_ELEMENTS(arrobj) (((EJSArray*)arrobj)->elements)

ejsval _ejs_ArrayBuffer_proto;
ejsval _ejs_ArrayBuffer;

ejsval _ejs_Int8Array_proto;
ejsval _ejs_Int8Array;

ejsval _ejs_Uint8Array_proto;
ejsval _ejs_Uint8Array;

ejsval _ejs_Uint8ClampedArray_proto;
ejsval _ejs_Uint8ClampedArray;

ejsval _ejs_Int16Array_proto;
ejsval _ejs_Int16Array;

ejsval _ejs_Uint16Array_proto;
ejsval _ejs_Uint16Array;

ejsval _ejs_Int32Array_proto;
ejsval _ejs_Int32Array;

ejsval _ejs_Uint32Array_proto;
ejsval _ejs_Uint32Array;

ejsval _ejs_Float32Array_proto;
ejsval _ejs_Float32Array;

ejsval _ejs_Float64Array_proto;
ejsval _ejs_Float64Array;

ejsval
_ejs_arraybuffer_new (int size)
{
    EJSArrayBuffer *rv = _ejs_gc_new(EJSArrayBuffer);

    _ejs_init_object ((EJSObject*)rv, _ejs_ArrayBuffer_proto, &_ejs_arraybuffer_specops);

    rv->dependent = EJS_FALSE;
    rv->size = size;
    rv->data.alloced_buf = calloc(1, size);

    return OBJECT_TO_EJSVAL((EJSObject*)rv);
}

ejsval
_ejs_arraybuffer_new_slice (ejsval bufferval, int offset, int size)
{
    EJSArrayBuffer* rv = _ejs_gc_new(EJSArrayBuffer);
    EJSArrayBuffer* buffer = (EJSArrayBuffer*)EJSVAL_TO_OBJECT(bufferval);

    _ejs_init_object ((EJSObject*)rv, _ejs_ArrayBuffer_proto, &_ejs_arraybuffer_specops);

    rv->dependent = EJS_TRUE;
    rv->data.dependent.offset = MIN(buffer->size, offset);
    rv->data.dependent.buf = bufferval;
    rv->size = size;
    if (rv->size + rv->data.dependent.offset > buffer->size)
        rv->size = buffer->size - offset;

    return OBJECT_TO_EJSVAL((EJSObject*)rv);
}


static ejsval
_ejs_ArrayBuffer_impl (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    if (EJSVAL_IS_UNDEFINED(_this)) {
        printf ("ArrayBuffer called as a function\n");
        EJS_NOT_IMPLEMENTED();
    }

    uint32_t size = 0;
    if (argc > 0) size = ToUint32(args[0]);

    EJSArrayBuffer* buffer = (EJSArrayBuffer*)EJSVAL_TO_OBJECT(_this);
    buffer->dependent = EJS_FALSE;
    buffer->size = size;
    buffer->data.alloced_buf = calloc (1, size);

    return _this;
}

static ejsval
_ejs_ArrayBuffer_prototype_slice (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    EJSArrayBuffer* buffer = (EJSArrayBuffer*)EJSVAL_TO_OBJECT(_this);

    uint32_t len;
    uint32_t offset;

    switch (argc) {
    case 0:
        len = buffer->size;
        offset = 0;
        break;
    case 1:
        len = ToUint32(args[0]);
        offset = 0;
        break;
    case 2:
    default:
        len = ToUint32(args[0]);
        offset = ToUint32(args[1]);
        break;
    }

    return _ejs_arraybuffer_new_slice(_this, offset, len);
}

#define EJS_TYPED_ARRAY(ArrayType, arraytype, elementtype, elementSizeInBytes) \
    static ejsval                                                       \
    _ejs_##ArrayType##Array_impl (ejsval env, ejsval _this, uint32_t argc, ejsval *args) \
    {                                                                   \
     if (EJSVAL_IS_UNDEFINED(_this))                                    \
         _ejs_throw_nativeerror (EJS_TYPE_ERROR, "Constructor cannot be called as a function"); \
                                                                        \
     EJSTypedArray* arr = (EJSTypedArray*)EJSVAL_TO_OBJECT(_this);      \
                                                                        \
     uint32_t array_len = 0;                                            \
                                                                        \
     if (argc == 0)                                                     \
         goto construct_from_array_len;                                 \
                                                                        \
     /* TypedArray(TypedArray array) */                                 \
     /* TypedArray(type[] array) */                                     \
     /* TypedArray(ArrayBuffer buffer) */                               \
                                                                        \
     if (EJSVAL_IS_OBJECT(args[0])) {                                   \
         if (EJSVAL_IS_TYPEDARRAY(args[0])) {                           \
             /* TypedArray(TypedArray array) */                         \
             EJSTypedArray* typed_array = (EJSTypedArray*)EJSVAL_TO_OBJECT(args[0]); \
                                                                        \
             array_len = typed_array->length;                           \
             arr->length = array_len;                                   \
             arr->byteOffset = 0;                                       \
             arr->byteLength = array_len * (elementSizeInBytes);        \
             arr->buffer = _ejs_arraybuffer_new (arr->byteLength);      \
                                                                        \
             printf ("need to copy the existing data from the typed array to this array\n"); \
             EJS_NOT_IMPLEMENTED();                                     \
         }                                                              \
         else if (EJSVAL_IS_ARRAY(args[0])) {                           \
             /* TypedArray(type[] array) */                             \
                                                                        \
             array_len = EJS_ARRAY_LEN(args[0]);                        \
             arr->length = array_len;                                   \
             arr->byteOffset = 0;                                       \
             arr->byteLength = array_len * (elementSizeInBytes);        \
             arr->buffer = _ejs_arraybuffer_new (arr->byteLength);      \
                                                                        \
             printf ("need to copy the existing data from the array to this array\n"); \
             EJS_NOT_IMPLEMENTED();                                     \
         }                                                              \
         else if (EJSVAL_IS_ARRAYBUFFER(args[0])) {                     \
             EJSArrayBuffer* buffer = (EJSArrayBuffer*)EJSVAL_TO_OBJECT(args[0]); \
             /* TypedArray(ArrayBuffer buffer) */                       \
             /* TypedArray(ArrayBuffer buffer, unsigned long byteOffset) */ \
             /* TypedArray(ArrayBuffer buffer, unsigned long byteOffset, unsigned long length) */ \
             uint32_t byteOffset = 0;                                   \
             uint32_t byteLength = buffer->size;                        \
                                                                        \
             if (argc > 1) byteOffset = ToUint32(args[1]);              \
             if (argc > 2) byteLength = ToUint32(args[2]);              \
                                                                        \
             if (byteOffset > buffer->size)              byteOffset = buffer->size; \
             if (byteOffset + byteLength > buffer->size) byteLength = buffer->size - byteOffset; \
                                                                        \
             arr->length = byteLength / sizeof (elementtype);           \
             arr->byteOffset = byteOffset;                              \
             arr->byteLength = arr->length * (elementSizeInBytes);      \
             arr->buffer = args[0];                                     \
         }                                                              \
         else {                                                         \
             goto construct_from_array_len;                             \
         }                                                              \
     }                                                                  \
     else if (EJSVAL_IS_NUMBER(args[0])) {                              \
         /* TypedArray(unsigned long length) */                         \
         array_len = ToUint32(args[0]);                                 \
     construct_from_array_len: {                                        \
             arr->length = array_len;                                   \
             arr->byteOffset = 0;                                       \
             arr->byteLength = array_len * (elementSizeInBytes);        \
             arr->buffer = _ejs_arraybuffer_new (arr->byteLength);      \
         }                                                              \
     }                                                                  \
     else {                                                             \
         EJS_NOT_IMPLEMENTED();                                         \
     }                                                                  \
                                                                        \
     _ejs_object_define_value_property (_this, _ejs_atom_length, DOUBLE_TO_EJSVAL_IMPL(arr->length), EJS_PROP_FLAGS_ENUMERABLE); \
     _ejs_object_define_value_property (_this, _ejs_atom_byteOffset, DOUBLE_TO_EJSVAL_IMPL(arr->byteOffset), EJS_PROP_FLAGS_ENUMERABLE); \
     _ejs_object_define_value_property (_this, _ejs_atom_byteLength, DOUBLE_TO_EJSVAL_IMPL(arr->byteLength), EJS_PROP_FLAGS_ENUMERABLE); \
     _ejs_object_define_value_property (_this, _ejs_atom_buffer, arr->buffer, EJS_PROP_FLAGS_ENUMERABLE); \
                                                                        \
     return _this;                                                      \
 }                                                                      \
                                                                        \
 static ejsval                                                          \
 _ejs_##arraytype##array_specop_get (ejsval obj, ejsval propertyName, EJSBool isCStr) \
 {                                                                      \
     EJS_NOT_IMPLEMENTED();                                             \
 }                                                                      \
                                                                        \
 static EJSPropertyDesc*                                                \
 _ejs_##arraytype##array_specop_get_own_property (ejsval obj, ejsval propertyName) \
 {                                                                      \
     EJS_NOT_IMPLEMENTED();                                             \
 }                                                                      \
                                                                        \
 static EJSPropertyDesc*                                                \
 _ejs_##arraytype##array_specop_get_property (ejsval obj, ejsval propertyName) \
 {                                                                      \
     EJS_NOT_IMPLEMENTED();                                             \
 }                                                                      \
                                                                        \
 static void                                                            \
 _ejs_##arraytype##array_specop_put (ejsval obj, ejsval propertyName, ejsval val, EJSBool flag) \
 {                                                                      \
     EJS_NOT_IMPLEMENTED();                                             \
 }                                                                      \
                                                                        \
 static EJSBool                                                         \
 _ejs_##arraytype##array_specop_can_put (ejsval obj, ejsval propertyName) \
 {                                                                      \
     EJS_NOT_IMPLEMENTED();                                             \
 }                                                                      \
                                                                        \
 static EJSBool                                                         \
 _ejs_##arraytype##array_specop_has_property (ejsval obj, ejsval propertyName) \
 {                                                                      \
     EJS_NOT_IMPLEMENTED();                                             \
 }                                                                      \
                                                                        \
 EJSSpecOps _ejs_##arraytype##array_specops

EJS_TYPED_ARRAY(Int8, int8, int8_t, 1);
EJS_TYPED_ARRAY(Uint8, uint8, uint8_t, 1);
//EJS_TYPED_ARRAY(Uint8Clamped, uint8clamped, uint8_t, 1);
EJS_TYPED_ARRAY(Int16, int16, int16_t, 2);
EJS_TYPED_ARRAY(Uint16, uint16, uint16_t, 2);
EJS_TYPED_ARRAY(Int32, int32, int32_t, 4);
EJS_TYPED_ARRAY(Uint32, uint32, uint32_t, 4);
EJS_TYPED_ARRAY(Float32, float32, float, 4);
EJS_TYPED_ARRAY(Float64, float64, double, 8);


void
_ejs_typedarrays_init(ejsval global)
{
    START_SHADOW_STACK_FRAME;

#define OBJ_METHOD(t,x) EJS_INSTALL_FUNCTION(_ejs_##t, EJS_STRINGIFY(x), _ejs_##t##_##x)
#define PROTO_METHOD(t,x) EJS_INSTALL_FUNCTION(_ejs_##t##_proto, EJS_STRINGIFY(x), _ejs_##t##_prototype_##x)

    // ArrayBuffer
    {
        _ejs_gc_add_named_root (_ejs_ArrayBuffer_proto);
        _ejs_ArrayBuffer_proto = _ejs_object_new(_ejs_null, &_ejs_object_specops);

        PROTO_METHOD(ArrayBuffer, slice);

        ADD_STACK_ROOT(ejsval, tmpobj, _ejs_function_new (_ejs_null, _ejs_atom_ArrayBuffer, (EJSClosureFunc)_ejs_ArrayBuffer_impl));
        _ejs_ArrayBuffer = tmpobj;
        _ejs_object_setprop (_ejs_ArrayBuffer, _ejs_atom_prototype,   _ejs_ArrayBuffer_proto);
        _ejs_object_setprop (global,           _ejs_atom_ArrayBuffer, _ejs_ArrayBuffer);

        _ejs_int8array_specops = _ejs_typedarray_specops;
        _ejs_int8array_specops.class_name = "Int8Array";
        _ejs_int8array_specops.get = _ejs_int8array_specop_get;
        _ejs_int8array_specops.get_own_property = _ejs_int8array_specop_get_own_property;
        _ejs_int8array_specops.get_property = _ejs_int8array_specop_get_property;
        _ejs_int8array_specops.put = _ejs_int8array_specop_put;
        _ejs_int8array_specops.can_put = _ejs_int8array_specop_can_put;
        _ejs_int8array_specops.has_property = _ejs_int8array_specop_has_property;
    }


#define ADD_TYPEDARRAY(ArrayType, elementSizeInBytes) EJS_MACRO_START   \
    _ejs_gc_add_named_root (_ejs_##ArrayType##Array_proto);         \
    _ejs_##ArrayType##Array_proto = _ejs_object_new(_ejs_null, &_ejs_object_specops); \
                                                                        \
    ADD_STACK_ROOT(ejsval, tmpobj, _ejs_function_new (_ejs_null, _ejs_atom_##ArrayType##Array, (EJSClosureFunc)_ejs_##ArrayType##Array_impl)); \
    _ejs_##ArrayType##Array = tmpobj;                                            \
                                                                        \
    /* make sure ctor.BYTES_PER_ELEMENT is defined */                   \
    _ejs_object_define_value_property (_ejs_##ArrayType##Array, _ejs_atom_BYTES_PER_ELEMENT, NUMBER_TO_EJSVAL(elementSizeInBytes), EJS_PROP_FLAGS_ENUMERABLE); \
                                                                        \
    _ejs_object_setprop (_ejs_##ArrayType##Array, _ejs_atom_prototype,  _ejs_##ArrayType##Array_proto); \
    _ejs_object_setprop (global,         _ejs_atom_##ArrayType##Array,  _ejs_##ArrayType##Array); \
                                                                        \
    EJS_MACRO_END

    ADD_TYPEDARRAY(Int8, 1);
    ADD_TYPEDARRAY(Uint8, 1);
    //ADD_TYPEDARRAY(Uint8Clamped, 1);
    ADD_TYPEDARRAY(Int16, 2);
    ADD_TYPEDARRAY(Uint16, 2);
    ADD_TYPEDARRAY(Int32, 4);
    ADD_TYPEDARRAY(Uint32, 4);
    ADD_TYPEDARRAY(Float32, 4);
    ADD_TYPEDARRAY(Float64, 8);

    END_SHADOW_STACK_FRAME;
}

static ejsval
_ejs_arraybuffer_specop_get (ejsval obj, ejsval propertyName, EJSBool isCStr)
{
    // check if propertyName is an integer, or a string that we can convert to an int
    EJSBool is_index = EJS_FALSE;
    int idx = 0;
    if (!isCStr && EJSVAL_IS_NUMBER(propertyName)) {
        double n = EJSVAL_TO_NUMBER(propertyName);
        if (floor(n) == n) {
            idx = (int)n;
            is_index = EJS_TRUE;
        }
    }

    if (is_index) {
        if (idx < 0 || idx > EJS_ARRAY_LEN(obj)) {
            printf ("getprop(%d) on an array, returning undefined\n", idx);
            return _ejs_undefined;
        }
        return EJS_ARRAY_ELEMENTS(obj)[idx];
    }

    // we also handle the length getter here
    if ((isCStr && !strcmp("length", (char*)EJSVAL_TO_PRIVATE_PTR_IMPL(propertyName)))
        || (!isCStr && EJSVAL_IS_STRING(propertyName) && !ucs2_strcmp (_ejs_ucs2_length, EJSVAL_TO_FLAT_STRING(propertyName)))) {
        return NUMBER_TO_EJSVAL (EJS_ARRAY_LEN(obj));
    }

    // otherwise we fallback to the object implementation
    return _ejs_object_specops.get (obj, propertyName, isCStr);
}

static EJSPropertyDesc*
_ejs_arraybuffer_specop_get_own_property (ejsval obj, ejsval propertyName)
{
    if (EJSVAL_IS_NUMBER(propertyName)) {
        double needle = EJSVAL_TO_NUMBER(propertyName);
        int needle_int;
        if (EJSDOUBLE_IS_INT32(needle, &needle_int)) {
            if (needle_int >= 0 && needle_int < EJS_ARRAY_LEN(obj))
                return NULL; // XXX
        }
    }

    if (!ucs2_strcmp (_ejs_ucs2_length, EJSVAL_TO_FLAT_STRING(propertyName))) {
        EJSArray* arr = (EJSArray*)EJSVAL_TO_OBJECT(obj);
        _ejs_property_desc_set_value (&arr->array_length_desc, NUMBER_TO_EJSVAL(EJSARRAY_LEN(arr)));
        return &arr->array_length_desc;
    }

    return _ejs_object_specops.get_own_property (obj, propertyName);
}

static EJSPropertyDesc*
_ejs_arraybuffer_specop_get_property (ejsval obj, ejsval propertyName)
{
    return _ejs_object_specops.get_property (obj, propertyName);
}

static void
_ejs_arraybuffer_specop_put (ejsval obj, ejsval propertyName, ejsval val, EJSBool flag)
{
    // check if propertyName is a uint32, or a string that we can convert to an uint32
    int idx = -1;
    if (EJSVAL_IS_NUMBER(propertyName)) {
        double n = EJSVAL_TO_NUMBER(propertyName);
        if (floor(n) == n) {
            idx = (int)n;
        }
    }

    if (idx != -1) {
        if (idx >= EJS_ARRAY_ALLOC(obj)) {
            int new_alloc = idx + 10;
            ejsval* new_elements = (ejsval*)malloc (sizeof(ejsval*) * new_alloc);
            memmove (new_elements, EJS_ARRAY_ELEMENTS(obj), EJS_ARRAY_ALLOC(obj) * sizeof(ejsval));
            free (EJS_ARRAY_ELEMENTS(obj));
            EJS_ARRAY_ELEMENTS(obj) = new_elements;
            EJS_ARRAY_ALLOC(obj) = new_alloc;
        }
        EJS_ARRAY_ELEMENTS(obj)[idx] = val;
        EJS_ARRAY_LEN(obj) = idx + 1;
        if (EJS_ARRAY_LEN(obj) >= EJS_ARRAY_ALLOC(obj))
            abort();
        return;
    }
    // if we fail there, we fall back to the object impl below

    _ejs_object_specops.put (obj, propertyName, val, flag);
}

static EJSBool
_ejs_arraybuffer_specop_can_put (ejsval obj, ejsval propertyName)
{
    return _ejs_object_specops.can_put (obj, propertyName);
}

static EJSBool
_ejs_arraybuffer_specop_has_property (ejsval obj, ejsval propertyName)
{
    // check if propertyName is a uint32, or a string that we can convert to an uint32
    int idx = -1;
    if (EJSVAL_IS_NUMBER(propertyName)) {
        double n = EJSVAL_TO_NUMBER(propertyName);
        if (floor(n) == n) {
            idx = (int)n;

            return idx > 0 && idx < EJS_ARRAY_LEN(obj);
        }
    }

    // if we fail there, we fall back to the object impl below

    return _ejs_object_specops.has_property (obj, propertyName);
}

static EJSBool
_ejs_arraybuffer_specop_delete (ejsval obj, ejsval propertyName, EJSBool flag)
{
    // check if propertyName is a uint32, or a string that we can convert to an uint32
    int idx = -1;
    if (EJSVAL_IS_NUMBER(propertyName)) {
        double n = EJSVAL_TO_NUMBER(propertyName);
        if (floor(n) == n) {
            idx = (int)n;
        }
    }

    if (idx == -1)
        return _ejs_object_specops._delete (obj, propertyName, flag);

    // if it's outside the array bounds, do nothing
    if (idx < EJS_ARRAY_LEN(obj))
        EJS_ARRAY_ELEMENTS(obj)[idx] = _ejs_undefined;
    return EJS_TRUE;
}

static ejsval
_ejs_arraybuffer_specop_default_value (ejsval obj, const char *hint)
{
    return _ejs_object_specops.default_value (obj, hint);
}

static EJSBool
_ejs_arraybuffer_specop_define_own_property (ejsval obj, ejsval propertyName, EJSPropertyDesc* propertyDescriptor, EJSBool flag)
{
    return _ejs_object_specops.define_own_property (obj, propertyName, propertyDescriptor, flag);
}

static EJSObject*
_ejs_arraybuffer_specop_allocate()
{
    return (EJSObject*)_ejs_gc_new (EJSArrayBuffer);
}


static void
_ejs_arraybuffer_specop_finalize (EJSObject* obj)
{
    EJSArrayBuffer *arraybuf = (EJSArrayBuffer*)obj;
    if (!arraybuf->dependent) {
        free (arraybuf->data.alloced_buf);
        arraybuf->data.alloced_buf = NULL;
    }
    _ejs_object_specops.finalize (obj);
}

static void
_ejs_arraybuffer_specop_scan (EJSObject* obj, EJSValueFunc scan_func)
{
    EJSArrayBuffer *arraybuf = (EJSArrayBuffer*)obj;
    if (arraybuf->dependent) {
        scan_func (arraybuf->data.dependent.buf);
    }
    _ejs_object_specops.scan (obj, scan_func);
}


static EJSObject*
_ejs_typedarray_specop_allocate()
{
    return (EJSObject*)_ejs_gc_new (EJSTypedArray);
}


static void
_ejs_typedarray_specop_finalize (EJSObject* obj)
{
    //EJSTypedArray *arr = (EJSTypedArray*)obj;
    _ejs_object_specops.finalize (obj);
}

static void
_ejs_typedarray_specop_scan (EJSObject* obj, EJSValueFunc scan_func)
{
    EJSTypedArray *arr = (EJSTypedArray*)obj;
    scan_func(arr->buffer);
    _ejs_object_specops.scan (obj, scan_func);
}
