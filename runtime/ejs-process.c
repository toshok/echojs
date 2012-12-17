#include "ejs.h"
#include "ejs-ops.h"
#include "ejs-value.h"

static EJSValue*
_ejs_process_exit (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  int exit_status = 0;

  // FIXME ignore argc/args[0] for now

  exit (exit_status);
}

void
_ejs_process_init(EJSValue* global, int argc, char **argv)
{
  START_SHADOW_STACK_FRAME;

  ADD_STACK_ROOT(EJSValue*, _ejs_process, _ejs_object_new (NULL));
  ADD_STACK_ROOT(EJSValue*, _argv, _ejs_array_new (argc));
  int i;

  for (i = 0; i < argc; i ++) {
    START_SHADOW_STACK_FRAME;
    ADD_STACK_ROOT(EJSValue*, _i, _ejs_number_new(i));
    ADD_STACK_ROOT(EJSValue*, _argv_i, _ejs_string_new_utf8(argv[i]));
    _ejs_object_setprop (_argv, _i, _argv_i);
    END_SHADOW_STACK_FRAME;
  }

  _ejs_object_setprop_utf8 (_ejs_process, "argv", _argv);

#define OBJ_METHOD(x) do { ADD_STACK_ROOT(EJSValue*, funcname, _ejs_string_new_utf8(#x)); ADD_STACK_ROOT(EJSValue*, tmpfunc, _ejs_function_new (NULL, funcname, (EJSClosureFunc)_ejs_process_##x)); _ejs_object_setprop (_ejs_process, funcname, tmpfunc); } while (0)

  OBJ_METHOD(exit);

#undef OBJ_METHOD

  _ejs_object_setprop_utf8 (global, "process", _ejs_process);

  END_SHADOW_STACK_FRAME;
}
