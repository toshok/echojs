/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#ifndef _ejs_set_h_
#define _ejs_set_h_

#include "ejs.h"
#include "ejs-value.h"
#include "ejs-object.h"

typedef struct _EJSSetValueEntry {
    // the next entry in insertion order
    struct _EJSSetValueEntry *next_insert;
    ejsval value;
} EJSSetValueEntry;

typedef struct {
    /* object header */
    EJSObject obj;

    EJSSetValueEntry* head_insert;
    EJSSetValueEntry* tail_insert;
} EJSSet;

EJS_BEGIN_DECLS

extern ejsval _ejs_Set;
extern ejsval _ejs_Set_prototype;
extern EJSSpecOps _ejs_Set_specops;

void _ejs_set_init(ejsval global);

void _ejs_set_new ();

EJS_END_DECLS

#endif
