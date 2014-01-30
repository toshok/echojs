/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#ifndef _ejs_error_h_
#define _ejs_error_h_

#include "ejs.h"
#include "ejs-object.h"

EJS_BEGIN_DECLS

typedef enum {
    EJS_ERROR,
    EJS_EVAL_ERROR,
    EJS_RANGE_ERROR,
    EJS_REFERENCE_ERROR,
    EJS_SYNTAX_ERROR,
    EJS_TYPE_ERROR,
    EJS_URI_ERROR
} EJSNativeErrorType;

#define EJSVAL_IS_ERROR(v)     (EJSVAL_IS_OBJECT(v) && (EJSVAL_TO_OBJECT(v)->ops == &_ejs_Error_specops))

extern EJSSpecOps _ejs_Error_specops;

extern ejsval _ejs_Error_proto;
extern ejsval _ejs_EvalError_proto;
extern ejsval _ejs_RangeError_proto;
extern ejsval _ejs_ReferenceError_proto;
extern ejsval _ejs_SyntaxError_proto;
extern ejsval _ejs_TypeError_proto;
extern ejsval _ejs_URIError_proto;

extern ejsval _ejs_nativeerror_new (EJSNativeErrorType error_type, ejsval message);
extern ejsval _ejs_nativeerror_new_utf8 (EJSNativeErrorType error_type, const char* message);

extern void _ejs_throw_nativeerror (EJSNativeErrorType error_type, ejsval message) __attribute__ ((noreturn));
extern void _ejs_throw_nativeerror_utf8 (EJSNativeErrorType error_type, const char *message) __attribute__ ((noreturn));

extern void _ejs_error_init(ejsval global);

EJS_END_DECLS

#endif
