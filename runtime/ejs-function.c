#include <assert.h>

#include "ejs-value.h"
#include "ejs-object.h"
#include "ejs-function.h"

EJSValue*
_ejs_closure_new (EJSClosureEnv* env, EJSClosureFunc func)
{
  EJSValue* rv = (EJSValue*)calloc(1, sizeof (EJSValue));
  rv->type = EJSValueTypeFunction;
  rv->u.closure.proto = _ejs_function_get_prototype();
  rv->u.closure.map = _ejs_propertymap_new (8);
  rv->u.closure.fields = (EJSValue**)calloc(8, sizeof (EJSValue*));
  rv->u.closure.func = func;
  rv->u.closure.env = env;
  return rv;
}


EJSValue* _ejs_Function;
static EJSValue*
_ejs_Function_impl (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  if (EJSVAL_IS_UNDEFINED(_this)) {
    // called as a function
    printf ("called Function() as a function!\n");
    return NULL;
  }
  else {
    // called as a constructor
    printf ("called Function() as a constructor!\n");
    return NULL;
  }
}

static EJSValue* _ejs_Function_proto;
EJSValue*
_ejs_function_get_prototype()
{
  return _ejs_Function_proto;
}

static EJSValue*
_ejs_Function_prototype_call (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  assert (EJSVAL_IS_CLOSURE(_this));
  EJSValue* thisArg = _ejs_undefined;
  
  if (argc > 0) {
    thisArg = args[0];
    args = &args[1];
    argc --;
  }

  return _this->u.closure.func (_this->u.closure.env, thisArg, argc, argc == 0 ? NULL : args);
}

void
_ejs_function_init(EJSValue *global)
{
  _ejs_Function = _ejs_closure_new (NULL, (EJSClosureFunc)_ejs_Function_impl);
  _ejs_Function_proto = _ejs_object_new(_ejs_object_get_prototype());

  _ejs_object_setprop (_ejs_Function,       _ejs_string_new_utf8("prototype"),  _ejs_Function_proto);
  _ejs_object_setprop (_ejs_Function_proto, _ejs_string_new_utf8("prototype"),  _ejs_function_get_prototype());

  _ejs_object_setprop (_ejs_Function_proto, _ejs_string_new_utf8("call"),       _ejs_closure_new (NULL, (EJSClosureFunc)_ejs_Function_prototype_call));

  _ejs_object_setprop (global, _ejs_string_new_utf8("Function"), _ejs_Function);
}
