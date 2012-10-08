#include <math.h>
#include <stdlib.h>

#include "ejs.h"
#include "ejs-value.h"
#include "ejs-object.h"
#include "ejs-number.h"
#include "ejs-string.h"
#include "ejs-ops.h"

EJSValue* NumberToString(double d)
{
  char num_buf[256];
  snprintf (num_buf, sizeof(num_buf), EJS_NUMBER_FORMAT, d);
  return _ejs_string_new_utf8 (num_buf);
}

// returns an EJSPrimString*.
// maybe we could change it to return a char* to match ToDouble?  that way string concat wouldn't create
// temporary strings for non-PrimString objects only to throw them away after concatenation?
EJSValue* ToString(EJSValue *exp)
{
  switch (exp->tag) {
  case EJSValueTagBoolean:
    return _ejs_string_new_utf8 (EJSVAL_TO_BOOLEAN(exp) ? "true" : "false");
  case EJSValueTagNumber: {
    return NumberToString(EJSVAL_TO_NUMBER(exp));
  }
  case EJSValueTagString:
    return exp;
  case EJSValueTagUndefined:
    return _ejs_string_new_utf8 ("undefined");
  case EJSValueTagObject: {
    EJSValue* toString = _ejs_object_getprop_utf8 (exp, "toString");
    if (!EJSVAL_IS_FUNCTION(toString))
      NOT_IMPLEMENTED();

    // should we be checking if this is a string?  i'd assume so...
    return _ejs_invoke_closure_0 (toString, exp, 0);
  }
  default:
    NOT_IMPLEMENTED();
  }
}

double ToDouble(EJSValue *exp)
{
  switch (exp->tag) {
  case EJSValueTagBoolean:
    return EJSVAL_TO_BOOLEAN(exp) ? 1 : 0;
  case EJSValueTagNumber:
    return EJSVAL_TO_NUMBER(exp);
  case EJSValueTagString:
    return atof(EJSVAL_TO_STRING(exp)); // XXX NaN
  case EJSValueTagUndefined:
    return 0; // XXX NaN
  case EJSValueTagObject:
    return 0; // XXX .length if exp is an array and .length > 0, otherwise NaN
  default:
    NOT_IMPLEMENTED();
  }
}

int ToInteger(EJSValue *exp)
{
  return (int)ToDouble(exp);
}

EJSValue* ToObject(EJSValue *exp)
{
  switch (exp->tag) {
  case EJSValueTagBoolean:
    NOT_IMPLEMENTED();
  case EJSValueTagNumber: {
    EJSObject* new_number = _ejs_number_alloc_instance();
    _ejs_init_object (new_number, _ejs_number_get_prototype());
    return _ejs_invoke_closure_1 (_ejs_Number, (EJSValue*)new_number, 1, exp);
  }
  case EJSValueTagString: {
    EJSObject* new_str = _ejs_string_alloc_instance();
    _ejs_init_object (new_str, _ejs_string_get_prototype());
    return _ejs_invoke_closure_1 (_ejs_String, (EJSValue*)new_str, 1, exp);
  }
  case EJSValueTagUndefined:
    return exp; // XXX
  case EJSValueTagObject:
    return exp;
  default:
    NOT_IMPLEMENTED();
  }
}

EJSValue* ToBoolean(EJSValue *exp)
{
  if (exp == NULL)
    return _ejs_false;

  switch (exp->tag) {
  case EJSValueTagBoolean:
    return exp;
  case EJSValueTagNumber:
    return EJSVAL_TO_NUMBER(exp) == 0 ? _ejs_false : _ejs_true;
  case EJSValueTagString:
    return EJSVAL_TO_STRLEN(exp) == 0 ? _ejs_false : _ejs_true;
  case EJSValueTagUndefined:
    return _ejs_false;
  case EJSValueTagObject:
    return _ejs_false; // XXX this breaks for any of the builtin objects that wrap primitive types.
  default:
    NOT_IMPLEMENTED();
  }
}

EJSValue*
_ejs_op_neg (EJSValue* exp)
{
  return _ejs_number_new (-ToDouble(exp));
}

EJSValue*
_ejs_op_plus (EJSValue* exp)
{
  return _ejs_number_new (ToDouble(exp));
}

EJSValue*
_ejs_op_not (EJSValue* exp)
{
  EJSBool truthy= _ejs_truthy (exp);
  return _ejs_boolean_new (!truthy);
}

EJSValue*
_ejs_op_void (EJSValue* exp)
{
  return _ejs_undefined;
}

EJSValue*
_ejs_op_typeof (EJSValue* exp)
{
  char *rv;
  if (exp == NULL) {
    rv = "object";
  }
  else 
    switch (exp->tag) {
    case EJSValueTagBoolean:   rv = "boolean"; break;
    case EJSValueTagNumber:    rv = "number"; break;
    case EJSValueTagString:    rv = "string"; break;
    case EJSValueTagUndefined: rv = "undefined"; break;
    case EJSValueTagObject:
      if (EJSVAL_IS_FUNCTION(exp))
	rv = "function";
      else
	rv = "object";
      break;
    default:
      NOT_IMPLEMENTED();
  }

  return _ejs_string_new_utf8 (rv);
}

EJSValue*
_ejs_op_delete (EJSValue* obj, EJSValue* prop)
{
  return _ejs_true;
}

EJSValue*
_ejs_op_mod (EJSValue* lhs, EJSValue* rhs)
{
  if (EJSVAL_IS_NUMBER(lhs)) {
    if (EJSVAL_IS_NUMBER(rhs)) {
      return _ejs_number_new (fmod(EJSVAL_TO_NUMBER(lhs), EJSVAL_TO_NUMBER(rhs)));
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

  return NULL;
}

EJSValue*
_ejs_op_bitwise_and (EJSValue* lhs, EJSValue* rhs)
{
  int lhs_int = ToInteger(lhs);
  int rhs_int = ToInteger(rhs);
  return _ejs_number_new (lhs_int & rhs_int);
}

EJSValue*
_ejs_op_bitwise_or (EJSValue* lhs, EJSValue* rhs)
{
  int lhs_int = ToInteger(lhs);
  int rhs_int = ToInteger(rhs);
  return _ejs_number_new (lhs_int | rhs_int);
}

EJSValue*
_ejs_op_rsh (EJSValue* lhs, EJSValue* rhs)
{
  if (EJSVAL_IS_NUMBER(lhs)) {
    if (EJSVAL_IS_NUMBER(rhs)) {
      return _ejs_number_new ((int)((int)EJSVAL_TO_NUMBER(lhs) >> (((unsigned int)EJSVAL_TO_NUMBER(rhs)) & 0x1f)));
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

  return NULL;
}

EJSValue*
_ejs_op_ursh (EJSValue* lhs, EJSValue* rhs)
{
  if (EJSVAL_IS_NUMBER(lhs)) {
    if (EJSVAL_IS_NUMBER(rhs)) {
      return _ejs_number_new ((unsigned int)((unsigned int)EJSVAL_TO_NUMBER(lhs) >> (((unsigned int)EJSVAL_TO_NUMBER(rhs)) & 0x1f)));
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

  return NULL;
}

EJSValue*
_ejs_op_add (EJSValue* lhs, EJSValue* rhs)
{
  if (EJSVAL_IS_NUMBER(lhs)) {
    return _ejs_number_new (EJSVAL_TO_NUMBER(lhs) + ToDouble (rhs));
  }
  else if (EJSVAL_IS_STRING(lhs)) {
    EJSValue *rhstring = ToString(rhs);

    char *combined = malloc (EJSVAL_TO_STRLEN(lhs) + EJSVAL_TO_STRLEN(rhstring) + 1);
    strcpy (combined, EJSVAL_TO_STRING(lhs));
    strcpy (combined + EJSVAL_TO_STRLEN(lhs), EJSVAL_TO_STRING(rhstring));
    EJSValue* result = _ejs_string_new_utf8(combined);
    free(combined);
    return result;
  }
  else {
    // object+... how does js implement this anyway?
    NOT_IMPLEMENTED();
  }

  return NULL;
}

EJSValue*
_ejs_op_mult (EJSValue* lhs, EJSValue* rhs)
{
  if (EJSVAL_IS_NUMBER(lhs)) {
    return _ejs_number_new (EJSVAL_TO_NUMBER(lhs) * ToDouble (rhs));
  }
  else if (EJSVAL_IS_STRING(lhs)) {
    // string+ with anything we don't implement yet - it will call toString() on objects, and convert a number to a string
    NOT_IMPLEMENTED();
  }
  else {
    // object+... how does js implement this anyway?
    NOT_IMPLEMENTED();
  }

  return NULL;
}

EJSValue*
_ejs_op_div (EJSValue* lhs, EJSValue* rhs)
{
  if (EJSVAL_IS_NUMBER(lhs)) {
    return _ejs_number_new (EJSVAL_TO_NUMBER(lhs) / ToDouble (rhs));
  }
  else if (EJSVAL_IS_STRING(lhs)) {
    // string+ with anything we don't implement yet - it will call toString() on objects, and convert a number to a string
    NOT_IMPLEMENTED();
  }
  else {
    // object+... how does js implement this anyway?
    NOT_IMPLEMENTED();
  }

  return NULL;
}

EJSValue*
_ejs_op_lt (EJSValue* lhs, EJSValue* rhs)
{
  if (EJSVAL_IS_NUMBER(lhs)) {
    return _ejs_boolean_new (EJSVAL_TO_NUMBER(lhs) < ToDouble (rhs));
  }
  else if (EJSVAL_IS_STRING(lhs)) {
    EJSValue* rhs_string = ToString(rhs);
    EJSValue* rhs_primStr;

    if (EJSVAL_IS_STRING(rhs_string))
      rhs_primStr = rhs_string;
    else
      rhs_primStr = ((EJSString*)rhs_string)->primStr;

    return _ejs_boolean_new (strcmp (EJSVAL_TO_STRING(lhs), EJSVAL_TO_STRING(rhs_primStr)) < 0);
  }
  else {
    // object+... how does js implement this anyway?
    NOT_IMPLEMENTED();
  }

  return NULL;
}

EJSValue*
_ejs_op_le (EJSValue* lhs, EJSValue* rhs)
{
  if (EJSVAL_IS_NUMBER(lhs)) {
    return _ejs_boolean_new (EJSVAL_TO_NUMBER(lhs) <= ToDouble (rhs));
  }
  else if (EJSVAL_IS_STRING(lhs)) {
    EJSValue* rhs_string = ToString(rhs);
    EJSValue* rhs_primStr;

    if (EJSVAL_IS_STRING(rhs_string))
      rhs_primStr = rhs_string;
    else
      rhs_primStr = ((EJSString*)rhs_string)->primStr;

    return _ejs_boolean_new (strcmp (EJSVAL_TO_STRING(lhs), EJSVAL_TO_STRING(rhs_primStr)) <= 0);
  }
  else {
    // object+... how does js implement this anyway?
    NOT_IMPLEMENTED();
  }

  return NULL;
}

EJSValue*
_ejs_op_gt (EJSValue* lhs, EJSValue* rhs)
{
  if (EJSVAL_IS_NUMBER(lhs)) {
    return _ejs_boolean_new (EJSVAL_TO_NUMBER(lhs) > ToDouble (rhs));
  }
  else if (EJSVAL_IS_STRING(lhs)) {
    EJSValue* rhs_string = ToString(rhs);
    EJSValue* rhs_primStr;

    if (EJSVAL_IS_STRING(rhs_string))
      rhs_primStr = rhs_string;
    else
      rhs_primStr = ((EJSString*)rhs_string)->primStr;

    return _ejs_boolean_new (strcmp (EJSVAL_TO_STRING(lhs), EJSVAL_TO_STRING(rhs_primStr)) > 0);
  }
  else {
    // object+... how does js implement this anyway?
    NOT_IMPLEMENTED();
  }

  return NULL;
}

EJSValue*
_ejs_op_ge (EJSValue* lhs, EJSValue* rhs)
{
  if (EJSVAL_IS_NUMBER(lhs)) {
    return _ejs_boolean_new (EJSVAL_TO_NUMBER(lhs) >= ToDouble (rhs));
  }
  else if (EJSVAL_IS_STRING(lhs)) {
    EJSValue* rhs_string = ToString(rhs);
    EJSValue* rhs_primStr;

    if (EJSVAL_IS_STRING(rhs_string))
      rhs_primStr = rhs_string;
    else
      rhs_primStr = ((EJSString*)rhs_string)->primStr;

    return _ejs_boolean_new (strcmp (EJSVAL_TO_STRING(lhs), EJSVAL_TO_STRING(rhs_primStr)) >= 0);
  }
  else {
    // object+... how does js implement this anyway?
    NOT_IMPLEMENTED();
  }

  return NULL;
}

EJSValue*
_ejs_op_sub (EJSValue* lhs, EJSValue* rhs)
{
  if (EJSVAL_IS_NUMBER(lhs)) {
    return _ejs_number_new (EJSVAL_TO_NUMBER(lhs) - ToDouble (rhs));
  }
  else if (EJSVAL_IS_STRING(lhs)) {
    // string+ with anything we don't implement yet - it will call toString() on objects, and convert a number to a string
    NOT_IMPLEMENTED();
  }
  else {
    // object+... how does js implement this anyway?
    NOT_IMPLEMENTED();
  }

  return NULL;
}

EJSValue*
_ejs_op_strict_eq (EJSValue* lhs, EJSValue* rhs)
{
  if (!lhs)
    return _ejs_boolean_new (rhs == NULL);
  else if (EJSVAL_IS_NUMBER(lhs)) {
    return _ejs_boolean_new (rhs && EJSVAL_IS_NUMBER(rhs) && EJSVAL_TO_NUMBER(lhs) == EJSVAL_TO_NUMBER(rhs));
  }
  else if (EJSVAL_IS_STRING(lhs)) {
    return _ejs_boolean_new (rhs && EJSVAL_IS_STRING(rhs) && !strcmp (EJSVAL_TO_STRING(lhs), EJSVAL_TO_STRING(rhs)));
  }
  else if (EJSVAL_IS_BOOLEAN(lhs)) {
    return _ejs_boolean_new (rhs && EJSVAL_IS_BOOLEAN(rhs) && EJSVAL_TO_BOOLEAN(lhs) == EJSVAL_TO_BOOLEAN(rhs));
  }
  else {
    return _ejs_boolean_new (lhs == rhs);
  }
}

EJSValue*
_ejs_op_strict_neq (EJSValue* lhs, EJSValue* rhs)
{
  if (!lhs)
    return _ejs_boolean_new (rhs != NULL);
  else if (EJSVAL_IS_NUMBER(lhs)) {
    return _ejs_boolean_new (!rhs || !EJSVAL_IS_NUMBER(rhs) || EJSVAL_TO_NUMBER(lhs) != EJSVAL_TO_NUMBER(rhs));
  }
  else if (EJSVAL_IS_STRING(lhs)) {
    return _ejs_boolean_new (!rhs || !EJSVAL_IS_STRING(rhs) || strcmp (EJSVAL_TO_STRING(lhs), EJSVAL_TO_STRING(rhs)));
  }
  else if (EJSVAL_IS_BOOLEAN(lhs)) {
    return _ejs_boolean_new (!rhs || !EJSVAL_IS_BOOLEAN(rhs) || EJSVAL_TO_BOOLEAN(lhs) != EJSVAL_TO_BOOLEAN(rhs));
  }
  else {
    return _ejs_boolean_new (lhs != rhs);
  }
}

EJSValue*
_ejs_op_eq (EJSValue* lhs, EJSValue* rhs)
{
  if (lhs == NULL) {
    return _ejs_boolean_new (rhs == NULL || EJSVAL_IS_UNDEFINED(rhs));
  }
  else if (EJSVAL_IS_UNDEFINED(lhs)) {
    return _ejs_boolean_new (rhs == NULL || EJSVAL_IS_UNDEFINED(rhs));
  }
  else if (EJSVAL_IS_NUMBER(lhs)) {
    return _ejs_boolean_new (rhs && EJSVAL_TO_NUMBER(lhs) == ToDouble(rhs));
  }
  else if (EJSVAL_IS_STRING(lhs)) {
    EJSBool eq;
    if (!rhs)
      eq = FALSE;
    else {
      EJSValue* rhstring = ToString(rhs);
      eq = (!strcmp (EJSVAL_TO_STRING(lhs), EJSVAL_TO_STRING(rhstring)));
    }
    return _ejs_boolean_new(eq);
  }
  else if (EJSVAL_IS_UNDEFINED(lhs)) {
    return _ejs_boolean_new (!rhs || EJSVAL_IS_UNDEFINED(rhs));
  }
  else if (EJSVAL_IS_OBJECT(lhs)) {
    return _ejs_boolean_new (rhs && lhs == rhs);
  }

  NOT_IMPLEMENTED();
}

EJSValue*
_ejs_op_neq (EJSValue* lhs, EJSValue* rhs)
{
  if (lhs == NULL) {
    return _ejs_boolean_new (rhs != NULL && !EJSVAL_IS_UNDEFINED(rhs));
  }
  else if (EJSVAL_IS_UNDEFINED(lhs)) {
    return _ejs_boolean_new (rhs != NULL && !EJSVAL_IS_UNDEFINED(rhs));
  }
  else if (EJSVAL_IS_NUMBER(lhs)) {
    return _ejs_boolean_new (!rhs || (EJSVAL_TO_NUMBER(lhs) != ToDouble(rhs)));
  }
  else if (EJSVAL_IS_STRING(lhs)) {
    EJSBool neq;
    if (!rhs)
      neq = TRUE;
    else {
      EJSValue* rhstring = ToString(rhs);
      neq = (strcmp (EJSVAL_TO_STRING(lhs), EJSVAL_TO_STRING(rhstring)));
    }
    return _ejs_boolean_new (neq);
  }
  else if (EJSVAL_IS_UNDEFINED(lhs)) {
    return _ejs_boolean_new (!rhs || !EJSVAL_IS_UNDEFINED(rhs));
  }
  else if (EJSVAL_IS_OBJECT(lhs)) {
    return _ejs_boolean_new (!rhs || lhs != rhs);
  }

  NOT_IMPLEMENTED();
}

EJSValue*
_ejs_op_instanceof (EJSValue* lhs, EJSValue* rhs)
{
  if (EJSVAL_IS_PRIMITIVE(lhs))
    return _ejs_false;
  NOT_IMPLEMENTED();
}

EJSValue*
_ejs_op_in (EJSValue* lhs, EJSValue* rhs)
{
  NOT_IMPLEMENTED();
}

EJSBool
_ejs_truthy (EJSValue* val)
{
  return _ejs_true == ToBoolean(val);
}


void
_ejs_throw (EJSValue* exp)
{
  NOT_IMPLEMENTED();
}

EJSValue*
_ejs_isNaN (EJSValue *env, EJSValue* _this, int argc, EJSValue** args)
{
  NOT_IMPLEMENTED();
}

EJSValue*
_ejs_isFinite (EJSValue *env, EJSValue* _this, int argc, EJSValue** args)
{
  NOT_IMPLEMENTED();
}

EJSValue*
_ejs_parseInt (EJSValue *env, EJSValue* _this, int argc, EJSValue** args)
{
  NOT_IMPLEMENTED();
}

EJSValue*
_ejs_parseFloat (EJSValue *env, EJSValue* _this, int argc, EJSValue** args)
{
  if (argc == 0)
    return _ejs_nan;

  EJSValue* arg0 = ToString(args[0]);

  return _ejs_number_new (strtod (EJSVAL_TO_STRING(arg0), NULL));
}
