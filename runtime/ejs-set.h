/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#ifndef _ejs_set_h_
#define _ejs_set_h_

#include "ejs.h"
#include "ejs-value.h"
#include "ejs-object.h"

#define EJSVAL_IS_SET(v)                                                       \
    (EJSVAL_IS_OBJECT(v) && (EJSVAL_TO_OBJECT(v)->ops == &_ejs_Set_specops))
#define EJSVAL_TO_SET(v) ((EJSSet *)EJSVAL_TO_OBJECT(v))

typedef struct _EJSSetValueEntry {
    // the next entry in insertion order
    struct _EJSSetValueEntry *next_insert;
    ejsval value;
} EJSSetValueEntry;

typedef struct {
    /* object header */
    EJSObject obj;

    EJSSetValueEntry *head_insert;
    EJSSetValueEntry *tail_insert;
} EJSSet;

EJS_BEGIN_DECLS

extern ejsval _ejs_Set;
extern ejsval _ejs_Set_prototype;
extern EJSSpecOps _ejs_Set_specops;

void _ejs_set_init(ejsval global);

ejsval _ejs_set_new();

ejsval _ejs_set_add(ejsval set, ejsval value);
ejsval _ejs_set_delete(ejsval set, ejsval value);
ejsval _ejs_set_has(ejsval set, ejsval value);

#define EJSVAL_IS_SETITERATOR(v)                                               \
    (EJSVAL_IS_OBJECT(v) &&                                                    \
     (EJSVAL_TO_OBJECT(v)->ops == &_ejs_SetIterator_specops))

typedef enum {
    EJS_SET_ITER_KIND_KEY,
    EJS_SET_ITER_KIND_VALUE,
    EJS_SET_ITER_KIND_KEYVALUE
} EJSSetIteratorKind;

typedef struct {
    /* object header */
    EJSObject obj;

    ejsval iterated;
    EJSSetIteratorKind kind;
    int next_index;
} EJSSetIterator;

extern ejsval _ejs_SetIterator;
extern ejsval _ejs_SetIterator_prototype;
extern EJSSpecOps _ejs_SetIterator_specops;

ejsval _ejs_set_iterator_new(ejsval set, EJSSetIteratorKind kind);

EJS_END_DECLS

#endif
