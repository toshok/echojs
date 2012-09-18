#include "ejs.h"
#include "ejs-ops.h"
#include "ejs-value.h"

static EJSValue*
_ejs_process_exit_impl (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  int exit_status = 0;

  // FIXME ignore argc/args[0] for now

  exit (exit_status);
}

void
_ejs_process_init(EJSValue* global, int argc, char **argv)
{
  EJSValue* _ejs_process = _ejs_object_new (NULL);
  EJSValue* _argv = _ejs_array_new (argc);
  int i;

  for (i = 0; i < argc; i ++) {
    _ejs_object_setprop (_argv, _ejs_number_new(i), _ejs_string_new_utf8(argv[i]));
  }

  _ejs_object_setprop_utf8 (_ejs_process, "argv", _argv);
  _ejs_object_setprop_utf8 (_ejs_process, "exit", _ejs_function_new (NULL, _ejs_process_exit_impl));
  _ejs_object_setprop_utf8 (global, "process", _ejs_process);
}
