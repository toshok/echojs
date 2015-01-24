/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include <string.h>
#include <math.h>

#include "ejs.h"
#include "ejs-ops.h"
#include "ejs-typedarrays.h"
#include "ejs-function.h"
#include "ejs-string.h"
#include "ejs-array.h"
#include "ejs-error.h"
#include "ejs-symbol.h"
#include "ejs-proxy.h"

#define EJS_TYPEDARRAY_LEN(arrobj)      (((EJSTypedArray*)EJSVAL_TO_OBJECT(arrobj))->length)

static inline int
max(int a, int b)
{
    if (a > b) return a; else return b;
}

static inline int
min(int a, int b)
{
    if (a > b) return b; else return a;
}

ejsval _ejs_ArrayBuffer_prototype EJSVAL_ALIGNMENT;
ejsval _ejs_ArrayBuffer EJSVAL_ALIGNMENT;

ejsval _ejs_Int8Array_prototype EJSVAL_ALIGNMENT;
ejsval _ejs_Int8Array EJSVAL_ALIGNMENT;

ejsval _ejs_Uint8Array_prototype EJSVAL_ALIGNMENT;
ejsval _ejs_Uint8Array EJSVAL_ALIGNMENT;

ejsval _ejs_Uint8ClampedArray_prototype EJSVAL_ALIGNMENT;
ejsval _ejs_Uint8ClampedArray EJSVAL_ALIGNMENT;

ejsval _ejs_Int16Array_prototype EJSVAL_ALIGNMENT;
ejsval _ejs_Int16Array EJSVAL_ALIGNMENT;

ejsval _ejs_Uint16Array_prototype EJSVAL_ALIGNMENT;
ejsval _ejs_Uint16Array EJSVAL_ALIGNMENT;

ejsval _ejs_Int32Array_prototype EJSVAL_ALIGNMENT;
ejsval _ejs_Int32Array EJSVAL_ALIGNMENT;

ejsval _ejs_Uint32Array_prototype EJSVAL_ALIGNMENT;
ejsval _ejs_Uint32Array EJSVAL_ALIGNMENT;

ejsval _ejs_Float32Array_prototype EJSVAL_ALIGNMENT;
ejsval _ejs_Float32Array EJSVAL_ALIGNMENT;

ejsval _ejs_Float64Array_prototype EJSVAL_ALIGNMENT;
ejsval _ejs_Float64Array EJSVAL_ALIGNMENT;

ejsval _ejs_DataView_prototype EJSVAL_ALIGNMENT;
ejsval _ejs_DataView EJSVAL_ALIGNMENT;

static EJSBool
IsDetachedBuffer (ejsval arrayBuffer)
{
    /* 1. Assert: Type(arrayBuffer) is Object and it has [[ArrayBufferData]] internal slot. */
    if (!EJSVAL_IS_ARRAYBUFFER(arrayBuffer))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "value not an array buffer"); // assertion error?

    EJSArrayBuffer *Obj = EJSVAL_TO_ARRAYBUFFER(arrayBuffer);

    /* 2. If arrayBuffer’s [[ArrayBufferData]] internal slot is null, then return true. */
    if (Obj->dependent)
        return EJS_TRUE;

    /* 3. Return false. */
    return EJS_FALSE;
}

ejsval
_ejs_arraybuffer_new (int size)
{
    EJSArrayBuffer *rv = _ejs_gc_new(EJSArrayBuffer);

    _ejs_init_object ((EJSObject*)rv, _ejs_ArrayBuffer_prototype, &_ejs_ArrayBuffer_specops);

    rv->dependent = EJS_FALSE;
    rv->size = size;
    if (size)
        rv->data.alloced_buf = calloc(1, size);

    return OBJECT_TO_EJSVAL(rv);
}

ejsval
_ejs_arraybuffer_new_slice (ejsval bufferval, int offset, int size)
{
    EJSArrayBuffer* rv = _ejs_gc_new(EJSArrayBuffer);
    EJSArrayBuffer* buffer = (EJSArrayBuffer*)EJSVAL_TO_OBJECT(bufferval);

    _ejs_init_object ((EJSObject*)rv, _ejs_ArrayBuffer_prototype, &_ejs_ArrayBuffer_specops);

    rv->dependent = EJS_TRUE;
    rv->data.dependent.offset = MIN(buffer->size, offset);
    rv->data.dependent.buf = bufferval;
    rv->size = size;
    if (rv->size + rv->data.dependent.offset > buffer->size)
        rv->size = buffer->size - offset;

    return OBJECT_TO_EJSVAL(rv);
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
    if (size)
        buffer->data.alloced_buf = calloc (1, size);

    return _this;
}

static ejsval
_ejs_ArrayBuffer_create (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval F = _this;

    if (!EJSVAL_IS_CONSTRUCTOR(F)) 
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "'this' in ArrayBuffer[Symbol.create] is not a constructor");

    EJSObject* F_ = EJSVAL_TO_OBJECT(F);

    // 1. Let obj be the result of calling OrdinaryCreateFromConstructor(constructor, "%ArrayBufferPrototype%", ( [[ArrayBufferData]], [[ArrayBufferByteLength]]) ). 
    // 2. ReturnIfAbrupt(obj). 
    ejsval proto = OP(F_,Get)(F, _ejs_atom_prototype, F);
    if (EJSVAL_IS_UNDEFINED(proto))
        proto = _ejs_ArrayBuffer_prototype;

    EJSObject* obj = (EJSObject*)_ejs_gc_new (EJSArrayBuffer);
    _ejs_init_object (obj, proto, &_ejs_ArrayBuffer_specops);
    
    // 3. Set the [[ArrayBufferByteLength]] internal slot of obj to 0. 
    // 4. Return obj. 
    return OBJECT_TO_EJSVAL(obj);
}

static ejsval
_ejs_ArrayBuffer_get_species (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    return _ejs_ArrayBuffer;
}

static ejsval
_ejs_ArrayBuffer_isView (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval arg = _ejs_undefined;

    if (argc > 0)
        arg = args[0];

    /* 1. If Type(arg) is not Object, return false. */
    if (!EJSVAL_IS_OBJECT(arg))
        return BOOLEAN_TO_EJSVAL(EJS_FALSE);

    /* 2. If arg has a [[ViewedArrayBuffer]] internal slot, then return true. */
    if (EJSVAL_IS_DATAVIEW(arg) || EJSVAL_IS_TYPEDARRAY(arg))
        return BOOLEAN_TO_EJSVAL(EJS_TRUE);

    /* 3. Return false. */
    return BOOLEAN_TO_EJSVAL(EJS_FALSE);
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

static ejsval
_ejs_DataView_impl (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    if (EJSVAL_IS_UNDEFINED(_this)) {
        _ejs_log ("DataView called as a function\n");
        EJS_NOT_IMPLEMENTED();
    }

    if (argc == 0 || !EJSVAL_IS_ARRAYBUFFER(args[0])) {
        _ejs_log ("arg0 not an ArrayBuffer object\n");
        EJS_NOT_IMPLEMENTED();
    }

    EJSDataView* view = (EJSDataView*)EJSVAL_TO_OBJECT(_this);
    EJSArrayBuffer* buff = (EJSArrayBuffer*)EJSVAL_TO_OBJECT(args[0]);

    uint32_t offset;
    uint32_t len;

    switch (argc) {
    case 1:
        offset = 0;
        len = buff->size;
        break;
    case 2:
        offset = EJSVAL_TO_NUMBER(args[1]);
        len = buff->size - offset;
        break;
    default:
        offset = EJSVAL_TO_NUMBER(args[1]);
        len = EJSVAL_TO_NUMBER(args[2]);
    }

    view->buffer = args[0];
    view->byteOffset = offset;
    view->byteLength = len;

    _ejs_object_define_value_property (_this, _ejs_atom_byteLength, DOUBLE_TO_EJSVAL_IMPL(view->byteLength), EJS_PROP_FLAGS_ENUMERABLE);
    _ejs_object_define_value_property (_this, _ejs_atom_byteOffset, DOUBLE_TO_EJSVAL_IMPL(view->byteOffset), EJS_PROP_FLAGS_ENUMERABLE);
    _ejs_object_define_value_property (_this, _ejs_atom_buffer, view->buffer, EJS_PROP_FLAGS_ENUMERABLE);

    return _this;
}

static ejsval
_ejs_DataView_create (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    // 1. Let F be the this value. 
    ejsval F = _this;

    if (!EJSVAL_IS_CONSTRUCTOR(F)) 
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "'this' in DataView[Symbol.create] is not a constructor");

    EJSObject* F_ = EJSVAL_TO_OBJECT(F);

    // 2. Let obj be the result of calling OrdinaryCreateFromConstructor(F, "%DataViewPrototype%", ([[DataView]], [[ViewedArrayBuffer]], [[ByteLength]], [[ByteOffset]]) ). 
    ejsval proto = OP(F_,Get)(F, _ejs_atom_prototype, F);
    if (EJSVAL_IS_UNDEFINED(proto))
        proto = _ejs_DataView_prototype;

    // 3. Set the value of obj’s [[DataView]] internal slot to true. 

    EJSObject* obj = (EJSObject*)_ejs_gc_new (EJSDataView);
    _ejs_init_object (obj, proto, &_ejs_DataView_specops);
    
    // 4. Return obj. 
    return OBJECT_TO_EJSVAL(obj);
}


static inline EJSBool
needToSwap(EJSBool littleEndian)
{
#ifdef IS_LITTLE_ENDIAN
    return !littleEndian;
#else
    return littleEndian;
#endif
}

#define EJS_SWAP_2BYTES(x)                                              \
    ( ((x >> 8) & 0x00FF) | ((x << 8) & 0xFF00) )

#define EJS_SWAP_4BYTES(x)                                              \
    ( ((x >> 24) & 0x000000FF) | ((x >> 8) & 0x0000FF00) |              \
      ((x << 8) & 0x00FF0000) | ((x << 24) & 0xFF000000) )

#define EJS_SWAP_8BYTES(x)                                              \
   ( ((x >> 56) & 0x00000000000000FF) | ((x >> 40) & 0x000000000000FF00) |      \
      ((x >> 24) & 0x0000000000FF0000) | ((x >> 8) & 0x000000000FF000000) |     \
      ((x << 8) & 0x000000FF00000000) | ((x << 24) & 0x0000FF0000000000) |      \
      ((x << 40) & 0x00FF000000000000) | ((x << 56) & 0xFF00000000000000) )

static void
swapBytes (void* value, int elementSizeInBytes)
{
    switch (elementSizeInBytes) {
    case 2:
        *((uint16_t*)value) = EJS_SWAP_2BYTES(*((uint16_t*)value));
        break;
    case 4:
        *((uint32_t*)value) = EJS_SWAP_4BYTES(*((uint32_t*)value));
        break;
    case 8:
        *((uint64_t*)value) = EJS_SWAP_8BYTES(*((uint64_t*)value));
        break;
    default: /* just return the original value */
        break;
    }
}

#define EJS_DATA_VIEW_METHOD_IMPL(ElementType, elementtype, elementSizeInBytes)     \
    static ejsval                                                   \
    _ejs_DataView_prototype_get##ElementType##_impl (ejsval env, ejsval _this, uint32_t argc, ejsval *args) \
    {                                                               \
        if (argc < 1) {                                             \
            _ejs_log ("wrong number of arguments\n");               \
            EJS_NOT_IMPLEMENTED();                                  \
        }                                                           \
                                                                    \
        uint32_t idx = EJSVAL_TO_NUMBER(args[0]);                   \
        EJSBool littleEndian = EJS_FALSE;                           \
        if (argc > 1)                                               \
            littleEndian = EJSVAL_TO_BOOLEAN(args[1]);              \
                                                                    \
        char* data = _ejs_dataview_get_data (EJSVAL_TO_OBJECT(_this)); \
        elementtype val;                                            \
        memcpy (&val, data + idx, elementSizeInBytes);              \
        if (needToSwap(littleEndian))                               \
            swapBytes(&val, elementSizeInBytes);                    \
                                                                    \
        return NUMBER_TO_EJSVAL(val);                               \
    }                                                               \
                                                                    \
    static void                                                     \
    _ejs_DataView_prototype_set##ElementType##_impl (ejsval env, ejsval _this, uint32_t argc, ejsval *args) \
    {                                                               \
        if (argc < 2) {                                             \
            _ejs_log ("wrong number of arguments\n");               \
            EJS_NOT_IMPLEMENTED();                                  \
        }                                                           \
                                                                    \
        uint32_t idx = EJSVAL_TO_NUMBER(args[0]);                   \
        elementtype val = (elementtype)EJSVAL_TO_NUMBER(args[1]);   \
        EJSBool littleEndian = EJS_FALSE;                           \
        if (argc > 2)                                               \
            littleEndian = EJSVAL_TO_BOOLEAN(args[2]);              \
                                                                    \
        if (needToSwap(littleEndian))                               \
            swapBytes(&val, elementSizeInBytes);                    \
                                                                    \
        char* data = _ejs_dataview_get_data (EJSVAL_TO_OBJECT(_this)); \
        memcpy (data+idx, &val, elementSizeInBytes);                \
    }                                                               \

EJS_DATA_VIEW_METHOD_IMPL(Int8, int8_t, 1);
EJS_DATA_VIEW_METHOD_IMPL(Int16, int16_t, 2);
EJS_DATA_VIEW_METHOD_IMPL(Int32, int32_t, 4);
EJS_DATA_VIEW_METHOD_IMPL(Float32, float, 4);
EJS_DATA_VIEW_METHOD_IMPL(Float64, double, 8);

#define EJS_TYPED_ARRAY(EnumType, ArrayType, arraytype, elementtype, elementSizeInBytes) \
    static ejsval                                                       \
    _ejs_##ArrayType##Array_impl (ejsval env, ejsval _this, uint32_t argc, ejsval *args) \
    {                                                                   \
     if (EJSVAL_IS_UNDEFINED(_this))                                    \
         _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Constructor cannot be called as a function"); \
                                                                        \
     EJSTypedArray* arr = (EJSTypedArray*)EJSVAL_TO_OBJECT(_this);      \
                                                                        \
     uint32_t array_len = 0;                                            \
                                                                        \
     arr->element_type = EJS_TYPEDARRAY_##EnumType;                     \
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
             _ejs_log ("need to copy the existing data from the typed array to this array\n"); \
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
             void* buf_data = ((EJSArrayBuffer*)EJSVAL_TO_OBJECT(arr->buffer))->data.alloced_buf; \
             if (EJSVAL_IS_DENSE_ARRAY(args[0])) {                      \
                 EJSObject* arr = EJSVAL_TO_OBJECT(args[0]);            \
                 int i;                                                 \
                 for (i = 0; i < EJSARRAY_LEN (arr); i ++) {            \
                     ((elementtype*)buf_data)[i] = (elementtype)EJSVAL_TO_NUMBER(EJSDENSEARRAY_ELEMENTS(arr)[i]); \
                 }                                                      \
             }                                                          \
             else {                                                     \
                 _ejs_log ("need to implement normal array object copying for sparse arrays.  or do we?\n"); \
                 EJS_NOT_IMPLEMENTED();                                 \
             }                                                          \
         }                                                              \
         else if (EJSVAL_IS_ARRAYBUFFER(args[0])) {                     \
             EJSArrayBuffer* buffer = (EJSArrayBuffer*)EJSVAL_TO_OBJECT(args[0]); \
             /* TypedArray(ArrayBuffer buffer) */                       \
             /* TypedArray(ArrayBuffer buffer, unsigned long byteOffset) */ \
             /* TypedArray(ArrayBuffer buffer, unsigned long byteOffset, unsigned long length) */ \
             uint32_t byteOffset = 0;                                   \
             uint32_t byteLength = buffer->size;                        \
             EJSBool lengthSpecified = EJS_FALSE;                       \
                                                                        \
             if (argc > 1) byteOffset = ToUint32(args[1]);              \
             if (argc > 2) {                                            \
                 byteLength = ToUint32(args[2]) * elementSizeInBytes;   \
                 lengthSpecified = EJS_TRUE;                            \
             }                                                          \
                                                                        \
             if (byteOffset > buffer->size)              byteOffset = buffer->size; \
             if (byteOffset + byteLength > buffer->size) {              \
                 if (lengthSpecified)                                   \
                     _ejs_throw_nativeerror_utf8 (EJS_RANGE_ERROR, "Length is out of range."); \
                 else                                                   \
                     byteLength = buffer->size - byteOffset;            \
             }                                                          \
                                                                        \
             if ((byteOffset % sizeof (elementtype)) != 0)              \
                 _ejs_throw_nativeerror_utf8 (EJS_RANGE_ERROR, "Byte offset / length is not aligned."); \
             if ((byteLength % sizeof (elementtype)) != 0)              \
                 _ejs_throw_nativeerror_utf8 (EJS_RANGE_ERROR, "Byte offset / length is not aligned."); \
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
         _ejs_log ("arg0 not a number or object...\n");                      \
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
 _ejs_##ArrayType##array_specop_get (ejsval obj, ejsval propertyName, ejsval receiver)  \
 {                                                                      \
     /* check if propertyName is an integer, or a string that we can convert to an int */ \
     EJSBool is_index = EJS_FALSE;                                      \
     ejsval idx_val = ToNumber(propertyName);                           \
     int idx;                                                           \
     if (EJSVAL_IS_NUMBER(idx_val)) {                                   \
         double n = EJSVAL_TO_NUMBER(idx_val);                          \
         if (floor(n) == n) {                                           \
             idx = (int)n;                                              \
             is_index = EJS_TRUE;                                       \
         }                                                              \
     }                                                                  \
                                                                        \
     if (is_index) {                                                    \
         if (idx < 0 || idx > EJS_TYPEDARRAY_LEN(obj)) {                \
             return _ejs_undefined;                                     \
         }                                                              \
         void* data = _ejs_typedarray_get_data (EJSVAL_TO_OBJECT(obj)); \
         return NUMBER_TO_EJSVAL ((double)((elementtype*)data)[idx]);   \
     }                                                                  \
                                                                        \
     /* we also handle the length getter here */                        \
     if (EJSVAL_IS_STRING(propertyName) && !ucs2_strcmp (_ejs_ucs2_length, EJSVAL_TO_FLAT_STRING(propertyName))) { \
         return NUMBER_TO_EJSVAL (EJS_TYPEDARRAY_LEN(obj));             \
     }                                                                  \
                                                                        \
     /* otherwise we fallback to the object implementation */           \
     return _ejs_Object_specops.Get (obj, propertyName, receiver);      \
 }                                                                      \
                                                                        \
 static EJSPropertyDesc*                                                \
 _ejs_##ArrayType##array_specop_get_own_property (ejsval obj, ejsval propertyName, ejsval* exc) \
 {                                                                      \
     if (EJSVAL_IS_NUMBER(propertyName)) {                              \
         double needle = EJSVAL_TO_NUMBER(propertyName);                \
         int needle_int;                                                \
         if (EJSDOUBLE_IS_INT32(needle, &needle_int)) {                 \
             if (needle_int >= 0 && needle_int < EJS_TYPEDARRAY_LEN(obj)) \
                 return NULL; /* XXX */                                 \
         }                                                              \
     }                                                                  \
     return _ejs_Object_specops.GetOwnProperty (obj, propertyName, exc); \
 }                                                                      \
                                                                        \
 static EJSBool                                                         \
 _ejs_##ArrayType##array_specop_set (ejsval obj, ejsval propertyName, ejsval val, ejsval receiver) \
 {                                                                      \
     /* check if propertyName is an integer, or a string that we can convert to an int */ \
     EJSBool is_index = EJS_FALSE;                                      \
     ejsval idx_val = ToNumber(propertyName);                           \
     int idx;                                                           \
     if (EJSVAL_IS_NUMBER(idx_val)) {                                   \
         double n = EJSVAL_TO_NUMBER(idx_val);                          \
         if (floor(n) == n) {                                           \
             idx = (int)n;                                              \
             is_index = EJS_TRUE;                                       \
         }                                                              \
     }                                                                  \
                                                                        \
     if (is_index) {                                                    \
         if (idx < 0 || idx >= EJS_TYPEDARRAY_LEN(obj)) {               \
             return EJS_FALSE;                                          \
         }                                                              \
         void* data = _ejs_typedarray_get_data (EJSVAL_TO_OBJECT(obj)); \
         ((elementtype*)data)[idx] = (elementtype)EJSVAL_TO_NUMBER(val); \
     }                                                                  \
     return EJS_TRUE; /* XXX */                                         \
 }                                                                      \
                                                                        \
 static EJSBool                                                         \
 _ejs_##ArrayType##array_specop_define_own_property (ejsval obj, ejsval propertyName, EJSPropertyDesc* propertyDescriptor, EJSBool flag) \
 {                                                                      \
     return _ejs_Object_specops.DefineOwnProperty (obj, propertyName, propertyDescriptor, flag); \
 }                                                                      \
                                                                        \
 static EJSBool                                                         \
 _ejs_##ArrayType##array_specop_has_property (ejsval obj, ejsval propertyName) \
 {                                                                      \
     EJS_NOT_IMPLEMENTED();                                             \
 }                                                                      \
                                                                        \
 static ejsval                                                          \
 _ejs_##ArrayType##Array_prototype_set_impl (ejsval env, ejsval _this, uint32_t argc, ejsval *args) \
 {                                                                      \
     /* XXX needs a lot of help here... */                              \
     OP(EJSVAL_TO_OBJECT(_this), Set)(_this, args[0], args[1], _this); \
     return args[1];                                                    \
 }                                                                      \
                                                                        \
 static ejsval                                                          \
 _ejs_##ArrayType##Array_prototype_get_impl (ejsval env, ejsval _this, uint32_t argc, ejsval *args) \
 {                                                                      \
     EJS_NOT_IMPLEMENTED();                                             \
 }                                                                      \
                                                                        \
 static ejsval                                                          \
 _ejs_##ArrayType##Array_prototype_subarray_impl (ejsval env, ejsval _this, uint32_t argc, ejsval *args) \
 {                                                                      \
     ejsval begin = _ejs_undefined;                                     \
     ejsval end = _ejs_undefined;                                       \
                                                                        \
     if (argc > 0)                                                      \
        begin = args [0];                                               \
     if (argc > 1)                                                      \
        end = args [1];                                                 \
                                                                        \
     /* 1. Let O be the this value. */                                  \
     ejsval O = _this;                                                  \
                                                                        \
     /* 2. If Type(O) is not Object, throw a TypeError exception. */    \
     if (!EJSVAL_IS_OBJECT(O))                                          \
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "argument is not an Object"); \
                                                                        \
     /* 3. If O does not have a [[TypedArrayName]] internal slot, then throw a TypeError exception. */ \
     if (!EJSVAL_IS_TYPEDARRAY(O))                                       \
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "argument is not a typed array"); \
                                                                        \
     EJSTypedArray *Oobj = (EJSTypedArray*) EJSVAL_TO_OBJECT(O);       \
                                                                        \
     /* 4. Assert: O has a [[ViewedArrayBuffer]] internal slot. */      \
     /* 5. Let buffer be the value of O’s [[ViewedArrayBuffer]] internal slot. */ \
     ejsval buffer = Oobj->buffer;                                      \
                                                                        \
     /* 6. If buffer is undefined, then throw a TypeError exception. */ \
     if (EJSVAL_IS_UNDEFINED(buffer))                                   \
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "typed arrray's buffer is undefined"); \
                                                                        \
     /* 7. Let srcLength be the value of O’s [[ArrayLength]] internal slot. */ \
     uint32_t srcLength = Oobj->length;                                 \
                                                                        \
     /* 8. Let beginInt be ToInteger(begin) */                          \
     /* 9. ReturnIfAbrupt(beginInt). */                                 \
     int32_t beginInt = ToInteger(begin);                               \
                                                                        \
     /* 10. If beginInt < 0, then let beginInt be srcLength + beginInt. */  \
     if (beginInt < 0)                                                  \
        beginInt = srcLength + beginInt;                                \
                                                                        \
     /* 11. Let beginIndex be min(srcLength, max(0, beginInt)). */      \
     uint32_t beginIndex = min(srcLength, max(0, beginInt));             \
                                                                        \
     /* 12. If end is undefined, then let end be srcLength. */          \
     if (EJSVAL_IS_UNDEFINED(end))                                      \
        end = NUMBER_TO_EJSVAL(srcLength);                              \
                                                                        \
     /* 13. Let endInt be ToInteger(end). */                            \
     /* 14. ReturnIfAbrupt(endInt). */                                  \
     int endInt = ToInteger(end);                                       \
                                                                        \
     /* 15. If endInt < 0, then let endInt be srcLength + endInt. */    \
     if (endInt < 0)                                                    \
        endInt = srcLength + endInt;                                    \
                                                                        \
     /* 16. Let endIndex be max(0,min(srcLength, endInt)). */           \
     uint32_t endIndex = max(0, min(srcLength, endInt));                 \
                                                                        \
     /* 17. If endIndex < beginIndex, then let endIndex be beginIndex. */ \
     if (endIndex < beginIndex)                                         \
        endIndex = beginIndex;                                          \
                                                                        \
     /* 18. Let newLength be endIndex - beginIndex. */                  \
     uint32_t newLength = endIndex - beginIndex;                        \
                                                                        \
     /* 19. Let constructorName be the string value of O’s [[TypedArrayName]] internal slot. */ \
     /* 20. Let elementType be the string value of the Element Type value in Table 46 for constructorName. */ \
     /* 21. Let elementSize be the Number value of the Element Size value specified in Table 46 for constructorName. */ \
     uint32_t elementSize = elementSizeInBytes;                         \
                                                                        \
     /* 22. Let srcByteOffset be the value of O’s [[ByteOffset]] internal slot. */ \
     uint32_t srcByteOffset = Oobj->byteOffset;                         \
                                                                        \
     /* 23. Let beginByteOffset be srcByteOffset + beginIndex × elementSize. */ \
     uint32_t beginByteOffset = srcByteOffset + beginIndex * elementSize; \
                                                                        \
     /* TODO: From step 24, till the end of the method, */              \
     /* %TypedArray%.prototype.subarray() doesn't create instances of */\
     /* subclasses. Implement such support once we get species */       \
     /* constructor stuff */                                            \
                                                                        \
     /* 24. Let defaultConstructor be the intrinsic object listed in column one of Table 46 for constructorName. */ \
     /* 25. Let constructor be SpeciesConstructor(O, defaultConstructor). */ \
     /* 26. ReturnIfAbrupt(constructor). */                             \
                                                                        \
     /* 27. Let argumentsList be «buffer, beginByteOffset, newLength». */ \
     ejsval argumentsList[3] = { buffer, NUMBER_TO_EJSVAL(beginByteOffset), NUMBER_TO_EJSVAL(newLength) }; \
                                                                        \
     EJSTypedArray *rv = _ejs_gc_new(EJSTypedArray);                    \
     _ejs_init_object ((EJSObject*)rv, _ejs_##ArrayType##Array_prototype, _ejs_typed_array_specops[EJS_TYPEDARRAY_##EnumType]); \
                                                                        \
     /* 28. Return the result of calling the [[Construct]] internal method of constructor with argument argumentsList. */ \
     ejsval ctor = _ejs_##ArrayType##Array;                             \
     return _ejs_invoke_closure(ctor, OBJECT_TO_EJSVAL((EJSObject*)rv), 3, argumentsList); \
 }                                                                      \
                                                                        \
 /* this should be a single getter reused by all typed-arrays */        \
static ejsval                                                           \
_ejs_##ArrayType##Array_prototype_get_toStringTag (ejsval env, ejsval _this, uint32_t argc, ejsval *args) \
{                                                                       \
    if (!EJSVAL_IS_TYPEDARRAY(_this))                                   \
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "get toStringTag called on non-type array"); \
                                                                        \
    EJSTypedArray* arr = EJSVAL_TO_TYPEDARRAY(_this);                   \
    switch (arr->element_type) {                                        \
    case EJS_TYPEDARRAY_INT8:          return _ejs_atom_Int8Array;      \
    case EJS_TYPEDARRAY_UINT8:         return _ejs_atom_Uint8Array;     \
    case EJS_TYPEDARRAY_UINT8CLAMPED:  return _ejs_atom_Uint8ClampedArray; \
    case EJS_TYPEDARRAY_INT16:         return _ejs_atom_Int16Array;     \
    case EJS_TYPEDARRAY_UINT16:        return _ejs_atom_Uint16Array;    \
    case EJS_TYPEDARRAY_INT32:         return _ejs_atom_Int32Array;     \
    case EJS_TYPEDARRAY_UINT32:        return _ejs_atom_Uint32Array;    \
    case EJS_TYPEDARRAY_FLOAT32:       return _ejs_atom_Float32Array;   \
    case EJS_TYPEDARRAY_FLOAT64:       return _ejs_atom_Float64Array;   \
    default: EJS_NOT_IMPLEMENTED();                                     \
    }                                                                   \
}                                                                       \
                                                                        \
static ejsval                                                           \
_ejs_##ArrayType##Array_get_species (ejsval env, ejsval _this, uint32_t argc, ejsval *args) \
{                                                                       \
    return _ejs_##ArrayType##Array;                                     \
}                                                                       \
                                                                        \
static ejsval                                                           \
 _ejs_##ArrayType##Array_create (ejsval env, ejsval _this, uint32_t argc, ejsval *args) \
{                                                                       \
    /* 1. Let F be the this value. */                                   \
    /* 2. If Type(F) is not Object, then throw a TypeError exception. */ \
    ejsval F = _this;                                                   \
                                                                        \
    if (!EJSVAL_IS_CONSTRUCTOR(F))                                      \
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "'this' in "#ArrayType"Array[Symbol.create] is not a constructor"); \
                                                                        \
    EJSObject* F_ = EJSVAL_TO_OBJECT(F);                                \
                                                                        \
    /* 3. Let proto be GetPrototypeFromConstructor(F, "%TypedArrayPrototype%").  */ \
    /* 4. ReturnIfAbrupt(proto).  */                                    \
    ejsval proto = OP(F_,Get)(F, _ejs_atom_prototype, F);               \
    if (EJSVAL_IS_UNDEFINED(proto)) {                                   \
        proto = _ejs_##ArrayType##Array_prototype;                      \
    }                                                                   \
    /* 5. Let obj be IntegerIndexedObjectCreate (proto).  */            \
    EJSTypedArray* obj = _ejs_gc_new (EJSTypedArray);           \
    _ejs_init_object ((EJSObject*)obj, proto, _ejs_typed_array_specops[EJS_TYPEDARRAY_##EnumType]); \
    /* 6. Add a [[ViewedArrayBuffer]] internal slot to obj and set its initial value to undefined.  */ \
    obj->buffer = _ejs_undefined;                                       \
    /* 7. Add a [[TypedArrayName]] internal slot to obj and set its initial value to undefined .  */ \
    /* XXX element type? */                                             \
    /* 8. Add a [[ByteLength]] internal slot to obj and set its initial value to 0. */ \
    obj->byteLength = 0;                                                \
    /* 9. Add a [[ByteOffset]] internal slot to obj and set its initial value to 0. */ \
    obj->byteOffset = 0;                                                \
    /* 10. Add an [[ArrayLength]] internal slot to obj and set its initial value to 0. */ \
    obj->length = 0;                                                    \
    /* 11. Return obj. */                                               \
    return OBJECT_TO_EJSVAL((EJSObject*)obj);                           \
}                                                                       \
                                                                        \
static ejsval                                                           \
_ejs_##ArrayType##Array_of_impl (ejsval env, ejsval _this, uint32_t argc, ejsval *args) \
{                                                                       \
    ejsval newObj;                                                      \
                                                                        \
    /* 1. Let len be the actual number of arguments passed to this function. */ \
    uint32_t len = argc;                                                \
                                                                        \
    /* 2. Let items be the List of arguments passed to this function. */\
    ejsval *items = args;                                               \
                                                                        \
    /* 3. Let C be the this value. */                                   \
    ejsval C = _this;                                                   \
                                                                        \
    /* 4. If IsConstructor(C) is true, then */                          \
    if (EJSVAL_IS_CONSTRUCTOR(C))                                       \
        /* a. Let newObj be the result of calling the [[Construct]] internal method of C with argument «len». */ \
        newObj = _ejs_typedarray_new (EJS_TYPEDARRAY_##EnumType, len);  \
                                                                        \
    /* 5. Else, */                                                      \
    else                                                                \
        /* a. Throw a TypeError exception. */                           \
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Type is not a constructor"); \
                                                                        \
    /* 6. Let k be 0. */                                                \
    uint32_t k = 0;                                                     \
                                                                        \
    /* 7. Repeat, while k < len */                                      \
    while (k < len) {                                                   \
        /* a. Let kValue be element k of items. */                      \
        ejsval kValue = items[k];                                       \
                                                                        \
        /* b. Let Pk be ToString(k). */                                 \
        ejsval Pk = ToString(NUMBER_TO_EJSVAL(k));                      \
                                                                        \
        /* c. Let status be Put(newObj,Pk, kValue.[[value]], true). */  \
        Put (newObj, Pk, kValue, EJS_TRUE);                             \
                                                                        \
        /* d. Increase k by 1. */                                       \
        k++;                                                            \
    }                                                                   \
    /* . Return newObj. */                                              \
    return newObj;                                                      \
}


EJS_TYPED_ARRAY(INT8, Int8, int8, int8_t, 1);
EJS_TYPED_ARRAY(UINT8, Uint8, uint8, uint8_t, 1);
EJS_TYPED_ARRAY(UINT8CLAMPED, Uint8Clamped, uint8, uint8_t, 1);
EJS_TYPED_ARRAY(INT16, Int16, int16, int16_t, 2);
EJS_TYPED_ARRAY(UINT16, Uint16, uint16, uint16_t, 2);
EJS_TYPED_ARRAY(INT32, Int32, int32, int32_t, 4);
EJS_TYPED_ARRAY(UINT32, Uint32, uint32, uint32_t, 4);
EJS_TYPED_ARRAY(FLOAT32, Float32, float32, float, 4);
EJS_TYPED_ARRAY(FLOAT64, Float64, float64, double, 8);

int _ejs_typed_array_elsizes[EJS_TYPEDARRAY_TYPE_COUNT];
ejsval _ejs_typed_array_protos[EJS_TYPEDARRAY_TYPE_COUNT];
EJSSpecOps* _ejs_typed_array_specops[EJS_TYPEDARRAY_TYPE_COUNT];

static ejsval
_ejs_TypedArray_prototype_find (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval predicate = _ejs_undefined;
    ejsval thisArg = _ejs_undefined;

    if (argc >= 1)
        predicate = args[0];

    if (argc >= 2)
        thisArg = args[1];

    if (EJSVAL_IS_NULL_OR_UNDEFINED(_this))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "TypedArray.prototype.find called on null or undefined");

    /* This function is not generic. */
    if (!EJSVAL_IS_TYPEDARRAY(_this))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "TypedArray.prototype.find called on non typed-array object");

    /* 1. 1. Let O be the result of calling ToObject passing the this value as the argument. */
    ejsval O = ToObject(_this);
    EJSTypedArray *Oobj = (EJSTypedArray*)EJSVAL_TO_OBJECT(O);

    /* 3. Let lenValue be Get(O, "length"). */
    /* 4. Let len be ToLength(lenValue). */
    int32_t len = Oobj->length;

    /* 6. If IsCallable(callbackfn) is false, throw a TypeError exception. */
    if (!EJSVAL_IS_CALLABLE(predicate))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "argument is not a function");

    /* 7. If thisArg was supplied, let T be thisArg; else let T be undefined. */
    ejsval T = thisArg;

    /* 8. Let k be 0. */
    uint32_t k = 0;

    /* 9. Repeat, while k < len */
    while (k < len) {
        /* a. Let Pk be ToString(k). */
        ejsval Pk = ToString (NUMBER_TO_EJSVAL(k));

        /* b. Let kValue be Get(O, Pk). */
        ejsval kValue = Get(O, Pk);

        /* d. Let testResult be Call(predicate, T, «kValue, k, O»). */
        ejsval predicate_args[3] = { kValue, NUMBER_TO_EJSVAL(k), O };
        ejsval testResult = _ejs_invoke_closure (predicate, T, 3, predicate_args);

        /* f. If ToBoolean(testResult) is true, return k. */
        if (EJSVAL_TO_BOOLEAN(testResult))
            return NUMBER_TO_EJSVAL(k);

        /* g.  d. Increase k by 1.  */
        k++;
    }

    return NUMBER_TO_EJSVAL(-1);
}

static ejsval
_ejs_TypedArray_prototype_forEach (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval callbackfn = _ejs_undefined;
    ejsval thisArg = _ejs_undefined;

    if (argc >= 1)
        callbackfn = args[0];

    if (argc >= 2)
        thisArg = args[1];

    if (EJSVAL_IS_NULL_OR_UNDEFINED(_this))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "TypedArray.prototype.forEach called on null or undefined");

    /* This function is not generic. */
    if (!EJSVAL_IS_TYPEDARRAY(_this))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "TypedArray.prototype.forEach called on non typed-array object");

    /* 1. 1. Let O be the result of calling ToObject passing the this value as the argument. */
    ejsval O = ToObject(_this);
    EJSTypedArray *Oobj = (EJSTypedArray*)EJSVAL_TO_OBJECT(O);

    /* 3. Let lenValue be Get(O, "length"). */
    /* 4. Let len be ToLength(lenValue). */
    uint32_t len = Oobj->length;

    /* 6. If IsCallable(callbackfn) is false, throw a TypeError exception. */
    if (!EJSVAL_IS_CALLABLE(callbackfn))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "argument is not a function");

    /* 7. If thisArg was supplied, let T be thisArg; else let T be undefined. */
    ejsval T = thisArg;

    /* 8. Let k be 0. */
    uint32_t k = 0;

    /* 9. Repeat, while k < len */
    while (k < len) {
        /* a. Let Pk be ToString(k). */
        ejsval Pk = ToString (NUMBER_TO_EJSVAL(k));

        /* b. Let kPresent be HasProperty(O, Pk). */
        /*EJSBool kPresent = OP(EJSVAL_TO_OBJECT(O), HasProperty)(O, Pk);*/

        /* d. If kPresent is true, then */
        /*  i. Let kValue be the result of calling the [[Get]] internal method of O with argument Pk. */
        ejsval kValue = Get(O, Pk);

        /*  ii. Call the [[Call]] internal method of callbackfn with T as the this value and argument list containing kValue, k, and O.  */
        ejsval foreach_args[3] = { kValue, NUMBER_TO_EJSVAL(k), O };
        _ejs_invoke_closure (callbackfn, T, 3, foreach_args);

        /* d.  d. Increase k by 1.  */
        k++;
    }

    return _ejs_undefined;
}

static ejsval
_ejs_TypedArray_prototype_indexOf (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval searchElement = _ejs_undefined;
    ejsval fromIndex = _ejs_undefined;

    if (argc >= 1)
        searchElement = args[0];
    if (argc >= 2)
        fromIndex = args[1];

    /* 1. Let O be the result of calling ToObject passing the this value as the argument. */
    ejsval O = ToObject(_this);

    /* This function is not generic. */
    if (!EJSVAL_IS_TYPEDARRAY(_this))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "TypedArray.prototype.indexOf called on non typed-array object");

    EJSTypedArray *Oobj = (EJSTypedArray*)EJSVAL_TO_OBJECT(O);

    /* 3. Let lenValue be Get(O, "length") */
    /* 4. Let len be ToLength(lenValue). */
    /* (Make 'len' signed, so we can properly compare it to the unsigned 'n' */
    int32_t len = Oobj->length;

    /* 6. If len is 0, return −1. */
    if (len == 0)
        return NUMBER_TO_EJSVAL(-1);

    /* 7. If argument fromIndex was passed let n be ToInteger(fromIndex); else let n be 0. */
    int32_t n = EJSVAL_IS_UNDEFINED(fromIndex) ? 0 : ToInteger(fromIndex);

    /* 9. If n ≥ len, return −1. */
    if (n >= len)
        return NUMBER_TO_EJSVAL(-1);

    /* 10. If n ≥ 0, then */
    int32_t k;
    if (n >= 0)
        /* a. Let k be n. */
        k = n;
    /* 11. Else */
    else {
        /* a. Let k be len - abs(n). */
        k = len - abs (n);
        /* b. If k < 0, then let k be 0. */
        if (k < 0)
            k = 0;
    }

    /* 12. Repeat, while k<len */
    while (k < len) {
        /* (We optimize for non-null, non-sparse access, invariant of typed arrays */

        /* a. Let kPresent be HasProperty(O, ToString(k)). */
        /* c. If kPresent is true, then */
        /*  i. Let elementK be the result of Get(O, ToString(k)). */
        ejsval elementK = Get(O, ToString(NUMBER_TO_EJSVAL(k)));

        /*  iii. Let same be the result of performing Strict Equality Comparison searchElement === elementK. */
        ejsval same = _ejs_op_strict_eq (searchElement, elementK);

        /*  iv. If same is true, return k. */
        if (EJSVAL_TO_BOOLEAN(same))
            return NUMBER_TO_EJSVAL(k);

        /* d. Increase k by 1. */
        k++;
    }

    return NUMBER_TO_EJSVAL(-1);
}

static ejsval
_ejs_TypedArray_prototype_join (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval separator = _ejs_undefined;
    if (argc >= 1)
        separator = args[0];

    /* This function is not generic. */
    if (!EJSVAL_IS_TYPEDARRAY(_this))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "TypedArray.prototype.join called on non typed-array object");

    /* 1. Let O be the result of calling ToObject passing the this value as the argument. */
    ejsval O = ToObject(_this);
    EJSTypedArray *Oobj = (EJSTypedArray*)EJSVAL_TO_OBJECT(O);

    /* 3. Let lenVal be the result of Get(O, "length"). */
    /* 4. Let len be ToLength(lenVal). */
    uint32_t len = Oobj->length;

    ejsval sep;
    const jschar *sep_str;
    uint32_t sep_len;

    /* 6. If separator is undefined, let separator be the single-element String ",". */
    /* 7. Let sep be ToString(separator). */
    if (EJSVAL_IS_UNDEFINED(separator)) {
        static jschar comma [] = {  (jschar)',' };
        sep_str = comma;
        sep_len = 1;
    } else {
        sep = ToString(separator);
        sep_str = EJSVAL_TO_FLAT_STRING(sep);
        sep_len = EJSVAL_TO_STRLEN(sep);
    }

    /* 8. If len is zero, return the empty String. */
    if (len == 0)
        return _ejs_atom_empty;

    /* (Prepare the internal buffer and pre calculate the length */
    jschar *result;
    uint32_t result_len = 0;

    ejsval *strings = (ejsval*)malloc (sizeof (ejsval) * len);
    int i;

    for (i = 0; i < len; i++) {
        strings[i] = ToString(Get(O, NUMBER_TO_EJSVAL(i)));
        result_len += EJSVAL_TO_STRLEN(strings[i]);
    }

    result_len += sep_len * (len - 1) + 1; /* \0 terminator */
    result = (jschar*)malloc (sizeof(jschar) * result_len);
    jschar *p = result; /* current position */

    /* 9. Let element0 be the result of Get(O, "0"). */
    /* 10. If element0 is undefined or null, let R be the empty String; otherwise, let R be ToString(element0). */
    ejsval R = strings[0];

    /* (Since R will be used as starting point, copy it now to the internal buffer */
    jschar *R_str = EJSVAL_TO_FLAT_STRING(R);
    uint32_t R_len = EJSVAL_TO_STRLEN(R);
    memmove (p, R_str, R_len * sizeof(jschar));
    p += R_len;

    /* 12. Let k be 1. */
    uint32_t k = 1;

    /* 13. Repeat, while k < len */
    while (k < len) {
        /* a. Let S be the String value produced by concatenating R and sep. */
        memmove (p, sep_str, sep_len * sizeof(jschar));
        p += sep_len;

        /* b. Let element be Get(O, ToString(k)). */
        /* c. If element is undefined or null, then let next be the empty String; otherwise, let next be ToString(element). */
        ejsval next = strings[k];

        /* e. Let R be a String value produced by concatenating S and next. */
        jschar *next_str = EJSVAL_TO_FLAT_STRING(next);
        uint32_t next_len = EJSVAL_TO_STRLEN(next);
        memmove (p, next_str, next_len * sizeof(jschar));
        p += next_len;

        /* f. Increase k by 1. */
        k++;
    }
    *p = 0;

    ejsval rv = _ejs_string_new_ucs2(result);

    free (result);
    free (strings);

    /* 14. Return R. */
    return rv;
}

static ejsval
 _ejs_TypedArray_prototype_keys (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    /* 1. Let O be the this value. */
    ejsval O = _this;

    /* 2. If Type(O) is not Object, throw a TypeError exception. */
    if (!EJSVAL_IS_OBJECT(O))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "argument is not an Object");

    /* 3. If O does not have a [[ViewedArrayBuffer]] internal slot throw a TypeError exception. */
    if (!EJSVAL_IS_TYPEDARRAY(O))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "argument is not a typed array");

    EJSTypedArray *Oobj = (EJSTypedArray*)EJSVAL_TO_OBJECT(O);

    /* 4. Let buffer be the value of O’s [[ViewedArrayBuffer]] internal slot. */
    ejsval buffer = Oobj->buffer;

    /* 5. If buffer is undefined, then throw a TypeError exception. */
    if (EJSVAL_IS_UNDEFINED(buffer))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "typed arrray's buffer is undefined");

    /* 6. If IsDetachedBuffer(buffer) is true, throw a TypeError exception. */
    if (IsDetachedBuffer(buffer))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "typed array's buffer is detached");

    /* 7. Return CreateArrayIterator(O, "key"). */
    return _ejs_array_iterator_new (O, EJS_ARRAYITER_KIND_KEY);
}

static ejsval
_ejs_TypedArray_prototype_lastIndexOf (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval searchElement = _ejs_undefined;
    ejsval fromIndex = _ejs_undefined;

    if (argc >= 1)
        searchElement = args[0];
    if (argc >= 2)
        fromIndex = args[1];

    /* 1. Let O be the result of calling ToObject passing the this value as the argument. */
    ejsval O = ToObject(_this);

    /* This function is not generic. */
    if (!EJSVAL_IS_TYPEDARRAY(_this))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "TypedArray.prototype.lastIndexOf called on non typed-array object");

    EJSTypedArray *Oobj = (EJSTypedArray*)EJSVAL_TO_OBJECT(O);

    /* 3. Let lenValue be Get(O, "length") */
    /* 4. Let len be ToLength(lenValue). */
    /* (Make 'len' signed, so we can properly compare it to the unsigned 'n' */
    int32_t len = Oobj->length;

    /* 6. If len is 0, return -1. */
    if (len == 0)
        return NUMBER_TO_EJSVAL(-1);

    /* 7. If argument fromIndex was passed let n be ToInteger(fromIndex); else let n be len-1. */
    int32_t n = EJSVAL_IS_UNDEFINED(fromIndex) ? len - 1 : ToInteger(fromIndex);

    int32_t k;

    /* 9. If n ≥ 0, then let k be min(n, len – 1). */
    if (n >= 0)
        k = min (n, len - 1);
    /* 10. Else n < 0, */
    else
        /* a. Let k be len - abs(n). */
        k = len - abs (n);

    /* 11. Repeat, while k≥ 0 */
    while (k >= 0) {
        /* a. Let kPresent be HasProperty(O, ToString(k)). */
        /* b. ReturnIfAbrupt(kPresent). */
        /* c. If kPresent is true, then */

        /*  i. Let elementK be Get(O, ToString(k)). */
        ejsval elementK = Get(O, ToString(NUMBER_TO_EJSVAL(k)));

        /*  iii.  Let same be the result of performing Strict Equality Comparison searchElement === elementK. */
        ejsval same = _ejs_op_strict_eq (searchElement, elementK);

        /*  iv. If same is true, return k. */
        if (EJSVAL_TO_BOOLEAN(same))
            return NUMBER_TO_EJSVAL(k);

        /*  v. Decrease k by 1. */
        k--;
    }

    /* 12. Return -1. */
    return NUMBER_TO_EJSVAL(-1);
}

static ejsval
_ejs_TypedArray_prototype_values (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    /* 1. Let O be the this value. */
    ejsval O = _this;

    /* 2. If Type(O) is not Object, throw a TypeError exception. */
    if (!EJSVAL_IS_OBJECT(O))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "argument is not an Object");

    /* 3. If O does not have a [[ViewedArrayBuffer]] internal slot throw a TypeError exception. */
    if (!EJSVAL_IS_TYPEDARRAY(O))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "argument is not a typed array");

    EJSTypedArray *Oobj = (EJSTypedArray*)EJSVAL_TO_OBJECT(O);

    /* 4. Let buffer be the value of O’s [[ViewedArrayBuffer]] internal slot. */
    ejsval buffer = Oobj->buffer;

    /* 5. If buffer is undefined, then throw a TypeError exception. */
    if (EJSVAL_IS_UNDEFINED(buffer))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "typed arrray's buffer is undefined");

    /* 6. If IsDetachedBuffer(buffer) is true, throw a TypeError exception. */
    if (IsDetachedBuffer(buffer))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "typed array's buffer is detached");

    /* 7. Return CreateArrayIterator(O, "value"). */
    return _ejs_array_iterator_new (O, EJS_ARRAYITER_KIND_VALUE);
}

static ejsval
_ejs_TypedArray_prototype_toString (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    /* 1. Let array be the result of calling ToObject on the this value. */
    ejsval array = ToObject(_this);

    /* 3. Let func be Get(array, "join"). */
    ejsval func = Get(array, _ejs_atom_join);

    /* 6. Return Call(func, array). */
    return _ejs_invoke_closure (func, array, 0, NULL);
}

ejsval
_ejs_typedarray_new (EJSTypedArrayType element_type, uint32_t length)
{
    int size = length * _ejs_typed_array_elsizes[element_type];

    ejsval buffer = _ejs_arraybuffer_new (size);

    EJSTypedArray *rv = _ejs_gc_new(EJSTypedArray);

    _ejs_init_object ((EJSObject*)rv, _ejs_typed_array_protos[element_type], _ejs_typed_array_specops[element_type]);

    rv->buffer = buffer;
    rv->element_type = element_type;
    rv->length = length;
    rv->byteOffset = 0;
    rv->byteLength = size;

    return OBJECT_TO_EJSVAL(rv);
}

ejsval
_ejs_typedarray_new_from_array (EJSTypedArrayType element_type, ejsval arrayObj)
{
    EJSObject *arr = EJSVAL_TO_OBJECT(arrayObj);
    int arrlen = EJSARRAY_LEN(arr);
    ejsval typedarr = _ejs_typedarray_new (element_type, arrlen);
    int i;

    void* data = _ejs_typedarray_get_data (EJSVAL_TO_OBJECT(typedarr));

    // this is woefully underoptimized...

    for (i = 0; i < arrlen; i ++) {
        ejsval item = _ejs_object_getprop (arrayObj, NUMBER_TO_EJSVAL(i));
        switch (element_type) {
        case EJS_TYPEDARRAY_INT8: ((int8_t*)data)[i] = (int8_t)EJSVAL_TO_NUMBER(item); break;
        case EJS_TYPEDARRAY_UINT8: ((uint8_t*)data)[i] = (uint8_t)EJSVAL_TO_NUMBER(item); break;
        case EJS_TYPEDARRAY_UINT8CLAMPED: EJS_NOT_IMPLEMENTED();
        case EJS_TYPEDARRAY_INT16: ((int16_t*)data)[i] = (int16_t)EJSVAL_TO_NUMBER(item); break;
        case EJS_TYPEDARRAY_UINT16: ((uint16_t*)data)[i] = (uint16_t)EJSVAL_TO_NUMBER(item); break;
        case EJS_TYPEDARRAY_INT32: ((int32_t*)data)[i] = (int32_t)EJSVAL_TO_NUMBER(item); break;
        case EJS_TYPEDARRAY_UINT32: ((uint32_t*)data)[i] = (uint32_t)EJSVAL_TO_NUMBER(item); break;
        case EJS_TYPEDARRAY_FLOAT32: ((float*)data)[i] = (float)EJSVAL_TO_NUMBER(item); break;
        case EJS_TYPEDARRAY_FLOAT64: ((double*)data)[i] = (double)EJSVAL_TO_NUMBER(item); break;
        default: EJS_NOT_REACHED();
        }
    }

    return typedarr;
}

void*
_ejs_arraybuffer_get_data (EJSObject* buf)
{
    EJSArrayBuffer *array_buffer = (EJSArrayBuffer*)buf;

    if (array_buffer->dependent) {
        return _ejs_arraybuffer_get_data (EJSVAL_TO_OBJECT(array_buffer->data.dependent.buf)) + array_buffer->data.dependent.offset;
    }

    return array_buffer->data.alloced_buf;
}

void*
_ejs_typedarray_get_data(EJSObject* arr)
{
    EJSTypedArray* typed_array = (EJSTypedArray*)arr;
    void* buffer_data = _ejs_arraybuffer_get_data (EJSVAL_TO_OBJECT(typed_array->buffer));
        
    return buffer_data + typed_array->byteOffset;
}

void*
_ejs_dataview_get_data(EJSObject* view)
{
    EJSDataView* data_view = (EJSDataView*)view;
    void *buffer_data = _ejs_arraybuffer_get_data (EJSVAL_TO_OBJECT(data_view->buffer));

    return buffer_data + data_view->byteOffset;
}

void
_ejs_typedarrays_init(ejsval global)
{
#define OBJ_METHOD(t,x) EJS_INSTALL_ATOM_FUNCTION(_ejs_##t, x, _ejs_##t##_##x)
#define OBJ_METHOD_IMPL(t,x) EJS_INSTALL_ATOM_FUNCTION(_ejs_##t, x, _ejs_##t##_##x##_impl)
#define PROTO_METHOD(t,x) EJS_INSTALL_ATOM_FUNCTION(_ejs_##t##_prototype, x, _ejs_##t##_prototype_##x)
#define PROTO_METHOD_IMPL_GENERIC(t, x) EJS_INSTALL_ATOM_FUNCTION(_ejs_##t##_prototype, x, _ejs_TypedArray_prototype_##x)
#define PROTO_METHOD_IMPL(t,x) EJS_INSTALL_ATOM_FUNCTION(_ejs_##t##_prototype, x, _ejs_##t##_prototype_##x##_impl)
#define PROTO_GETTER(t,x) EJS_INSTALL_SYMBOL_GETTER(_ejs_##t##_prototype, x, _ejs_##t##_prototype_get_##x)

    // ArrayBuffer
    {
        _ejs_ArrayBuffer = _ejs_function_new_without_proto (_ejs_null, _ejs_atom_ArrayBuffer, (EJSClosureFunc)_ejs_ArrayBuffer_impl);
        _ejs_object_setprop (global,           _ejs_atom_ArrayBuffer, _ejs_ArrayBuffer);

        _ejs_gc_add_root (&_ejs_ArrayBuffer_prototype);
        _ejs_ArrayBuffer_prototype = _ejs_object_new(_ejs_null, &_ejs_Object_specops);
        _ejs_object_setprop (_ejs_ArrayBuffer, _ejs_atom_prototype,   _ejs_ArrayBuffer_prototype);

        PROTO_METHOD(ArrayBuffer, slice);

        OBJ_METHOD(ArrayBuffer, isView);

        _ejs_object_define_value_property (_ejs_ArrayBuffer_prototype, _ejs_Symbol_toStringTag, _ejs_atom_ArrayBuffer, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_WRITABLE | EJS_PROP_CONFIGURABLE);
        EJS_INSTALL_SYMBOL_FUNCTION_FLAGS (_ejs_ArrayBuffer, create, _ejs_ArrayBuffer_create, EJS_PROP_NOT_ENUMERABLE);
        EJS_INSTALL_SYMBOL_GETTER (_ejs_ArrayBuffer, species, _ejs_ArrayBuffer_get_species);
    }

    // DataView
    {
        _ejs_DataView = _ejs_function_new_without_proto (_ejs_null, _ejs_atom_DataView, (EJSClosureFunc)_ejs_DataView_impl);
        _ejs_object_setprop (global,            _ejs_atom_DataView, _ejs_DataView);

        _ejs_gc_add_root (&_ejs_DataView_prototype);
        _ejs_DataView_prototype = _ejs_object_new (_ejs_null, &_ejs_Object_specops);
        _ejs_object_setprop (_ejs_DataView, _ejs_atom_prototype, _ejs_DataView_prototype);

        PROTO_METHOD_IMPL(DataView, getInt8);
        PROTO_METHOD_IMPL(DataView, setInt8);
        PROTO_METHOD_IMPL(DataView, getInt16);
        PROTO_METHOD_IMPL(DataView, setInt16);
        PROTO_METHOD_IMPL(DataView, getInt32);
        PROTO_METHOD_IMPL(DataView, setInt32);
        PROTO_METHOD_IMPL(DataView, getFloat32);
        PROTO_METHOD_IMPL(DataView, setFloat32);
        PROTO_METHOD_IMPL(DataView, getFloat64);
        PROTO_METHOD_IMPL(DataView, setFloat64);

        _ejs_object_define_value_property (_ejs_DataView_prototype, _ejs_Symbol_toStringTag, _ejs_atom_DataView, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_WRITABLE | EJS_PROP_CONFIGURABLE);

        EJS_INSTALL_SYMBOL_FUNCTION_FLAGS (_ejs_DataView, create, _ejs_DataView_create, EJS_PROP_NOT_ENUMERABLE);
    }

#define ADD_TYPEDARRAY(EnumType, ArrayType, arraytype, elementSizeInBytes) EJS_MACRO_START \
    _ejs_##ArrayType##Array = _ejs_function_new_without_proto (_ejs_null, _ejs_atom_##ArrayType##Array, (EJSClosureFunc)_ejs_##ArrayType##Array_impl); \
    _ejs_object_setprop (global,         _ejs_atom_##ArrayType##Array,  _ejs_##ArrayType##Array); \
                                                                        \
    _ejs_gc_add_root (&_ejs_##ArrayType##Array_prototype);                  \
    _ejs_##ArrayType##Array_prototype = _ejs_object_new(_ejs_null, &_ejs_Object_specops); \
    _ejs_object_setprop (_ejs_##ArrayType##Array, _ejs_atom_prototype,  _ejs_##ArrayType##Array_prototype); \
                                                                        \
    /* make sure ctor.BYTES_PER_ELEMENT is defined */                   \
    _ejs_object_define_value_property (_ejs_##ArrayType##Array, _ejs_atom_BYTES_PER_ELEMENT, NUMBER_TO_EJSVAL(elementSizeInBytes), EJS_PROP_FLAGS_ENUMERABLE); \
                                                                        \
    _ejs_typed_array_elsizes[EJS_TYPEDARRAY_##EnumType] = elementSizeInBytes; \
    _ejs_typed_array_protos[EJS_TYPEDARRAY_##EnumType] = _ejs_##ArrayType##Array_prototype; \
    _ejs_typed_array_specops[EJS_TYPEDARRAY_##EnumType] = &_ejs_##ArrayType##Array_specops; \
                                                                        \
    PROTO_METHOD_IMPL(ArrayType##Array, get);                           \
    PROTO_METHOD_IMPL(ArrayType##Array, set);                           \
    PROTO_METHOD_IMPL(ArrayType##Array, subarray);                      \
                                                                        \
    PROTO_METHOD_IMPL_GENERIC(ArrayType##Array, find);                  \
    PROTO_METHOD_IMPL_GENERIC(ArrayType##Array, forEach);               \
    PROTO_METHOD_IMPL_GENERIC(ArrayType##Array, indexOf);               \
    PROTO_METHOD_IMPL_GENERIC(ArrayType##Array, join);                  \
    PROTO_METHOD_IMPL_GENERIC(ArrayType##Array, keys);                  \
    PROTO_METHOD_IMPL_GENERIC(ArrayType##Array, lastIndexOf);           \
    PROTO_METHOD_IMPL_GENERIC(ArrayType##Array, toString);              \
                                                                        \
    PROTO_GETTER(ArrayType##Array, toStringTag); /* XXX needs to be enumerable: false, configurable: true */ \
                                                                        \
    /* Manually expand values() so we can use it for the iterator */    \
    ejsval _values = _ejs_function_new_native(_ejs_null, _ejs_atom_values, (EJSClosureFunc) _ejs_TypedArray_prototype_values); \
    _ejs_object_define_value_property(_ejs_##ArrayType##Array_prototype, _ejs_atom_values, _values, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_WRITABLE | EJS_PROP_CONFIGURABLE); \
    _ejs_object_define_value_property(_ejs_##ArrayType##Array_prototype, _ejs_Symbol_iterator, _values, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_WRITABLE | EJS_PROP_CONFIGURABLE); \
                                                                        \
    OBJ_METHOD_IMPL(ArrayType##Array, of);                              \
                                                                        \
    EJS_INSTALL_SYMBOL_FUNCTION_FLAGS (_ejs_##ArrayType##Array, create, _ejs_##ArrayType##Array_create, EJS_PROP_NOT_ENUMERABLE); \
    EJS_INSTALL_SYMBOL_GETTER (_ejs_##ArrayType##Array, species, _ejs_##ArrayType##Array_get_species); \
                                                                        \
EJS_MACRO_END

    ADD_TYPEDARRAY(INT8, Int8, int8, 1);
    ADD_TYPEDARRAY(UINT8, Uint8, uint8, 1);
    ADD_TYPEDARRAY(UINT8CLAMPED, Uint8Clamped, uint8, 1); // XXX
    ADD_TYPEDARRAY(INT16, Int16, int16, 2);
    ADD_TYPEDARRAY(UINT16, Uint16, uint16, 2);
    ADD_TYPEDARRAY(INT32, Int32, int32, 4);
    ADD_TYPEDARRAY(UINT32, Uint32, uint32, 4);
    ADD_TYPEDARRAY(FLOAT32, Float32, float32, 4);
    ADD_TYPEDARRAY(FLOAT64, Float64, float64, 8);
}

static ejsval
_ejs_arraybuffer_specop_get (ejsval obj, ejsval propertyName, ejsval receiver)
{
    // check if propertyName is an integer, or a string that we can convert to an int
    EJSBool is_index = EJS_FALSE;
    int idx = 0;
    if (EJSVAL_IS_NUMBER(propertyName)) {
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
        return EJS_DENSE_ARRAY_ELEMENTS(obj)[idx];
    }

    // we also handle the length getter here
    if (EJSVAL_IS_STRING(propertyName) && !ucs2_strcmp (_ejs_ucs2_byteLength, EJSVAL_TO_FLAT_STRING(propertyName))) {
        return NUMBER_TO_EJSVAL (EJS_ARRAY_BUFFER_BYTE_LEN(obj));
    }

    // otherwise we fallback to the object implementation
    return _ejs_Object_specops.Get (obj, propertyName, receiver);
}

static EJSPropertyDesc*
_ejs_arraybuffer_specop_get_own_property (ejsval obj, ejsval propertyName, ejsval *exc)
{
    if (EJSVAL_IS_NUMBER(propertyName)) {
        double needle = EJSVAL_TO_NUMBER(propertyName);
        int needle_int;
        if (EJSDOUBLE_IS_INT32(needle, &needle_int)) {
            if (needle_int >= 0 && needle_int < EJS_ARRAY_LEN(obj))
                return NULL; // XXX
        }
    }

    // XXX we need to handle the length property here (see EJSArray's get_own_property)

    return _ejs_Object_specops.GetOwnProperty (obj, propertyName, exc);
}

static EJSBool
_ejs_arraybuffer_specop_set (ejsval obj, ejsval propertyName, ejsval val, ejsval receiver)
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
        if (idx >= EJS_DENSE_ARRAY_ALLOC(obj)) {
            int new_alloc = idx + 10;
            ejsval* new_elements = (ejsval*)malloc (sizeof(ejsval) * new_alloc);
            memmove (new_elements, EJS_DENSE_ARRAY_ELEMENTS(obj), EJS_DENSE_ARRAY_ALLOC(obj) * sizeof(ejsval));
            free (EJS_DENSE_ARRAY_ELEMENTS(obj));
            EJS_DENSE_ARRAY_ELEMENTS(obj) = new_elements;
            EJS_DENSE_ARRAY_ALLOC(obj) = new_alloc;
        }
        EJS_DENSE_ARRAY_ELEMENTS(obj)[idx] = val;
        EJS_ARRAY_LEN(obj) = idx + 1;
        if (EJS_ARRAY_LEN(obj) >= EJS_DENSE_ARRAY_ALLOC(obj))
            abort();
        return EJS_TRUE;
    }
    // if we fail there, we fall back to the object impl below

    return _ejs_Object_specops.Set (obj, propertyName, val, receiver);
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

    return _ejs_Object_specops.HasProperty (obj, propertyName);
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
        return _ejs_Object_specops.Delete (obj, propertyName, flag);

    // if it's outside the array bounds, do nothing
    if (idx < EJS_ARRAY_LEN(obj))
        EJS_DENSE_ARRAY_ELEMENTS(obj)[idx] = _ejs_undefined;
    return EJS_TRUE;
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
    _ejs_Object_specops.Finalize (obj);
}

static void
_ejs_arraybuffer_specop_scan (EJSObject* obj, EJSValueFunc scan_func)
{
    EJSArrayBuffer *arraybuf = (EJSArrayBuffer*)obj;
    if (arraybuf->dependent) {
        scan_func (arraybuf->data.dependent.buf);
    }
    _ejs_Object_specops.Scan (obj, scan_func);
}

EJS_DEFINE_CLASS(ArrayBuffer,
                 OP_INHERIT, // [[GetPrototypeOf]]
                 OP_INHERIT, // [[SetPrototypeOf]]
                 OP_INHERIT, // [[IsExtensible]]
                 OP_INHERIT, // [[PreventExtensions]]
                 _ejs_arraybuffer_specop_get_own_property,
                 OP_INHERIT, // [[DefineOwnProperty]]
                 _ejs_arraybuffer_specop_has_property,
                 _ejs_arraybuffer_specop_get,
                 _ejs_arraybuffer_specop_set,
                 _ejs_arraybuffer_specop_delete,
                 OP_INHERIT, // [[Enumerate]]
                 OP_INHERIT, // [[OwnPropertyKeys]]
                 _ejs_arraybuffer_specop_allocate,
                 _ejs_arraybuffer_specop_finalize,
                 _ejs_arraybuffer_specop_scan
                 )

static EJSObject*
_ejs_typedarray_specop_allocate()
{
    return (EJSObject*)_ejs_gc_new (EJSTypedArray);
}

static void
_ejs_typedarray_specop_finalize (EJSObject* obj)
{
    //EJSTypedArray *arr = (EJSTypedArray*)obj;
    _ejs_Object_specops.Finalize (obj);
}

static void
_ejs_typedarray_specop_scan (EJSObject* obj, EJSValueFunc scan_func)
{
    EJSTypedArray *arr = (EJSTypedArray*)obj;
    scan_func(arr->buffer);
    _ejs_Object_specops.Scan (obj, scan_func);
}

static ejsval
_ejs_dataview_specop_get (ejsval obj, ejsval propertyName, ejsval receiver)
{
    // check if propertyName is an integer, or a string that we can convert to an int
    EJSBool is_index = EJS_FALSE;
    int idx = 0;
    if (EJSVAL_IS_NUMBER(propertyName)) {
        double n = EJSVAL_TO_NUMBER(propertyName);
        if (floor(n) == n) {
            idx = (int)n;
            is_index = EJS_TRUE;
        }
    }

    // Index for DataView is byte-based.
    if (is_index) {
        if (idx < 0 || idx > EJS_DATA_VIEW_BYTE_LEN(obj))
            return _ejs_undefined;

         void *data = _ejs_dataview_get_data (EJSVAL_TO_OBJECT(obj));
         return NUMBER_TO_EJSVAL ((double)((unsigned char*)data)[idx]);
    }

    // otherwise we fallback to the object implementation
    return _ejs_Object_specops.Get (obj, propertyName, receiver);
}

static EJSPropertyDesc*
_ejs_dataview_specop_get_own_property (ejsval obj, ejsval propertyName, ejsval* exc)
{
    if (EJSVAL_IS_NUMBER(propertyName)) {
        double needle = EJSVAL_TO_NUMBER(propertyName);
        int needle_int;
        if (EJSDOUBLE_IS_INT32(needle, &needle_int)) {
            if (needle_int >= 0 && needle_int < EJS_DATA_VIEW_BYTE_LEN(obj))
                return NULL; // XXX
        }
    }

    return _ejs_Object_specops.GetOwnProperty (obj, propertyName, exc);
}

static EJSBool
_ejs_dataview_specop_set (ejsval obj, ejsval propertyName, ejsval val, ejsval receiver)
{
     EJSBool is_index = EJS_FALSE;
     ejsval idx_val = ToNumber(propertyName);
     int idx;
     if (EJSVAL_IS_NUMBER(idx_val)) {
         double n = EJSVAL_TO_NUMBER(idx_val);
         if (floor(n) == n) {
             idx = (int)n;
             is_index = EJS_TRUE;
         }
     }

     if (is_index) {
         if (idx < 0 || idx >= EJS_DATA_VIEW_BYTE_LEN(obj))
             return EJS_FALSE;

         void* data = _ejs_dataview_get_data (EJSVAL_TO_OBJECT(obj));
         ((unsigned char*)data)[idx] = (unsigned char)EJSVAL_TO_NUMBER(val);

         return EJS_TRUE;
     }

     return _ejs_Object_specops.Set (obj, propertyName, val, receiver);
}

static EJSBool
_ejs_dataview_specop_has_property (ejsval obj, ejsval propertyName)
{
    // check if propertyName is a uint32, or a string that we can convert to an uint32
    int idx = -1;
    if (EJSVAL_IS_NUMBER(propertyName)) {
        double n = EJSVAL_TO_NUMBER(propertyName);
        if (floor(n) == n) {
            idx = (int)n;

            return idx > 0 && idx < EJS_DATA_VIEW_BYTE_LEN(obj);
        }
    }

    return _ejs_Object_specops.HasProperty (obj, propertyName);
}

static EJSBool
_ejs_dataview_specop_delete (ejsval obj, ejsval propertyName, EJSBool flag)
{
    int idx = -1;
    if (EJSVAL_IS_NUMBER(propertyName)) {
        double n = EJSVAL_TO_NUMBER(propertyName);
        if (floor(n) == n) {
            idx = (int)n;
        }
    }

    if (idx == -1)
        return _ejs_Object_specops.Delete (obj, propertyName, flag);

    if (idx < EJS_DATA_VIEW_BYTE_LEN(obj)) {
         //void* data = _ejs_dataview_get_data (EJSVAL_TO_OBJECT(obj));
         //((unsigned char*)data)[idx] = _ejs_undefined;
    }

    return EJS_FALSE;
}

static EJSBool
_ejs_dataview_specop_define_own_property (ejsval obj, ejsval propertyName, EJSPropertyDesc* propertyDescriptor, EJSBool flag)
{
    return _ejs_Object_specops.DefineOwnProperty (obj, propertyName, propertyDescriptor, flag);
}

static EJSObject*
_ejs_dataview_specop_allocate ()
{
    return (EJSObject*)_ejs_gc_new (EJSDataView);
}

static void
_ejs_dataview_specop_scan (EJSObject* obj, EJSValueFunc scan_func)
{
    EJSDataView *view = (EJSDataView*)obj;
    scan_func (view->buffer);
    _ejs_Object_specops.Scan (obj, scan_func);
}

EJS_DEFINE_CLASS(DataView,
                 OP_INHERIT, // [[GetPrototypeOf]]
                 OP_INHERIT, // [[SetPrototypeOf]]
                 OP_INHERIT, // [[IsExtensible]]
                 OP_INHERIT, // [[PreventExtensions]]
                 _ejs_dataview_specop_get_own_property,
                 _ejs_dataview_specop_define_own_property,
                 _ejs_dataview_specop_has_property,
                 _ejs_dataview_specop_get,
                 _ejs_dataview_specop_set,
                 _ejs_dataview_specop_delete,
                 OP_INHERIT, // [[Enumerate]]
                 OP_INHERIT, // [[OwnPropertyKeys]]
                 _ejs_dataview_specop_allocate,
                 OP_INHERIT, // [[Finalize]]
                 _ejs_dataview_specop_scan
                 )

#define ADD_TYPEDARRAY_SPECOPS(ArrayType, arraytype)                   \
    EJSSpecOps _ejs_##ArrayType##Array_specops = {                     \
        #ArrayType "Array",                                            \
        OP_INHERIT, /* [[GetPrototypeOf]] */                           \
        OP_INHERIT, /* [[SetPrototypeOf]] */                           \
        OP_INHERIT, /* [[IsExtensible]] */                             \
        OP_INHERIT, /* [[PreventExtensions]] */                        \
        _ejs_##ArrayType##array_specop_get_own_property,               \
        _ejs_##ArrayType##array_specop_define_own_property,            \
        _ejs_##ArrayType##array_specop_has_property,                   \
        _ejs_##ArrayType##array_specop_get,                            \
        _ejs_##ArrayType##array_specop_set,                            \
        OP_INHERIT, /* [[Delete]] */                                   \
        OP_INHERIT, /* [[Enumerate]] */                                \
        OP_INHERIT, /* [[OwnPropertyKeys]] */                          \
        _ejs_typedarray_specop_allocate,                               \
        _ejs_typedarray_specop_finalize,                               \
        _ejs_typedarray_specop_scan                                    \
    };


ADD_TYPEDARRAY_SPECOPS(Int8, int8);
ADD_TYPEDARRAY_SPECOPS(Uint8, uint8);
ADD_TYPEDARRAY_SPECOPS(Uint8Clamped, uint8); // XXX
ADD_TYPEDARRAY_SPECOPS(Int16, int16);
ADD_TYPEDARRAY_SPECOPS(Uint16, uint16);
ADD_TYPEDARRAY_SPECOPS(Int32, int32);
ADD_TYPEDARRAY_SPECOPS(Uint32, uint32);
ADD_TYPEDARRAY_SPECOPS(Float32, float32);
ADD_TYPEDARRAY_SPECOPS(Float64, float64);
