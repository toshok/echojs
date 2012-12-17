#include "ejs.h"
#include "ejs-gc.h"
#include "ejs-ops.h"
#include "ejs-value.h"

static EJSValue*
_ejs_console_log (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  START_SHADOW_STACK_FRAME;

  if (argc > 0) {
    ADD_STACK_ROOT(EJSValue*, strval, ToString(args[0]));

    fprintf (stdout, "%s\n", EJSVAL_TO_STRING(strval));
  }

  END_SHADOW_STACK_FRAME;

  return _ejs_undefined;
}

static EJSValue*
_ejs_console_warn (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  START_SHADOW_STACK_FRAME;

  if (argc > 0) {
    ADD_STACK_ROOT(EJSValue*, strval, ToString(args[0]));

    fprintf (stderr, "%s\n", EJSVAL_TO_STRING(strval));
  }

  END_SHADOW_STACK_FRAME;

  return _ejs_undefined;
}


void
_ejs_console_init(EJSValue* global)
{
  START_SHADOW_STACK_FRAME;

  ADD_STACK_ROOT(EJSValue*, _ejs_console, _ejs_object_new (NULL));

#define OBJ_METHOD(x) do { ADD_STACK_ROOT(EJSValue*, funcname, _ejs_string_new_utf8(#x)); ADD_STACK_ROOT(EJSValue*, tmpfunc, _ejs_function_new (NULL, funcname, (EJSClosureFunc)_ejs_console_##x)); _ejs_object_setprop (_ejs_console, funcname, tmpfunc); } while (0)

  OBJ_METHOD(log);
  OBJ_METHOD(warn);

  _ejs_object_setprop_utf8 (global, "console", _ejs_console);

#undef OBJ_METHOD

  END_SHADOW_STACK_FRAME;
}
