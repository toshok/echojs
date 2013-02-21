/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */
#include <assert.h>
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

EJSSpecOps _ejs_nativeerror_specops;

ejsval _ejs_EvalError;
ejsval _ejs_EvalError_proto;
ejsval _ejs_RangeError;
ejsval _ejs_RangeError_proto;
ejsval _ejs_ReferenceError;
ejsval _ejs_ReferenceError_proto;
ejsval _ejs_SyntaxError;
ejsval _ejs_SyntaxError_proto;
ejsval _ejs_TypeError;
ejsval _ejs_TypeError_proto;
ejsval _ejs_UriError;
ejsval _ejs_UriError_proto;

#define NATIVE_ERROR_CTOR(err)                                          \
    static ejsval                                                       \
    _ejs_##err##_impl (ejsval env, ejsval _this, uint32_t argc, ejsval*args) \
    {                                                                   \
        if (EJSVAL_IS_UNDEFINED(_this))                                 \
            _this = _ejs_object_new (_ejs_##err##_proto, &_ejs_nativeerror_specops); \
                                                                        \
        if (argc >= 1) {                                                \
            _ejs_object_setprop (_this, _ejs_atom_message, ToString(args[0])); \
        }                                                               \
                                                                        \
        return _this;                                                   \
    }

NATIVE_ERROR_CTOR(EvalError);
NATIVE_ERROR_CTOR(RangeError);
NATIVE_ERROR_CTOR(ReferenceError);
NATIVE_ERROR_CTOR(SyntaxError);
NATIVE_ERROR_CTOR(TypeError);
NATIVE_ERROR_CTOR(UriError);

ejsval
_ejs_nativeerror_new_utf8 (EJSNativeErrorType err_type, const char *message)
{
    EJSObject* exc_obj = _ejs_gc_new (EJSObject);

    ejsval proto;
    switch (err_type) {
    case EJS_EVAL_ERROR:      proto = _ejs_EvalError_proto; break;
    case EJS_RANGE_ERROR:     proto = _ejs_RangeError_proto; break;
    case EJS_REFERENCE_ERROR: proto = _ejs_ReferenceError_proto; break;
    case EJS_SYNTAX_ERROR:    proto = _ejs_SyntaxError_proto; break;
    case EJS_TYPE_ERROR:      proto = _ejs_TypeError_proto; break;
    case EJS_URI_ERROR:       proto = _ejs_UriError_proto; break;
    }

    _ejs_init_object (exc_obj, proto, &_ejs_nativeerror_specops);

    ejsval msg = _ejs_string_new_utf8 (message);
    ejsval exc = OBJECT_TO_EJSVAL(exc_obj);

    switch (err_type) {
    case EJS_EVAL_ERROR:      _ejs_EvalError_impl (_ejs_null, exc, 1, &msg); break;
    case EJS_RANGE_ERROR:     _ejs_RangeError_impl (_ejs_null, exc, 1, &msg); break;
    case EJS_REFERENCE_ERROR: _ejs_ReferenceError_impl (_ejs_null, exc, 1, &msg); break;
    case EJS_SYNTAX_ERROR:    _ejs_SyntaxError_impl (_ejs_null, exc, 1, &msg); break;
    case EJS_TYPE_ERROR:      _ejs_TypeError_impl (_ejs_null, exc, 1, &msg); break;
    case EJS_URI_ERROR:       _ejs_UriError_impl (_ejs_null, exc, 1, &msg); break;
    }

    return exc;
}

static void
_ejs_nativeerror_init(ejsval global)
{
    START_SHADOW_STACK_FRAME;

    _ejs_nativeerror_specops =  _ejs_object_specops;
    _ejs_nativeerror_specops.class_name = "Error";

#define EJS_ADD_NATIVE_ERROR_TYPE(err) EJS_MACRO_START              \
    _ejs_gc_add_named_root (_ejs_##err##_proto);                    \
    _ejs_##err##_proto = _ejs_object_new(_ejs_null, &_ejs_object_specops); \
    ADD_STACK_ROOT(ejsval, tmpobj, _ejs_function_new (_ejs_null, _ejs_atom_##err, (EJSClosureFunc)_ejs_##err##_impl)); \
    _ejs_##err = tmpobj;                                            \
    _ejs_object_setprop (_ejs_##err,       _ejs_atom_prototype,  _ejs_##err##_proto); \
    _ejs_object_setprop (_ejs_##err##_proto, _ejs_atom_name, _ejs_atom_##err); \
EJS_MACRO_END

    EJS_ADD_NATIVE_ERROR_TYPE(EvalError);
    EJS_ADD_NATIVE_ERROR_TYPE(RangeError);
    EJS_ADD_NATIVE_ERROR_TYPE(ReferenceError);
    EJS_ADD_NATIVE_ERROR_TYPE(SyntaxError);
    EJS_ADD_NATIVE_ERROR_TYPE(TypeError);
    EJS_ADD_NATIVE_ERROR_TYPE(UriError);

    END_SHADOW_STACK_FRAME;
}





EJSSpecOps _ejs_error_specops;

ejsval _ejs_Error;
ejsval _ejs_Error_proto;

static ejsval
_ejs_Error_impl (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    if (EJSVAL_IS_NULL(_this) || EJSVAL_IS_UNDEFINED(_this)) {
        printf ("Error called as a function, ecma262 15.11.1\n");
        EJS_NOT_IMPLEMENTED();
    }
    else {
        // called as a constructor
        EJSObject* err = (EJSObject*)EJSVAL_TO_OBJECT(_this);
        err->ops = &_ejs_error_specops;

        if (argc > 0)
            _ejs_object_setprop (_this, _ejs_atom_message, ToString(args[0]));

        return _this;
    }
}

static ejsval
_ejs_Error_prototype_toString (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    EJS_NOT_IMPLEMENTED();
}


void
_ejs_error_init(ejsval global)
{
    _ejs_error_specops = _ejs_object_specops;
    _ejs_error_specops.class_name = "Error";

    START_SHADOW_STACK_FRAME;

    _ejs_gc_add_named_root (_ejs_Error_proto);
    _ejs_Error_proto = _ejs_object_new(_ejs_null, &_ejs_object_specops);

    ADD_STACK_ROOT(ejsval, tmpobj, _ejs_function_new (_ejs_null, _ejs_atom_Error, (EJSClosureFunc)_ejs_Error_impl));
    _ejs_Error = tmpobj;

    _ejs_object_setprop (_ejs_Error,       _ejs_atom_prototype,  _ejs_Error_proto);

#define OBJ_METHOD(x) EJS_INSTALL_FUNCTION(_ejs_Error, EJS_STRINGIFY(x), _ejs_Error_##x)
#define PROTO_METHOD(x) EJS_INSTALL_FUNCTION(_ejs_Error_proto, EJS_STRINGIFY(x), _ejs_Error_prototype_##x)

    PROTO_METHOD(toString);

#undef OBJ_METHOD
#undef PROTO_METHOD

    _ejs_object_setprop (global, _ejs_atom_Error, _ejs_Error);

    END_SHADOW_STACK_FRAME;

    _ejs_nativeerror_init (global);
}

void
_ejs_throw_nativeerror (EJSNativeErrorType error_type, const char *message)
{
    ejsval exc = _ejs_nativeerror_new_utf8 (error_type, message);
    _ejs_throw (exc);
}

