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

typedef struct _EJSTypedArray {
    /* object header */
    EJSObject obj;

    /* buffer view data */
    ejsval buffer;
    uint32_t byteOffset;
    uint32_t byteLength;

    /* typed array data */
    uint32_t length;
} EJSTypedArray;

#define EJS_ARRAY_ALLOC(obj) (((EJSArray*)EJSVAL_TO_OBJECT(obj))->array_alloc)
#define EJS_ARRAY_LEN(obj) (((EJSArray*)EJSVAL_TO_OBJECT(obj))->array_length)
#define EJS_ARRAY_ELEMENTS(obj) (((EJSArray*)EJSVAL_TO_OBJECT(obj))->elements)

#define EJSARRAY_ALLOC(obj) (((EJSArray*)(obj))->array_alloc)
#define EJSARRAY_LEN(obj) (((EJSArray*)(obj))->array_length)
#define EJSARRAY_ELEMENTS(obj) (((EJSArray*)(obj))->elements)

EJS_BEGIN_DECLS

extern ejsval _ejs_ArrayBuffer;
extern ejsval _ejs_ArrayBuffer_proto;
extern EJSSpecOps _ejs_arraybuffer_specops;

extern ejsval _ejs_Int8Array;
extern ejsval _ejs_Int8Array_proto;
extern EJSSpecOps _ejs_int8array_specops;

void _ejs_typedarrays_init(ejsval global);

EJS_END_DECLS

#endif /* _ejs_array_h */
