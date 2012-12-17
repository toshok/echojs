#include "ejs.h"
#include "ejs-gc.h"
#include "ejs-value.h"
#include "ejs-require.h"
#include "ejs-function.h"

#define GC_ON_SHUTDOWN 1

extern const char *entry_filename;

int
main(int argc, char** argv)
{
  _ejs_init(argc, argv);

  llvm_gc_root_chain = NULL;

  START_SHADOW_STACK_FRAME;

  EJSValue *entry_name = _ejs_string_new_utf8(entry_filename);

  _ejs_invoke_closure_1 (_ejs_require, NULL, 1, entry_name);

  END_SHADOW_STACK_FRAME;

#if GC_ON_SHUTDOWN
  _ejs_gc_shutdown();
#endif
  return 0;
}
