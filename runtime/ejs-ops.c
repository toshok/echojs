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

ejsval NumberToString(double d)
{
    int32_t i;
    if (EJSDOUBLE_IS_INT32(d, &i)) {
        if (i >=0 && i <= 200)
            return *builtin_numbers_atoms[i];
        jschar int_buf[UINT32_CHAR_BUFFER_LENGTH+1];
        jschar *cp = IntToUCS2(int_buf, i, 10);
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
    else
        snprintf (num_buf, sizeof(num_buf), EJS_NUMBER_FORMAT, d);
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
        return NumberToString(EJSVAL_TO_NUMBER(exp));
    else if (EJSVAL_IS_STRING(exp))
        return exp;
    else if (EJSVAL_IS_OBJECT(exp)) {
        ejsval toString = _ejs_object_getprop (exp, _ejs_atom_toString);
        if (!EJSVAL_IS_FUNCTION(toString)) {
            return _ejs_Object_prototype_toString(_ejs_null, exp, 0, NULL);
        }

        // should we be checking if this returns a string?  i'd assume so...
        return _ejs_invoke_closure (toString, exp, 0, NULL);
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
    ejsval number = ToNumber(exp);
    /* 2. If number is NaN, return +0. */
    if (EJSVAL_EQ(number, _ejs_nan))
        return 0;
    /* 3. If number is +0, 0, +, or , return number. */
    double d = ToDouble(number);
    int classified = fpclassify(d);
    if (classified == FP_ZERO || classified == FP_INFINITE)
        return d;
    /* 4. Return the result of computing sign(number)  floor(abs(number) */
    int sign = d < 0 ? -1 : 1;
    return (int64_t)(sign * floor(abs(d)));
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
        ejsval new_boolean = _ejs_object_new (_ejs_Boolean_prototype, &_ejs_Boolean_specops);
        _ejs_invoke_closure (_ejs_Boolean, new_boolean, 1, &exp);
        return new_boolean;
    }
    else if (EJSVAL_IS_NUMBER(exp)) {
        ejsval new_number = _ejs_object_new (_ejs_Number_prototype, &_ejs_Number_specops);
        _ejs_invoke_closure (_ejs_Number, new_number, 1, &exp);
        return new_number;
    }
    else if (EJSVAL_IS_STRING(exp)) {
        ejsval new_str = _ejs_object_new (_ejs_String_prototype, &_ejs_String_specops);
        _ejs_invoke_closure (_ejs_String, new_str, 1, &exp);
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

typedef enum {
    TO_PRIM_HINT_DEFAULT,
    TO_PRIM_HINT_STRING,
    TO_PRIM_HINT_NUMBER
} ToPrimitiveHint;
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
        if (EJSVAL_IS_CALLABLE(method)) {
            //       i. Let result be the result of calling the [[Call]] internal method of method, with O as thisArgument and an empty List as argumentsList. 
            //       ii. ReturnIfAbrupt(result). 
            ejsval result = _ejs_invoke_closure(method, O, 0, NULL);
            //       iii. If Type(result) is not Object, then return result. 
            if (!EJSVAL_IS_OBJECT(result))
                return result;
        }
    }
    // 6. Throw a TypeError exception. 
    _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "couldn't convert object to primitive");
}

// ECMA262: 7.1.1 ToPrimitive
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
        //    a. Let result be the result of calling the [[Call]] internal method of exoticToPrim, with input argument as thisArgument and a List containing( hint) as argumentsList. 
        //    b. ReturnIfAbrupt(result). 
        ejsval result = _ejs_invoke_closure (exoticToPrim, inputargument, 1, &hint);
        //    c. If result is an ECMAScript language value and Type(result) is not Object, then return result. 
        if (!EJSVAL_IS_OBJECT(result))
            return result;
        //    d. Else, throw a TypeError exception. 
        else
            _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "@@toPrimitive returned an object");
    }
    // 7. If hint is "default" then, let hint be "number". 
    if (PreferredType == TO_PRIM_HINT_DEFAULT)
        PreferredType = TO_PRIM_HINT_NUMBER;

    // 8. Return OrdinaryToPrimitive(inputargument,hint).
    return OrdinaryToPrimitive(inputargument, PreferredType);
}

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
        //    a. If x is NaN and y is NaN, return true.
        if (isnan(EJSVAL_TO_NUMBER(x)) && isnan(EJSVAL_TO_NUMBER(y))) return EJS_TRUE;
        //    b. If x is +0 and y is -0, return false.
        if (EJSVAL_TO_NUMBER(x) == 0.0 && EJSDOUBLE_IS_NEGZERO(EJSVAL_TO_NUMBER(y))) return EJS_FALSE;
        //    c. If x is -0 and y is +0, return false.
        if (EJSDOUBLE_IS_NEGZERO(EJSVAL_TO_NUMBER(x)) == 0.0 && EJSVAL_TO_NUMBER(y) == 0) return EJS_FALSE;
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

// same as SameValue, except in its treatment of +/- 0
EJSBool
SameValueZero(ejsval x, ejsval y)
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
        //    a. If x is NaN and y is NaN, return true.
        if (isnan(EJSVAL_TO_NUMBER(x)) && isnan(EJSVAL_TO_NUMBER(y))) return EJS_TRUE;
        //    b. If x is +0 and y is -0, return true.
        if (EJSVAL_TO_NUMBER(x) == 0.0 && EJSDOUBLE_IS_NEGZERO(EJSVAL_TO_NUMBER(y))) return EJS_TRUE;
        //    c. If x is -0 and y is +0, return tryue.
        if (EJSDOUBLE_IS_NEGZERO(EJSVAL_TO_NUMBER(x)) == 0.0 && EJSVAL_TO_NUMBER(y) == 0) return EJS_TRUE;
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
    int lhs_int = ToInteger(lhs);
    int rhs_int = ToInteger(rhs);
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
    if (EJSVAL_IS_NUMBER(lhs)) {
        return BOOLEAN_TO_EJSVAL (EJSVAL_TO_NUMBER(lhs) < ToDouble (rhs));
    }
    else if (EJSVAL_IS_STRING(lhs)) {
        ejsval rhs_string = ToString(rhs);
        ejsval rhs_primStr;

        if (EJSVAL_IS_STRING(rhs_string))
            rhs_primStr = rhs_string;
        else
            rhs_primStr = ((EJSString*)EJSVAL_TO_STRING(rhs_string))->primStr;

        return BOOLEAN_TO_EJSVAL (ucs2_strcmp (EJSVAL_TO_FLAT_STRING(lhs), EJSVAL_TO_FLAT_STRING(rhs_primStr)) < 0);
    }
    else {
        // object+... how does js implement this anyway?
        EJS_NOT_IMPLEMENTED();
    }

    return _ejs_nan;
}

EJSBool
_ejs_op_lt_ejsbool (ejsval lhs, ejsval rhs)
{
    if (EJSVAL_IS_NUMBER(lhs)) {
        return EJSVAL_TO_NUMBER(lhs) < ToDouble (rhs);
    }
    else if (EJSVAL_IS_STRING(lhs)) {
        ejsval rhs_string = ToString(rhs);
        ejsval rhs_primStr;

        if (EJSVAL_IS_STRING(rhs_string))
            rhs_primStr = rhs_string;
        else
            rhs_primStr = ((EJSString*)EJSVAL_TO_STRING(rhs_string))->primStr;

        return ucs2_strcmp (EJSVAL_TO_FLAT_STRING(lhs), EJSVAL_TO_FLAT_STRING(rhs_primStr)) < 0;
    }
    else {
        // object+... how does js implement this anyway?
        EJS_NOT_IMPLEMENTED();
    }

    return EJS_FALSE;
}

ejsval
_ejs_op_le (ejsval lhs, ejsval rhs)
{
    if (EJSVAL_IS_NUMBER(lhs)) {
        return BOOLEAN_TO_EJSVAL (EJSVAL_TO_NUMBER(lhs) <= ToDouble (rhs));
    }
    else if (EJSVAL_IS_STRING(lhs)) {
        ejsval rhs_string = ToString(rhs);
        ejsval rhs_primStr;

        if (EJSVAL_IS_STRING(rhs_string))
            rhs_primStr = rhs_string;
        else
            rhs_primStr = ((EJSString*)EJSVAL_TO_OBJECT(rhs_string))->primStr;

        return BOOLEAN_TO_EJSVAL (ucs2_strcmp (EJSVAL_TO_FLAT_STRING(lhs), EJSVAL_TO_FLAT_STRING(rhs_primStr)) <= 0);
    }
    else {
        // object+... how does js implement this anyway?
        EJS_NOT_IMPLEMENTED();
    }

    return _ejs_nan;
}

ejsval
_ejs_op_gt (ejsval lhs, ejsval rhs)
{
    if (EJSVAL_IS_NUMBER(lhs)) {
        return BOOLEAN_TO_EJSVAL (EJSVAL_TO_NUMBER(lhs) > ToDouble (rhs));
    }
    else if (EJSVAL_IS_STRING(lhs)) {
        ejsval rhs_string = ToString(rhs);
        ejsval rhs_primStr;

        if (EJSVAL_IS_STRING(rhs_string))
            rhs_primStr = rhs_string;
        else
            rhs_primStr = ((EJSString*)EJSVAL_TO_OBJECT(rhs_string))->primStr;

        return BOOLEAN_TO_EJSVAL (ucs2_strcmp (EJSVAL_TO_FLAT_STRING(lhs), EJSVAL_TO_FLAT_STRING(rhs_primStr)) > 0);
    }
    else {
        // object+... how does js implement this anyway?
        EJS_NOT_IMPLEMENTED();
    }

    return _ejs_nan;
}

ejsval
_ejs_op_ge (ejsval lhs, ejsval rhs)
{
    if (EJSVAL_IS_NUMBER(lhs)) {
        return BOOLEAN_TO_EJSVAL (EJSVAL_TO_NUMBER(lhs) >= ToDouble (rhs));
    }
    else if (EJSVAL_IS_STRING(lhs)) {
        ejsval rhs_string = ToString(rhs);
        ejsval rhs_primStr;

        if (EJSVAL_IS_STRING(rhs_string))
            rhs_primStr = rhs_string;
        else
            rhs_primStr = ((EJSString*)EJSVAL_TO_OBJECT(rhs_string))->primStr;

        return BOOLEAN_TO_EJSVAL (ucs2_strcmp (EJSVAL_TO_FLAT_STRING(lhs), EJSVAL_TO_FLAT_STRING(rhs_primStr)) >= 0);
    }
    else {
        // object+... how does js implement this anyway?
        EJS_NOT_IMPLEMENTED();
    }

    return _ejs_nan;
}

ejsval
_ejs_op_sub (ejsval lhs, ejsval rhs)
{
    return NUMBER_TO_EJSVAL(ToDouble(lhs) - ToDouble(rhs));
}

ejsval
_ejs_op_strict_eq (ejsval lhs, ejsval rhs)
{
    if (EJSVAL_IS_NULL(lhs))
        return BOOLEAN_TO_EJSVAL (EJSVAL_IS_NULL(rhs));
    else if (EJSVAL_IS_NUMBER(lhs)) {
        return BOOLEAN_TO_EJSVAL (EJSVAL_IS_NUMBER(rhs) && EJSVAL_TO_NUMBER(lhs) == EJSVAL_TO_NUMBER(rhs));
    }
    else if (EJSVAL_IS_STRING(lhs)) {
        return BOOLEAN_TO_EJSVAL (EJSVAL_IS_STRING(rhs) && !ucs2_strcmp (EJSVAL_TO_FLAT_STRING(lhs), EJSVAL_TO_FLAT_STRING(rhs)));
    }
    else if (EJSVAL_IS_BOOLEAN(lhs)) {
        return BOOLEAN_TO_EJSVAL (EJSVAL_IS_BOOLEAN(rhs) && EJSVAL_TO_BOOLEAN(lhs) == EJSVAL_TO_BOOLEAN(rhs));
    }
    else {
        return BOOLEAN_TO_EJSVAL (EJSVAL_EQ(lhs, rhs));
    }
}

ejsval
_ejs_op_strict_neq (ejsval lhs, ejsval rhs)
{
    return BOOLEAN_TO_EJSVAL (!EJSVAL_TO_BOOLEAN (_ejs_op_strict_eq (lhs, rhs)));
}

// ECMA262: 11.9.1 + 11.9.3
ejsval
_ejs_op_eq (ejsval x, ejsval y)
{
    /* 1. If Type(x) is the same as Type(y), then */
    if (EJSVAL_TO_TAG(x) == EJSVAL_TO_TAG(y)) {
        /*    a. If Type(x) is Undefined, return true. */
        if (EJSVAL_IS_UNDEFINED(x)) return _ejs_true;
        /*    b. If Type(x) is Null, return true. */
        if (EJSVAL_IS_NULL(x)) return _ejs_true;
        /*    c. If Type(x) is Number, then */
        if (EJSVAL_IS_NUMBER(x)) {
            /*       i. If x is NaN, return false. */
            if (isnan(EJSVAL_TO_NUMBER(x))) return _ejs_false;
            /*       ii. If y is NaN, return false. */
            if (isnan(EJSVAL_TO_NUMBER(y))) return _ejs_false;
            /*       iii. If x is the same Number value as y, return true. */
            if (EJSVAL_TO_NUMBER(x) == EJSVAL_TO_NUMBER(y)) return _ejs_true;
            /*       iv. If x is +0 and y is -0, return true.*/
            /*       v. If x is -0 and y is +0, return true. */
            /*       vi. Return false. */
            return _ejs_false;
        }
        /*    d. If Type(x) is String, then return true if x and y are exactly the same sequence of characters (same  */
        /*       length and same characters in corresponding positions). Otherwise, return false. */
        if (EJSVAL_IS_STRING(x)) {
            if (EJSVAL_TO_STRLEN(x) != EJSVAL_TO_STRLEN(y)) return _ejs_false;

            // XXX there is doubtless a more efficient way to compare two ropes, but we convert but to flat strings for now.
            return ucs2_strcmp (EJSVAL_TO_FLAT_STRING(x), EJSVAL_TO_FLAT_STRING(y)) ? _ejs_false : _ejs_true;
        }
        /*    e. If Type(x) is Boolean, return true if x and y are both true or both false. Otherwise, return false. */
        if (EJSVAL_IS_BOOLEAN(x))
            return EJSVAL_TO_BOOLEAN(x) == EJSVAL_TO_BOOLEAN(y) ? _ejs_true : _ejs_false;

        /*       if. Return true if x and y refer to the same object. Otherwise, return false. */
        return (EJSVAL_TO_OBJECT(x) == EJSVAL_TO_OBJECT(y)) ? _ejs_true : _ejs_false;
    }
    
    /* 2. If x is null and y is undefined, return true. */
    if (EJSVAL_IS_NULL(x) && EJSVAL_IS_UNDEFINED(y)) return _ejs_true;
    /* 3. If x is undefined and y is null, return true. */
    if (EJSVAL_IS_UNDEFINED(x) && EJSVAL_IS_NULL(y)) return _ejs_true;
    /* 4. If Type(x) is Number and Type(y) is String, */
    if (EJSVAL_IS_NUMBER(x) && EJSVAL_IS_STRING(y)) {
        /*    return the result of the comparison x == ToNumber(y). */
        return _ejs_op_eq(x, ToNumber(y));
    }
    /* 5. If Type(x) is String and Type(y) is Number, */
    if (EJSVAL_IS_STRING(x) && EJSVAL_IS_NUMBER(y)) {
        /*    return the result of the comparison ToNumber(x) == y.*/
        return _ejs_op_eq(ToNumber(x), y);
    }
    /* 6. If Type(x) is Boolean, return the result of the comparison ToNumber(x) == y. */
    if (EJSVAL_IS_BOOLEAN(x)) {
        return _ejs_op_eq(ToNumber(x), y);
    }
    /* 7. If Type(y) is Boolean, return the result of the comparison x == ToNumber(y). */
    if (EJSVAL_IS_BOOLEAN(y)) {
        return _ejs_op_eq(x, ToNumber(y));
    }
    /* 8. If Type(x) is either String or Number and Type(y) is Object, */
    if (EJSVAL_IS_OBJECT(y) && (EJSVAL_IS_STRING(x) || EJSVAL_IS_STRING(x))) {
        /*    return the result of the comparison x == ToPrimitive(y). */
        return _ejs_op_eq(x, ToPrimitive(y, TO_PRIM_HINT_DEFAULT));
    }
    /* 9. If Type(x) is Object and Type(y) is either String or Number, */
    if (EJSVAL_IS_OBJECT(x) && (EJSVAL_IS_STRING(y) || EJSVAL_IS_STRING(y))) {
        /*    return the result of the comparison ToPrimitive(x) == y. */
        return _ejs_op_eq(ToPrimitive(x, TO_PRIM_HINT_DEFAULT), y);
    }
    /* 10. Return false. */
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
    if (!EJSVAL_IS_CALLABLE(C))
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
        ejsval result = _ejs_invoke_closure(instOfHandler, C, 1, &O);
        //    b. Return ToBoolean(result). 
        return ToBoolean(result);
    }

    // 5. If IsCallable(C) is false, then throw a TypeError exception. 
    if (!EJSVAL_IS_CALLABLE(C))
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
    return OP(obj,HasProperty) (rhs, lhs) ? _ejs_true : _ejs_false;
}

EJSBool
_ejs_truthy (ejsval val)
{
    return EJSVAL_EQ(_ejs_true, ToBoolean(val));
}


void
_ejs_throw (ejsval exp)
{
    _ejs_exception_throw (ToObject(exp));
}

void
_ejs_rethrow ()
{
    _ejs_exception_rethrow ();
}

ejsval
_ejs_isNaN_impl (ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
    ejsval num = _ejs_undefined;
    if (argc >= 1)
        num = args[0];

    return isnan(ToDouble(num)) ? _ejs_true : _ejs_false;
}

ejsval
_ejs_isFinite_impl (ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
    if (argc < 1)
        return _ejs_false;

    ejsval num = args[0];

    return isfinite(ToDouble(num)) ? _ejs_true : _ejs_false;
}

// ECMA262 15.1.2.2
// parseInt (string , radix)
ejsval
_ejs_parseInt_impl (ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
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

    jschar* radix_chars;
    switch (R) {
    case 8:
        radix_chars = radix_8_chars;
        break;
    case 10:
        radix_chars = radix_10_chars;
        break;
    case 16:
        radix_chars = radix_16_chars;
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
        if (ucs2_strstr(radix_chars, needle)== NULL)
            break;
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

        int digitval = (ucs2_strstr(radix_chars, needle) - radix_chars);
        mathInt = mathInt*R + digitval;
    }

    /* 14. Let number be the Number value for mathInt. */
    int32_t number = mathInt * sign;

    /* 15. Return sign * number */
    return NUMBER_TO_EJSVAL(number);
}

ejsval
_ejs_parseFloat_impl (ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
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
    if (!EJSVAL_IS_CALLABLE(method))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "method is not a function");

    /* 3. Let iterator be the result of calling the [[Call]] internal method of method with
     * obj as thisArgument and an empty List as argumentsList. */
    ejsval iterator = _ejs_invoke_closure (method, obj, 0, NULL);

    /* 4. If Type(iterator) is not Object, then throw a TypeError exception. */
    if (!EJSVAL_IS_OBJECT(iterator))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "iterator is not an object");

    return iterator;
}

/* 7.4.3 IteratorNext ( iterator, value ) */
ejsval
IteratorNext (ejsval iterator, ejsval value)
{
    ejsval result;

    /* 1. If value was not passed, */
    if (EJSVAL_IS_UNDEFINED(value))
        /* Let result be Invoke(iterator, "next", ( )). */
        result = _ejs_invoke_closure (Get(iterator, _ejs_atom_next), iterator, 0, NULL);
    /* 2. Else, */
    else
        /* a. Let result be Invoke(iterator, "next", (value)). */
        result = _ejs_invoke_closure (Get(iterator, _ejs_atom_next), iterator, 1, &value);

    /* 3. If Type(result) is not Object, then throw a TypeError exception. */
    if (!EJSVAL_IS_OBJECT(result))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "result is not an object");

    /* 5. Return result. */
    return result;
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

