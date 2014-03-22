/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#ifndef _ejs_array_h_
#define _ejs_array_h_

#include "ejs-object.h"

typedef struct {
    int     start_idx;
    int     length;
    int     alloc;
    ejsval* elements;
} Arraylet;

typedef struct {
    /* dense array data */
    int              array_alloc;
    EJSPropertyDesc* element_descs;
    ejsval*          elements;
} EJSDenseArrayData;

typedef struct {
    /* sparse array data */
    int       arraylet_alloc;
    int       arraylet_num;
    Arraylet *arraylets;
} EJSSparseArrayData;

typedef struct {
    /* object header */
    EJSObject obj;

    EJSPropertyDesc array_length_desc;
    int             array_length;
    union {
        EJSDenseArrayData dense;
        EJSSparseArrayData sparse;
    };
} EJSArray;

#define EJSARRAY_LEN(obj)  (((EJSArray*)(obj))->array_length)
#define EJS_ARRAY_LEN(obj) EJSARRAY_LEN(EJSVAL_TO_OBJECT(obj))

#define EJSDENSEARRAY_ALLOC(obj) (((EJSArray*)(obj))->dense.array_alloc)
#define EJSDENSEARRAY_ELEMENTS(obj) (((EJSArray*)(obj))->dense.elements)

#define EJS_DENSE_ARRAY_ALLOC(obj)    EJSDENSEARRAY_ALLOC(EJSVAL_TO_OBJECT(obj))
#define EJS_DENSE_ARRAY_ELEMENTS(obj) EJSDENSEARRAY_ELEMENTS(EJSVAL_TO_OBJECT(obj))

#define EJSVAL_IS_DENSE_ARRAY(v) (EJSVAL_IS_OBJECT(v) && (EJSVAL_TO_OBJECT(v)->ops == &_ejs_Array_specops))
#define EJSVAL_IS_SPARSE_ARRAY(v) (EJSVAL_IS_OBJECT(v) && (EJSVAL_TO_OBJECT(v)->ops == &_ejs_sparsearray_specops))
#define EJSARRAY_IS_SPARSE(arrobj) (((EJSObject*)(arrobj))->ops == &_ejs_sparsearray_specops)

EJS_BEGIN_DECLS

extern ejsval _ejs_Array;
extern ejsval _ejs_Array_proto;
extern EJSSpecOps _ejs_Array_specops;
extern EJSSpecOps _ejs_sparsearray_specops;

ejsval _ejs_array_new (int numElements, EJSBool fill);

// creates a new array and populates it by pushing numElements from
// the vector elements
ejsval _ejs_array_new_copy (int numElements, ejsval *elements);

void _ejs_array_foreach_element (EJSArray* arr, EJSValueFunc foreach_func);
int _ejs_array_indexof (EJSArray* haystack, ejsval needle);

void _ejs_array_init(ejsval global);

uint32_t _ejs_array_push_dense (ejsval array, int argc, ejsval* args);
ejsval   _ejs_array_pop_dense (ejsval array);

ejsval _ejs_array_join (ejsval array, ejsval sep);

EJS_END_DECLS

#endif /* _ejs_array_h */
