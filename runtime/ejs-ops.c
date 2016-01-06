/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "ejs.h"
#include "ejs-exception.h"
#include "ejs-value.h"
#include "ejs-date.h"
#include "ejs-function.h"
#include "ejs-number.h"
#include "ejs-object.h"
#include "ejs-string.h"
#include "ejs-symbol.h"
#include "ejs-boolean.h"
#include "ejs-ops.h"
#include "ejs-error.h"
#include "ejs-array.h"
#include "ejs-proxy.h"

ejsval _ejs_isNaN EJSVAL_ALIGNMENT;
ejsval _ejs_isFinite EJSVAL_ALIGNMENT;
ejsval _ejs_parseInt EJSVAL_ALIGNMENT;
ejsval _ejs_parseFloat EJSVAL_ALIGNMENT;

typedef enum {
    TO_PRIM_HINT_DEFAULT,
    TO_PRIM_HINT_STRING,
    TO_PRIM_HINT_NUMBER
} ToPrimitiveHint;
ejsval
ToPrimitive(ejsval inputargument, ToPrimitiveHint PreferredType);

static const size_t UINT32_CHAR_BUFFER_LENGTH = sizeof("4294967295") - 1;

static char *
IntToCString(char *cbuf, jsint i, jsint base)
{
    jsuint u = (i < 0) ? -i : i;
    char *cp = cbuf + UINT32_CHAR_BUFFER_LENGTH;

    *cp = '\0';

    /* Build the string from behind. */
    switch (base) {
    case 10: {
        // cp = BackfillIndexInCharBuffer(u, cp);
        uint32_t index = u;
        char *end = cp;
        do {
            uint32_t next = index / 10, digit = index % 10;
            *--end = '0' + digit;
            index = next;
        } while (index > 0);
        return end;
      break;
    }
    case 16:
      do {
          jsuint newu = u / 16;
          *--cp = "0123456789abcdef"[u - newu * 16];
          u = newu;
      } while (u != 0);
      break;
    default:
      EJS_ASSERT(base >= 2 && base <= 36);
      do {
          jsuint newu = u / base;
          *--cp = "0123456789abcdefghijklmnopqrstuvwxyz"[u - newu * base];
          u = newu;
      } while (u != 0);
      break;
    }
    if (i < 0)
        *--cp = '-';

    return cp;
}

static jschar *
IntToUCS2(jschar *cbuf, jsint i, jsint base)
{
    jsuint u = (i < 0) ? -i : i;
    jschar *cp = cbuf + UINT32_CHAR_BUFFER_LENGTH;

    *cp = '\0';

    /* Build the string from behind. */
    switch (base) {
    case 10: {
        // cp = BackfillIndexInCharBuffer(u, cp);
        uint32_t index = u;
        jschar *end = cp;
        do {
            uint32_t next = index / 10, digit = index % 10;
            *--end = '0' + digit;
            index = next;
        } while (index > 0);
        cp = end;
        break;
    }
    case 16:
      do {
          jsuint newu = u / 16;
          *--cp = "0123456789abcdef"[u - newu * 16];
          u = newu;
      } while (u != 0);
      break;
    default:
      EJS_ASSERT(base >= 2 && base <= 36);
      do {
          jsuint newu = u / base;
          *--cp = "0123456789abcdefghijklmnopqrstuvwxyz"[u - newu * base];
          u = newu;
      } while (u != 0);
      break;
    }
    if (i < 0)
        *--cp = '-';

    return cp;
}

static const ejsval* builtin_numbers_atoms[] = {
    &_ejs_atom_0,&_ejs_atom_1,&_ejs_atom_2,&_ejs_atom_3,&_ejs_atom_4,&_ejs_atom_5,&_ejs_atom_6,&_ejs_atom_7,&_ejs_atom_8,&_ejs_atom_9,&_ejs_atom_10,
    &_ejs_atom_11,&_ejs_atom_12,&_ejs_atom_13,&_ejs_atom_14,&_ejs_atom_15,&_ejs_atom_16,&_ejs_atom_17,&_ejs_atom_18,&_ejs_atom_19,&_ejs_atom_20,&_ejs_atom_21,
    &_ejs_atom_22,&_ejs_atom_23,&_ejs_atom_24,&_ejs_atom_25,&_ejs_atom_26,&_ejs_atom_27,&_ejs_atom_28,&_ejs_atom_29,&_ejs_atom_30,&_ejs_atom_31,&_ejs_atom_32,
    &_ejs_atom_33,&_ejs_atom_34,&_ejs_atom_35,&_ejs_atom_36,&_ejs_atom_37,&_ejs_atom_38,&_ejs_atom_39,&_ejs_atom_40,&_ejs_atom_41,&_ejs_atom_42,&_ejs_atom_43,
    &_ejs_atom_44,&_ejs_atom_45,&_ejs_atom_46,&_ejs_atom_47,&_ejs_atom_48,&_ejs_atom_49,&_ejs_atom_50,&_ejs_atom_51,&_ejs_atom_52,&_ejs_atom_53,&_ejs_atom_54,
    &_ejs_atom_55,&_ejs_atom_56,&_ejs_atom_57,&_ejs_atom_58,&_ejs_atom_59,&_ejs_atom_60,&_ejs_atom_61,&_ejs_atom_62,&_ejs_atom_63,&_ejs_atom_64,&_ejs_atom_65,
    &_ejs_atom_66,&_ejs_atom_67,&_ejs_atom_68,&_ejs_atom_69,&_ejs_atom_70,&_ejs_atom_71,&_ejs_atom_72,&_ejs_atom_73,&_ejs_atom_74,&_ejs_atom_75,&_ejs_atom_76,
    &_ejs_atom_77,&_ejs_atom_78,&_ejs_atom_79,&_ejs_atom_80,&_ejs_atom_81,&_ejs_atom_82,&_ejs_atom_83,&_ejs_atom_84,&_ejs_atom_85,&_ejs_atom_86,&_ejs_atom_87,
    &_ejs_atom_88,&_ejs_atom_89,&_ejs_atom_90,&_ejs_atom_91,&_ejs_atom_92,&_ejs_atom_93,&_ejs_atom_94,&_ejs_atom_95,&_ejs_atom_96,&_ejs_atom_97,&_ejs_atom_98,
    &_ejs_atom_99,&_ejs_atom_100,&_ejs_atom_101,&_ejs_atom_102,&_ejs_atom_103,&_ejs_atom_104,&_ejs_atom_105,&_ejs_atom_106,&_ejs_atom_107,&_ejs_atom_108,&_ejs_atom_109,
    &_ejs_atom_110,&_ejs_atom_111,&_ejs_atom_112,&_ejs_atom_113,&_ejs_atom_114,&_ejs_atom_115,&_ejs_atom_116,&_ejs_atom_117,&_ejs_atom_118,&_ejs_atom_119,&_ejs_atom_120,
    &_ejs_atom_121,&_ejs_atom_122,&_ejs_atom_123,&_ejs_atom_124,&_ejs_atom_125,&_ejs_atom_126,&_ejs_atom_127,&_ejs_atom_128,&_ejs_atom_129,&_ejs_atom_130,&_ejs_atom_131,
    &_ejs_atom_132,&_ejs_atom_133,&_ejs_atom_134,&_ejs_atom_135,&_ejs_atom_136,&_ejs_atom_137,&_ejs_atom_138,&_ejs_atom_139,&_ejs_atom_140,&_ejs_atom_141,&_ejs_atom_142,
    &_ejs_atom_143,&_ejs_atom_144,&_ejs_atom_145,&_ejs_atom_146,&_ejs_atom_147,&_ejs_atom_148,&_ejs_atom_149,&_ejs_atom_150,&_ejs_atom_151,&_ejs_atom_152,&_ejs_atom_153,
    &_ejs_atom_154,&_ejs_atom_155,&_ejs_atom_156,&_ejs_atom_157,&_ejs_atom_158,&_ejs_atom_159,&_ejs_atom_160,&_ejs_atom_161,&_ejs_atom_162,&_ejs_atom_163,&_ejs_atom_164,
    &_ejs_atom_165,&_ejs_atom_166,&_ejs_atom_167,&_ejs_atom_168,&_ejs_atom_169,&_ejs_atom_170,&_ejs_atom_171,&_ejs_atom_172,&_ejs_atom_173,&_ejs_atom_174,&_ejs_atom_175,
    &_ejs_atom_176,&_ejs_atom_177,&_ejs_atom_178,&_ejs_atom_179,&_ejs_atom_180,&_ejs_atom_181,&_ejs_atom_182,&_ejs_atom_183,&_ejs_atom_184,&_ejs_atom_185,&_ejs_atom_186,
    &_ejs_atom_187,&_ejs_atom_188,&_ejs_atom_189,&_ejs_atom_190,&_ejs_atom_191,&_ejs_atom_192,&_ejs_atom_193,&_ejs_atom_194,&_ejs_atom_195,&_ejs_atom_196,&_ejs_atom_197,
    &_ejs_atom_198,&_ejs_atom_199,&_ejs_atom_200
};

ejsval NumberToString(double d, int base)
{
    int32_t i;
    if (EJSDOUBLE_IS_INT32(d, &i)) {
        if (i >=0 && i <= 200 && base == 10)
            return *builtin_numbers_atoms[i];
        jschar int_buf[UINT32_CHAR_BUFFER_LENGTH+1];
        jschar *cp = IntToUCS2(int_buf, i, base);
        return _ejs_string_new_ucs2 (cp);
    }

    char num_buf[256];
    int classified = fpclassify(d);
    if (classified == FP_INFINITE) {
        if (d < 0)
            return _ejs_atom_NegativeInfinity;
        else
            return _ejs_atom_Infinity;
    }
    else if (classified == FP_NAN) {
        return _ejs_atom_NaN;
    }
    else {
        // XXX we need to take @base into account
        _ejs_dtoa(d, num_buf, sizeof(num_buf));
    }
    return _ejs_string_new_utf8 (num_buf);
}

// returns an EJSPrimString*.
// maybe we could change it to return a char* to match ToDouble?  that way string concat wouldn't create
// temporary strings for non-PrimString objects only to throw them away after concatenation?
ejsval ToString(ejsval exp)
{
    if (EJSVAL_IS_MAGIC_IMPL(exp)) {
        // holes in dense arrays end up here
        return _ejs_atom_empty;
    }
    else if (EJSVAL_IS_NULL(exp))
        return _ejs_atom_null;
    else if (EJSVAL_IS_UNDEFINED(exp))
        return _ejs_atom_undefined;
    else if (EJSVAL_IS_BOOLEAN(exp)) 
        return EJSVAL_TO_BOOLEAN(exp) ? _ejs_atom_true : _ejs_atom_false;
    else if (EJSVAL_IS_NUMBER(exp))
        return NumberToString(EJSVAL_TO_NUMBER(exp), 10);
    else if (EJSVAL_IS_STRING(exp))
        return exp;
    else if (EJSVAL_IS_OBJECT(exp)) {
        ejsval prim = ToPrimitive(exp, TO_PRIM_HINT_STRING);
        return ToString(prim);
    }
    else
        EJS_NOT_IMPLEMENTED();
}

ejsval ToNumber(ejsval exp)
{
    if (EJSVAL_IS_NUMBER(exp))
        return exp;
    else if (EJSVAL_IS_BOOLEAN(exp))
        return EJSVAL_TO_BOOLEAN(exp) ? _ejs_one : _ejs_zero;
    else if (EJSVAL_IS_STRING(exp)) {
        char num_utf8_buf[128];
        memset(num_utf8_buf, 0, sizeof(num_utf8_buf));
        char* num_utf8 = ucs2_to_utf8_buf(EJSVAL_TO_FLAT_STRING(exp), num_utf8_buf, sizeof(num_utf8_buf));
        if (num_utf8 == NULL) {
            num_utf8 = ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(exp));
        }
        char *endptr;
        double d = strtod(num_utf8, &endptr);
        if (*endptr != '\0') {
            if (num_utf8 != num_utf8_buf) free (num_utf8);
            return _ejs_nan;
        }
        ejsval rv = NUMBER_TO_EJSVAL(d); // XXX NaN
        if (num_utf8 != num_utf8_buf) free (num_utf8);
        return rv;
    }
    else if (EJSVAL_IS_UNDEFINED(exp))
        return _ejs_nan;
    else if (EJSVAL_IS_OBJECT(exp)) {
        // 1. Let primValue be ToPrimitive(argument, hint Number).
        ejsval primValue = ToPrimitive(exp, TO_PRIM_HINT_NUMBER);
        // 2. Return ToNumber(primValue).
        return ToNumber(primValue);
#if oldcode
        if (EJSVAL_IS_DATE(exp)) {
            return NUMBER_TO_EJSVAL(_ejs_date_get_time ((EJSDate*)EJSVAL_TO_OBJECT(exp)));
        }
        else if (EJSVAL_IS_ARRAY(exp)) {
            int len = EJS_ARRAY_LEN(exp);
            if (len == 0) return _ejs_zero;
            else if (len > 1) return _ejs_nan;
            else {
                // XXX we need to support sparse arrays here too
                EJS_ASSERT (EJSVAL_IS_DENSE_ARRAY(exp));
                return ToNumber(EJS_DENSE_ARRAY_ELEMENTS(exp)[0]);
            }
        }
        else
            return _ejs_nan;
#endif
    }
    else
        EJS_NOT_IMPLEMENTED();
}

double ToDouble(ejsval exp)
{
    return EJSVAL_TO_NUMBER(ToNumber(exp));
}

int64_t ToInteger(ejsval exp)
{
    // XXX sorely lacking
    /* 1. Let number be the result of calling ToNumber on the input argument. */
    double number = ToDouble(exp);
    /* 2. If number is NaN, return +0. */
    if (isnan(number))
        return 0;
    /* 3. If number is +0, -0, +inf, or -inf, return number. */
    int classified = fpclassify(number);
    if (classified == FP_ZERO || classified == FP_INFINITE)
        return number;
    /* 4. Return the result of computing sign(number) * floor(abs(number) */
    int sign = number < 0 ? -1 : 1;
    return (int64_t)(sign * floor(fabs(number)));
}

// ES6: 7.1.5
// ToInt32 ( argument ) 
int32_t ToInt32(ejsval argument)
{
    // 1. Let number be ToNumber(argument).
    // 2. ReturnIfAbrupt(number).
    double number = ToDouble(argument);
    // 3. If number is NaN, +0, −0, +∞, or −∞, return +0.
    if (isnan(number)) return 0;
    int classified = fpclassify(number);
    if (classified == FP_ZERO || classified == FP_INFINITE)
        return 0;

    // 4. Let int be sign(number) * floor(abs(number)).
    int sign = number < 0 ? -1 : 1;
    int64_t i = (int64_t)(sign * floor(fabs(number)));

    // 5. Let int32bit be int modulo 2^32.
    int64_t int32bit = i % (1LL<<32);

    // 6. If int32bit ≥ 2^31, return int32bit − 2^32, otherwise return int32bit.
    if (int32bit >= 1LL<<31)
        return int32bit - (1LL<<32);
    else
        return int32bit;
}

// ECMA262 7.1.15
// ToLength ( argument )
int64_t ToLength(ejsval exp)
{
    // 1. ReturnIfAbrupt(argument).
    // 2. Let len be ToInteger(argument).
    // 3. ReturnIfAbrupt(len).
    int64_t len = ToInteger(exp);

    // 4. If len <= +0, then return +0.
    if (len <= 0)
        return 0;

    // 5. Return min(len, 2^53-1)
    return MIN(len, EJS_MAX_SAFE_INTEGER);
}

uint32_t ToUint32(ejsval exp)
{
    // XXX sorely lacking
    return (uint32_t)ToDouble(exp);
}

uint16_t ToUint16(ejsval exp)
{
    // XXX sorely lacking
    return (uint16_t)ToDouble(exp);
}

ejsval ToObject(ejsval exp)
{
    if (EJSVAL_IS_BOOLEAN(exp)) {
        ejsval new_boolean;
        _ejs_construct_closure (_ejs_Boolean, &new_boolean, 1, &exp, _ejs_Boolean);
        return new_boolean;
    }
    else if (EJSVAL_IS_NUMBER(exp)) {
        ejsval new_number;
        _ejs_construct_closure (_ejs_Number, &new_number, 1, &exp, _ejs_Number);
        return new_number;
    }
    else if (EJSVAL_IS_STRING(exp)) {
        ejsval new_str;
        _ejs_construct_closure (_ejs_String, &new_str, 1, &exp, _ejs_String);
        return new_str;
    }
    else if (EJSVAL_IS_UNDEFINED(exp)) {
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "1"); // XXX
    }
    else if (EJSVAL_IS_NULL(exp)) {
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "2"); // XXX
    }
    else if (EJSVAL_IS_OBJECT(exp))
        return exp;
    else
        EJS_NOT_IMPLEMENTED();
}

EJSBool ToEJSBool(ejsval exp)
{
    if (EJSVAL_IS_NULL(exp) || EJSVAL_IS_UNDEFINED(exp))
        return EJS_FALSE;
    else if (EJSVAL_IS_BOOLEAN(exp))
        return EJSVAL_TO_BOOLEAN(exp);
    else if (EJSVAL_IS_NUMBER(exp))
        return EJSVAL_TO_NUMBER(exp) != 0;
    else if (EJSVAL_IS_STRING(exp))
        return EJSVAL_TO_STRLEN(exp) != 0;
    else if (EJSVAL_IS_OBJECT(exp))
        return EJS_TRUE;
    else
        EJS_NOT_IMPLEMENTED();
}

ejsval ToBoolean(ejsval exp)
{
    return BOOLEAN_TO_EJSVAL(ToEJSBool(exp));
}

static ejsval
OrdinaryToPrimitive(ejsval O, ToPrimitiveHint hint)
{
    // 1. Assert: Type(O) is Object 
    // 2. Assert: Type(hint) is String and its value is either "string" or "number". 
    ejsval stringMethodNames[] = { _ejs_atom_toString, _ejs_atom_valueOf };
    ejsval numberMethodNames[] = { _ejs_atom_valueOf, _ejs_atom_toString };
    ejsval *methodNames;

    // 3. If hint is "string", then 
    if (hint == TO_PRIM_HINT_STRING) {
        //    a. Let methodNames be the List ( "toString", "valueOf"). 
        methodNames = stringMethodNames;
    }
    // 4. Else, 
    else {
        //    a. Let methodNames be the List ( "valueOf", "toString"). 
        methodNames = numberMethodNames;
    }
    // 5. For each name in methodNames in List order, do 
    for (int i = 0; i < 2; i ++) {
        ejsval name = methodNames[i];
        //    a. Let method be Get(O, name). 
        //    b. ReturnIfAbrupt(method). 
        ejsval method = Get(O, name);
        //    c. If IsCallable(method) is true then, 
        if (IsCallable(method)) {
            //       i. Let result be the result of calling the [[Call]] internal method of method, with O as thisArgument and an empty List as argumentsList. 
            //       ii. ReturnIfAbrupt(result). 
            ejsval result = _ejs_invoke_closure(method, &O, 0, NULL, _ejs_undefined);
            //       iii. If Type(result) is not Object, then return result. 
            if (!EJSVAL_IS_OBJECT(result))
                return result;
        }
    }
    // 6. Throw a TypeError exception. 
    _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "couldn't convert object to primitive");
}

// ECMA262 7.1.1
// ToPrimitive ( input [, PreferredType] )
ejsval
ToPrimitive(ejsval inputargument, ToPrimitiveHint PreferredType)
{
    if (!EJSVAL_IS_OBJECT(inputargument))
        return inputargument;

    ejsval hint;
    switch (PreferredType) {
        // 1. If PreferredType was not passed, let hint be "default". 
    case TO_PRIM_HINT_DEFAULT: hint = _ejs_atom_default; break;
        // 2. Else if PreferredType is hint String, let hint be "string". 
    case TO_PRIM_HINT_STRING: hint = _ejs_atom_string; break;
        // 3. Else PreferredType is hint Number, let hint be "number". 
    case TO_PRIM_HINT_NUMBER: hint = _ejs_atom_number; break;
    }

    // 4. Let exoticToPrim be GetMethod(inputargument, @@toPrimitive). 
    // 5. ReturnIfAbrupt(exoticToPrim). 
    ejsval exoticToPrim = GetMethod(inputargument, _ejs_Symbol_toPrimitive);

    // 6. If exoticToPrim is not undefined, then 
    if (!EJSVAL_IS_UNDEFINED(exoticToPrim)) {
        // a. Let result be the result of calling the [[Call]] internal method of exoticToPrim, with input argument as thisArgument and a List containing( hint) as argumentsList. 
        // b. ReturnIfAbrupt(result). 
        ejsval result = _ejs_invoke_closure (exoticToPrim, &inputargument, 1, &hint, _ejs_undefined);
        // c. If Type(result) is not Object, then return result.
        if (!EJSVAL_IS_OBJECT(result))
            return result;

        // d. Throw a TypeError exception. 
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "@@toPrimitive returned an object");
    }
    // 7. If hint is "default" then, let hint be "number". 
    if (PreferredType == TO_PRIM_HINT_DEFAULT)
        PreferredType = TO_PRIM_HINT_NUMBER;

    // 8. Return OrdinaryToPrimitive(inputargument,hint).
    return OrdinaryToPrimitive(inputargument, PreferredType);
}

// ECMA262 7.2.9
// SameValue(x, y)
EJSBool
SameValue(ejsval x, ejsval y)
{
    // 1. ReturnIfAbrupt(x).
    // 2. ReturnIfAbrupt(y).

    // 3. If Type(x) is different from Type(y), return false.
    if (EJSVAL_TO_TAG(x) != EJSVAL_TO_TAG(y)) return EJS_FALSE;

    // 4. If Type(x) is Undefined, return true.
    if (EJSVAL_IS_UNDEFINED(x)) return EJS_TRUE;

    // 5. If Type(x) is Null, return true.
    if (EJSVAL_IS_NULL(x)) return EJS_TRUE;

    // 6. If Type(x) is Number, then
    if (EJSVAL_IS_NUMBER(x)) {
        // a. If x is NaN and y is NaN, return true.
        if (isnan(EJSVAL_TO_NUMBER(x)) && isnan(EJSVAL_TO_NUMBER(y))) return EJS_TRUE;
        // b. If x is +0 and y is -0, return false.
        if (EJSVAL_TO_NUMBER(x) == 0.0 && EJSDOUBLE_IS_NEGZERO(EJSVAL_TO_NUMBER(y))) return EJS_FALSE;
        // c. If x is -0 and y is +0, return false.
        if (EJSDOUBLE_IS_NEGZERO(EJSVAL_TO_NUMBER(x)) == 0.0 && EJSVAL_TO_NUMBER(y) == 0) return EJS_FALSE;
        // d. If x is the same Number value as y, return true.
        if (EJSVAL_TO_NUMBER(x) == EJSVAL_TO_NUMBER(y)) return EJS_TRUE;
        // e. Return false.
        return EJS_FALSE;
    }
    // 7. If Type(x) is String, then
    if (EJSVAL_IS_STRING(x)) {
        // a. If x and y are exactly the same sequence of code units (same length and same code units in corresponding positions) return true;
        //    otherwise, return false.
        if (EJSVAL_TO_STRLEN(x) != EJSVAL_TO_STRLEN(y)) return EJS_FALSE;

        // XXX there is doubtless a more efficient way to compare two ropes, but we convert but to flat strings for now.
        return ucs2_strcmp (EJSVAL_TO_FLAT_STRING(x), EJSVAL_TO_FLAT_STRING(y)) ? EJS_FALSE : EJS_TRUE;
    }
    // 8. If Type(x) is Boolean, then
    if (EJSVAL_IS_BOOLEAN(x)) {
        // a. If x and y are both true or both false, then return true; otherwise, return false.
        return EJSVAL_TO_BOOLEAN(x) == EJSVAL_TO_BOOLEAN(y) ? EJS_TRUE : EJS_FALSE;
    }
    // 9. If Type(x) is Symbol, then
    if (EJSVAL_IS_SYMBOL(x)) {
        // a. If x and y are both the same Symbol value, then return true; otherwise, return false.
        EJS_NOT_IMPLEMENTED();
    }
    // 10. Return true if x and y are the same Object value. Otherwise, return false.
    return EJSVAL_EQ(x, y);
}


// ECMA262 7.2.10
// SameValueZero(x, y)
// same as SameValue, except in its treatment of +/- 0
EJSBool
SameValueZero(ejsval x, ejsval y)
{
    // 1. ReturnIfAbrupt(x).
    // 2. ReturnIfAbrupt(y).

    // 3. If Type(x) is different from Type(y), return false.
    if ((EJSVAL_IS_NUMBER(x) != EJSVAL_IS_NUMBER(y)) &&
        (EJSVAL_TO_TAG(x) != EJSVAL_TO_TAG(y)))
        return EJS_FALSE;

    // 4. If Type(x) is Undefined, return true.
    if (EJSVAL_IS_UNDEFINED(x)) return EJS_TRUE;

    // 5. If Type(x) is Null, return true.
    if (EJSVAL_IS_NULL(x)) return EJS_TRUE;

    // 6. If Type(x) is Number, then
    if (EJSVAL_IS_NUMBER(x)) {
        //    a. If x is NaN and y is NaN, return true.
        if (isnan(EJSVAL_TO_NUMBER(x)) && isnan(EJSVAL_TO_NUMBER(y))) return EJS_TRUE;
        //    b. If x is +0 and y is -0, return true.
        if (EJSVAL_TO_NUMBER(x) == 0.0 && EJSDOUBLE_IS_NEGZERO(EJSVAL_TO_NUMBER(y))) return EJS_TRUE;
        //    c. If x is -0 and y is +0, return true.
        if (EJSDOUBLE_IS_NEGZERO(EJSVAL_TO_NUMBER(x)) && EJSVAL_TO_NUMBER(y) == 0) return EJS_TRUE;
        //    d. If x is the same Number value as y, return true.
        if (EJSVAL_TO_NUMBER(x) == EJSVAL_TO_NUMBER(y)) return EJS_TRUE;
        //    e. Return false.
        return EJS_FALSE;
    }
    // 7. If Type(x) is String, then
    if (EJSVAL_IS_STRING(x)) {
        //    a. If x and y are exactly the same sequence of code units (same length and same code units in corresponding positions) return true;
        //       otherwise, return false.
        if (EJSVAL_TO_STRLEN(x) != EJSVAL_TO_STRLEN(y)) return EJS_FALSE;

        // XXX there is doubtless a more efficient way to compare two ropes, but we convert but to flat strings for now.
        return ucs2_strcmp (EJSVAL_TO_FLAT_STRING(x), EJSVAL_TO_FLAT_STRING(y)) ? EJS_FALSE : EJS_TRUE;
    }
    // 8. If Type(x) is Boolean, then
    if (EJSVAL_IS_BOOLEAN(x)) {
        //    a. If x and y are both true or both false, then return true; otherwise, return false.
        return EJSVAL_TO_BOOLEAN(x) == EJSVAL_TO_BOOLEAN(y) ? EJS_TRUE : EJS_FALSE;
    }
    // 9. If Type(x) is Symbol, then
    if (EJSVAL_IS_SYMBOL(x)) {
        //    a. If x and y are both the same Symbol value, then return true; otherwise, return false.
        EJS_NOT_IMPLEMENTED();
    }
    // 10. Return true if x and y are the same Object value. Otherwise, return false.
    return EJSVAL_EQ(x, y);
}

ejsval
_ejs_op_neg (ejsval exp)
{
    return NUMBER_TO_EJSVAL (-ToDouble(exp));
}

ejsval
_ejs_op_plus (ejsval exp)
{
    return NUMBER_TO_EJSVAL (ToDouble(exp));
}

ejsval
_ejs_op_not (ejsval exp)
{
    EJSBool truthy= _ejs_truthy (exp);
    return BOOLEAN_TO_EJSVAL (!truthy);
}

ejsval
_ejs_op_bitwise_not (ejsval val)
{
    int val_int = ToInteger(val);
    return NUMBER_TO_EJSVAL (~val_int);
}

ejsval
_ejs_op_void (ejsval exp)
{
    return _ejs_undefined;
}

ejsval
_ejs_op_typeof_is_object(ejsval exp)
{
    return EJSVAL_IS_OBJECT(exp) ? _ejs_true : _ejs_false;
}

ejsval
_ejs_op_typeof_is_function(ejsval exp)
{
    return EJSVAL_IS_FUNCTION(exp) ? _ejs_true : _ejs_false;
}

ejsval
_ejs_op_typeof_is_string(ejsval exp)
{
    return EJSVAL_IS_STRING(exp) ? _ejs_true : _ejs_false;
}

ejsval
_ejs_op_typeof_is_symbol(ejsval exp)
{
    return EJSVAL_IS_SYMBOL(exp) ? _ejs_true : _ejs_false;
}

ejsval
_ejs_op_typeof_is_number(ejsval exp)
{
    return EJSVAL_IS_NUMBER(exp) ? _ejs_true : _ejs_false;
}

ejsval
_ejs_op_typeof_is_undefined(ejsval exp)
{
    return EJSVAL_IS_UNDEFINED(exp) ? _ejs_true : _ejs_false;
}

ejsval
_ejs_op_typeof_is_boolean(ejsval exp)
{
    return EJSVAL_IS_BOOLEAN(exp) ? _ejs_true : _ejs_false;
}

ejsval
_ejs_op_typeof_is_null(ejsval exp)
{
    return EJSVAL_IS_NULL(exp) ? _ejs_true : _ejs_false;
}

int
_ejs_op_foo(ejsval exp)
{
    return (EJSVAL_IS_NULL(exp) || EJSVAL_IS_UNDEFINED(exp)) ? 1 : 0;
}


ejsval
_ejs_op_typeof (ejsval exp)
{
    if (EJSVAL_IS_NULL(exp))
        return _ejs_atom_null;
    else if (EJSVAL_IS_BOOLEAN(exp))
        return _ejs_atom_boolean;
    else if (EJSVAL_IS_STRING(exp))
        return _ejs_atom_string;
    else if (EJSVAL_IS_SYMBOL(exp))
        return _ejs_atom_symbol;
    else if (EJSVAL_IS_NUMBER(exp))
        return _ejs_atom_number;
    else if (EJSVAL_IS_UNDEFINED(exp))
        return _ejs_atom_undefined;
    else if (EJSVAL_IS_OBJECT(exp)) {
        if (EJSVAL_IS_FUNCTION(exp))
            return _ejs_atom_function;
        else
            return _ejs_atom_object;
    }
    else
        EJS_NOT_IMPLEMENTED();
}

ejsval
_ejs_op_delete (ejsval obj, ejsval prop)
{
    EJSObject *obj_ = EJSVAL_TO_OBJECT(obj);

    EJSBool delete_rv = OP(obj_,Delete)(obj, prop, EJS_FALSE); // we need this for the mozilla tests... strict mode problem?

    return BOOLEAN_TO_EJSVAL(delete_rv);
}

ejsval
_ejs_op_mod (ejsval lhs, ejsval rhs)
{
    if (EJSVAL_IS_NUMBER(lhs)) {
        if (EJSVAL_IS_NUMBER(rhs)) {
            return NUMBER_TO_EJSVAL (fmod(EJSVAL_TO_NUMBER(lhs), EJSVAL_TO_NUMBER(rhs)));
        }
        else {
            // need to call valueOf() on the object, or convert the string to a number
            EJS_NOT_IMPLEMENTED();
        }
    }
    else if (EJSVAL_IS_STRING(lhs)) {
        // string+ with anything we don't implement yet - it will call toString() on objects, and convert a number to a string
        EJS_NOT_IMPLEMENTED();
    }
    else {
        // object+... how does js implement this anyway?
        EJS_NOT_IMPLEMENTED();
    }

    return _ejs_nan;
}

ejsval
_ejs_op_bitwise_xor (ejsval lhs, ejsval rhs)
{
    int lhs_int = ToInt32(lhs);
    int rhs_int = ToInt32(rhs);
    return NUMBER_TO_EJSVAL (lhs_int ^ rhs_int);
}

ejsval
_ejs_op_bitwise_and (ejsval lhs, ejsval rhs)
{
    int lhs_int = ToInteger(lhs);
    int rhs_int = ToInteger(rhs);
    return NUMBER_TO_EJSVAL (lhs_int & rhs_int);
}

ejsval
_ejs_op_bitwise_or (ejsval lhs, ejsval rhs)
{
    int lhs_int = ToInteger(lhs);
    int rhs_int = ToInteger(rhs);
    return NUMBER_TO_EJSVAL (lhs_int | rhs_int);
}

ejsval
_ejs_op_rsh (ejsval lhs, ejsval rhs)
{
    if (EJSVAL_IS_NUMBER(lhs)) {
        if (EJSVAL_IS_NUMBER(rhs)) {
            return NUMBER_TO_EJSVAL ((int)((int)EJSVAL_TO_NUMBER(lhs) >> (((unsigned int)EJSVAL_TO_NUMBER(rhs)) & 0x1f)));
        }
        else {
            // need to call valueOf() on the object, or convert the string to a number
            EJS_NOT_IMPLEMENTED();
        }
    }
    else if (EJSVAL_IS_STRING(lhs)) {
        // string+ with anything we don't implement yet - it will call toString() on objects, and convert a number to a string
        EJS_NOT_IMPLEMENTED();
    }
    else {
        // object+... how does js implement this anyway?
        EJS_NOT_IMPLEMENTED();
    }

    return _ejs_nan;
}

ejsval
_ejs_op_ursh (ejsval lhs, ejsval rhs)
{
    if (EJSVAL_IS_NUMBER(lhs)) {
        if (EJSVAL_IS_NUMBER(rhs)) {
            return NUMBER_TO_EJSVAL ((unsigned int)((unsigned int)EJSVAL_TO_NUMBER(lhs) >> (((unsigned int)EJSVAL_TO_NUMBER(rhs)) & 0x1f)));
        }
        else {
            // need to call valueOf() on the object, or convert the string to a number
            EJS_NOT_IMPLEMENTED();
        }
    }
    else if (EJSVAL_IS_STRING(lhs)) {
        // string+ with anything we don't implement yet - it will call toString() on objects, and convert a number to a string
        EJS_NOT_IMPLEMENTED();
    }
    else {
        // object+... how does js implement this anyway?
        EJS_NOT_IMPLEMENTED();
    }

    return _ejs_nan;
}

ejsval
_ejs_op_lsh (ejsval lhs, ejsval rhs)
{
    if (EJSVAL_IS_NUMBER(lhs)) {
        if (EJSVAL_IS_NUMBER(rhs)) {
            return NUMBER_TO_EJSVAL ((int)((int)EJSVAL_TO_NUMBER(lhs) << (((unsigned int)EJSVAL_TO_NUMBER(rhs)) & 0x1f)));
        }
        else {
            // need to call valueOf() on the object, or convert the string to a number
            EJS_NOT_IMPLEMENTED();
        }
    }
    else if (EJSVAL_IS_STRING(lhs)) {
        // string+ with anything we don't implement yet - it will call toString() on objects, and convert a number to a string
        EJS_NOT_IMPLEMENTED();
    }
    else {
        // object+... how does js implement this anyway?
        EJS_NOT_IMPLEMENTED();
    }

    return _ejs_nan;
}

ejsval
_ejs_op_ulsh (ejsval lhs, ejsval rhs)
{
    if (EJSVAL_IS_NUMBER(lhs)) {
        if (EJSVAL_IS_NUMBER(rhs)) {
            return NUMBER_TO_EJSVAL ((unsigned int)((unsigned int)EJSVAL_TO_NUMBER(lhs) << (((unsigned int)EJSVAL_TO_NUMBER(rhs)) & 0x1f)));
        }
        else {
            // need to call valueOf() on the object, or convert the string to a number
            EJS_NOT_IMPLEMENTED();
        }
    }
    else if (EJSVAL_IS_STRING(lhs)) {
        // string+ with anything we don't implement yet - it will call toString() on objects, and convert a number to a string
        EJS_NOT_IMPLEMENTED();
    }
    else {
        // object+... how does js implement this anyway?
        EJS_NOT_IMPLEMENTED();
    }

    return _ejs_nan;
}

ejsval
_ejs_op_add (ejsval lhs, ejsval rhs)
{
    ejsval rv;

    ejsval lprim, rprim;

    lprim = ToPrimitive(lhs, TO_PRIM_HINT_DEFAULT);
    rprim = ToPrimitive(rhs, TO_PRIM_HINT_DEFAULT);

    if (EJSVAL_IS_STRING(lhs) || EJSVAL_IS_STRING(rhs)) {
        ejsval lhstring = ToString(lhs);
        ejsval rhstring = ToString(rhs);

        ejsval result = _ejs_string_concat (lhstring, rhstring);
        rv = result;
    }
    else {
        rv = NUMBER_TO_EJSVAL (ToDouble(lprim) + ToDouble(rprim));
    }

    return rv;
}

ejsval
_ejs_op_mult (ejsval lhs, ejsval rhs)
{
    if (EJSVAL_IS_NUMBER(lhs) || EJSVAL_IS_NUMBER(rhs)) {
        return NUMBER_TO_EJSVAL (ToDouble(lhs) * ToDouble(rhs));
    }
    else if (EJSVAL_IS_STRING(lhs)) {
        // string+ with anything we don't implement yet - it will call toString() on objects, and convert a number to a string
        EJS_NOT_IMPLEMENTED();
    }
    else {
        // object+... how does js implement this anyway?
        EJS_NOT_IMPLEMENTED();
    }

    return _ejs_nan;
}

ejsval
_ejs_op_div (ejsval lhs, ejsval rhs)
{
    if (EJSVAL_IS_NUMBER(lhs)) {
        return NUMBER_TO_EJSVAL (EJSVAL_TO_NUMBER(lhs) / ToDouble (rhs));
    }
    else if (EJSVAL_IS_STRING(lhs)) {
        // string+ with anything we don't implement yet - it will call toString() on objects, and convert a number to a string
        EJS_NOT_IMPLEMENTED();
    }
    else {
        // object+... how does js implement this anyway?
        EJS_NOT_IMPLEMENTED();
    }

    return _ejs_nan;
}

ejsval
_ejs_op_lt (ejsval lhs, ejsval rhs)
{
    return BOOLEAN_TO_EJSVAL(_ejs_op_lt_ejsbool(lhs, rhs));
}

EJSBool
_ejs_op_lt_ejsbool (ejsval lhs, ejsval rhs)
{
    ejsval lprim, rprim;

    lprim = ToPrimitive(lhs, TO_PRIM_HINT_NUMBER);
    rprim = ToPrimitive(rhs, TO_PRIM_HINT_NUMBER);

    if (EJSVAL_IS_STRING_TYPE(lprim) && EJSVAL_IS_STRING_TYPE(rprim)) {
        ejsval lstr = ToString(lprim);
        ejsval rstr = ToString(rprim);

        return ucs2_strcmp (EJSVAL_TO_FLAT_STRING(lstr), EJSVAL_TO_FLAT_STRING(rstr)) < 0;
    }

    return ToDouble(lprim) < ToDouble(rprim);
}

ejsval
_ejs_op_le (ejsval lhs, ejsval rhs)
{
    ejsval lprim, rprim;

    lprim = ToPrimitive(lhs, TO_PRIM_HINT_NUMBER);
    rprim = ToPrimitive(rhs, TO_PRIM_HINT_NUMBER);

    if (EJSVAL_IS_STRING_TYPE(lprim) && EJSVAL_IS_STRING_TYPE(rprim)) {
        ejsval lstr = ToString(lprim);
        ejsval rstr = ToString(rprim);

        return BOOLEAN_TO_EJSVAL (ucs2_strcmp (EJSVAL_TO_FLAT_STRING(lstr), EJSVAL_TO_FLAT_STRING(rstr)) <= 0);
    }

    return BOOLEAN_TO_EJSVAL(ToDouble(lprim) <= ToDouble(rprim));
}

ejsval
_ejs_op_gt (ejsval lhs, ejsval rhs)
{
    ejsval lprim, rprim;

    lprim = ToPrimitive(lhs, TO_PRIM_HINT_NUMBER);
    rprim = ToPrimitive(rhs, TO_PRIM_HINT_NUMBER);

    if (EJSVAL_IS_STRING_TYPE(lprim) && EJSVAL_IS_STRING_TYPE(rprim)) {
        ejsval lstr = ToString(lprim);
        ejsval rstr = ToString(rprim);

        return BOOLEAN_TO_EJSVAL (ucs2_strcmp (EJSVAL_TO_FLAT_STRING(lstr), EJSVAL_TO_FLAT_STRING(rstr)) > 0);
    }

    return BOOLEAN_TO_EJSVAL(ToDouble(lprim) > ToDouble(rprim));
}

ejsval
_ejs_op_ge (ejsval lhs, ejsval rhs)
{
    ejsval lprim, rprim;

    lprim = ToPrimitive(lhs, TO_PRIM_HINT_NUMBER);
    rprim = ToPrimitive(rhs, TO_PRIM_HINT_NUMBER);

    if (EJSVAL_IS_STRING_TYPE(lprim) && EJSVAL_IS_STRING_TYPE(rprim)) {
        ejsval lstr = ToString(lprim);
        ejsval rstr = ToString(rprim);

        return BOOLEAN_TO_EJSVAL (ucs2_strcmp (EJSVAL_TO_FLAT_STRING(lstr), EJSVAL_TO_FLAT_STRING(rstr)) >= 0);
    }

    return BOOLEAN_TO_EJSVAL(ToDouble(lprim) >= ToDouble(rprim));
}

ejsval
_ejs_op_sub (ejsval lhs, ejsval rhs)
{
    return NUMBER_TO_EJSVAL(ToDouble(lhs) - ToDouble(rhs));
}

// ECMA262 7.2.13
// Strict Equality Comparison 
ejsval
_ejs_op_strict_eq (ejsval x, ejsval y)
{
    // 1. If Type(x) is different from Type(y), return false.
    if (EJSVAL_TO_TAG(x) != EJSVAL_TO_TAG(y)) return _ejs_false;
    
    // 2. If Type(x) is Undefined, return true.
    if (EJSVAL_IS_UNDEFINED(x)) return _ejs_true;

    // 3. If Type(x) is Null, return true.
    if (EJSVAL_IS_NULL(x)) return _ejs_true;

    // 4. If Type(x) is Number, then
    if (EJSVAL_IS_NUMBER(x)) {
        //    a. If x is NaN, return false.
        if (isnan(EJSVAL_TO_NUMBER(x))) return _ejs_false;

        //    b. If y is NaN, return false.
        if (isnan(EJSVAL_TO_NUMBER(y))) return _ejs_false;

        //    c. If x is the same Number value as y, return true.
        if (EJSVAL_TO_NUMBER(x) == EJSVAL_TO_NUMBER(y)) return _ejs_true;
        
        //    d. If x is +0 and y is -0, return true.
        if (EJSVAL_TO_NUMBER(x) == 0.0 && EJSDOUBLE_IS_NEGZERO(EJSVAL_TO_NUMBER(y))) return _ejs_true;
        //    e. If x is -0 and y is +0, return true.
        if (EJSDOUBLE_IS_NEGZERO(EJSVAL_TO_NUMBER(x)) == 0.0 && EJSVAL_TO_NUMBER(y) == 0) return _ejs_true;

        //    f. Return false.
        return _ejs_false;
    }
    // 5. If Type(x) is String, then
    if (EJSVAL_IS_STRING(x)) {
        //    a. If x and y are exactly the same sequence of characters (same length and same characters in corresponding positions), return true.
        //    b. Else, return false.
        if (EJSVAL_TO_STRLEN(x) != EJSVAL_TO_STRLEN(y))
            return _ejs_false;
        return BOOLEAN_TO_EJSVAL (!ucs2_strcmp (EJSVAL_TO_FLAT_STRING(x), EJSVAL_TO_FLAT_STRING(y)));
    }
    // 6. If Type(x) is Boolean, then
    if (EJSVAL_IS_BOOLEAN(x)) {
        //    a. If x and y are both true or both false, return true.
        //    b. Else, return false.
        return BOOLEAN_TO_EJSVAL (EJSVAL_TO_BOOLEAN(x) == EJSVAL_TO_BOOLEAN(y));
    }
    // 7. If x and y are the same Symbol value, return true.
    if (EJSVAL_IS_SYMBOL(x)) {
        return BOOLEAN_TO_EJSVAL(EJSVAL_EQ(x,y)); // XXX is this sufficient?
    }
    // 8. If x and y are the same Object value, return true.
    // 9. Return false.
    return BOOLEAN_TO_EJSVAL (EJSVAL_EQ(x, y));
}

ejsval
_ejs_op_strict_neq (ejsval lhs, ejsval rhs)
{
    return BOOLEAN_TO_EJSVAL (!EJSVAL_TO_BOOLEAN (_ejs_op_strict_eq (lhs, rhs)));
}

// ECMA262 7.2.12
// Abstract Equality Comparison 
ejsval
_ejs_op_eq (ejsval x, ejsval y)
{
    // 1. ReturnIfAbrupt(x).
    // 2. ReturnIfAbrupt(y).
    // 3. If Type(x) is the same as Type(y), then
    if (EJSVAL_TO_TAG(x) == EJSVAL_TO_TAG(y))
        // a. Return the result of performing Strict Equality Comparison x === y.
        return _ejs_op_strict_eq(x, y);
    // 4. If x is null and y is undefined, return true.
    if (EJSVAL_IS_NULL(x) && EJSVAL_IS_UNDEFINED(y)) return _ejs_true;

    // 5. If x is undefined and y is null, return true.
    if (EJSVAL_IS_UNDEFINED(x) && EJSVAL_IS_NULL(y)) return _ejs_true;

    // 6. If Type(x) is Number and Type(y) is String, return the result of the comparison x == ToNumber(y).
    if (EJSVAL_IS_NUMBER(x) && EJSVAL_IS_STRING(y)) return _ejs_op_eq(x, ToNumber(y));

    // 7. If Type(x) is String and Type(y) is Number, return the result of the comparison ToNumber(x) == y.
    if (EJSVAL_IS_STRING(x) && EJSVAL_IS_NUMBER(y)) return _ejs_op_eq(ToNumber(x), y);

    // 8. If Type(x) is Boolean, return the result of the comparison ToNumber(x) == y.
    if (EJSVAL_IS_BOOLEAN(x)) return _ejs_op_eq(ToNumber(x), y);

    // 9. If Type(y) is Boolean, return the result of the comparison x == ToNumber(y).
    if (EJSVAL_IS_BOOLEAN(y)) return _ejs_op_eq(x, ToNumber(y));

    // 10. If Type(x) is either String or Number and Type(y) is Object, return the result of the comparison x == ToPrimitive(y).
    if ((EJSVAL_IS_STRING(x) || EJSVAL_IS_NUMBER(x)) && EJSVAL_IS_OBJECT(y)) return _ejs_op_eq(x, ToPrimitive(y, TO_PRIM_HINT_DEFAULT));

    // 11. If Type(x) is Object and Type(y) is either String or Number, return the result of the comparison ToPrimitive(x) == y.
    if (EJSVAL_IS_OBJECT(x) && (EJSVAL_IS_STRING(y) || EJSVAL_IS_NUMBER(y))) return _ejs_op_eq(ToPrimitive(x, TO_PRIM_HINT_DEFAULT), y);

    // 12. Return false.
    return _ejs_false;
}

ejsval
_ejs_op_neq (ejsval lhs, ejsval rhs)
{
    return BOOLEAN_TO_EJSVAL (!EJSVAL_TO_BOOLEAN (_ejs_op_eq (lhs, rhs)));
}

static ejsval
OrdinaryHasInstance(ejsval C, ejsval O)
{
    // 1. If IsCallable(C) is false, return false. 
    if (!IsCallable(C))
        return _ejs_false;

    // 2. If C has a [[BoundTargetFunction]] internal slot, then 
    if (EJSVAL_IS_BOUND_FUNCTION(C)) {
        //    a. Let BC be the value of C’s [[BoundTargetFunction]] internal slot. 
        //    b. Return InstanceofOperator(O,BC) (see 12.9.4). 
        EJS_NOT_IMPLEMENTED();
    }
    // 3. If Type(O) is not Object, return false. 
    if (!EJSVAL_IS_OBJECT(O))
        return _ejs_false;

    // 4. Let P be Get(C, "prototype"). 
    // 5. ReturnIfAbrupt(P). 
    ejsval P = Get(C, _ejs_atom_prototype);
    // 6. If Type(P) is not Object, throw a TypeError exception. 
    if (!EJSVAL_IS_OBJECT(P))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "prototype is not an object");
        
    // 7. Repeat 
    while (EJS_TRUE) {
        //    a. Set O to the result of calling the [[GetPrototypeOf]] internal method of O with no arguments. 
        //    b. ReturnIfAbrupt(O). 
        O = OP(EJSVAL_TO_OBJECT(O),GetPrototypeOf)(O);
        //    c. If O is null, return false. 
        if (EJSVAL_IS_NULL(O))
            return _ejs_false;
        //    d. If SameValue(P, O) is true, return true. 
        if (SameValue(P, O))
            return _ejs_true;
    }
}

ejsval
_ejs_op_instanceof (ejsval O, ejsval C)
{
    // ECMA262 12.9.4 Runtime Semantics: InstanceofOperator(O, C) 
    // 1. If Type(C) is not Object, throw a TypeError exception. 
    if (!EJSVAL_IS_OBJECT(C)) {
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "rhs of instanceof check must be an object");
    }
    // 2. Let instOfHandler be GetMethod(C,@@hasInstance). 
    // 3. ReturnIfAbrupt(instOfHandler). 
    ejsval instOfHandler = GetMethod(C, _ejs_Symbol_hasInstance);

    // 4. If instOfHandler is not undefined, then 
    if (!EJSVAL_IS_UNDEFINED(instOfHandler)) {
        //    a. Let result be the result of calling the [[Call]] internal method of instOfHandler passing C as thisArgument and a new List containing O as argumentsList. 
        ejsval result = _ejs_invoke_closure(instOfHandler, &C, 1, &O, _ejs_undefined);
        //    b. Return ToBoolean(result). 
        return ToBoolean(result);
    }

    // 5. If IsCallable(C) is false, then throw a TypeError exception. 
    if (!IsCallable(C))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "1"); // XXX

    // 6. Return OrdinaryHasInstance(C, O). 
    return OrdinaryHasInstance(C, O);
}

// ECMA262: 11.8.7
ejsval
_ejs_op_in (ejsval lhs, ejsval rhs)
{
    /* 1. Let lref be the result of evaluating RelationalExpression. */
    /* 2. Let lval be GetValue(lref). */
    /* 3. Let rref be the result of evaluating ShiftExpression. */
    /* 4. Let rval be GetValue(rref). */
    /* 5. If Type(rval) is not Object, throw a TypeError exception. */
    if (!EJSVAL_IS_OBJECT(rhs)) {
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "rhs of 'in' must be an object");
    }

    EJSObject *obj = EJSVAL_TO_OBJECT(rhs);

    /* 6. Return the result of calling the [[HasProperty]] internal method of rval with argument ToString(lval). */
    return OP(obj,HasProperty) (rhs, ToString(lhs)) ? _ejs_true : _ejs_false;
}

EJSBool
_ejs_truthy (ejsval val)
{
    return EJSVAL_EQ(_ejs_true, ToBoolean(val));
}


void
_ejs_throw (ejsval exp)
{
    _ejs_exception_throw (exp);
}

void
_ejs_rethrow ()
{
    _ejs_exception_rethrow ();
}

EJS_NATIVE_FUNC(_ejs_isNaN_impl) {
    ejsval num = _ejs_undefined;
    if (argc >= 1)
        num = args[0];

    return isnan(ToDouble(num)) ? _ejs_true : _ejs_false;
}

EJS_NATIVE_FUNC(_ejs_isFinite_impl) {
    if (argc < 1)
        return _ejs_false;

    ejsval num = args[0];

    return isfinite(ToDouble(num)) ? _ejs_true : _ejs_false;
}

// ECMA262 15.1.2.2
// parseInt (string , radix)
EJS_NATIVE_FUNC(_ejs_parseInt_impl) {
    ejsval string = _ejs_undefined;
    ejsval radix = _ejs_undefined;

    if (argc > 0) string = args[0];
    if (argc > 1) radix = args[1];

    /* 1. Let inputString be ToString(string). */
    ejsval inputString = ToString(string);

    /* 2. Let  S be a newly created substring of  inputString consisting of the first character that is not a  */
    /*    StrWhiteSpaceChar and all characters following that character. (In other words, remove leading white  */
    /*    space.) If inputString does not contain any such characters, let S be the empty string. */
    jschar* S = EJSVAL_TO_FLAT_STRING(inputString);

    /* 3. Let sign be 1. */
    int32_t sign = 1;
    int Sidx = 0;
    int Slength = EJSVAL_TO_STRLEN(inputString);

    /* 4. If S is not empty and the first character of S is a minus sign -, let sign be -1. */
    if (Slength != 0 && S[Sidx] == '-')
        sign = -1;

    /* 5. If S is not empty and the first character of S is a plus sign + or a minus sign -, then remove the first character */
    /*    from S. */
    if (Slength != 0 && (S[Sidx] == '-' || S[Sidx] == '+')) {
        Sidx ++;
        Slength --;
    }

    /* 6. Let R = ToInt32(radix). */
    int32_t R = ToInteger(radix);

    /* 7. Let stripPrefix be true. */
    EJSBool stripPrefix = EJS_TRUE;

    /* 8. If R != 0, then */
    if (!EJSVAL_IS_UNDEFINED(radix) &&  R != 0) {
        /* a. If R < 2 or R > 36, then return NaN. */
        if (R < 2 || R > 36) return _ejs_nan;

        /* b. If R != 16, let stripPrefix be false. */
        if (R != 16) stripPrefix = EJS_FALSE;
    }
    /* 9. Else, R = 0 */
    else {
        /* a. Let R = 10. */
        R = 10;
    }

    /* 10. If stripPrefix is true, then */
    if (stripPrefix) {
        /* a. If the length of S is at least 2 and the first two characters of S are either "0x" or "0X", then remove */
        /*    the first two characters from S and let R = 16.*/
        if (Slength > 2 && S[Sidx] == '0' && (S[Sidx+1] == 'x' || S[Sidx+1] == 'X')) {
            Sidx += 2;
            Slength -= 2;
            R = 16;
        }
    }
    static jschar radix_8_chars[] = { '0', '1', '2', '3', '4', '5', '6', '7', 0 };
    static jschar radix_10_chars[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 0 };
    static jschar radix_16_chars[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f', 0 };
    static jschar radix_16up_chars[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F', 0 };

    jschar* radix_chars;
    jschar* radix_chars2 = NULL;

    switch (R) {
    case 8:
        radix_chars = radix_8_chars;
        break;
    case 10:
        radix_chars = radix_10_chars;
        break;
    case 16:
        radix_chars = radix_16_chars;
        radix_chars2 = radix_16up_chars;
        break;
    default:
        EJS_NOT_IMPLEMENTED();
    }

    /* 11. If S contains any character that is not a radix-R digit, then let Z be the substring of S consisting of all  */
    /*     characters before the first such character; otherwise, let Z be S. */
    int i;
    for (i = 0; i < Slength; i ++) {
        jschar needle[2];
        needle[0] = S[Sidx+i];
        needle[1] = 0;
        if (ucs2_strstr(radix_chars, needle) == NULL) {
            if (!radix_chars2 || ucs2_strstr(radix_chars2, needle) == NULL)
                break;
        }
    }
    /* 12. If Z is empty, return NaN. */
    if (i == 0) {
        return _ejs_nan;
    }
    
    /* 13. Let mathInt be the mathematical integer value that is represented by  Z in radix-R notation, using the letters  */
    /*     A-Z and  a-z for digits with values 10 through 35. (However, if  R is 10 and  Z contains more than 20  */
    /*     significant digits, every significant digit after the 20th may be replaced by a  0  digit, at the option of the */
    /*     implementation; and if  R is not 2, 4, 8, 10, 16, or 32, then  mathInt may be an implementation-dependent */
    /*     approximation to the mathematical integer value that is represented by Z in radix-R notation.) */

    int mathInt = 0;
    int32_t Zlen = i;
    for (i = 0; i < Zlen; i ++) {
        jschar needle[2];
        needle[0] = S[Sidx+i];
        needle[1] = 0;

        int digitval;
        jschar* r;
        if ((r = ucs2_strstr(radix_chars, needle))) {
            digitval = r - radix_chars;
        }
        else {
            r = ucs2_strstr(radix_chars2, needle);
            digitval = r - radix_chars2;
        }
        mathInt = mathInt*R + digitval;
    }

    /* 14. Let number be the Number value for mathInt. */
    int32_t number = mathInt * sign;

    /* 15. Return sign * number */
    return NUMBER_TO_EJSVAL(number);
}

EJS_NATIVE_FUNC(_ejs_parseFloat_impl) {
    if (argc == 0)
        return _ejs_nan;

    char *float_utf8 = ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(ToString(args[0])));

    ejsval rv = NUMBER_TO_EJSVAL (strtod (float_utf8, NULL));

    free (float_utf8);

    return rv;
}

/* 7.4.1 CheckIterable ( obj ) */
ejsval
CheckIterable (ejsval obj)
{
    /* 1. If Type(obj) is not Object, then return undefined. */
    if (!EJSVAL_IS_OBJECT(obj))
        return _ejs_undefined;

    /* 2. Let iteratorGetter be Get(obj, @@iterator). */
    ejsval iteratorGetter = Get (obj, _ejs_Symbol_iterator);

    /* 3. Return iteratorGetter. */
    return iteratorGetter;
}

/* 7.4.2 GetIterator ( obj, method ) */
ejsval
GetIterator (ejsval obj, ejsval method)
{
    /* 1. If method was not passed, then */
    if (EJSVAL_IS_UNDEFINED(method))
        /* a. Let method be CheckIterable(obj). */
        /* b. ReturnIfAbrupt(method). */
        method = CheckIterable (obj);

    /* 2. If IsCallable(method) is false, then throw a TypeError exception. */
    if (!IsCallable(method))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "method is not a function");

    /* 3. Let iterator be the result of calling the [[Call]] internal method of method with
     * obj as thisArgument and an empty List as argumentsList. */
    ejsval iterator = _ejs_invoke_closure (method, &obj, 0, NULL, _ejs_undefined);

    /* 4. If Type(iterator) is not Object, then throw a TypeError exception. */
    if (!EJSVAL_IS_OBJECT(iterator))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "iterator is not an object");

    return iterator;
}

/* a version of GetIterator that returns true/false instead of throwing an exception */
static ejsval call_get_iterator(void* data) {
    ejsval* arg = (ejsval*)data;
    return GetIterator(*arg, _ejs_undefined);
}

EJSBool
GetIterator_internal(ejsval* iterator, ejsval iterable)
{
    return _ejs_invoke_func_catch(iterator, call_get_iterator, &iterable);
}

/* 7.4.3 IteratorNext ( iterator, value ) */
ejsval
IteratorNext (ejsval iterator, ejsval value)
{
    ejsval result;

    /* 1. If value was not passed, */
    if (EJSVAL_IS_UNDEFINED(value))
        /* Let result be Invoke(iterator, "next", ( )). */
        result = _ejs_invoke_closure (Get(iterator, _ejs_atom_next), &iterator, 0, NULL, _ejs_undefined);
    /* 2. Else, */
    else
        /* a. Let result be Invoke(iterator, "next", (value)). */
        result = _ejs_invoke_closure (Get(iterator, _ejs_atom_next), &iterator, 1, &value, _ejs_undefined);

    /* 3. If Type(result) is not Object, then throw a TypeError exception. */
    if (!EJSVAL_IS_OBJECT(result))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "result is not an object");

    /* 5. Return result. */
    return result;
}

/* a version of IteratorNext that returns true/false instead of throwing an exception */
typedef struct {
    ejsval iterator;
    ejsval value;
} call_next_data;

static ejsval call_iterator_next(void* data) {
    call_next_data* arg = (call_next_data*)data;
    return IteratorNext(arg->iterator, arg->value);
}
EJSBool
IteratorNext_internal(ejsval* next, ejsval iterator, ejsval value)
{
    call_next_data data;
    data.iterator = iterator;
    data.value = value;
    return _ejs_invoke_func_catch(next, call_iterator_next, &data);
}

/* 7.4.4 IteratorComplete ( iterResult ) */
ejsval
IteratorComplete (ejsval iterResult)
{
    /* 1. Assert: Type(iterResult) is Object. */
    if (!EJSVAL_IS_OBJECT(iterResult))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "iterResult is not an object");

    /* 2. Let done be Get(iterResult, "done"). */
    ejsval done = Get(iterResult, _ejs_atom_done);

    /* 3. Return ToBoolean(done). */
    return ToBoolean(done);
}

/* a version of IteratorComplete that returns true/false instead of an ejsval */
EJSBool
IteratorComplete_internal (ejsval iterResult) {
    return EJSVAL_TO_BOOLEAN(IteratorComplete(iterResult));
}


/* 7.4.5 IteratorValue ( iterResult ) */
ejsval
IteratorValue (ejsval iterResult)
{
    /* 1. Assert: Type(iterResult) is Object. */
    if (!EJSVAL_IS_OBJECT(iterResult))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "iterResult is not an object");

    /* 2. Return Get(iterResult, "value"). */
    return Get(iterResult, _ejs_atom_value);
}


/* a version of IteratorValue that returns true/false instead of throwing an exception */
static ejsval call_iterator_value(void* data) {
    ejsval* arg = (ejsval*)data;
    return IteratorValue(*arg);
}
EJSBool
IteratorValue_internal(ejsval* value, ejsval iterResult)
{
    return _ejs_invoke_func_catch(value, call_iterator_value, &iterResult);
}

// ES2015, June 2015
// 7.4.6 IteratorClose ( iterator, completion )
ejsval
IteratorClose (ejsval iterator, ejsval completion, EJSBool completionIsThrow ) {
    // 1. Assert: Type(iterator) is Object.
    // 2. Assert: completion is a Completion Record.
    // 3. Let return be GetMethod(iterator, "return").
    // 4. ReturnIfAbrupt(return).
    ejsval _return = GetMethod(iterator, _ejs_atom_return);

    // 5. If return is undefined, return Completion(completion).
    if (EJSVAL_IS_UNDEFINED(_return)) {
        if (completionIsThrow) _ejs_throw(completion);
        return completion;
    }

    // 6. Let innerResult be Call(return, iterator, « »).
    // XXX _ejs_invoke_closure won't call proxy methods
    ejsval innerResult = _ejs_invoke_closure(_return, &iterator, 0, NULL, _ejs_undefined);

    // 7. If completion.[[type]] is throw, return Completion(completion).
    if (completionIsThrow) _ejs_throw(completion);

    // 8. If innerResult.[[type]] is throw, return Completion(innerResult).

    // 9. If Type(innerResult.[[value]]) is not Object, throw a TypeError exception.
    if (!EJSVAL_IS_OBJECT(innerResult)) _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "1");

    // 10. Return Completion(completion).
    if (completionIsThrow) _ejs_throw(completion);
    return completion;
}

/* 7.4.6 IteratorStep ( iterator ) */
ejsval
IteratorStep (ejsval iterator)
{
    /* 1. Let result be IteratorNext(iterator). */
    /* 2. ReturnIfAbrupt(result). */
    ejsval result = IteratorNext (iterator, _ejs_undefined);

    /* 3. Let done be IteratorComplete(result). */
    ejsval done = IteratorComplete (result);

    /* 5. If done is true, then return false. */
    if (EJSVAL_TO_BOOLEAN(done))
        return _ejs_false;

    /* 6. Return result */
    return result;
}

/* a version of IteratorStep that returns true/false instead of throwing an exception */
static ejsval call_iterator_step(void* data) {
    ejsval* arg = (ejsval*)data;
    return IteratorStep(*arg);
}
EJSBool
IteratorStep_internal(ejsval* next, ejsval iterator)
{
    return _ejs_invoke_func_catch(next, call_iterator_step, &iterator);
}

/* 7.4.7 CreateIterResultObject (value, done) */
ejsval
_ejs_create_iter_result (ejsval value, ejsval done)
{
    /* 1. Assert: Type(done) is Boolean. */

    /* 2. Let obj be ObjectCreate(%ObjectPrototype%). */
    // Or _ejs_Object_prototype?
    ejsval obj = _ejs_object_new (_ejs_null, &_ejs_Object_specops);

    /* 3. Perform CreateDataProperty(obj, "value", value). */
    _ejs_object_setprop (obj, _ejs_atom_value, value);

    /* 4. Perform CreateDataProperty(obj, "done", done). */
    _ejs_object_setprop (obj, _ejs_atom_done, done);

    return obj;
}

// ES2015, June 2015
// 22.1.3.1.1 IsConcatSpreadable ( O )
EJSBool
IsConcatSpreadable (ejsval O)
{
    // 1. If Type(O) is not Object, return false.
    if (!EJSVAL_IS_OBJECT(O)) return EJS_FALSE;

    // 2. Let spreadable be Get(O, @@isConcatSpreadable).
    // 3. ReturnIfAbrupt(spreadable).
    ejsval spreadable = Get(O, _ejs_Symbol_isConcatSpreadable);

    // 4. If spreadable is not undefined, return ToBoolean(spreadable).
    if (!EJSVAL_IS_UNDEFINED(spreadable))
        return ToEJSBool(spreadable);

    // 5. Return IsArray(O).
    return IsArray(O);
}

// ES2015, June 2015
// 7.2.3 IsCallable (argument)
EJSBool
IsCallable(ejsval argument) {
    // 1. ReturnIfAbrupt(argument).
    // 2. If Type(argument) is not Object, return false.
    if (!EJSVAL_IS_OBJECT(argument)) return EJS_FALSE;

    // 3. If argument has a [[Call]] internal method, return true.
    EJSObject* obj = EJSVAL_TO_OBJECT(argument);
    if (OP(obj, Call) != NULL) return EJS_TRUE;

    // 4. Return false.
    return EJS_FALSE;
}

// ES2015, June 2015
// 7.2.4 IsConstructor ( argument )
EJSBool
IsConstructor(ejsval argument) {
    // 1. ReturnIfAbrupt(argument).
    // 2. If Type(argument) is not Object, return false.
    if (!EJSVAL_IS_OBJECT(argument)) return EJS_FALSE;

    // 3. If argument has a [[Construct]] internal method, return true.
    EJSObject* obj = EJSVAL_TO_OBJECT(argument);
    if (OP(obj, Construct) != NULL) return EJS_TRUE;

    // 4. Return false.
    return EJS_FALSE;
}

// ES2015, June 2015
// 7.2.5
EJSBool
IsExtensible(ejsval O)
{
    // 1. Assert: Type(O) is Object.
    EJS_ASSERT(EJSVAL_IS_OBJECT(O));
    // 2. Return O.[[IsExtensible]]().
    return OP(EJSVAL_TO_OBJECT(O), IsExtensible)(O);
}

ejsval GetPrototypeFromConstructor(ejsval ctor, ejsval default_proto) {
    ejsval proto = OP(EJSVAL_TO_OBJECT(ctor),Get)(ctor, _ejs_atom_prototype, ctor);
    if (EJSVAL_IS_UNDEFINED(proto))
        return default_proto;
    return proto;
}

ejsval OrdinaryCreateFromConstructor(ejsval ctor, ejsval default_proto, EJSSpecOps *ops) {
    ejsval proto = GetPrototypeFromConstructor(ctor, default_proto);
    EJSObject* O_ = ops->Allocate();
    ejsval O = OBJECT_TO_EJSVAL(O_);
    _ejs_init_object ((EJSObject*)O_, proto, ops);
    return O;
}


// ES2015, June 2015
// 7.3.13
// Construct (F, [argumentsList], [newTarget])
ejsval
Construct (ejsval F, ejsval newTarget, uint32_t argc, ejsval* args)
{
    // 1. If newTarget was not passed, let newTarget be F.
    if (EJSVAL_IS_UNDEFINED(newTarget)) newTarget = F;

    // 2. If argumentsList was not passed, let argumentsList be a new empty List.

    // 3. Assert: IsConstructor (F) is true.
    EJS_ASSERT(IsConstructor(F));

    // 4. Assert: IsConstructor (newTarget) is true.
    EJS_ASSERT(IsConstructor(newTarget));

    // 5. Return the result of calling the [[Construct]] internal method of F passing argumentsList and newTarget as the arguments.
    return OP(EJSVAL_TO_OBJECT(F),Construct) (F, newTarget, argc, args);
}

// ES6 Draft January 15, 2015
// 7.3.19
// SpeciesConstructor (O, defaultConstructor)
ejsval
SpeciesConstructor(ejsval O, ejsval defaultConstructor)
{
    // 1. Assert: Type(O) is Object.
    EJS_ASSERT(EJSVAL_IS_OBJECT(O));

    // 2. Let C be Get(O, "constructor").
    // 3. ReturnIfAbrupt(C).
    ejsval C = Get(O, _ejs_atom_constructor);

    // 4. If C is undefined, return defaultConstructor.
    if (EJSVAL_IS_UNDEFINED(C))
        return defaultConstructor;

    // 5. If Type(C) is not Object, throw a TypeError exception.
    if (!EJSVAL_IS_OBJECT(C))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "'constructor' is not an object");

    // 6. Let S be Get(C, @@species).
    // 7. ReturnIfAbrupt(S).
    ejsval S = Get(C, _ejs_Symbol_species);

    // 8. If S is either undefined or null, return defaultConstructor.
    if (EJSVAL_IS_UNDEFINED(S) || EJSVAL_IS_NULL(S))
        return defaultConstructor;

    // 9. If IsConstructor(S) is true, return S.
    if (IsConstructor(S)) {
        return S;
    }

    // 10. Throw a TypeError exception.
    _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "constructor[@@species] is not a constructor");
}
