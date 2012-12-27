#include <math.h>
#include <stdlib.h>

#include "ejs.h"
#include "ejs-exception.h"
#include "ejs-value.h"
#include "ejs-function.h"
#include "ejs-number.h"
#include "ejs-object.h"
#include "ejs-string.h"
#include "ejs-ops.h"

ejsval NumberToString(double d)
{
  char num_buf[256];
  snprintf (num_buf, sizeof(num_buf), EJS_NUMBER_FORMAT, d);
  return _ejs_string_new_utf8 (num_buf);
}

static void NumberToStringBuf(char* buf, size_t buf_size, double d)
{
  snprintf (buf, buf_size, EJS_NUMBER_FORMAT, d);
}

// returns an EJSPrimString*.
// maybe we could change it to return a char* to match ToDouble?  that way string concat wouldn't create
// temporary strings for non-PrimString objects only to throw them away after concatenation?
ejsval ToString(ejsval exp)
{
  if (EJSVAL_IS_NULL(exp))
    return _ejs_string_new_utf8 ("null");
  else if (EJSVAL_IS_BOOLEAN(exp)) 
    return _ejs_string_new_utf8 (EJSVAL_TO_BOOLEAN(exp) ? "true" : "false");
  else if (EJSVAL_IS_NUMBER(exp))
    return NumberToString(EJSVAL_TO_NUMBER(exp));
  else if (EJSVAL_IS_STRING(exp))
    return exp;
  else if (EJSVAL_IS_UNDEFINED(exp))
    return _ejs_string_new_utf8 ("undefined");
  else if (EJSVAL_IS_OBJECT(exp)) {
    ejsval toString = _ejs_object_getprop_utf8 (exp, "toString");
    // XXX nanboxing breaks this if (!EJSVAL_IS_FUNCTION(toString))
    // NOT_IMPLEMENTED();

    // should we be checking if this is a string?  i'd assume so...
    return _ejs_invoke_closure_0 (toString, exp, 0);
  }
  else
    NOT_IMPLEMENTED();
}

double ToDouble(ejsval exp)
{
  if (EJSVAL_IS_NUMBER(exp))
    return EJSVAL_TO_NUMBER(exp);
  else if (EJSVAL_IS_BOOLEAN(exp))
    return EJSVAL_TO_BOOLEAN(exp) ? 1 : 0;
  else if (EJSVAL_IS_STRING(exp))
    return atof(EJSVAL_TO_STRING(exp)); // XXX NaN
  else if (EJSVAL_IS_UNDEFINED(exp))
    return 0; // XXX NaN
  else if (EJSVAL_IS_OBJECT(exp))
    // XXX if it's an array
    //       and .length == 0, return 0.
    //       and .length == 1, return ToDouble(array->elements[0]) - yes, it's recursive
    //       else return NaN
    // for anything else, NaN
    return 0;
  else
    NOT_IMPLEMENTED();
}

int ToInteger(ejsval exp)
{
  return (int)ToDouble(exp);
}

ejsval ToObject(ejsval exp)
{
  if (EJSVAL_IS_BOOLEAN(exp))
    NOT_IMPLEMENTED();
  else if (EJSVAL_IS_NUMBER(exp)) {
    EJSObject* new_number = _ejs_number_alloc_instance();
    _ejs_init_object (new_number, _ejs_Number_proto, &_ejs_number_specops);
    return _ejs_invoke_closure_1 (_ejs_Number, OBJECT_TO_EJSVAL(new_number), 1, exp);
  }
  else if (EJSVAL_IS_STRING(exp)) {
    EJSObject* new_str = _ejs_string_alloc_instance();
    _ejs_init_object (new_str, _ejs_String_proto, &_ejs_string_specops);
    return _ejs_invoke_closure_1 (_ejs_String, OBJECT_TO_EJSVAL(new_str), 1, exp);
  }
  else if (EJSVAL_IS_UNDEFINED(exp))
    return exp; // XXX
  else if (EJSVAL_IS_OBJECT(exp))
    return exp;
  else
    NOT_IMPLEMENTED();
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
    return _ejs_false; // XXX this breaks for any of the builtin objects that wrap primitive types.
  else
    NOT_IMPLEMENTED();
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
_ejs_op_void (ejsval exp)
{
  return _ejs_undefined;
}

ejsval
_ejs_op_typeof (ejsval exp)
{
  char *rv;
  if (EJSVAL_IS_NULL(exp))
    rv = "object";
  else if (EJSVAL_IS_BOOLEAN(exp))
    rv = "boolean";
  else if (EJSVAL_IS_STRING(exp))
    rv = "string";
  else if (EJSVAL_IS_NUMBER(exp))
    rv = "number";
  else if (EJSVAL_IS_UNDEFINED(exp))
    rv = "undefined";
  else if (EJSVAL_IS_OBJECT(exp)) {
    if (EJSVAL_IS_FUNCTION(exp))
      rv = "function";
    else
      rv = "object";
  }
  else
    NOT_IMPLEMENTED();

  return _ejs_string_new_utf8 (rv);
}

ejsval
_ejs_op_delete (ejsval obj, ejsval prop)
{
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
      NOT_IMPLEMENTED();
    }
  }
  else if (EJSVAL_IS_STRING(lhs)) {
    // string+ with anything we don't implement yet - it will call toString() on objects, and convert a number to a string
    NOT_IMPLEMENTED();
  }
  else {
    // object+... how does js implement this anyway?
    NOT_IMPLEMENTED();
  }

  return _ejs_nan;
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
      NOT_IMPLEMENTED();
    }
  }
  else if (EJSVAL_IS_STRING(lhs)) {
    // string+ with anything we don't implement yet - it will call toString() on objects, and convert a number to a string
    NOT_IMPLEMENTED();
  }
  else {
    // object+... how does js implement this anyway?
    NOT_IMPLEMENTED();
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
      NOT_IMPLEMENTED();
    }
  }
  else if (EJSVAL_IS_STRING(lhs)) {
    // string+ with anything we don't implement yet - it will call toString() on objects, and convert a number to a string
    NOT_IMPLEMENTED();
  }
  else {
    // object+... how does js implement this anyway?
    NOT_IMPLEMENTED();
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
    if (EJSVAL_IS_NUMBER(rhs)) {
      char buf[256];
      NumberToStringBuf(buf, sizeof(buf), EJSVAL_TO_NUMBER(rhs));
      int rhs_len = strlen(buf);
      int lhs_len = EJSVAL_TO_STRLEN(lhs);

      if (lhs_len + rhs_len >= (sizeof(buf) - 1)) {
	char *combined = malloc (lhs_len + rhs_len + 1);
	strcpy (combined, EJSVAL_TO_STRING(lhs));
	strcpy (combined + lhs_len, buf);
	ejsval result = _ejs_string_new_utf8(combined);
	free(combined);
	rv = result;
      }
      else {
	memmove (buf + lhs_len, buf, rhs_len);
	memmove (buf, EJSVAL_TO_STRING(lhs), lhs_len);
	buf[lhs_len + rhs_len] = 0;
	rv = _ejs_string_new_utf8(buf);
      }
    }
    else {
      ADD_STACK_ROOT(ejsval, rhstring, ToString(rhs));

      char *combined = malloc (EJSVAL_TO_STRLEN(lhs) + EJSVAL_TO_STRLEN(rhstring) + 1);
      strcpy (combined, EJSVAL_TO_STRING(lhs));
      strcpy (combined + EJSVAL_TO_STRLEN(lhs), EJSVAL_TO_STRING(rhstring));
      ejsval result = _ejs_string_new_utf8(combined);
      free(combined);
      rv = result;
    }
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
    NOT_IMPLEMENTED();
  }
  else {
    // object+... how does js implement this anyway?
    NOT_IMPLEMENTED();
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
    NOT_IMPLEMENTED();
  }
  else {
    // object+... how does js implement this anyway?
    NOT_IMPLEMENTED();
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

    return BOOLEAN_TO_EJSVAL (strcmp (EJSVAL_TO_STRING(lhs), EJSVAL_TO_STRING(rhs_primStr)) < 0);
  }
  else {
    // object+... how does js implement this anyway?
    NOT_IMPLEMENTED();
  }

  return _ejs_nan;
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

    return BOOLEAN_TO_EJSVAL (strcmp (EJSVAL_TO_STRING(lhs), EJSVAL_TO_STRING(rhs_primStr)) <= 0);
  }
  else {
    // object+... how does js implement this anyway?
    NOT_IMPLEMENTED();
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

    return BOOLEAN_TO_EJSVAL (strcmp (EJSVAL_TO_STRING(lhs), EJSVAL_TO_STRING(rhs_primStr)) > 0);
  }
  else {
    // object+... how does js implement this anyway?
    NOT_IMPLEMENTED();
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

    return BOOLEAN_TO_EJSVAL (strcmp (EJSVAL_TO_STRING(lhs), EJSVAL_TO_STRING(rhs_primStr)) >= 0);
  }
  else {
    // object+... how does js implement this anyway?
    NOT_IMPLEMENTED();
  }

  return _ejs_nan;
}

ejsval
_ejs_op_sub (ejsval lhs, ejsval rhs)
{
  if (EJSVAL_IS_NUMBER(lhs)) {
    return NUMBER_TO_EJSVAL (EJSVAL_TO_NUMBER(lhs) - ToDouble (rhs));
  }
  else if (EJSVAL_IS_STRING(lhs)) {
    // string+ with anything we don't implement yet - it will call toString() on objects, and convert a number to a string
    NOT_IMPLEMENTED();
  }
  else {
    // object+... how does js implement this anyway?
    NOT_IMPLEMENTED();
  }

  return _ejs_nan;
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
    return BOOLEAN_TO_EJSVAL (EJSVAL_IS_STRING(rhs) && !strcmp (EJSVAL_TO_STRING(lhs), EJSVAL_TO_STRING(rhs)));
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
  if (EJSVAL_IS_NULL(lhs))
    return BOOLEAN_TO_EJSVAL (!EJSVAL_IS_NULL(rhs));
  else if (EJSVAL_IS_NUMBER(lhs)) {
    return BOOLEAN_TO_EJSVAL (!EJSVAL_IS_NUMBER(rhs) || EJSVAL_TO_NUMBER(lhs) != EJSVAL_TO_NUMBER(rhs));
  }
  else if (EJSVAL_IS_STRING(lhs)) {
    return BOOLEAN_TO_EJSVAL (!EJSVAL_IS_STRING(rhs) || strcmp (EJSVAL_TO_STRING(lhs), EJSVAL_TO_STRING(rhs)));
  }
  else if (EJSVAL_IS_BOOLEAN(lhs)) {
    return BOOLEAN_TO_EJSVAL (!EJSVAL_IS_BOOLEAN(rhs) || EJSVAL_TO_BOOLEAN(lhs) != EJSVAL_TO_BOOLEAN(rhs));
  }
  else {
    return BOOLEAN_TO_EJSVAL (!EJSVAL_EQ(lhs, rhs));
  }
}

ejsval
_ejs_op_eq (ejsval lhs, ejsval rhs)
{
  if (EJSVAL_IS_NULL(lhs)) {
    return BOOLEAN_TO_EJSVAL (EJSVAL_IS_NULL(rhs) || EJSVAL_IS_UNDEFINED(rhs));
  }
  else if (EJSVAL_IS_UNDEFINED(lhs)) {
    return BOOLEAN_TO_EJSVAL (EJSVAL_IS_NULL(rhs) || EJSVAL_IS_UNDEFINED(rhs));
  }
  else if (EJSVAL_IS_NUMBER(lhs)) {
    return BOOLEAN_TO_EJSVAL (!EJSVAL_IS_NULL(rhs) && EJSVAL_TO_NUMBER(lhs) == ToDouble(rhs));
  }
  else if (EJSVAL_IS_STRING(lhs)) {
    EJSBool eq;
    if (EJSVAL_IS_NULL(rhs))
      eq = EJS_FALSE;
    else {
      ejsval rhstring = ToString(rhs);
      eq = (!strcmp (EJSVAL_TO_STRING(lhs), EJSVAL_TO_STRING(rhstring)));
    }
    return BOOLEAN_TO_EJSVAL(eq);
  }
  else if (EJSVAL_IS_OBJECT(lhs)) {
    return BOOLEAN_TO_EJSVAL (EJSVAL_TO_OBJECT(lhs) == EJSVAL_TO_OBJECT(ToObject(rhs)));
  }

  NOT_IMPLEMENTED();
}

ejsval
_ejs_op_neq (ejsval lhs, ejsval rhs)
{
  if (EJSVAL_IS_NULL(lhs)) {
    return BOOLEAN_TO_EJSVAL (!EJSVAL_IS_NULL(rhs) && !EJSVAL_IS_UNDEFINED(rhs));
  }
  else if (EJSVAL_IS_UNDEFINED(lhs)) {
    return BOOLEAN_TO_EJSVAL (!EJSVAL_IS_NULL(rhs) && !EJSVAL_IS_UNDEFINED(rhs));
  }
  else if (EJSVAL_IS_NUMBER(lhs)) {
    return BOOLEAN_TO_EJSVAL (EJSVAL_IS_NULL(rhs) || (EJSVAL_TO_NUMBER(lhs) != ToDouble(rhs)));
  }
  else if (EJSVAL_IS_STRING(lhs)) {
    EJSBool neq;
    if (EJSVAL_IS_NULL(rhs))
      neq = EJS_TRUE;
    else {
      ejsval rhstring = ToString(rhs);
      neq = (strcmp (EJSVAL_TO_STRING(lhs), EJSVAL_TO_STRING(rhstring)));
    }
    return BOOLEAN_TO_EJSVAL (neq);
  }
  else if (EJSVAL_IS_UNDEFINED(lhs)) {
    return BOOLEAN_TO_EJSVAL (EJSVAL_IS_NULL(rhs) || !EJSVAL_IS_UNDEFINED(rhs));
  }
  else if (EJSVAL_IS_OBJECT(lhs)) {
    return BOOLEAN_TO_EJSVAL (EJSVAL_IS_NULL(rhs) || EJSVAL_TO_OBJECT(lhs) != EJSVAL_TO_OBJECT(ToObject(rhs)));
  }

  NOT_IMPLEMENTED();
}

ejsval
_ejs_op_instanceof (ejsval lhs, ejsval rhs)
{
  if (EJSVAL_IS_PRIMITIVE(lhs))
    return _ejs_false;
  NOT_IMPLEMENTED();
}

ejsval
_ejs_op_in (ejsval lhs, ejsval rhs)
{
  NOT_IMPLEMENTED();
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
_ejs_isNaN (ejsval env, ejsval _this, int argc, ejsval* args)
{
  NOT_IMPLEMENTED();
}

ejsval
_ejs_isFinite (ejsval env, ejsval _this, int argc, ejsval* args)
{
  NOT_IMPLEMENTED();
}

ejsval
_ejs_parseInt (ejsval env, ejsval _this, int argc, ejsval* args)
{
  NOT_IMPLEMENTED();
}

ejsval
_ejs_parseFloat (ejsval env, ejsval _this, int argc, ejsval* args)
{
  if (argc == 0)
    return _ejs_nan;

  ejsval arg0 = ToString(args[0]);

  return NUMBER_TO_EJSVAL (strtod (EJSVAL_TO_STRING(arg0), NULL));
}
