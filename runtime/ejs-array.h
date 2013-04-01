/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#ifndef _ejs_array_h_
#define _ejs_array_h_

#include "ejs-object.h"

typedef struct {
    /* object header */
    EJSObject obj;

    /* array data */
    EJSPropertyDesc array_length_desc;
    int array_length;
    int array_alloc;
    ejsval *elements;
} EJSArray;

#define EJS_ARRAY_ALLOC(obj) (((EJSArray*)EJSVAL_TO_OBJECT(obj))->array_alloc)
#define EJS_ARRAY_LEN(obj) (((EJSArray*)EJSVAL_TO_OBJECT(obj))->array_length)
#define EJS_ARRAY_ELEMENTS(obj) (((EJSArray*)EJSVAL_TO_OBJECT(obj))->elements)

#define EJSARRAY_ALLOC(obj) (((EJSArray*)(obj))->array_alloc)
#define EJSARRAY_LEN(obj) (((EJSArray*)(obj))->array_length)
#define EJSARRAY_ELEMENTS(obj) (((EJSArray*)(obj))->elements)

#define EJSVAL_IS_DENSE_ARRAY(v) (EJSVAL_IS_OBJECT(v) && (EJSVAL_TO_OBJECT(v)->ops == &_ejs_array_specops))
#define EJSVAL_IS_SPARSE_ARRAY(v) (EJSVAL_IS_OBJECT(v) && (EJSVAL_TO_OBJECT(v)->ops == &_ejs_sparsearray_specops))
#define EJS_ARRAY_IS_SPARSE(arrobj) ((arrobj)->ops == &_ejs_sparsearray_specops))

EJS_BEGIN_DECLS

extern ejsval _ejs_Array;
extern ejsval _ejs_Array_proto;
extern EJSSpecOps _ejs_array_specops;
extern EJSSpecOps _ejs_sparsearray_specops;

ejsval _ejs_array_new (int numElements);

void _ejs_array_foreach_element (EJSArray* arr, EJSValueFunc foreach_func);

void _ejs_array_init(ejsval global);

uint32_t _ejs_array_push_dense (ejsval array, int argc, ejsval* args);
ejsval _ejs_array_pop_dense (ejsval array);

ejsval _ejs_array_join (ejsval array, ejsval sep);

EJS_END_DECLS

#endif /* _ejs_array_h */
