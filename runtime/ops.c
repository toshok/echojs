#include "ejs.h"
#include "object.h"
#include "ops.h"

EJSBool
_ejs_op_add (EJSValue* lhs, EJSValue* rhs, EJSValue** result)
{
  *result = NULL;

  if (EJSVAL_IS_NUMBER(lhs)) {
    if (EJSVAL_IS_NUMBER(rhs)) {
      *result = _ejs_number_new (EJSVAL_TO_NUMBER(lhs) + EJSVAL_TO_NUMBER(rhs));
      return TRUE;
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

  return FALSE;
}

EJSBool
_ejs_op_sub (EJSValue* lhs, EJSValue* rhs, EJSValue** result)
{
  *result = NULL;

  if (EJSVAL_IS_NUMBER(lhs)) {
    if (EJSVAL_IS_NUMBER(rhs)) {
      *result = _ejs_number_new (EJSVAL_TO_NUMBER(lhs) - EJSVAL_TO_NUMBER(rhs));
      return TRUE;
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

  return FALSE;
}

EJSBool
_ejs_op_strict_eq (EJSValue* lhs, EJSValue* rhs, EJSValue** result)
{
  *result = NULL;

  if (EJSVAL_IS_NUMBER(lhs)) {
    if (EJSVAL_IS_NUMBER(rhs)) {
      *result = _ejs_boolean_new (EJSVAL_TO_NUMBER(lhs) == EJSVAL_TO_NUMBER(rhs));
      return TRUE;
    }
  }

  abort();
}

EJSBool
_ejs_truthy (EJSValue* val, EJSBool *result)
{
  *result = FALSE;

  if (EJSVAL_IS_NUMBER(val)) {
    *result = EJSVAL_TO_NUMBER(val) != 0;
    return TRUE;
  }
  else if (EJSVAL_IS_BOOLEAN(val)) {
    *result = EJSVAL_TO_BOOLEAN(val);
    return TRUE;
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
