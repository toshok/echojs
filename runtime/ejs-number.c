/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include <assert.h>
#include <string.h>

#include "ejs-ops.h"
#include "ejs-value.h"
#include "ejs-number.h"
#include "ejs-function.h"
#include "ejs-string.h"

static ejsval  _ejs_number_specop_get (ejsval obj, ejsval propertyName, EJSBool isCStr);
static EJSPropertyDesc* _ejs_number_specop_get_own_property (ejsval obj, ejsval propertyName);
static EJSPropertyDesc* _ejs_number_specop_get_property (ejsval obj, ejsval propertyName);
static void    _ejs_number_specop_put (ejsval obj, ejsval propertyName, ejsval val, EJSBool flag);
static EJSBool _ejs_number_specop_can_put (ejsval obj, ejsval propertyName);
static EJSBool _ejs_number_specop_has_property (ejsval obj, ejsval propertyName);
static EJSBool _ejs_number_specop_delete (ejsval obj, ejsval propertyName, EJSBool flag);
static ejsval  _ejs_number_specop_default_value (ejsval obj, const char *hint);
static EJSBool _ejs_number_specop_define_own_property (ejsval obj, ejsval propertyName, EJSPropertyDesc* propertyDescriptor, EJSBool flag);
static EJSObject* _ejs_number_specop_allocate ();
static void    _ejs_number_specop_finalize (EJSObject* obj);
static void    _ejs_number_specop_scan (EJSObject* obj, EJSValueFunc scan_func);

EJSSpecOps _ejs_number_specops = {
    "Number",
    _ejs_number_specop_get,
    _ejs_number_specop_get_own_property,
    _ejs_number_specop_get_property,
    _ejs_number_specop_put,
    _ejs_number_specop_can_put,
    _ejs_number_specop_has_property,
    _ejs_number_specop_delete,
    _ejs_number_specop_default_value,
    _ejs_number_specop_define_own_property,

    _ejs_number_specop_allocate,
    _ejs_number_specop_finalize,
    _ejs_number_specop_scan
};

ejsval _ejs_Number;
ejsval _ejs_Number_proto;

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
    START_SHADOW_STACK_FRAME;

    _ejs_gc_add_named_root (_ejs_Number_proto);
    _ejs_Number_proto = _ejs_object_new(_ejs_Object_prototype, &_ejs_number_specops);

    ADD_STACK_ROOT(ejsval, tmpobj, _ejs_function_new (_ejs_null, _ejs_atom_Number, (EJSClosureFunc)_ejs_Number_impl));
    _ejs_Number = tmpobj;

    _ejs_object_setprop (_ejs_Number,       _ejs_atom_prototype,  _ejs_Number_proto);

#define PROTO_METHOD(x) EJS_INSTALL_FUNCTION(_ejs_Number_proto, EJS_STRINGIFY(x), _ejs_Number_prototype_##x)

    PROTO_METHOD(valueOf);
    PROTO_METHOD(toString);

#undef PROTO_METHOD

    _ejs_object_setprop (global, _ejs_atom_Number, _ejs_Number);

    END_SHADOW_STACK_FRAME;
}


static ejsval
_ejs_number_specop_get (ejsval obj, ejsval propertyName, EJSBool isCStr)
{
    return _ejs_object_specops.get (obj, propertyName, isCStr);
}

static EJSPropertyDesc*
_ejs_number_specop_get_own_property (ejsval obj, ejsval propertyName)
{
    return _ejs_object_specops.get_own_property (obj, propertyName);
}

static EJSPropertyDesc*
_ejs_number_specop_get_property (ejsval obj, ejsval propertyName)
{
    return _ejs_object_specops.get_property (obj, propertyName);
}

static void
_ejs_number_specop_put (ejsval obj, ejsval propertyName, ejsval val, EJSBool flag)
{
    _ejs_object_specops.put (obj, propertyName, val, flag);
}

static EJSBool
_ejs_number_specop_can_put (ejsval obj, ejsval propertyName)
{
    return _ejs_object_specops.can_put (obj, propertyName);
}

static EJSBool
_ejs_number_specop_has_property (ejsval obj, ejsval propertyName)
{
    return _ejs_object_specops.has_property (obj, propertyName);
}

static EJSBool
_ejs_number_specop_delete (ejsval obj, ejsval propertyName, EJSBool flag)
{
    return _ejs_object_specops._delete (obj, propertyName, flag);
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

static EJSBool
_ejs_number_specop_define_own_property (ejsval obj, ejsval propertyName, EJSPropertyDesc* propertyDescriptor, EJSBool flag)
{
    return _ejs_object_specops.define_own_property (obj, propertyName, propertyDescriptor, flag);
}

EJSObject*
_ejs_number_specop_allocate()
{
    return (EJSObject*)_ejs_gc_new (EJSNumber);
}

static void
_ejs_number_specop_finalize (EJSObject* obj)
{
    _ejs_object_specops.finalize (obj);
}

static void
_ejs_number_specop_scan (EJSObject* obj, EJSValueFunc scan_func)
{
    _ejs_object_specops.scan (obj, scan_func);
}
