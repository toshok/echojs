#include "ejs.h"
#include "ejs-value.h"

static EJSValue*
_ejs_log_impl (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
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


void
_ejs_console_init(EJSValue* global)
{
  EJSValue* _ejs_console = _ejs_object_new (NULL);
  _ejs_object_setprop_utf8 (_ejs_console, "log", _ejs_closure_new (NULL, _ejs_log_impl));
  _ejs_object_setprop_utf8 (global, "console", _ejs_console);
}
