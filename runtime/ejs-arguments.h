/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#ifndef _ejs_arguments_h_
#define _ejs_arguments_h_

#include "ejs-object.h"

#define EJSVAL_IS_ARGUMENTS(v)     (EJSVAL_IS_OBJECT(v) && (EJSVAL_TO_OBJECT(v)->ops == &_ejs_Arguments_specops))
#define EJSVAL_TO_ARGUMENTS(v)     ((EJSArguments*)EJSVAL_TO_OBJECT(v))


typedef struct {
    /* object header */
    EJSObject obj;

    /* arguments specific data */
    uint32_t argc;
    ejsval*  args;
} EJSArguments;

EJS_BEGIN_DECLS

extern ejsval _ejs_Arguments__proto__;
extern EJSSpecOps _ejs_Arguments_specops;

void   _ejs_arguments_init(ejsval global);
ejsval _ejs_arguments_new (int numElements, ejsval* args);

EJS_END_DECLS

#endif /* _ejs_arguments_h */
