/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */
#ifndef _ejs_string_h_
#define _ejs_string_h_

#include "ejs-object.h"

typedef struct {
    /* object header */
    EJSObject obj;

    /* string specific data */
    ejsval primStr;
} EJSString;

EJS_BEGIN_DECLS

extern ejsval _ejs_String;
extern ejsval _ejs_String_proto;
extern EJSSpecOps _ejs_string_specops;

EJSObject* _ejs_string_alloc_instance();

void _ejs_string_init(ejsval global);

EJS_END_DECLS

#endif /* _ejs_string_h */
