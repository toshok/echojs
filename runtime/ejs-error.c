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
}
