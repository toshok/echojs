#include "ejs.h"
#include "ejs-ops.h"
#include "ejs-require.h"
#include "ejs-array.h"
#include "ejs-object.h"
#include "ejs-value.h"
#include "ejs-string.h"
#include "ejs-regexp.h"
#include "ejs-console.h"
#include "ejs-function.h"

EJSValue* _ejs_undefined;
EJSValue* _ejs_true;
EJSValue* _ejs_false;

EJSValue* _ejs_global;

void
_ejs_init(int argc, char** argv)
{
  _ejs_global = _ejs_object_new(NULL);

  _ejs_function_init(_ejs_global);
  _ejs_object_init(_ejs_global);
  _ejs_array_init(_ejs_global);
  _ejs_string_init(_ejs_global);
  _ejs_regexp_init(_ejs_global);
  _ejs_require_init(_ejs_global);
  _ejs_console_init(_ejs_global);

  _ejs_undefined = _ejs_undefined_new ();
  _ejs_object_setprop (_ejs_global, _ejs_string_new_utf8("undefined"), _ejs_undefined);

  _ejs_true = _ejs_boolean_new (TRUE);
  _ejs_false = _ejs_boolean_new (FALSE);

  EJSValue* _ejs_process = _ejs_object_new (NULL);
  EJSValue* _argv = _ejs_array_new (argc);
  int i;

  for (i = 0; i < argc; i ++) {
    _ejs_object_setprop (_argv, _ejs_number_new(i), _ejs_string_new_utf8(argv[i]));
  }

  _ejs_object_setprop (_ejs_process, _ejs_string_new_utf8("argv"), _argv);
}
