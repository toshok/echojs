#include "ejs.h"
#include "ejs-gc.h"
#include "ejs-value.h"
#include "ejs-require.h"
#include "ejs-function.h"

#define GC_ON_SHUTDOWN 1

extern const char *entry_filename;
extern EJSBool _ejs_gc_started;

int
main(int argc, char** argv)
{
  GCObjectPtr stack_bottom = NULL;

  _ejs_gc_set_stack_bottom (&stack_bottom);

  _ejs_init(argc, argv);
  EJSValue *entry_name = _ejs_string_new_utf8(entry_filename);

  _ejs_gc_started = TRUE;

  _ejs_invoke_closure_1 (_ejs_require, NULL, 1, entry_name);
#if GC_ON_SHUTDOWN
  _ejs_gc_shutdown();
#endif
  return 0;
}
