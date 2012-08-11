#include "ejs.h"
#include "ops.h"

EJSValue* _ejs_undefined;
EJSValue* _ejs_global;

void
_ejs_init()
{
  _ejs_print = _ejs_closure_new (NULL, (EJSClosureFunc0)_ejs_print_impl);
  _ejs_undefined = _ejs_undefined_new ();
  _ejs_global = _ejs_object_new(NULL);
}
