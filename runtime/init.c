#include "ejs.h"
#include "ops.h"

void
_ejs_init()
{
  _ejs_print = _ejs_closure_new (NULL, (EJSClosureFunc0)_ejs_print_impl);
}
