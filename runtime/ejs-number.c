/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include <math.h>
#include <float.h>
#include <string.h>

#include "ejs-ops.h"
#include "ejs-value.h"
#include "ejs-number.h"
#include "ejs-function.h"
#include "ejs-string.h"

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

static ejsval
_ejs_Number_prototype_clz (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    EJSNumber *num = (EJSNumber*)EJSVAL_TO_OBJECT(_this);
    return _ejs_clz32 (NUMBER_TO_EJSVAL(num->number));
}

static ejsval
_ejs_Number_isFinite (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval number = _ejs_undefined;
    if (argc > 0)
        number = args[0];

    // 1. If Type(number) is not Number, return false.
    if (!EJSVAL_IS_NUMBER(number))
        return _ejs_false;

    double number_ = EJSVAL_TO_NUMBER(number);

    // 2. If number is NaN, +∞, or -∞, return false.
    if (isnan(number_))
        return _ejs_false;

    if (fpclassify(number_) == FP_INFINITE)
        return _ejs_false;

    // 3. Otherwise, return true.
    return _ejs_true;
}

static ejsval
_ejs_Number_toInteger (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval argument = _ejs_undefined;
    if (argc > 0) argument = args[0];

    double number = ToDouble(argument);
    if (isnan(number))
        return NUMBER_TO_EJSVAL(+0);
    
    if (number == 0 || fpclassify(number) == FP_INFINITE)
        return NUMBER_TO_EJSVAL(number);

    return NUMBER_TO_EJSVAL ((number < 0 ? -1 : 1) * floor(fabs(number)));
}

static ejsval
_ejs_Number_isInteger (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval number = _ejs_undefined;
    if (argc > 0)
        number = args[0];

    // 1. If Type(number) is not Number, return false.
    if (!EJSVAL_IS_NUMBER(number))
        return _ejs_false;

    double number_ = EJSVAL_TO_NUMBER(number);

    // 2. If number is NaN, +∞, or -∞, return false.
    if (isnan(number_))
        return _ejs_false;

    if (fpclassify(number_) == FP_INFINITE)
        return _ejs_false;

    // 3. Let integer be ToInteger(number).
    int integer = ToInteger(number);

    // 4. If integer is not equal to number, return false.
    if (integer != number_)
        return _ejs_false;

    // 5. Otherwise, return true.
    return _ejs_true;
}

static ejsval
_ejs_Number_isSafeInteger (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval number = _ejs_undefined;
    if (argc > 0)
        number = args[0];

    // 1. If Type(number) is not Number, return false.
    if (!EJSVAL_IS_NUMBER(number))
        return _ejs_false;

    double number_ = EJSVAL_TO_NUMBER(number);

    // 2. If number is NaN, +∞, or -∞, return false.
    if (isnan(number_))
        return _ejs_false;

    if (fpclassify(number_) == FP_INFINITE)
        return _ejs_false;

    // 3. Let integer be ToInteger(number).
    int64_t integer = ToInteger(number);

    // 4. If integer is not equal to number, return false.
    if (integer != number_)
        return _ejs_false;

    // 5. If abs(integer) ≤ 253-1, then return true.
    if (llabs(integer) <= ((int64_t)2<<53)-1)
        return _ejs_true;

    // 6. Otherwise, return false.
    return _ejs_false;
}
static ejsval
_ejs_Number_isNaN (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval number = _ejs_undefined;
    if (argc > 0)
        number = args[0];

    // 1. If Type(number) is not Number, return false.
    if (!EJSVAL_IS_NUMBER(number))
        return _ejs_false;

    // 2. If number is NaN, return true.
    if (isnan(EJSVAL_TO_NUMBER(number)))
        return _ejs_true;

    // 3. Otherwise, return false.
    return _ejs_false;
}

void
_ejs_number_init(ejsval global)
{
    _ejs_Number = _ejs_function_new_without_proto (_ejs_null, _ejs_atom_Number, (EJSClosureFunc)_ejs_Number_impl);
    _ejs_object_setprop (global, _ejs_atom_Number, _ejs_Number);

    _ejs_gc_add_root (&_ejs_Number_proto);
    _ejs_Number_proto = _ejs_object_new(_ejs_Object_prototype, &_ejs_Number_specops);
    _ejs_object_setprop (_ejs_Number,       _ejs_atom_prototype,  _ejs_Number_proto);

#define PROTO_METHOD(x) EJS_INSTALL_ATOM_FUNCTION(_ejs_Number_proto, x, _ejs_Number_prototype_##x)
#define OBJ_METHOD(x) EJS_INSTALL_ATOM_FUNCTION(_ejs_Number, x, _ejs_Number_##x)

    PROTO_METHOD(valueOf);
    PROTO_METHOD(toString);
    // ES6
    PROTO_METHOD(clz);
    OBJ_METHOD(isFinite);
    OBJ_METHOD(isInteger);
    OBJ_METHOD(isSafeInteger);
    OBJ_METHOD(isNaN);
    OBJ_METHOD(toInteger);

    _ejs_object_setprop (_ejs_Number, _ejs_atom_EPSILON, NUMBER_TO_EJSVAL(nextafter(1, INFINITY)));
    _ejs_object_setprop (_ejs_Number, _ejs_atom_MAX_SAFE_INTEGER, NUMBER_TO_EJSVAL(EJS_MAX_SAFE_INTEGER));
    _ejs_object_setprop (_ejs_Number, _ejs_atom_MIN_SAFE_INTEGER, NUMBER_TO_EJSVAL(EJS_MIN_SAFE_INTEGER));
    _ejs_object_setprop (_ejs_Number, _ejs_atom_MAX_VALUE, NUMBER_TO_EJSVAL(DBL_MAX));
    _ejs_object_setprop (_ejs_Number, _ejs_atom_MIN_VALUE, NUMBER_TO_EJSVAL(DBL_MIN));
    _ejs_object_setprop (_ejs_Number, _ejs_atom_NaN, NUMBER_TO_EJSVAL(nan("7734")));
    _ejs_object_setprop (_ejs_Number, _ejs_atom_NEGATIVE_INFINITY, NUMBER_TO_EJSVAL(-INFINITY));
    _ejs_object_setprop (_ejs_Number, _ejs_atom_POSITIVE_INFINITY, NUMBER_TO_EJSVAL(INFINITY));


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
        return _ejs_Object_specops.default_value (obj, hint);
}

EJSObject*
_ejs_number_specop_allocate()
{
    return (EJSObject*)_ejs_gc_new (EJSNumber);
}

EJS_DEFINE_CLASS(Number,
                 OP_INHERIT, // [[GetPrototypeOf]]
                 OP_INHERIT, // [[SetPrototypeOf]]
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

