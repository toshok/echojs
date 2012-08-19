#include "ejs.h"
#include "ops.h"
#include "require.h"

EJSValue* _ejs_undefined;
EJSValue* _ejs_true;
EJSValue* _ejs_false;

EJSValue* _ejs_global;

void
_ejs_init()
{
  _ejs_undefined = _ejs_undefined_new ();
  _ejs_true = _ejs_boolean_new (TRUE);
  _ejs_false = _ejs_boolean_new (FALSE);
  _ejs_print = _ejs_closure_new (NULL, (EJSClosureFunc0)_ejs_print_impl);
  _ejs_require = _ejs_closure_new (NULL, (EJSClosureFunc0)_ejs_require_impl);
  _ejs_global = _ejs_object_new(NULL);
}
