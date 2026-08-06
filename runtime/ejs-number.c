/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include <math.h>
#include <float.h>
#include <string.h>

#include "ejs-ops.h"
#include "ejs-bigint.h"
#include "ejs-value.h"
#include "ejs-number.h"
#include "ejs-function.h"
#include "ejs-string.h"
#include "ejs-error.h"
#include "ejs-proxy.h"
#include "ejs-symbol.h"

ejsval _ejs_Number EJSVAL_ALIGNMENT;
ejsval _ejs_Number_prototype EJSVAL_ALIGNMENT;

// ES2015, June 2015
// 20.1.1.1 Number ( [ value ] )

static EJS_NATIVE_FUNC(_ejs_Number_impl) {
    double n;

    // 1. If no arguments were passed to this function invocation, let n be +0.
    if (argc == 0)
        n = 0;
    // 2. Else, let prim be ToNumeric(value): the Number constructor is
    //    the one ToNumber caller that CONVERTS bigints instead of
    //    throwing (ES2020 Number(value) step 1.a).
    // 3. ReturnIfAbrupt(n).
    else {
        ejsval prim = _ejs_op_to_numeric(args[0]);
        n = EJSVAL_IS_BIGINT(prim) ? _ejs_bigint_to_double(prim) : EJSVAL_TO_NUMBER(prim);
    }

    // 4. If NewTarget is undefined, return n.
    if (EJSVAL_IS_UNDEFINED(newTarget)) return NUMBER_TO_EJSVAL(n);

    // 5. Let O be OrdinaryCreateFromConstructor(NewTarget, "%NumberPrototype%", «[[NumberData]]» ).
    // 6. ReturnIfAbrupt(O).
    ejsval O = OrdinaryCreateFromConstructor(newTarget, _ejs_Number_prototype, &_ejs_Number_specops);
    *_this = O;

    // 7. Set the value of O’s [[NumberData]] internal slot to n.
    ((EJSNumber*)EJSVAL_TO_OBJECT(O))->number = n;

    // 8. Return O.
    return O;
}

static double
thisNumberValue(ejsval value)
{
    if (EJSVAL_IS_NUMBER(value))
        return EJSVAL_TO_NUMBER(value);
    else if (EJSVAL_IS_NUMBER_OBJECT(value))
        return EJSVAL_TO_NUMBER_OBJECT(value)->number;
    else
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "'this' is not a number");
}

// ES6 20.1.3.6
// Number.prototype.toString ( [ radix ] )
static EJS_NATIVE_FUNC(_ejs_Number_prototype_toString) {
    ejsval radix = _ejs_undefined;
    if (argc > 0) radix = args[0];

    // 1. Let x be thisNumberValue(this value).
    // 2. ReturnIfAbrupt(x).
    double x = thisNumberValue(*_this);

    int64_t radixNumber;
    // 3. If radix is not present, let radixNumber be 10.
    // 4. Else if radix is undefined, let radixNumber be 10.
    if (EJSVAL_IS_UNDEFINED(radix))
        radixNumber = 10;
    // 5. Else let radixNumber be ToInteger(radix).
    // 6. ReturnIfAbrupt(radixNumber).
    else
        radixNumber = ToInteger(radix);
    // 7. If radixNumber < 2 or radixNumber > 36, throw a RangeError exception.
    if (radixNumber < 2 || radixNumber > 36)
        _ejs_throw_nativeerror_utf8 (EJS_RANGE_ERROR, "radix must be >=2 and <=36");

    // 8. If radixNumber = 10, return ToString(x).
    // 9. Return the String representation of this Number value using
    //    the radix specified by radixNumber. Letters a-z are used for
    //    digits with values 10 through 35. The precise algorithm is
    //    implementation-dependent, however the algorithm should be a
    //    generalization of that specified in 7.1.12.1.
    return NumberToString(x, (int)radixNumber);
}

ejsval
_ejs_number_to_string(ejsval num)
{
    return _ejs_Number_prototype_toString(_ejs_undefined, &num, 0, NULL, _ejs_undefined);
}


// ES2015, June 2015
// 20.1.3.7 Number.prototype.valueOf ( )
static EJS_NATIVE_FUNC(_ejs_Number_prototype_valueOf) {
    // 1. Let x be thisNumberValue(this value).
    double x = thisNumberValue(*_this);
    // 2. Return x.
    return NUMBER_TO_EJSVAL(x);
}

// ECMA262: 20.1.3.3 Number.prototype.toFixed ( fractionDigits ) 
static EJS_NATIVE_FUNC(_ejs_Number_prototype_toFixed) {
    EJS_NOT_IMPLEMENTED();
#if notyet
    ejsval fractionDigits = _ejs_undefined;
    if (argc > 0) fractionDigits = args[0];

    // 1. Let x be thisNumberValue(this value). 
    // 2. ReturnIfAbrupt(x). 
    double x = thisNumberValue(*_this);

    // 3. Let f be ToInteger(fractionDigits). (If fractionDigits is undefined, this step produces the value 0). 
    // 4. ReturnIfAbrupt(f).
    int64_t f = ToInteger(fractionDigits);
    
    // 5. If f < 0 or f > 20, throw a RangeError exception.
    if (f < 0 || f > 20)
        _ejs_throw_nativeerror_utf8 (EJS_RANGE_ERROR, "fractionDigits must be in the range of 0 <= f < 21");

    // 6. If x is NaN, return the String "NaN". 
    if (isnan(x))
        return _ejs_atom_NaN;

    // 7. Let s be the empty String.
    ejsval s = _ejs_atom_empty;

    // 8. If x < 0, then 
    if (x < 0) {
        //    a. Let s be "-". 
        s = _ejs_atom_minus;
        //    b. Let x = –x. 
        x -= x;
    }
    ejsval m;
    // 9. If x >= 10^21, then 
    if (x >= 1e21) {
        //    a. Let m = ToString(x). 
        m = ToString(x);
    }
    // 10. Else x < 10^21
    else {
        //     a. Let n be an integer for which the exact mathematical value of n / 10^f – x is as close to zero as 
        //         possible. If there are two such n, pick the larger n. 

        //     b. If n = 0, let m be the String "0". Otherwise, let m be the String consisting of the digits of the 
        //        decimal representation of n (in order, with no leading zeroes). 
        if (n == 0)
            m = _ejs_atom_0;
        else
            EJS_NOT_IMPLEMENTED(); // XXX let m be the String consisting of the digits of the decimal representation of n (in order, with no leading zeroes). 
        //     c. If f != 0, then 
        if (f != 0) {
            //        i. Let k be the number of elements in m. 
            //        ii. If k <= f, then 
            if (k <= f) {
                //            1. Let z be the String consisting of f+1-k occurrences of the code unit 0x0030. 
                ejsval z = zeros[f+1-k];
                //            2. Let m be the concatenation of Strings z and m. 
                m = _ejs_string_concat (z, m);
                //            3. Let k = f + 1. 
                k = f + 1;
            }
            //        iii. Let a be the first k–f elements of m, and let b be the remaining f elements of m. 
            ejsval a = _ejs_string_new_substring(m, 0, k-f);
            ejsval b = _ejs_string_new_substring(m, k-f, f);
            //        iv. Let m be the concatenation of the three Strings a, ".", and b. 
            m = _ejs_string_concatv (a, _ejs_atom_dot, b, _ejs_null);
        }
    }
    // 11. Return the concatenation of the Strings s and m. 
    return _ejs_string_concat(s, m);
#endif
}

// ECMA262: 20.1.3.5 Number.prototype.toPrecision ( precision )
static EJS_NATIVE_FUNC(_ejs_Number_prototype_toPrecision) {
    ejsval precision = _ejs_undefined;
    if (argc > 0) precision = args[0];

    // 1. Let x be thisNumberValue(this value).
    double x = thisNumberValue(*_this);

    // 2. If precision is undefined, return ToString(x).
    if (EJSVAL_IS_UNDEFINED(precision))
        return ToString(NUMBER_TO_EJSVAL(x));

    // 3. Let p be ToIntegerOrInfinity(precision).
    double pd = ToDouble(precision);
    if (isnan(pd)) pd = 0;
    pd = trunc(pd);

    // 4. If x is not finite, return Number::toString(x).
    if (isnan(x))
        return _ejs_atom_NaN;
    if (!isfinite(x))
        return x < 0 ? _ejs_atom_NegativeInfinity : _ejs_atom_Infinity;

    // 5. If p < 1 or p > 100, throw a RangeError exception.
    if (pd < 1 || pd > 100)
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "toPrecision() argument must be between 1 and 100");
    int prec = (int)pd;

    // double-conversion's EcmaScriptConverter implements the exact ES
    // ToPrecision digit generation (correct decimal rounding + the
    // exponent-form thresholds)
    extern void _ejs_dtoa_precision(double d, int precision, char* buf, size_t buf_size);
    char out[128];
    _ejs_dtoa_precision(x, prec, out, sizeof(out));
    return _ejs_string_new_utf8(out);
}

static EJS_NATIVE_FUNC(_ejs_Number_isFinite) {
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

static EJS_NATIVE_FUNC(_ejs_Number_toInteger) {
    ejsval argument = _ejs_undefined;
    if (argc > 0) argument = args[0];

    double number = ToDouble(argument);
    if (isnan(number))
        return NUMBER_TO_EJSVAL(+0);
    
    if (number == 0 || fpclassify(number) == FP_INFINITE)
        return NUMBER_TO_EJSVAL(number);

    return NUMBER_TO_EJSVAL ((number < 0 ? -1 : 1) * floor(fabs(number)));
}

static EJS_NATIVE_FUNC(_ejs_Number_isInteger) {
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
    // 4. If integer is not equal to number, return false.
    // trunc, not an integer type: number_ can exceed any C integer range
    if (trunc(number_) != number_)
        return _ejs_false;

    // 5. Otherwise, return true.
    return _ejs_true;
}

static EJS_NATIVE_FUNC(_ejs_Number_isSafeInteger) {
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
    // 4. If integer is not equal to number, return false.
    // trunc, not an integer type: number_ can exceed any C integer range
    if (trunc(number_) != number_)
        return _ejs_false;

    // 5. If abs(integer) ≤ 2^53-1, then return true.
    if (fabs(number_) <= (double)EJS_MAX_SAFE_INTEGER)
        return _ejs_true;

    // 6. Otherwise, return false.
    return _ejs_false;
}

static EJS_NATIVE_FUNC(_ejs_Number_isNaN) {
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
    _ejs_Number = _ejs_function_new_without_proto (_ejs_null, _ejs_atom_Number, _ejs_Number_impl);
    _ejs_object_setprop (global, _ejs_atom_Number, _ejs_Number);

    _ejs_gc_add_root (&_ejs_Number_prototype);
    EJSNumber* prototype = (EJSNumber*)_ejs_gc_new(EJSNumber);
    _ejs_init_object ((EJSObject*)prototype, _ejs_Object_prototype, &_ejs_Number_specops);
    prototype->number = 0;
    _ejs_Number_prototype = OBJECT_TO_EJSVAL(prototype);

    _ejs_object_define_value_property (_ejs_Number, _ejs_atom_prototype, _ejs_Number_prototype, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_CONFIGURABLE | EJS_PROP_NOT_WRITABLE);

#define PROTO_METHOD(x) EJS_INSTALL_ATOM_FUNCTION(_ejs_Number_prototype, x, _ejs_Number_prototype_##x)
#define OBJ_METHOD(x) EJS_INSTALL_ATOM_FUNCTION(_ejs_Number, x, _ejs_Number_##x)

    PROTO_METHOD(valueOf);
    PROTO_METHOD(toString);
    PROTO_METHOD(toFixed);
    PROTO_METHOD(toPrecision);
    // ES6
    OBJ_METHOD(isFinite);
    OBJ_METHOD(isInteger);
    OBJ_METHOD(isSafeInteger);
    OBJ_METHOD(isNaN);
    OBJ_METHOD(toInteger);

#define OBJ_CONST(n,v) _ejs_object_define_value_property (_ejs_Number, _ejs_atom_##n, NUMBER_TO_EJSVAL(v), EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_CONFIGURABLE | EJS_PROP_NOT_WRITABLE)
    OBJ_CONST(EPSILON, nextafter(1, INFINITY)-1);
    OBJ_CONST(MAX_SAFE_INTEGER, EJS_MAX_SAFE_INTEGER);
    OBJ_CONST(MIN_SAFE_INTEGER, EJS_MIN_SAFE_INTEGER);
    OBJ_CONST(MAX_VALUE, DBL_MAX);
    OBJ_CONST(MIN_VALUE, DBL_MIN);
    OBJ_CONST(NaN, nan("7734"));
    OBJ_CONST(NEGATIVE_INFINITY, -INFINITY);
    OBJ_CONST(POSITIVE_INFINITY, INFINITY);
#undef OBJ_CONST

#undef PROTO_METHOD
}

static EJSObject*
_ejs_number_specop_allocate()
{
    return (EJSObject*)_ejs_gc_new (EJSNumber);
}

EJS_DEFINE_CLASS(Number,
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
                 _ejs_number_specop_allocate,
                 OP_INHERIT, // [[Finalize]]
                 OP_INHERIT  // [[Scan]]
                 )
