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

// ES2015, June 2015
// 19.3.1.1 Boolean ( value )
static EJS_NATIVE_FUNC(_ejs_Boolean_impl) {
    ejsval value = _ejs_undefined;
    if (argc > 0)
        value = args[0];

    // 1. Let b be ToBoolean(value).
    ejsval b = ToBoolean(value);

    // 2. If NewTarget is undefined, return b.
    if (EJSVAL_IS_UNDEFINED(newTarget))
        return b;

    // 3. Let O be OrdinaryCreateFromConstructor(NewTarget, "%BooleanPrototype%", «[[BooleanData]]» ).
    // 4. ReturnIfAbrupt(O).
    ejsval O = OrdinaryCreateFromConstructor(newTarget, _ejs_Boolean_prototype, &_ejs_Boolean_specops);
    *_this = O;

    // 5. Set the value of O’s [[BooleanData]] internal slot to b.
    EJSBoolean* O_ = (EJSBoolean*)EJSVAL_TO_OBJECT(O);
    O_->boolean_data = b;

    // 6. Return O.
    return O;
}

static EJS_NATIVE_FUNC(_ejs_Boolean_prototype_toString) {
    EJSBool b;

    if (EJSVAL_IS_BOOLEAN(*_this))
        b = EJSVAL_TO_BOOLEAN(*_this);
    else
        b = EJSVAL_TO_BOOLEAN(EJSVAL_TO_BOOLEAN_OBJECT(*_this)->boolean_data);

    return b ? _ejs_atom_true : _ejs_atom_false;
}

static EJS_NATIVE_FUNC(_ejs_Boolean_prototype_valueOf) {
    EJSBoolean *b = EJSVAL_TO_BOOLEAN_OBJECT(*_this);
    return b->boolean_data;
}

void
_ejs_boolean_init(ejsval global)
{
    _ejs_Boolean = _ejs_function_new_without_proto (_ejs_null, _ejs_atom_Boolean, _ejs_Boolean_impl);
    _ejs_object_setprop (global, _ejs_atom_Boolean, _ejs_Boolean);


    _ejs_gc_add_root (&_ejs_Boolean_prototype);

    EJSBoolean* prototype = (EJSBoolean*)_ejs_gc_new(EJSBoolean);
    _ejs_init_object ((EJSObject*)prototype, _ejs_null, &_ejs_Boolean_specops);
    prototype->boolean_data = BOOLEAN_TO_EJSVAL(EJS_FALSE);
    _ejs_Boolean_prototype = OBJECT_TO_EJSVAL(prototype);

    _ejs_object_setprop (_ejs_Boolean, _ejs_atom_prototype, _ejs_Boolean_prototype);

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
