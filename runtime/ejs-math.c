/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include <assert.h>

#include "ejs-ops.h"
#include "ejs-value.h"
#include "ejs-function.h"
#include "ejs-math.h"

ejsval _ejs_Math;

/* 15.8.2.1 */
static ejsval
_ejs_Math_abs (ejsval env, ejsval _this, int argc, ejsval *args)
{
    NOT_IMPLEMENTED();
}

/* 15.8.2.2 */
static ejsval
_ejs_Math_acos (ejsval env, ejsval _this, int argc, ejsval *args)
{
    NOT_IMPLEMENTED();
}

/* 15.8.2.3 */
static ejsval
_ejs_Math_asin (ejsval env, ejsval _this, int argc, ejsval *args)
{
    NOT_IMPLEMENTED();
}

/* 15.8.2.4 */
static ejsval
_ejs_Math_atan (ejsval env, ejsval _this, int argc, ejsval *args)
{
    NOT_IMPLEMENTED();
}

/* 15.8.2.5 */
static ejsval
_ejs_Math_atan2 (ejsval env, ejsval _this, int argc, ejsval *args)
{
    NOT_IMPLEMENTED();
}

/* 15.8.2.6 */
static ejsval
_ejs_Math_ceil (ejsval env, ejsval _this, int argc, ejsval *args)
{
    NOT_IMPLEMENTED();
}

/* 15.8.2.7 */
static ejsval
_ejs_Math_cos (ejsval env, ejsval _this, int argc, ejsval *args)
{
    NOT_IMPLEMENTED();
}

/* 15.8.2.8 */
static ejsval
_ejs_Math_exp (ejsval env, ejsval _this, int argc, ejsval *args)
{
    NOT_IMPLEMENTED();
}

/* 15.8.2.9 */
static ejsval
_ejs_Math_floor (ejsval env, ejsval _this, int argc, ejsval *args)
{
    NOT_IMPLEMENTED();
}

/* 15.8.2.10 */
static ejsval
_ejs_Math_log (ejsval env, ejsval _this, int argc, ejsval *args)
{
    NOT_IMPLEMENTED();
}

/* 15.8.2.11 */
static ejsval
_ejs_Math_max (ejsval env, ejsval _this, int argc, ejsval *args)
{
    NOT_IMPLEMENTED();
}

/* 15.8.2.12 */
static ejsval
_ejs_Math_min (ejsval env, ejsval _this, int argc, ejsval *args)
{
    NOT_IMPLEMENTED();
}

/* 15.8.2.13 */
static ejsval
_ejs_Math_pow (ejsval env, ejsval _this, int argc, ejsval *args)
{
    NOT_IMPLEMENTED();
}

/* 15.8.2.14 */
static ejsval
_ejs_Math_random (ejsval env, ejsval _this, int argc, ejsval *args)
{
    NOT_IMPLEMENTED();
}

/* 15.8.2.15 */
static ejsval
_ejs_Math_round (ejsval env, ejsval _this, int argc, ejsval *args)
{
    NOT_IMPLEMENTED();
}

/* 15.8.2.16 */
static ejsval
_ejs_Math_sin (ejsval env, ejsval _this, int argc, ejsval *args)
{
    NOT_IMPLEMENTED();
}

/* 15.8.2.17 */
static ejsval
_ejs_Math_sqrt (ejsval env, ejsval _this, int argc, ejsval *args)
{
    NOT_IMPLEMENTED();
}

/* 15.8.2.18 */
static ejsval
_ejs_Math_tan (ejsval env, ejsval _this, int argc, ejsval *args)
{
    NOT_IMPLEMENTED();
}

void
_ejs_math_init(ejsval global)
{
    START_SHADOW_STACK_FRAME;

    ADD_STACK_ROOT(ejsval, tmpobj, _ejs_object_new (_ejs_Object_proto));
    _ejs_Math = tmpobj;

#define OBJ_METHOD(x) EJS_MACRO_START ADD_STACK_ROOT(ejsval, funcname, _ejs_string_new_utf8(#x)); ADD_STACK_ROOT(ejsval, tmpfunc, _ejs_function_new (_ejs_null, funcname, (EJSClosureFunc)_ejs_Math_##x)); _ejs_object_setprop (_ejs_Math, funcname, tmpfunc); EJS_MACRO_END

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

    _ejs_object_setprop_utf8 (global, "Math", _ejs_Math);

    END_SHADOW_STACK_FRAME;
}

