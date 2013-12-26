/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include <math.h>

#include "ejs-ops.h"
#include "ejs-value.h"
#include "ejs-function.h"
#include "ejs-math.h"
#include "ejs-string.h"

ejsval _ejs_Math EJSVAL_ALIGNMENT;

// ECMA262: 15.8.2.1
static ejsval
_ejs_Math_abs (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval x = _ejs_undefined;
    if (argc > 0) x = args[0];

    double x_ = ToDouble(x);
    if (isnan(x_))
        return _ejs_nan;
    
    return NUMBER_TO_EJSVAL(x_ < 0 ? -x_ : x_);
}

// ECMA262: 15.8.2.2
static ejsval
_ejs_Math_acos (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval x = _ejs_undefined;
    if (argc > 0) x = args[0];

    double x_ = ToDouble(x);
    if (isnan(x_))
        return _ejs_nan;

    return NUMBER_TO_EJSVAL(acos(x_));
}

// ECMA262: 15.8.2.3
static ejsval
_ejs_Math_asin (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval x = _ejs_undefined;
    if (argc > 0) x = args[0];

    double x_ = ToDouble(x);
    if (isnan(x_))
        return _ejs_nan;

    return NUMBER_TO_EJSVAL(asin(x_));
}

// ECMA262: 15.8.2.4
static ejsval
_ejs_Math_atan (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval x = _ejs_undefined;
    if (argc > 0) x = args[0];

    double x_ = ToDouble(x);
    if (isnan(x_))
        return _ejs_nan;

    return NUMBER_TO_EJSVAL(atan(x_));
}

// ECMA262: 15.8.2.5
static ejsval
_ejs_Math_atan2 (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval y = _ejs_undefined;
    ejsval x = _ejs_undefined;
    if (argc > 0) y = args[0];
    if (argc > 1) x = args[1];

    double x_ = ToDouble(x);
    double y_ = ToDouble(y);

    if (isnan(x_) || isnan(y_))
        return _ejs_nan;

    return NUMBER_TO_EJSVAL(atan2(y_,x_));
}

// ECMA262: 15.8.2.6
static ejsval
_ejs_Math_ceil (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval x = _ejs_undefined;
    if (argc > 0) x = args[0];

    double x_ = ToDouble(x);
    if (isnan(x_))
        return _ejs_nan;

    return NUMBER_TO_EJSVAL(ceil(x_));
}

// ECMA262: 15.8.2.7
static ejsval
_ejs_Math_cos (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval x = _ejs_undefined;
    if (argc > 0) x = args[0];

    double x_ = ToDouble(x);
    if (isnan(x_))
        return _ejs_nan;

    return NUMBER_TO_EJSVAL(cos(x_));
}

// ECMA262: 15.8.2.8
static ejsval
_ejs_Math_exp (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval x = _ejs_undefined;
    if (argc > 0) x = args[0];

    double x_ = ToDouble(x);
    if (isnan(x_))
        return _ejs_nan;

    return NUMBER_TO_EJSVAL(exp(x_));
}

// ECMA262: 15.8.2.9
static ejsval
_ejs_Math_floor (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval x = _ejs_undefined;
    if (argc > 0) x = args[0];

    double x_ = ToDouble(x);
    if (isnan(x_))
        return _ejs_nan;

    return NUMBER_TO_EJSVAL(floor(x_));
}

// ECMA262: 15.8.2.10
static ejsval
_ejs_Math_log (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval x = _ejs_undefined;
    if (argc > 0) x = args[0];

    double x_ = ToDouble(x);
    if (isnan(x_))
        return _ejs_nan;

    return NUMBER_TO_EJSVAL(log(x_));
}

// ECMA262: 15.8.2.11
static ejsval
_ejs_Math_max (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    double current_max = -INFINITY;
    for (int i = 0; i < argc; i ++) {
        double d = ToDouble(args[i]);
        if (isnan (d))
            return _ejs_nan;

        if (d > current_max)
            current_max = d;
    }

    return NUMBER_TO_EJSVAL(current_max);
}

// ECMA262: 15.8.2.12
static ejsval
_ejs_Math_min (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    double current_min = INFINITY;
    for (int i = 0; i < argc; i ++) {
        double d = ToDouble(args[i]);
        if (isnan (d))
            return _ejs_nan;

        if (d < current_min)
            current_min = d;
    }

    return NUMBER_TO_EJSVAL(current_min);
}

// ECMA262: 15.8.2.13
static ejsval
_ejs_Math_pow (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval x = _ejs_undefined;
    ejsval y = _ejs_undefined;
    if (argc > 0) x = args[0];
    if (argc > 1) y = args[1];

    double x_ = ToDouble(x);
    double y_ = ToDouble(y);
    if (isnan(x_) || isnan(y_))
        return _ejs_nan;

    return NUMBER_TO_EJSVAL(pow(x_, y_));
}

// ECMA262: 15.8.2.14
static ejsval
_ejs_Math_random (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    return NUMBER_TO_EJSVAL((double)rand() / RAND_MAX);
}

// ECMA262: 15.8.2.15
static ejsval
_ejs_Math_round (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval x = _ejs_undefined;
    if (argc > 0) x = args[0];

    double x_ = ToDouble(x);
    if (isnan(x_))
        return _ejs_nan;

    return NUMBER_TO_EJSVAL(round (x_));
}

// ECMA262: 15.8.2.16
static ejsval
_ejs_Math_sin (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval x = _ejs_undefined;
    if (argc > 0) x = args[0];

    double x_ = ToDouble(x);
    if (isnan(x_))
        return _ejs_nan;

    return NUMBER_TO_EJSVAL(sin (x_));
}

// ECMA262: 15.8.2.17
static ejsval
_ejs_Math_sqrt (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval x = _ejs_undefined;
    if (argc > 0) x = args[0];

    double x_ = ToDouble(x);
    if (isnan(x_))
        return _ejs_nan;

    return NUMBER_TO_EJSVAL(sqrt (x_));
}

// ECMA262: 15.8.2.18
static ejsval
_ejs_Math_tan (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval x = _ejs_undefined;
    if (argc > 0) x = args[0];

    double x_ = ToDouble(x);
    if (isnan(x_))
        return _ejs_nan;

    return NUMBER_TO_EJSVAL(tan (x_));
}

void
_ejs_math_init(ejsval global)
{
    _ejs_Math = _ejs_object_new (_ejs_Object_prototype, &_ejs_object_specops);
    _ejs_object_setprop (global, _ejs_atom_Math, _ejs_Math);

#define OBJ_METHOD(x) EJS_INSTALL_ATOM_FUNCTION(_ejs_Math, x, _ejs_Math_##x)

    OBJ_METHOD(abs);
    OBJ_METHOD(acos);
    OBJ_METHOD(asin);
    OBJ_METHOD(atan);
    OBJ_METHOD(atan2);
    OBJ_METHOD(ceil);
    OBJ_METHOD(cos);
    OBJ_METHOD(exp);
    OBJ_METHOD(floor);
    OBJ_METHOD(log);
    OBJ_METHOD(max);
    OBJ_METHOD(min);
    OBJ_METHOD(pow);
    OBJ_METHOD(random);
    OBJ_METHOD(round);
    OBJ_METHOD(sin);
    OBJ_METHOD(sqrt);
    OBJ_METHOD(tan);

#undef OBJ_METHOD

    _ejs_object_setprop (_ejs_Math, _ejs_atom_PI, NUMBER_TO_EJSVAL(M_PI));
}

