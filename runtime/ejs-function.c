#include <assert.h>

#include "ejs-value.h"
#include "ejs-object.h"
#include "ejs-function.h"

EJSValue*
_ejs_function_new (EJSClosureEnv* env, EJSClosureFunc func)
{
  EJSFunction *rv = (EJSFunction*)calloc (1, sizeof(EJSFunction));

  _ejs_init_object ((EJSObject*)rv, _ejs_function_get_prototype());

  rv->func = func;
  rv->env = env;

  return (EJSValue*)rv;
}


EJSValue* _ejs_Function;
static EJSValue*
_ejs_Function_impl (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  printf ("Function() called either as a function or a constructor is not supported in ejs\n");
  abort();
}

static EJSValue* _ejs_Function_proto;
EJSValue*
_ejs_function_get_prototype()
{
  return _ejs_Function_proto;
}

// ECMA262 15.3.4.2
static EJSValue*
_ejs_Function_prototype_toString (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  abort();
}

// ECMA262 15.3.4.3
static EJSValue*
_ejs_Function_prototype_apply (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  abort();
}

// ECMA262 15.3.4.4
static EJSValue*
_ejs_Function_prototype_call (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  assert (EJSVAL_IS_FUNCTION(_this));
  EJSValue* thisArg = _ejs_undefined;
  
  if (argc > 0) {
    thisArg = args[0];
    args = &args[1];
    argc --;
  }

  return EJSVAL_TO_FUNC(_this) (EJSVAL_TO_ENV(_this), thisArg, argc, argc == 0 ? NULL : args);
}

// ECMA262 15.3.4.5
static EJSValue*
_ejs_Function_prototype_bind (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  abort();
}

void
_ejs_function_init(EJSValue *global)
{
  _ejs_Function = _ejs_function_new (NULL, (EJSClosureFunc)_ejs_Function_impl);
  _ejs_Function_proto = _ejs_object_new(_ejs_object_get_prototype());

  // ECMA262 15.3.3.1
  _ejs_object_setprop_utf8 (_ejs_Function,       "prototype",  _ejs_Function_proto); // FIXME:  { [[Writable]]: false, [[Enumerable]]: false, [[Configurable]]: false }.
  // ECMA262 15.3.3.2
  _ejs_object_setprop_utf8 (_ejs_Function,       "length",     _ejs_number_new(1)); // FIXME:  { [[Writable]]: false, [[Enumerable]]: false, [[Configurable]]: false }.

#define OBJ_METHOD(x) _ejs_object_setprop_utf8 (_ejs_Function, #x, _ejs_function_new (NULL, (EJSClosureFunc)_ejs_Function_##x))
#define PROTO_METHOD(x) _ejs_object_setprop_utf8 (_ejs_Function_proto, #x, _ejs_function_new (NULL, (EJSClosureFunc)_ejs_Function_prototype_##x))

  PROTO_METHOD(toString);
  PROTO_METHOD(apply);
  PROTO_METHOD(call);
  PROTO_METHOD(bind);

#undef PROTOTYPE_METHOD
#undef OBJ_METHOD

  _ejs_object_setprop_utf8 (global, "Function", _ejs_Function);
}



EJSValue*
_ejs_invoke_closure_0 (EJSValue* closure, EJSValue* _this, int argc)
{
  assert (EJSVAL_IS_FUNCTION(closure));
  return EJSVAL_TO_FUNC(closure) (EJSVAL_TO_ENV(closure), _this, argc, NULL);
}

EJSValue*
_ejs_invoke_closure_1 (EJSValue* closure, EJSValue* _this, int argc, EJSValue* arg1)
{
  assert (EJSVAL_IS_FUNCTION(closure));
  EJSValue *args[] = { arg1 };
  return EJSVAL_TO_FUNC(closure) (EJSVAL_TO_ENV(closure), _this, argc, args);
}

EJSValue*
_ejs_invoke_closure_2 (EJSValue* closure, EJSValue* _this, int argc, EJSValue* arg1, EJSValue* arg2)
{
  assert (EJSVAL_IS_FUNCTION(closure));
  EJSValue *args[] = { arg1, arg2 };
  return EJSVAL_TO_FUNC(closure) (EJSVAL_TO_ENV(closure), _this, argc, args);
}

EJSValue*
_ejs_invoke_closure_3 (EJSValue* closure, EJSValue* _this, int argc, EJSValue* arg1, EJSValue* arg2, EJSValue* arg3)
{
  assert (EJSVAL_IS_FUNCTION(closure));
  EJSValue *args[] = { arg1, arg2, arg3 };
  return EJSVAL_TO_FUNC(closure) (EJSVAL_TO_ENV(closure), _this, argc, args);
}

EJSValue*
_ejs_invoke_closure_4 (EJSValue* closure, EJSValue* _this, int argc, EJSValue* arg1, EJSValue* arg2, EJSValue* arg3, EJSValue* arg4)
{
  assert (EJSVAL_IS_FUNCTION(closure));
  EJSValue *args[] = { arg1, arg2, arg3, arg4 };
  return EJSVAL_TO_FUNC(closure) (EJSVAL_TO_ENV(closure), _this, argc, args);
}

EJSValue*
_ejs_invoke_closure_5 (EJSValue* closure, EJSValue* _this, int argc, EJSValue* arg1, EJSValue* arg2, EJSValue* arg3, EJSValue* arg4, EJSValue* arg5)
{
  assert (EJSVAL_IS_FUNCTION(closure));
  EJSValue *args[] = { arg1, arg2, arg3, arg4, arg5 };
  return EJSVAL_TO_FUNC(closure) (EJSVAL_TO_ENV(closure), _this, argc, args);
}

EJSValue*
_ejs_invoke_closure_6 (EJSValue* closure, EJSValue* _this, int argc, EJSValue* arg1, EJSValue* arg2, EJSValue* arg3, EJSValue* arg4, EJSValue* arg5, EJSValue* arg6)
{
  assert (EJSVAL_IS_FUNCTION(closure));
  EJSValue *args[] = { arg1, arg2, arg3, arg4, arg5, arg6 };
  return EJSVAL_TO_FUNC(closure) (EJSVAL_TO_ENV(closure), _this, argc, args);
}

EJSValue*
_ejs_invoke_closure_7 (EJSValue* closure, EJSValue* _this, int argc, EJSValue* arg1, EJSValue* arg2, EJSValue* arg3, EJSValue* arg4, EJSValue* arg5, EJSValue* arg6, EJSValue* arg7)
{
  assert (EJSVAL_IS_FUNCTION(closure));
  EJSValue *args[] = { arg1, arg2, arg3, arg4, arg5, arg6, arg7 };
  return EJSVAL_TO_FUNC(closure) (EJSVAL_TO_ENV(closure), _this, argc, args);
}

EJSValue*
_ejs_invoke_closure_8 (EJSValue* closure, EJSValue* _this, int argc, EJSValue* arg1, EJSValue* arg2, EJSValue* arg3, EJSValue* arg4, EJSValue* arg5, EJSValue* arg6, EJSValue* arg7, EJSValue* arg8)
{
  assert (EJSVAL_IS_FUNCTION(closure));
  EJSValue *args[] = { arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8 };
  return EJSVAL_TO_FUNC(closure) (EJSVAL_TO_ENV(closure), _this, argc, args);
}

EJSValue*
_ejs_invoke_closure_9 (EJSValue* closure, EJSValue* _this, int argc, EJSValue* arg1, EJSValue* arg2, EJSValue* arg3, EJSValue* arg4, EJSValue* arg5, EJSValue* arg6, EJSValue* arg7, EJSValue* arg8, EJSValue* arg9)
{
  assert (EJSVAL_IS_FUNCTION(closure));
  EJSValue *args[] = { arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9 };
  return EJSVAL_TO_FUNC(closure) (EJSVAL_TO_ENV(closure), _this, argc, args);
}

EJSValue*
_ejs_invoke_closure_10 (EJSValue* closure, EJSValue* _this, int argc, EJSValue* arg1, EJSValue* arg2, EJSValue* arg3, EJSValue* arg4, EJSValue* arg5, EJSValue* arg6, EJSValue* arg7, EJSValue* arg8, EJSValue* arg9, EJSValue* arg10)
{
  assert (EJSVAL_IS_FUNCTION(closure));
  EJSValue *args[] = { arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10 };
  return EJSVAL_TO_FUNC(closure) (EJSVAL_TO_ENV(closure), _this, argc, args);
}

