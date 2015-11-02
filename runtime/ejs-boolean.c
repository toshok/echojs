/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include <string.h>

#include "ejs-ops.h"
#include "ejs-value.h"
#include "ejs-boolean.h"
#include "ejs-function.h"
#include "ejs-string.h"
#include "ejs-symbol.h"
#include "ejs-error.h"
#include "ejs-proxy.h"

ejsval _ejs_Boolean EJSVAL_ALIGNMENT;
ejsval _ejs_Boolean_prototype EJSVAL_ALIGNMENT;

static ejsval
_ejs_Boolean_impl (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    if (EJSVAL_IS_UNDEFINED(_this)) {
        // called as a function
        EJSBool b = EJS_FALSE;

        if (argc > 0) {
            b = ToEJSBool(args[0]);
        }

        return BOOLEAN_TO_EJSVAL(b);
    }
    else {
        EJSBoolean* b = EJSVAL_TO_BOOLEAN_OBJECT(_this);

        if (argc > 0) {
            b->boolean_data = ToEJSBool(args[0]) ? _ejs_true : _ejs_false;
        }
        else {
            b->boolean_data = _ejs_false;
        }
        return _this;
    }
}

static ejsval
_ejs_Boolean_create (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval F = _this;


    if (!IsConstructor(F)) 
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "'this' in Boolean[Symbol.create] is not a constructor");
        
    EJSObject* F_ = EJSVAL_TO_OBJECT(F);

    ejsval proto = OP(F_,Get)(F, _ejs_atom_prototype, F);
    if (EJSVAL_IS_UNDEFINED(proto)) {
        proto = _ejs_Boolean_prototype;
    }

    if (!EJSVAL_IS_OBJECT(proto)) {
        EJS_NOT_IMPLEMENTED(); // cross-realm doesn't exist in ejs yet
    }

    EJSObject* obj = (EJSObject*)_ejs_gc_new (EJSBoolean);
    _ejs_init_object (obj, proto, &_ejs_Boolean_specops);
    return OBJECT_TO_EJSVAL(obj);
}

static ejsval
_ejs_Boolean_prototype_toString (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    EJSBool b;

    if (EJSVAL_IS_BOOLEAN(_this))
        b = EJSVAL_TO_BOOLEAN(_this);
    else
        b = EJSVAL_TO_BOOLEAN(EJSVAL_TO_BOOLEAN_OBJECT(_this)->boolean_data);

    return b ? _ejs_atom_true : _ejs_atom_false;
}

static ejsval
_ejs_Boolean_prototype_valueOf (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    EJSBoolean *b = EJSVAL_TO_BOOLEAN_OBJECT(_this);
    return b->boolean_data;
}

void
_ejs_boolean_init(ejsval global)
{
    _ejs_Boolean = _ejs_function_new_without_proto (_ejs_null, _ejs_atom_Boolean, (EJSClosureFunc)_ejs_Boolean_impl);
    _ejs_object_setprop (global, _ejs_atom_Boolean, _ejs_Boolean);


    _ejs_gc_add_root (&_ejs_Boolean_prototype);

    EJSBoolean* prototype = (EJSBoolean*)_ejs_gc_new(EJSBoolean);
    _ejs_init_object ((EJSObject*)prototype, _ejs_null, &_ejs_Boolean_specops);
    prototype->boolean_data = BOOLEAN_TO_EJSVAL(EJS_FALSE);
    _ejs_Boolean_prototype = OBJECT_TO_EJSVAL(prototype);

    _ejs_object_setprop (_ejs_Boolean, _ejs_atom_prototype, _ejs_Boolean_prototype);

    EJS_INSTALL_SYMBOL_FUNCTION_FLAGS (_ejs_Boolean, create, _ejs_Boolean_create, EJS_PROP_NOT_ENUMERABLE);

#define PROTO_METHOD(x) EJS_INSTALL_ATOM_FUNCTION_FLAGS (_ejs_Boolean_prototype, x, _ejs_Boolean_prototype_##x, EJS_PROP_NOT_ENUMERABLE)

    PROTO_METHOD(valueOf);
    PROTO_METHOD(toString);

#undef PROTO_METHOD

    _ejs_object_define_value_property (_ejs_Boolean_prototype, _ejs_Symbol_toStringTag, _ejs_atom_Boolean, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_WRITABLE | EJS_PROP_CONFIGURABLE);
}

EJSObject*
_ejs_boolean_specop_allocate()
{
    return (EJSObject*)_ejs_gc_new (EJSBoolean);
}

EJS_DEFINE_CLASS(Boolean,
                 OP_INHERIT, // [[GetPrototypeOf]]
                 OP_INHERIT, // [[SetPrototypeOf]]
                 OP_INHERIT, // [[IsExtensible]]
                 OP_INHERIT, // [[PreventExtensions]]
                 OP_INHERIT, // [[GetOwnProperty]]
                 OP_INHERIT, // [[DefineOwnProperty]]
                 OP_INHERIT, // [[HasProperty]]
                 OP_INHERIT, // [[Get]]
                 OP_INHERIT, // [[Set]]
                 OP_INHERIT, // [[Delete]]
                 OP_INHERIT, // [[Enumerate]]
                 OP_INHERIT, // [[OwnPropertyKeys]]
                 OP_INHERIT, // [[Call]]
                 OP_INHERIT, // [[Construct]]
                 _ejs_boolean_specop_allocate,
                 OP_INHERIT, // [[Finalize]]
                 OP_INHERIT  // [[Scan]]
                 )
