#include "ejs.h"
#include "value.h"
#include "require.h"
#include "function.h"

extern const char *entry_filename;

int
main(int argc, char** argv)
{
  _ejs_init();
  EJSValue *entry_name = _ejs_string_new_utf8(entry_filename);
  _ejs_invoke_closure_1 (_ejs_require, NULL, 1, entry_name);
  return 0;
}
