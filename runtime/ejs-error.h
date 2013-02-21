/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#ifndef _ejs_error_h_
#define _ejs_error_h_

#include "ejs.h"
#include "ejs-object.h"

EJS_BEGIN_DECLS

typedef enum {
    EJS_EVAL_ERROR,
    EJS_RANGE_ERROR,
    EJS_REFERENCE_ERROR,
    EJS_SYNTAX_ERROR,
    EJS_TYPE_ERROR,
    EJS_URI_ERROR
} EJSNativeErrorType;

extern ejsval _ejs_Error;
extern ejsval _ejs_error_get_prototype();

extern ejsval _ejs_nativeerror_new_utf8 (EJSNativeErrorType error_type, const char* message);

extern void _ejs_throw_nativeerror(EJSNativeErrorType error_type, const char *message);

extern void _ejs_error_init(ejsval global);

EJS_END_DECLS

#endif
