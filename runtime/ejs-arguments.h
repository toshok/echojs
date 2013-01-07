/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#ifndef _ejs_arguments_h_
#define _ejs_arguments_h_

#include "ejs-object.h"

typedef struct {
    /* object header */
    EJSObject obj;

    /* arguments specific data */
    int argc;
    ejsval* args;
} EJSArguments;

EJS_BEGIN_DECLS

extern ejsval _ejs_Arguments_proto;
extern EJSSpecOps _ejs_arguments_specops;

void _ejs_arguments_init(ejsval global);

EJS_END_DECLS

#endif /* _ejs_arguments_h */
