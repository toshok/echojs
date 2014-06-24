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
#include "ejs-error.h"
#include "ejs-proxy.h"
#include "ejs-symbol.h"

ejsval _ejs_Number EJSVAL_ALIGNMENT;
ejsval _ejs_Number_prototype EJSVAL_ALIGNMENT;

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

// ECMA262: 20.1.3.3 Number.prototype.toFixed ( fractionDigits ) 
static ejsval
_ejs_Number_prototype_toFixed (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    EJS_NOT_IMPLEMENTED();
#if notyet
    ejsval fractionDigits = _ejs_undefined;
    if (argc > 0) fractionDigits = args[0];

    // 1. Let x be thisNumberValue(this value). 
    // 2. ReturnIfAbrupt(x). 
    double x = thisNumberValue(_this);

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
static ejsval
_ejs_Number_prototype_toPrecision (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    EJS_NOT_IMPLEMENTED();
#if notyet
    ejsval precision = _ejs_undefined;
    if (argc > 0) precision = args[0];

    // 1. Let x be thisNumberValue(this value). 
    // 2. ReturnIfAbrupt(x). 
    double x = thisNumberValue(_this);

    // 3. If precision is undefined, return ToString(x). 
    if (EJSVAL_IS_UNDEFINED(precision))
        return ToString(x);

    // 4. Let p be ToInteger(precision). 
    // 5. ReturnIfAbrupt(p). 
    int64_t = ToInteger(precision);

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
        x = -x;
    }
    // 9. If x = +∞, then 
    int classified = fpclassify(x);
    if (classified == FP_INFINITE) {
        //    a. Return the concatenation of the Strings s and "Infinity". 
        return _ejs_string_concat (s, _ejs_atom_Infinity);
    }

    // 10. If p < 1 or p > 21, throw a RangeError exception. 
    if (p < 1 || p > 21) {
        _ejs_throw_nativeerror_utf8 (EJS_RANGE_ERROR, "precision must be in the range of 1 <= p <= 21");
    }

    double e;

    // 11. If x = 0, then 
    if (x == 0) {
        //     a. Let m be the String consisting of p occurrences of the code unit 0x0030 (the Unicode character ‘0’). 
        m = zeros[p];

        //     b. Let e = 0. 
        e = 0;
    }
    // 12. Else x != 0, 
    else {
        //     a. Let e and n be integers such that 10^(p–1) <= n < 10^p and for which the exact mathematical value of n * 10^(e–p+1) – x is as close to zero as possible.
        //        If there are two such sets of e and n, pick the e and n for which n * 10^(e–p+1) is larger. 
        //     b. Let m be the String consisting of the digits of the decimal representation of n (in order, with no leading zeroes). 
        //     c. If e < –6 or e >= p, then 
        //        i. Assert: e != 0 
        //        ii. Let a be the first element of m, and let b be the remaining p–1 elements of m. 
        //        iii. Let m be the concatenation of the three Strings a, ".", and b. 
        //        iv. If e > 0, then 
        if (e > 0)
            //           1. Let c = "+". 
            c = "+";
        //        v. Else e < 0, 
        else {
            //           1. Let c = "-". 
            c = "-";
            //           2. Let e = –e. 
            e = -e;
        }
        //        vi. Let d be the String consisting of the digits of the decimal representation of e (in order, with no leading zeroes). 
        //        vii. Return the concatenation of the five Strings s, m, "e", c, and d. 
        return _ejs_string_concatv (s, m, _ejs_string_new_utf8("e"), c, d, _ejs_null);
    }
    // 13. If e = p–1, then return the concatenation of the Strings s and m. 
    if (e == p - 1)
        return _ejs_string_concat(s, m);
    // 14. If e >= 0, then 
    if (e >= 0) {
        //     a. Let m be the concatenation of the first e+1 elements of m, the code unit 0x002E (Unicode character ‘.’), and the remaining p– (e+1) elements of m. 
    }
    // 15. Else e < 0, 
    else {
        //     a. Let m be the concatenation of the String "0.", –(e+1) occurrences of code unit 0x0030 (the Unicode character ‘0’), and the String m. 
        int z = -(e+1);
        ejsval zstr;
        if (z > 0 && z <= 21)
            zstr = zeros[z];
        else
            EJS_NOT_IMPLEMENTED(); // create a string z 0's long

        m = _ejs_string_concatv(zerodot, zstr, m, _ejs_null);
    }
    // 16. Return the concatenation of the Strings s and m.
    return _ejs_string_concat(s, m);
#endif
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

static ejsval
_ejs_Number_create (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    // 1. Let F be the this value. 
    ejsval F = _this;

    if (!EJSVAL_IS_CONSTRUCTOR(F)) 
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "'this' in Number[Symbol.create] is not a constructor");

    EJSObject* F_ = EJSVAL_TO_OBJECT(F);

    // 2. Let obj be the result of calling OrdinaryCreateFromConstructor(F, "%NumberPrototype%", ([[NumberData]]) ). 
    ejsval proto = OP(F_,Get)(F, _ejs_atom_prototype, F);
    if (EJSVAL_IS_UNDEFINED(proto))
        proto = _ejs_Number_prototype;

    EJSObject* obj = (EJSObject*)_ejs_gc_new (EJSNumber);
    _ejs_init_object (obj, proto, &_ejs_Number_specops);
    return OBJECT_TO_EJSVAL(obj);
}


void
_ejs_number_init(ejsval global)
{
    _ejs_Number = _ejs_function_new_without_proto (_ejs_null, _ejs_atom_Number, (EJSClosureFunc)_ejs_Number_impl);
    _ejs_object_setprop (global, _ejs_atom_Number, _ejs_Number);

    _ejs_gc_add_root (&_ejs_Number_prototype);
    EJSNumber* prototype = (EJSNumber*)_ejs_gc_new(EJSNumber);
    _ejs_init_object ((EJSObject*)prototype, _ejs_Object_prototype, &_ejs_Number_specops);
    prototype->number = 0;
    _ejs_Number_prototype = OBJECT_TO_EJSVAL(prototype);

    _ejs_object_setprop (_ejs_Number,       _ejs_atom_prototype,  _ejs_Number_prototype);

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

    _ejs_object_setprop (_ejs_Number, _ejs_atom_EPSILON, NUMBER_TO_EJSVAL(nextafter(1, INFINITY)));
    _ejs_object_setprop (_ejs_Number, _ejs_atom_MAX_SAFE_INTEGER, NUMBER_TO_EJSVAL(EJS_MAX_SAFE_INTEGER));
    _ejs_object_setprop (_ejs_Number, _ejs_atom_MIN_SAFE_INTEGER, NUMBER_TO_EJSVAL(EJS_MIN_SAFE_INTEGER));
    _ejs_object_setprop (_ejs_Number, _ejs_atom_MAX_VALUE, NUMBER_TO_EJSVAL(DBL_MAX));
    _ejs_object_setprop (_ejs_Number, _ejs_atom_MIN_VALUE, NUMBER_TO_EJSVAL(DBL_MIN));
    _ejs_object_setprop (_ejs_Number, _ejs_atom_NaN, NUMBER_TO_EJSVAL(nan("7734")));
    _ejs_object_setprop (_ejs_Number, _ejs_atom_NEGATIVE_INFINITY, NUMBER_TO_EJSVAL(-INFINITY));
    _ejs_object_setprop (_ejs_Number, _ejs_atom_POSITIVE_INFINITY, NUMBER_TO_EJSVAL(INFINITY));


    EJS_INSTALL_SYMBOL_FUNCTION_FLAGS (_ejs_Number, create, _ejs_Number_create, EJS_PROP_NOT_ENUMERABLE);

#undef PROTO_METHOD
}

EJS_DEFINE_INHERIT_ALL_CLASS(Number)
