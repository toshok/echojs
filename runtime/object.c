#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "object.h"

EJSValue*
_ejs_object_new (EJSValue *proto)
{
  EJSValue* rv = (EJSValue*)calloc(1, sizeof (EJSValue));
  rv->type = EJSValueTypeObject;
  rv->u.o.proto = proto;
  return rv;
}

EJSValue*
_ejs_string_new_utf8 (char* str)
{
  int str_len = strlen(str);
  int value_size = sizeof (EJSValueType) + sizeof (int) + str_len + 1;

  EJSValue* rv = (EJSValue*)calloc(1, value_size);
  rv->type = EJSValueTypeString;
  rv->u.s.len = str_len;
  memmove (&rv->u.s.data, str, str_len + 1);
  return rv;
}

EJSValue*
_ejs_number_new (double value)
{
  EJSValue* rv = (EJSValue*)calloc(1, sizeof (EJSValue));
  rv->type = EJSValueTypeNumber;
  rv->u.n.data = value;
  return rv;
}

EJSValue*
_ejs_boolean_new (EJSBool value)
{
  EJSValue* rv = (EJSValue*)calloc(1, sizeof (EJSValue));
  rv->type = EJSValueTypeBoolean;
  rv->u.b.data = value;
  return rv;
}

EJSValue*
_ejs_closure_new (EJSClosureFunc0 func, EJSClosureEnv* env)
{
  EJSValue* rv = (EJSValue*)calloc(1, sizeof (EJSValue));
  rv->type = EJSValueTypeClosure;;
  rv->u.closure.func = func;
  rv->u.closure.env = env;
  return rv;
}

EJSBool
_ejs_object_setprop (EJSValue* obj, EJSValue* key, EJSValue* value)
{
  if (!EJSVAL_IS_OBJECT(obj)) {
    printf ("setprop on !object\n");
    return FALSE;
  }

  return TRUE;
}

EJSBool
_ejs_object_getprop (EJSValue* obj, EJSValue* key, EJSValue** value)
{
  if (!EJSVAL_IS_OBJECT(obj)) {
    printf ("setprop on !object\n");
    return FALSE;
  }

  return TRUE;
}

EJSValue*
_ejs_invoke_closure_0 (EJSContext* context, EJSValue* closure, int argc)
{
  assert (EJSVAL_IS_CLOSURE(closure));
  return ((EJSClosureFunc0)closure->u.closure.func) (context, closure, argc);
}

EJSValue*
_ejs_invoke_closure_1 (EJSContext* context, EJSValue* closure, int argc, EJSValue* arg1)
{
  assert (EJSVAL_IS_CLOSURE(closure));
  return ((EJSClosureFunc1)closure->u.closure.func) (context, closure, argc, arg1);
}

EJSValue*
_ejs_invoke_closure_2 (EJSContext* context, EJSValue* closure, int argc, EJSValue* arg1, EJSValue* arg2)
{
  assert (EJSVAL_IS_CLOSURE(closure));
  return ((EJSClosureFunc2)closure->u.closure.func) (context, closure, argc, arg1, arg2);
}

EJSValue*
_ejs_invoke_closure_3 (EJSContext* context, EJSValue* closure, int argc, EJSValue* arg1, EJSValue* arg2, EJSValue* arg3)
{
  assert (EJSVAL_IS_CLOSURE(closure));
  return ((EJSClosureFunc3)closure->u.closure.func) (context, closure, argc, arg1, arg2, arg3);
}
