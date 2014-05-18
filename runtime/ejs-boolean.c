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

ejsval _ejs_Boolean EJSVAL_ALIGNMENT;
ejsval _ejs_Boolean_proto EJSVAL_ALIGNMENT;

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
        EJSBoolean* b = (EJSBoolean*)EJSVAL_TO_OBJECT(_this);

        if (argc > 0) {
            b->boolean = ToEJSBool(args[0]);
        }
        else {
            b->boolean = EJS_FALSE;
        }
        return _this;
    }
}

static ejsval
_ejs_Boolean_prototype_toString (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    EJSBool b;

    if (EJSVAL_IS_BOOLEAN(_this)) {
        b = EJSVAL_TO_BOOLEAN(_this);
    }
    else {
        b = ((EJSBoolean*)EJSVAL_TO_OBJECT(_this))->boolean;
    }

    return b ? _ejs_atom_true : _ejs_atom_false;
}

static ejsval
_ejs_Boolean_prototype_valueOf (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    EJSBoolean *b = (EJSBoolean*)EJSVAL_TO_OBJECT(_this);
    return BOOLEAN_TO_EJSVAL(b->boolean);
}

void
_ejs_boolean_init(ejsval global)
{
    _ejs_Boolean = _ejs_function_new_without_proto (_ejs_null, _ejs_atom_Boolean, (EJSClosureFunc)_ejs_Boolean_impl);
    _ejs_object_setprop (global, _ejs_atom_Boolean, _ejs_Boolean);


    _ejs_gc_add_root (&_ejs_Boolean_proto);
    _ejs_Boolean_proto = _ejs_object_new(_ejs_Object_prototype, &_ejs_Object_specops);
    _ejs_object_setprop (_ejs_Boolean,       _ejs_atom_prototype,  _ejs_Boolean_proto);

#define PROTO_METHOD(x) EJS_INSTALL_ATOM_FUNCTION_FLAGS (_ejs_Boolean_proto, x, _ejs_Boolean_prototype_##x, EJS_PROP_NOT_ENUMERABLE)

    PROTO_METHOD(valueOf);
    PROTO_METHOD(toString);

#undef PROTO_METHOD

    _ejs_object_define_value_property (_ejs_Boolean_proto, _ejs_Symbol_toStringTag, _ejs_atom_Boolean, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_WRITABLE | EJS_PROP_CONFIGURABLE);
}

static ejsval
_ejs_boolean_specop_default_value (ejsval obj, const char *hint)
{
    if (!strcmp (hint, "PreferredType") || !strcmp(hint, "Boolean")) {
        EJSBoolean *b = (EJSBoolean*)EJSVAL_TO_OBJECT(obj);
        return BOOLEAN_TO_EJSVAL(b->boolean);
    }
    else if (!strcmp (hint, "String")) {
        EJS_NOT_IMPLEMENTED();
    }
    else
        return _ejs_Object_specops.default_value (obj, hint);
}

EJSObject*
_ejs_boolean_specop_allocate()
{
    return (EJSObject*)_ejs_gc_new (EJSBoolean);
}

EJS_DEFINE_CLASS(Boolean,
                 OP_INHERIT, // [[GetPrototypeOf]]
                 OP_INHERIT, // [[SetPrototypeOf]]
                 OP_INHERIT, // get
                 OP_INHERIT, // get_own_property
                 OP_INHERIT, // get_property
                 OP_INHERIT, // put
                 OP_INHERIT, // can_put
                 OP_INHERIT, // has_property
                 OP_INHERIT, // delete
                 _ejs_boolean_specop_default_value,
                 OP_INHERIT, // define_own_property
                 OP_INHERIT, // has_instance
                 _ejs_boolean_specop_allocate,
                 OP_INHERIT, // finalize
                 OP_INHERIT  // scan
                 )

