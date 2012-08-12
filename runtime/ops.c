#include "ejs.h"
#include "object.h"
#include "ops.h"
#include "math.h"

EJSValue*
_ejs_op_not (EJSValue* exp)
{
  EJSBool truthy= _ejs_truthy (exp);
  return _ejs_boolean_new (!truthy);
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
void
_ejs_print_impl (EJSValue* env, int argc, EJSValue *val)
{
  if (val == NULL) {
    printf ("(null)\n");
  }
  else if (EJSVAL_IS_NUMBER(val)) {
    printf (EJS_NUMBER_FORMAT "\n", val->u.n.data);
  }
  else if (EJSVAL_IS_STRING(val)) {
    printf ("%s\n", val->u.s.data);
  }
  else {
    abort();
  }
}
