#include "ejs.h"
#include "ops.h"
#include "require.h"
#include "array.h"
#include "value.h"
#include "ejs-string.h"

EJSValue* _ejs_undefined;
EJSValue* _ejs_true;
EJSValue* _ejs_false;

EJSValue* _ejs_global;

void
_ejs_init()
{
  _ejs_global = _ejs_object_new(NULL);

  _ejs_object_init(_ejs_global);
  _ejs_array_init(_ejs_global);
  _ejs_string_init(_ejs_global);
  _ejs_require_init(_ejs_global);

  _ejs_undefined = _ejs_undefined_new ();
  _ejs_object_setprop (_ejs_global, _ejs_string_new_utf8("undefined"), _ejs_undefined);

  _ejs_true = _ejs_boolean_new (TRUE);
  _ejs_false = _ejs_boolean_new (FALSE);

  _ejs_print = _ejs_closure_new (NULL, _ejs_print_impl);
  _ejs_object_setprop (_ejs_global, _ejs_string_new_utf8("print"), _ejs_print);
}
