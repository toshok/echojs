#include "ejs.h"
#include "object.h"
#include "require.h"

extern const char *entry_filename;

int
main(int argc, char** argv)
{
  _ejs_init();
  EJSValue *entry_name = _ejs_string_new_utf8(entry_filename);
  _ejs_require_impl (NULL, NULL, 1, entry_name);
  return 0;
}
