/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <ctype.h>
#include <stdarg.h>

#include "ejs-value.h"
#include "ejs-array.h"
#include "ejs-string.h"
#include "ejs-function.h"
#include "ejs-regexp.h"
#include "ejs-ops.h"
#include "ejs-string.h"
#include "ejs-error.h"
#include "ejs-proxy.h"
#include "ejs-symbol.h"

#if IOS
#import <Foundation/Foundation.h>
#endif

ejsval _ejs_Error EJSVAL_ALIGNMENT;
ejsval _ejs_Error_prototype EJSVAL_ALIGNMENT;
ejsval _ejs_EvalError EJSVAL_ALIGNMENT;
ejsval _ejs_EvalError_prototype EJSVAL_ALIGNMENT;
ejsval _ejs_RangeError EJSVAL_ALIGNMENT;
ejsval _ejs_RangeError_prototype EJSVAL_ALIGNMENT;
ejsval _ejs_ReferenceError EJSVAL_ALIGNMENT;
ejsval _ejs_ReferenceError_prototype EJSVAL_ALIGNMENT;
ejsval _ejs_SyntaxError EJSVAL_ALIGNMENT;
ejsval _ejs_SyntaxError_prototype EJSVAL_ALIGNMENT;
ejsval _ejs_TypeError EJSVAL_ALIGNMENT;
ejsval _ejs_TypeError_prototype EJSVAL_ALIGNMENT;
ejsval _ejs_URIError EJSVAL_ALIGNMENT;
ejsval _ejs_URIError_prototype EJSVAL_ALIGNMENT;

#define NATIVE_ERROR_CTOR(err)                                          \
    static ejsval                                                       \
    _ejs_##err##_impl (ejsval env, ejsval _this, uint32_t argc, ejsval*args) \
    {                                                                   \
        if (EJSVAL_IS_UNDEFINED(_this))                                 \
            _this = _ejs_object_new (_ejs_##err##_prototype, &_ejs_Error_specops); \
                                                                        \
        if (argc >= 1) {                                                \
            _ejs_object_setprop (_this, _ejs_atom_message, ToString(args[0])); \
        }                                                               \
                                                                        \
        return _this;                                                   \
    }

NATIVE_ERROR_CTOR(Error);
NATIVE_ERROR_CTOR(EvalError);
NATIVE_ERROR_CTOR(RangeError);
NATIVE_ERROR_CTOR(ReferenceError);
NATIVE_ERROR_CTOR(SyntaxError);
NATIVE_ERROR_CTOR(TypeError);
NATIVE_ERROR_CTOR(URIError);

static ejsval
_ejs_Error_prototype_toString (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    if (!EJSVAL_IS_OBJECT(_this)) {
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Error.prototype.toString called on non-object");
    }

    EJSObject *_thisobj = EJSVAL_TO_OBJECT(_this);
    ejsval name = OP(_thisobj,Get)(_this, _ejs_atom_name, _this);
    if (EJSVAL_IS_NULL_OR_UNDEFINED(name))
        name = _ejs_atom_Error;

    ejsval message = OP(_thisobj,Get)(_this, _ejs_atom_message, _this);
    if (EJSVAL_IS_NULL_OR_UNDEFINED(message))
        return name;

    ejsval sep = _ejs_string_new_utf8(": ");
    return _ejs_string_concatv (name, sep, message, _ejs_null);
}

static ejsval
_ejs_Error_create (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    // 1. Let F be the this value. 
    ejsval F = _this;

    if (!EJSVAL_IS_CONSTRUCTOR(F)) 
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "'this' in Error[Symbol.create] is not a constructor");

    EJSObject* F_ = EJSVAL_TO_OBJECT(F);

    // 2. Let obj be the result of calling OrdinaryCreateFromConstructor(F, "%ErrorPrototype%", ([[ErrorData]]) ). 
    ejsval proto = OP(F_,Get)(F, _ejs_atom_prototype, F);
    if (EJSVAL_IS_UNDEFINED(proto))
        proto = _ejs_Error_prototype;

    EJSObject* obj = (EJSObject*)_ejs_gc_new (EJSObject);
    _ejs_init_object (obj, proto, &_ejs_Error_specops);
    return OBJECT_TO_EJSVAL(obj);
}

ejsval
_ejs_nativeerror_new (EJSNativeErrorType err_type, ejsval msg)
{
    EJSObject* exc_obj = _ejs_gc_new (EJSObject);

    ejsval proto;
    switch (err_type) {
    case EJS_ERROR:           proto = _ejs_Error_prototype; break;
    case EJS_EVAL_ERROR:      proto = _ejs_EvalError_prototype; break;
    case EJS_RANGE_ERROR:     proto = _ejs_RangeError_prototype; break;
    case EJS_REFERENCE_ERROR: proto = _ejs_ReferenceError_prototype; break;
    case EJS_SYNTAX_ERROR:    proto = _ejs_SyntaxError_prototype; break;
    case EJS_TYPE_ERROR:      proto = _ejs_TypeError_prototype; break;
    case EJS_URI_ERROR:       proto = _ejs_URIError_prototype; break;
    }

    _ejs_init_object (exc_obj, proto, &_ejs_Error_specops);

    ejsval exc = OBJECT_TO_EJSVAL(exc_obj);

    switch (err_type) {
    case EJS_ERROR:           _ejs_Error_impl (_ejs_null, exc, 1, &msg); break;
    case EJS_EVAL_ERROR:      _ejs_EvalError_impl (_ejs_null, exc, 1, &msg); break;
    case EJS_RANGE_ERROR:     _ejs_RangeError_impl (_ejs_null, exc, 1, &msg); break;
    case EJS_REFERENCE_ERROR: _ejs_ReferenceError_impl (_ejs_null, exc, 1, &msg); break;
    case EJS_SYNTAX_ERROR:    _ejs_SyntaxError_impl (_ejs_null, exc, 1, &msg); break;
    case EJS_TYPE_ERROR:      _ejs_TypeError_impl (_ejs_null, exc, 1, &msg); break;
    case EJS_URI_ERROR:       _ejs_URIError_impl (_ejs_null, exc, 1, &msg); break;
    }

    return exc;
}

ejsval
_ejs_nativeerror_new_utf8 (EJSNativeErrorType err_type, const char *message)
{
    ejsval msg = _ejs_string_new_utf8 (message);
    return _ejs_nativeerror_new (err_type, msg);
}

void
_ejs_error_init(ejsval global)
{
    ejsval toString = _ejs_function_new_native (_ejs_null, _ejs_atom_toString, (EJSClosureFunc)_ejs_Error_prototype_toString);
    _ejs_gc_add_root (&toString);
    
#define EJS_ADD_NATIVE_ERROR_TYPE(err) EJS_MACRO_START                  \
    _ejs_##err = _ejs_function_new_without_proto (_ejs_null, _ejs_atom_##err, (EJSClosureFunc)_ejs_##err##_impl); \
    _ejs_object_setprop (global, _ejs_atom_##err, _ejs_##err);          \
    _ejs_##err##_prototype = _ejs_object_new(_ejs_null, &_ejs_Object_specops); \
    _ejs_object_setprop (_ejs_##err,       _ejs_atom_prototype,  _ejs_##err##_prototype); \
    _ejs_object_define_value_property (_ejs_##err##_prototype, _ejs_atom_constructor, _ejs_##err,\
                                       EJS_PROP_NOT_ENUMERABLE | EJS_PROP_CONFIGURABLE | EJS_PROP_WRITABLE); \
                                                                    \
    _ejs_object_setprop (_ejs_##err##_prototype, _ejs_atom_name, _ejs_atom_##err); \
    _ejs_object_setprop (_ejs_##err##_prototype, _ejs_atom_toString, toString); \
                                                                        \
    EJS_INSTALL_SYMBOL_FUNCTION_FLAGS (_ejs_##err, create, _ejs_Error_create, EJS_PROP_NOT_ENUMERABLE); \
EJS_MACRO_END

    EJS_ADD_NATIVE_ERROR_TYPE(Error);
    EJS_ADD_NATIVE_ERROR_TYPE(EvalError);
    EJS_ADD_NATIVE_ERROR_TYPE(RangeError);
    EJS_ADD_NATIVE_ERROR_TYPE(ReferenceError);
    EJS_ADD_NATIVE_ERROR_TYPE(SyntaxError);
    EJS_ADD_NATIVE_ERROR_TYPE(TypeError);
    EJS_ADD_NATIVE_ERROR_TYPE(URIError);

    _ejs_gc_remove_root (&toString);
}

void
_ejs_throw_nativeerror_utf8 (EJSNativeErrorType error_type, const char *message)
{
    ejsval exc = _ejs_nativeerror_new_utf8 (error_type, message);
    //_ejs_log ("throwing exception with message %s\n", message);
    _ejs_throw (exc);
    EJS_NOT_REACHED();
}

void
_ejs_throw_nativeerror (EJSNativeErrorType error_type, ejsval message)
{
    ejsval exc = _ejs_nativeerror_new (error_type, message);

    char *message_utf8 = ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(message));
    //_ejs_log ("throwing exception with message %s\n", message_utf8);
    free (message_utf8);

    _ejs_throw (exc);
    EJS_NOT_REACHED();
}

EJS_DEFINE_INHERIT_ALL_CLASS(Error)
