/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#ifndef _ejs_map_h_
#define _ejs_map_h_

#include "ejs.h"
#include "ejs-value.h"
#include "ejs-object.h"

#define EJSVAL_IS_MAP(v)                                                       \
    (EJSVAL_IS_OBJECT(v) && (EJSVAL_TO_OBJECT(v)->ops == &_ejs_Map_specops))
#define EJSVAL_TO_MAP(v) ((EJSMap *)EJSVAL_TO_OBJECT(v))

typedef struct _EJSKeyValueEntry {
    // the next entry in insertion order
    struct _EJSKeyValueEntry *next_insert;
    ejsval key;
    ejsval value;
} EJSKeyValueEntry;

typedef struct {
    /* object header */
    EJSObject obj;

    ejsval comparator;

    EJSKeyValueEntry *head_insert;
    EJSKeyValueEntry *tail_insert;
} EJSMap;

EJS_BEGIN_DECLS

extern ejsval _ejs_Map;
extern ejsval _ejs_Map_prototype;
extern EJSSpecOps _ejs_Map_specops;

void _ejs_map_init(ejsval global);

ejsval _ejs_map_new();

ejsval _ejs_map_delete(ejsval map, ejsval key);
ejsval _ejs_map_has(ejsval map, ejsval key);
ejsval _ejs_map_get(ejsval map, ejsval key);
ejsval _ejs_map_set(ejsval map, ejsval key, ejsval value);

#define EJSVAL_IS_MAPITERATOR(v)                                               \
    (EJSVAL_IS_OBJECT(v) &&                                                    \
     (EJSVAL_TO_OBJECT(v)->ops == &_ejs_MapIterator_specops))

typedef enum {
    EJS_MAP_ITER_KIND_KEY,
    EJS_MAP_ITER_KIND_VALUE,
    EJS_MAP_ITER_KIND_KEYVALUE,
} EJSMapIteratorKind;

typedef struct {
    /* object header */
    EJSObject obj;

    ejsval iterated;
    EJSMapIteratorKind kind;
    int next_index;
} EJSMapIterator;

extern ejsval _ejs_MapIterator;
extern ejsval _ejs_MapIterator_prototype;
extern EJSSpecOps _ejs_MapIterator_specops;

ejsval _ejs_map_iterator_new(ejsval map, EJSMapIteratorKind kind);

EJS_END_DECLS

#endif
