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
#include "ejs-boolean.h"
#include "ejs-ops.h"

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

ejsval NumberToString(double d)
{
    int32_t i;
    if (EJSDOUBLE_IS_INT32(d, &i)) {
        char int_buf[UINT32_CHAR_BUFFER_LENGTH+1];
        char *cp = IntToCString(int_buf, i, 10);
        return _ejs_string_new_utf8 (cp);
    }

    char num_buf[256];
    snprintf (num_buf, sizeof(num_buf), EJS_NUMBER_FORMAT, d);
    return _ejs_string_new_utf8 (num_buf);
}

// returns an EJSPrimString*.
// maybe we could change it to return a char* to match ToDouble?  that way string concat wouldn't create
// temporary strings for non-PrimString objects only to throw them away after concatenation?
ejsval ToString(ejsval exp)
{
    if (EJSVAL_IS_NULL(exp))
        return _ejs_atom_null;
    else if (EJSVAL_IS_BOOLEAN(exp)) 
        return EJSVAL_TO_BOOLEAN(exp) ? _ejs_atom_true : _ejs_atom_false;
    else if (EJSVAL_IS_NUMBER(exp))
        return NumberToString(EJSVAL_TO_NUMBER(exp));
    else if (EJSVAL_IS_STRING(exp))
        return exp;
    else if (EJSVAL_IS_UNDEFINED(exp))
        return _ejs_atom_undefined;
    else if (EJSVAL_IS_OBJECT(exp)) {
        ejsval toString = _ejs_object_getprop_utf8 (exp, "toString");
        // XXX nanboxing breaks this if (!EJSVAL_IS_FUNCTION(toString))
        // EJS_NOT_IMPLEMENTED();

        // should we be checking if this is a string?  i'd assume so...
        return _ejs_invoke_closure_0 (toString, exp, 0);
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
    else if (EJSVAL_IS_STRING(exp))
        return NUMBER_TO_EJSVAL(atof(EJSVAL_TO_FLAT_STRING(exp))); // XXX NaN
    else if (EJSVAL_IS_UNDEFINED(exp))
        return _ejs_zero; // XXX NaN
    else if (EJSVAL_IS_OBJECT(exp)) {
        if (EJSVAL_IS_DATE(exp)) {
            return NUMBER_TO_EJSVAL(mktime(&((EJSDate*)EJSVAL_TO_OBJECT(exp))->tm));
        }
        // XXX if it's an array
        //       and .length == 0, return 0.
        //       and .length == 1, return ToDouble(array->elements[0]) - yes, it's recursive
        //       else return NaN
        // for anything else, NaN
        return _ejs_zero;
    }
    else
        EJS_NOT_IMPLEMENTED();
}

double ToDouble(ejsval exp)
{
    return EJSVAL_TO_NUMBER(ToNumber(exp));
}

int32_t ToInteger(ejsval exp)
{
    // XXX sorely lacking
    return (int)ToDouble(exp);
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
        ejsval new_boolean = _ejs_object_new (_ejs_Boolean_proto, &_ejs_boolean_specops);
        _ejs_invoke_closure_1 (_ejs_Boolean, new_boolean, 1, exp);
        return new_boolean;
    }
    else if (EJSVAL_IS_NUMBER(exp)) {
        ejsval new_number = _ejs_object_new (_ejs_Number_proto, &_ejs_number_specops);
        _ejs_invoke_closure_1 (_ejs_Number, new_number, 1, exp);
        return new_number;
    }
    else if (EJSVAL_IS_STRING(exp)) {
        ejsval new_str = _ejs_object_new (_ejs_String_prototype, &_ejs_string_specops);
        _ejs_invoke_closure_1 (_ejs_String, new_str, 1, exp);
        return new_str;
    }
    else if (EJSVAL_IS_UNDEFINED(exp))
        return exp; // XXX
    else if (EJSVAL_IS_OBJECT(exp))
        return exp;
    else
        EJS_NOT_IMPLEMENTED();
}

ejsval ToBoolean(ejsval exp)
{
    if (EJSVAL_IS_NULL(exp) || EJSVAL_IS_UNDEFINED(exp))
        return _ejs_false;
    else if (EJSVAL_IS_BOOLEAN(exp))
        return exp;
    else if (EJSVAL_IS_NUMBER(exp))
        return EJSVAL_TO_NUMBER(exp) == 0 ? _ejs_false : _ejs_true;
    else if (EJSVAL_IS_STRING(exp))
        return EJSVAL_TO_STRLEN(exp) == 0 ? _ejs_false : _ejs_true;
    else if (EJSVAL_IS_OBJECT(exp))
        return _ejs_true;
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

ejsval ToPrimitive(ejsval exp)
{
    if (EJSVAL_IS_OBJECT(exp)) {
        return OP(EJSVAL_TO_OBJECT(exp),default_value) (exp, "PreferredType");
    }
    else
        return exp;
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
_ejs_op_typeof (ejsval exp)
{
    if (EJSVAL_IS_NULL(exp))
        return _ejs_atom_object;
    else if (EJSVAL_IS_BOOLEAN(exp))
        return _ejs_atom_boolean;
    else if (EJSVAL_IS_STRING(exp))
        return _ejs_atom_string;
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

    OP(obj_,_delete)(obj, prop, EJS_TRUE);

    return _ejs_true;
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
    START_SHADOW_STACK_FRAME;

    ejsval rv = _ejs_nan;

    ejsval lprim, rprim;

    lprim = ToPrimitive(lhs);
    rprim = ToPrimitive(rhs);

    if (EJSVAL_IS_STRING(lhs) || EJSVAL_IS_STRING(rhs)) {
        ADD_STACK_ROOT(ejsval, lhstring, ToString(lhs));
        ADD_STACK_ROOT(ejsval, rhstring, ToString(rhs));

        ADD_STACK_ROOT(ejsval, result, _ejs_string_concat (lhstring, rhstring));
        rv = result;
    }
    else {
        rv = NUMBER_TO_EJSVAL (ToDouble(lprim) + ToDouble(rprim));
    }

    END_SHADOW_STACK_FRAME;
    return rv;
}

ejsval
_ejs_op_mult (ejsval lhs, ejsval rhs)
{
    if (EJSVAL_IS_NUMBER(lhs)) {
        return NUMBER_TO_EJSVAL (EJSVAL_TO_NUMBER(lhs) * ToDouble (rhs));
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

        return BOOLEAN_TO_EJSVAL (strcmp (EJSVAL_TO_FLAT_STRING(lhs), EJSVAL_TO_FLAT_STRING(rhs_primStr)) < 0);
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

        return strcmp (EJSVAL_TO_FLAT_STRING(lhs), EJSVAL_TO_FLAT_STRING(rhs_primStr)) < 0;
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

        return BOOLEAN_TO_EJSVAL (strcmp (EJSVAL_TO_FLAT_STRING(lhs), EJSVAL_TO_FLAT_STRING(rhs_primStr)) <= 0);
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

        return BOOLEAN_TO_EJSVAL (strcmp (EJSVAL_TO_FLAT_STRING(lhs), EJSVAL_TO_FLAT_STRING(rhs_primStr)) > 0);
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

        return BOOLEAN_TO_EJSVAL (strcmp (EJSVAL_TO_FLAT_STRING(lhs), EJSVAL_TO_FLAT_STRING(rhs_primStr)) >= 0);
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
        return BOOLEAN_TO_EJSVAL (EJSVAL_IS_STRING(rhs) && !strcmp (EJSVAL_TO_FLAT_STRING(lhs), EJSVAL_TO_FLAT_STRING(rhs)));
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
            /*       i. If x is NaN, return false.© Ecma International 2011 81 */
            if (isnan(EJSVAL_TO_NUMBER(x))) return _ejs_false;
            /*       ii. If y is NaN, return false. */
            if (isnan(EJSVAL_TO_NUMBER(y))) return _ejs_false;
            /*       iii. If x is the same Number value as y, return true. */
            if (EJSVAL_TO_NUMBER(x) == EJSVAL_TO_NUMBER(y)) return _ejs_true;
            /*       iv. If x is +0 and y is 0, return true.*/
            /*       v. If x is 0 and y is +0, return true. */
            /*       vi. Return false. */
            return _ejs_false;
        }
        /*    d. If Type(x) is String, then return true if x and y are exactly the same sequence of characters (same  */
        /*       length and same characters in corresponding positions). Otherwise, return false. */
        if (EJSVAL_IS_STRING(x)) {
            if (EJSVAL_TO_STRLEN(x) != EJSVAL_TO_STRLEN(y)) return _ejs_false;

            // XXX there is doubtless a more efficient way to compare two ropes, but we convert but to flat strings for now.
            return strcmp (EJSVAL_TO_FLAT_STRING(x), EJSVAL_TO_FLAT_STRING(y)) ? _ejs_false : _ejs_true;
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
        return _ejs_op_eq(x, ToPrimitive(y));
    }
    /* 9. If Type(x) is Object and Type(y) is either String or Number, */
    if (EJSVAL_IS_OBJECT(x) && (EJSVAL_IS_STRING(y) || EJSVAL_IS_STRING(y))) {
        /*    return the result of the comparison ToPrimitive(x) == y. */
        return _ejs_op_eq(ToPrimitive(x), y);
    }
    /* 10. Return false. */
    return _ejs_false;
}

ejsval
_ejs_op_neq (ejsval lhs, ejsval rhs)
{
    return BOOLEAN_TO_EJSVAL (!EJSVAL_TO_BOOLEAN (_ejs_op_eq (lhs, rhs)));
}

ejsval
_ejs_op_instanceof (ejsval lhs, ejsval rhs)
{
    if (EJSVAL_IS_PRIMITIVE(lhs))
        return _ejs_false;
    EJS_NOT_IMPLEMENTED();
}

ejsval
_ejs_op_in (ejsval lhs, ejsval rhs)
{
    EJS_NOT_IMPLEMENTED();
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
_ejs_isNaN (ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
    EJS_NOT_IMPLEMENTED();
}

ejsval
_ejs_isFinite (ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
    EJS_NOT_IMPLEMENTED();
}

ejsval
_ejs_parseInt (ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
    EJS_NOT_IMPLEMENTED();
}

ejsval
_ejs_parseFloat (ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
    if (argc == 0)
        return _ejs_nan;

    ejsval arg0 = ToString(args[0]);

    return NUMBER_TO_EJSVAL (strtod (EJSVAL_TO_FLAT_STRING(arg0), NULL));
}
