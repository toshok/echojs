#include "ejs.h"
#include "ejs-ops.h"
#include "ejs-value.h"

static EJSValue*
_ejs_log_impl (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  if (argc > 0) {
    EJSValue* strval = ToString(args[0]);

    printf ("%s\n", EJSVAL_TO_STRING(strval));
  }

  return _ejs_undefined;
}


void
_ejs_console_init(EJSValue* global)
{
  EJSValue* _ejs_console = _ejs_object_new (NULL);
  _ejs_object_setprop_utf8 (_ejs_console, "log", _ejs_function_new (NULL, _ejs_log_impl));
  _ejs_object_setprop_utf8 (_ejs_console, "warn", _ejs_function_new (NULL, _ejs_log_impl)); // FIXME there's no distinction between log and warn for now...
  _ejs_object_setprop_utf8 (global, "console", _ejs_console);
}
