#include <math.h>

#include "ejs.h"
#include "ejs-value.h"
#include "ejs-object.h"
#include "ejs-ops.h"

EJSValue*
_ejs_op_neg (EJSValue* exp)
{
  switch (exp->type) {
  case EJSValueTypeBoolean:
    return EJSVAL_TO_BOOLEAN(exp) ? _ejs_number_new (-1) : _ejs_number_new (0);
  case EJSValueTypeNumber:
    return _ejs_number_new (-EJSVAL_TO_NUMBER(exp));
  case EJSValueTypeString:
    return _ejs_number_new (0); // XXX NaN
  case EJSValueTypeUndefined:
  case EJSValueTypeObject:
  case EJSValueTypeFunction:
    return _ejs_number_new (0); // XXX NaN
    return _ejs_number_new (0); // XXX NaN
  case EJSValueTypeArray:
    return _ejs_number_new (0); // XXX NaN if .length > 0
  default:
    abort();
  }
}

EJSValue*
_ejs_op_plus (EJSValue* exp)
{
  switch (exp->type) {
  case EJSValueTypeBoolean:
    return EJSVAL_TO_BOOLEAN(exp) ? _ejs_number_new (1) : _ejs_number_new (0);
  case EJSValueTypeNumber:
    return _ejs_number_new (EJSVAL_TO_NUMBER(exp));
  case EJSValueTypeString:
    return _ejs_number_new (0); // XXX NaN
  case EJSValueTypeUndefined:
  case EJSValueTypeObject:
  case EJSValueTypeFunction:
    return _ejs_number_new (0); // XXX NaN
    return _ejs_number_new (0); // XXX NaN
  case EJSValueTypeArray:
    return _ejs_number_new (0); // XXX NaN if .length > 0
  default:
    abort();
  }
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
  if (exp == NULL) {
    return _ejs_string_new_utf8 ("object");
  }
  else if (EJSVAL_IS_UNDEFINED(exp)) {
    return _ejs_string_new_utf8 ("undefined");
  }
  abort();
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
      abort();
    }
  }
  else if (EJSVAL_IS_STRING(lhs)) {
    // string+ with anything we don't implement yet - it will call toString() on objects, and convert a number to a string
    abort();
  }
  else {
    // object+... how does js implement this anyway?
    abort();
  }

  return NULL;
}

EJSValue*
_ejs_op_bitwise_and (EJSValue* lhs, EJSValue* rhs)
{
  if (EJSVAL_IS_NUMBER(lhs)) {
    if (EJSVAL_IS_NUMBER(rhs)) {
      return _ejs_number_new ((unsigned int)EJSVAL_TO_NUMBER(lhs) & (unsigned int)EJSVAL_TO_NUMBER(rhs));
    }
    else {
      // need to call valueOf() on the object, or convert the string to a number
      abort();
    }
  }
  else if (EJSVAL_IS_STRING(lhs)) {
    // string+ with anything we don't implement yet - it will call toString() on objects, and convert a number to a string
    abort();
  }
  else {
    // object+... how does js implement this anyway?
    abort();
  }

  return NULL;
}

EJSValue*
_ejs_op_bitwise_or (EJSValue* lhs, EJSValue* rhs)
{
  if (EJSVAL_IS_NUMBER(lhs)) {
    if (EJSVAL_IS_NUMBER(rhs)) {
      return _ejs_number_new ((unsigned int)EJSVAL_TO_NUMBER(lhs) | (unsigned int)EJSVAL_TO_NUMBER(rhs));
    }
    else {
      // need to call valueOf() on the object, or convert the string to a number
      abort();
    }
  }
  else if (EJSVAL_IS_STRING(lhs)) {
    // string+ with anything we don't implement yet - it will call toString() on objects, and convert a number to a string
    abort();
  }
  else {
    // object+... how does js implement this anyway?
    abort();
  }

  return NULL;
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
      abort();
    }
  }
  else if (EJSVAL_IS_STRING(lhs)) {
    // string+ with anything we don't implement yet - it will call toString() on objects, and convert a number to a string
    abort();
  }
  else {
    // object+... how does js implement this anyway?
    abort();
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
      abort();
    }
  }
  else if (EJSVAL_IS_STRING(lhs)) {
    // string+ with anything we don't implement yet - it will call toString() on objects, and convert a number to a string
    abort();
  }
  else {
    // object+... how does js implement this anyway?
    abort();
  }

  return NULL;
}

EJSValue*
_ejs_op_add (EJSValue* lhs, EJSValue* rhs)
{
  if (EJSVAL_IS_NUMBER(lhs)) {
    if (EJSVAL_IS_NUMBER(rhs)) {
      return _ejs_number_new (EJSVAL_TO_NUMBER(lhs) + EJSVAL_TO_NUMBER(rhs));
    }
    else {
      // need to call valueOf() on the object, or convert the string to a number
      abort();
    }
  }
  else if (EJSVAL_IS_STRING(lhs)) {
    // string+ with anything we don't implement yet - it will call toString() on objects, and convert a number to a string
    abort();
  }
  else {
    // object+... how does js implement this anyway?
    abort();
  }

  return NULL;
}

EJSValue*
_ejs_op_mult (EJSValue* lhs, EJSValue* rhs)
{
  if (EJSVAL_IS_NUMBER(lhs)) {
    if (EJSVAL_IS_NUMBER(rhs)) {
      return _ejs_number_new (EJSVAL_TO_NUMBER(lhs) * EJSVAL_TO_NUMBER(rhs));
    }
    else {
      // need to call valueOf() on the object, or convert the string to a number
      abort();
    }
  }
  else if (EJSVAL_IS_STRING(lhs)) {
    // string+ with anything we don't implement yet - it will call toString() on objects, and convert a number to a string
    abort();
  }
  else {
    // object+... how does js implement this anyway?
    abort();
  }

  return NULL;
}

EJSValue*
_ejs_op_div (EJSValue* lhs, EJSValue* rhs)
{
  if (EJSVAL_IS_NUMBER(lhs)) {
    if (EJSVAL_IS_NUMBER(rhs)) {
      return _ejs_number_new (EJSVAL_TO_NUMBER(lhs) / EJSVAL_TO_NUMBER(rhs));
    }
    else {
      // need to call valueOf() on the object, or convert the string to a number
      abort();
    }
  }
  else if (EJSVAL_IS_STRING(lhs)) {
    // string+ with anything we don't implement yet - it will call toString() on objects, and convert a number to a string
    abort();
  }
  else {
    // object+... how does js implement this anyway?
    abort();
  }

  return NULL;
}

EJSValue*
_ejs_op_lt (EJSValue* lhs, EJSValue* rhs)
{
  if (EJSVAL_IS_NUMBER(lhs)) {
    if (EJSVAL_IS_NUMBER(rhs)) {
      return _ejs_boolean_new (EJSVAL_TO_NUMBER(lhs) < EJSVAL_TO_NUMBER(rhs));
    }
    else {
      // need to call valueOf() on the object, or convert the string to a number
      abort();
    }
  }
  else if (EJSVAL_IS_STRING(lhs)) {
    // string+ with anything we don't implement yet - it will call toString() on objects, and convert a number to a string
    abort();
  }
  else {
    // object+... how does js implement this anyway?
    abort();
  }

  return NULL;
}

EJSValue*
_ejs_op_le (EJSValue* lhs, EJSValue* rhs)
{
  if (EJSVAL_IS_NUMBER(lhs)) {
    if (EJSVAL_IS_NUMBER(rhs)) {
      return _ejs_boolean_new (EJSVAL_TO_NUMBER(lhs) <= EJSVAL_TO_NUMBER(rhs));
    }
    else {
      // need to call valueOf() on the object, or convert the string to a number
      abort();
    }
  }
  else if (EJSVAL_IS_STRING(lhs)) {
    // string+ with anything we don't implement yet - it will call toString() on objects, and convert a number to a string
    abort();
  }
  else {
    // object+... how does js implement this anyway?
    abort();
  }

  return NULL;
}

EJSValue*
_ejs_op_gt (EJSValue* lhs, EJSValue* rhs)
{
  if (EJSVAL_IS_NUMBER(lhs)) {
    if (EJSVAL_IS_NUMBER(rhs)) {
      return _ejs_boolean_new (EJSVAL_TO_NUMBER(lhs) > EJSVAL_TO_NUMBER(rhs));
    }
    else {
      // need to call valueOf() on the object, or convert the string to a number
      abort();
    }
  }
  else if (EJSVAL_IS_STRING(lhs)) {
    // string+ with anything we don't implement yet - it will call toString() on objects, and convert a number to a string
    abort();
  }
  else {
    // object+... how does js implement this anyway?
    abort();
  }

  return NULL;
}

EJSValue*
_ejs_op_ge (EJSValue* lhs, EJSValue* rhs)
{
  if (EJSVAL_IS_NUMBER(lhs)) {
    if (EJSVAL_IS_NUMBER(rhs)) {
      return _ejs_boolean_new (EJSVAL_TO_NUMBER(lhs) >= EJSVAL_TO_NUMBER(rhs));
    }
    else {
      // need to call valueOf() on the object, or convert the string to a number
      abort();
    }
  }
  else if (EJSVAL_IS_STRING(lhs)) {
    // string+ with anything we don't implement yet - it will call toString() on objects, and convert a number to a string
    abort();
  }
  else {
    // object+... how does js implement this anyway?
    abort();
  }

  return NULL;
}

EJSValue*
_ejs_op_sub (EJSValue* lhs, EJSValue* rhs)
{
  if (EJSVAL_IS_NUMBER(lhs)) {
    if (EJSVAL_IS_NUMBER(rhs)) {
      return _ejs_number_new (EJSVAL_TO_NUMBER(lhs) - EJSVAL_TO_NUMBER(rhs));
    }
    else {
      // need to call valueOf() on the object, or convert the string to a number
      abort();
    }
  }
  else if (EJSVAL_IS_STRING(lhs)) {
    // string+ with anything we don't implement yet - it will call toString() on objects, and convert a number to a string
    abort();
  }
  else {
    // object+... how does js implement this anyway?
    abort();
  }

  return NULL;
}

EJSValue*
_ejs_op_strict_eq (EJSValue* lhs, EJSValue* rhs)
{
  if (EJSVAL_IS_NUMBER(lhs)) {
    if (EJSVAL_IS_NUMBER(rhs)) {
      return _ejs_boolean_new (EJSVAL_TO_NUMBER(lhs) == EJSVAL_TO_NUMBER(rhs));
    }
  }
  else if (EJSVAL_IS_STRING(lhs)) {
    if (EJSVAL_IS_STRING(rhs)) {
      return _ejs_boolean_new (!strcmp (EJSVAL_TO_STRING(lhs), EJSVAL_TO_STRING(rhs)));
    }
  }

  abort();
}

EJSValue*
_ejs_op_strict_neq (EJSValue* lhs, EJSValue* rhs)
{
  if (EJSVAL_IS_NUMBER(lhs)) {
    if (EJSVAL_IS_NUMBER(rhs)) {
      return _ejs_boolean_new (EJSVAL_TO_NUMBER(lhs) != EJSVAL_TO_NUMBER(rhs));
    }
  }
  else if (EJSVAL_IS_STRING(lhs)) {
    if (EJSVAL_IS_STRING(rhs)) {
      return _ejs_boolean_new (strcmp (EJSVAL_TO_STRING(lhs), EJSVAL_TO_STRING(rhs)));
    }
  }

  abort();
}

EJSValue*
_ejs_op_eq (EJSValue* lhs, EJSValue* rhs)
{
  if (EJSVAL_IS_NUMBER(lhs)) {
    if (EJSVAL_IS_NUMBER(rhs)) {
      return _ejs_boolean_new (EJSVAL_TO_NUMBER(lhs) == EJSVAL_TO_NUMBER(rhs));
    }
  }
  else if (EJSVAL_IS_STRING(lhs)) {
    if (EJSVAL_IS_STRING(rhs)) {
      return _ejs_boolean_new (!strcmp (EJSVAL_TO_STRING(lhs), EJSVAL_TO_STRING(rhs)));
    }
  }

  abort();
}

EJSValue*
_ejs_op_neq (EJSValue* lhs, EJSValue* rhs)
{
  if (EJSVAL_IS_NUMBER(lhs)) {
    if (EJSVAL_IS_NUMBER(rhs)) {
      return _ejs_boolean_new (EJSVAL_TO_NUMBER(lhs) != EJSVAL_TO_NUMBER(rhs));
    }
  }
  else if (EJSVAL_IS_STRING(lhs)) {
    if (EJSVAL_IS_STRING(rhs)) {
      return _ejs_boolean_new (strcmp (EJSVAL_TO_STRING(lhs), EJSVAL_TO_STRING(rhs)));
    }
  }

  abort();
}

EJSValue*
_ejs_op_instanceof (EJSValue* lhs, EJSValue* rhs)
{
  abort();
}

EJSValue*
_ejs_op_in (EJSValue* lhs, EJSValue* rhs)
{
  abort();
}

EJSBool
_ejs_truthy (EJSValue* val)
{
  if (EJSVAL_IS_NUMBER(val)) {
    return EJSVAL_TO_NUMBER(val) != 0;
  }
  else if (EJSVAL_IS_BOOLEAN(val)) {
    return EJSVAL_TO_BOOLEAN(val);
  }

  abort();
  return FALSE;
}

EJSValue* _ejs_print;
EJSValue*
_ejs_print_impl (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  if (argc < 1)
    return _ejs_undefined;

  EJSValue *arg = args[0];
  if (arg == NULL) {
    printf ("(null)\n");
  }
  else if (EJSVAL_IS_NUMBER(arg)) {
    printf (EJS_NUMBER_FORMAT "\n", arg->u.n.data);
  }
  else if (EJSVAL_IS_STRING(arg)) {
    printf ("%s\n", arg->u.s.data);
  }
  else {
    abort();
  }
  return _ejs_undefined;
}
