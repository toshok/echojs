/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#ifndef _ejs_typedarrays_h_
#define _ejs_typedarrays_h_

#include "ejs-object.h"

typedef struct _EJSArrayBuffer {
    /* object header */
    EJSObject obj;

    /* buffer data */
    EJSBool dependent;
    int size;

    union {
        struct {
            ejsval buf;
            int offset;
        } dependent;

        void *alloced_buf;
    } data;
} EJSArrayBuffer;

typedef enum {
    EJS_TYPEDARRAY_INT8,
    EJS_TYPEDARRAY_UINT8,
    EJS_TYPEDARRAY_UINT8CLAMPED,
    EJS_TYPEDARRAY_INT16,
    EJS_TYPEDARRAY_UINT16,
    EJS_TYPEDARRAY_INT32,
    EJS_TYPEDARRAY_UINT32,
    EJS_TYPEDARRAY_FLOAT32,
    EJS_TYPEDARRAY_FLOAT64,

    EJS_TYPEDARRAY_TYPE_COUNT
} EJSTypedArrayType;

extern int _ejs_typed_array_elsizes[EJS_TYPEDARRAY_TYPE_COUNT];
extern ejsval _ejs_typed_array_protos[EJS_TYPEDARRAY_TYPE_COUNT];
extern EJSSpecOps* _ejs_typed_array_specops[EJS_TYPEDARRAY_TYPE_COUNT];

typedef struct _EJSTypedArray {
    /* object header */
    EJSObject obj;

    /* buffer view data */
    ejsval buffer;
    uint32_t byteOffset;
    uint32_t byteLength;

    /* typed array data */
    uint32_t length;

    EJSTypedArrayType element_type;
} EJSTypedArray;

typedef struct _EJSDataView {
    /* object header */
    EJSObject obj;

    /* buffer data view */
    ejsval buffer;
    uint32_t byteOffset;
    uint32_t byteLength;
} EJSDataView;

#define EJSVAL_IS_TYPEDARRAY(v) (EJSVAL_IS_OBJECT(v) &&         \
                                (EJSVAL_TO_OBJECT(v)->ops == &_ejs_int8array_specops || \
                                 EJSVAL_TO_OBJECT(v)->ops == &_ejs_int32array_specops || \
                                 EJSVAL_TO_OBJECT(v)->ops == &_ejs_float32array_specops))

#define EJSVAL_IS_ARRAYBUFFER(v) (EJSVAL_IS_OBJECT(v) && (EJSVAL_TO_OBJECT(v)->ops == &_ejs_ArrayBuffer_specops))

#define EJSOBJECT_IS_TYPEDARRAY(v) (v->ops == &_ejs_int8array_specops || v->ops == &_ejs_uint16array_specops || v->ops == &_ejs_int32array_specops || v->ops == &_ejs_float32array_specops)
#define EJSOBJECT_IS_ARRAYBUFFER(v) (v->ops == &_ejs_ArrayBuffer_specops)
#define EJSOBJECT_IS_DATAVIEW(v) (v->ops == &_ejs_DataView_specops)


#define EJS_TYPED_ARRAY_LEN(obj) (((EJSTypedArray*)EJSVAL_TO_OBJECT(obj))->length)
#define EJS_TYPED_ARRAY_BYTE_LEN(obj) (((EJSTypedArray*)EJSVAL_TO_OBJECT(obj))->byteLength)
#define EJS_ARRAY_BUFFER_BYTE_LEN(obj) (((EJSArrayBuffer*)EJSVAL_TO_OBJECT(obj))->size)
#define EJS_DATA_VIEW_BYTE_LEN(obj) (((EJSDataView*)EJSVAL_TO_OBJECT(obj))->byteLength)

#define EJSTYPEDARRAY_LEN(obj) (((EJSTypedArray*)(obj))->length)
#define EJSTYPEDARRAY_BYTE_LEN(obj) (((EJSTypedArray*)(obj))->byteLength)
#define EJSTYPEDARRAY_ELEMENT_TYPE(obj) (((EJSTypedArray*)(obj))->element_type)
#define EJSARRAYBUFFER_BYTE_LEN(obj) (((EJSArrayBuffer*)obj)->size)
#define EJSDATAVIEW_BYTE_LEN(obj) (((EJSDataView*)obj)->bytLength)

EJS_BEGIN_DECLS

extern ejsval _ejs_ArrayBuffer;
extern ejsval _ejs_ArrayBuffer_proto;
extern EJSSpecOps _ejs_ArrayBuffer_specops;

extern ejsval _ejs_Int8Array;
extern ejsval _ejs_Int8Array_proto;
extern EJSSpecOps _ejs_int8array_specops;

extern ejsval _ejs_Uint16Array;
extern ejsval _ejs_Uint16Array_proto;
extern EJSSpecOps _ejs_uint16array_specops;

extern ejsval _ejs_Int32Array;
extern ejsval _ejs_Int32Array_proto;
extern EJSSpecOps _ejs_int32array_specops;

extern ejsval _ejs_Float32Array;
extern ejsval _ejs_Float32Array_proto;
extern EJSSpecOps _ejs_float32array_specops;

extern ejsval _ejs_DataView;
extern ejsval _ejs_DataView_proto;
extern EJSSpecOps _ejs_DataView_specops;

void _ejs_typedarrays_init(ejsval global);

void* _ejs_arraybuffer_get_data(EJSObject* arr);
void* _ejs_typedarray_get_data(EJSObject* arr);
void* _ejs_dataview_get_data(EJSObject* view);

ejsval _ejs_typedarray_new (EJSTypedArrayType element_type, uint32_t length);
ejsval _ejs_typedarray_new_from_array (EJSTypedArrayType element_type, ejsval arrayObj);

EJS_END_DECLS

#endif /* _ejs_array_h */
