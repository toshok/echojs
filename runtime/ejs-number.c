/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include <string.h>

#include "ejs-ops.h"
#include "ejs-value.h"
#include "ejs-number.h"
#include "ejs-function.h"
#include "ejs-string.h"

static ejsval  _ejs_number_specop_default_value (ejsval obj, const char *hint);
static EJSObject* _ejs_number_specop_allocate ();

EJS_DEFINE_CLASS(number, "Number",
                 OP_INHERIT, // get
                 OP_INHERIT, // get_own_property
                 OP_INHERIT, // get_property
                 OP_INHERIT, // put
                 OP_INHERIT, // can_put
                 OP_INHERIT, // has_property
                 OP_INHERIT, // delete
                 _ejs_number_specop_default_value,
                 OP_INHERIT, // define_own_property
                 OP_INHERIT, // has_instance
                 _ejs_number_specop_allocate,
                 OP_INHERIT, // finalize
                 OP_INHERIT  // scan
                 )

ejsval _ejs_Number EJSVAL_ALIGNMENT;
ejsval _ejs_Number_proto EJSVAL_ALIGNMENT;

static ejsval
_ejs_Number_impl (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    if (EJSVAL_IS_UNDEFINED(_this)) {
        // called as a function
        double num = 0;

        if (argc > 0) {
            num = ToDouble(args[0]);
        }

        return NUMBER_TO_EJSVAL(num);
    }
    else {
        EJSNumber* num = (EJSNumber*)EJSVAL_TO_OBJECT(_this);

        if (argc > 0) {
            num->number = ToDouble(args[0]);
        }
        else {
            num->number = 0;
        }
        return _this;
    }
}

static ejsval
_ejs_Number_prototype_toString (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    EJSNumber *num = (EJSNumber*)EJSVAL_TO_OBJECT(_this);

    return NumberToString(num->number);
}

static ejsval
_ejs_Number_prototype_valueOf (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    EJSNumber *num = (EJSNumber*)EJSVAL_TO_OBJECT(_this);
    return NUMBER_TO_EJSVAL(num->number);
}

void
_ejs_number_init(ejsval global)
{
    _ejs_Number = _ejs_function_new_without_proto (_ejs_null, _ejs_atom_Number, (EJSClosureFunc)_ejs_Number_impl);
    _ejs_object_setprop (global, _ejs_atom_Number, _ejs_Number);

    _ejs_gc_add_root (&_ejs_Number_proto);
    _ejs_Number_proto = _ejs_object_new(_ejs_Object_prototype, &_ejs_number_specops);
    _ejs_object_setprop (_ejs_Number,       _ejs_atom_prototype,  _ejs_Number_proto);

#define PROTO_METHOD(x) EJS_INSTALL_ATOM_FUNCTION(_ejs_Number_proto, x, _ejs_Number_prototype_##x)

    PROTO_METHOD(valueOf);
    PROTO_METHOD(toString);

#undef PROTO_METHOD
}

static ejsval
_ejs_number_specop_default_value (ejsval obj, const char *hint)
{
    if (!strcmp (hint, "PreferredType") || !strcmp(hint, "Number")) {
        EJSNumber *num = (EJSNumber*)EJSVAL_TO_OBJECT(obj);
        return NUMBER_TO_EJSVAL(num->number);
    }
    else if (!strcmp (hint, "String")) {
        EJS_NOT_IMPLEMENTED();
    }
    else
        return _ejs_object_specops.default_value (obj, hint);
}

EJSObject*
_ejs_number_specop_allocate()
{
    return (EJSObject*)_ejs_gc_new (EJSNumber);
}



