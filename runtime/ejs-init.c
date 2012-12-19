#include <math.h>

#include "ejs.h"
#include "ejs-gc.h"
#include "ejs-ops.h"
#include "ejs-arguments.h"
#include "ejs-require.h"
#include "ejs-array.h"
#include "ejs-object.h"
#include "ejs-value.h"
#include "ejs-string.h"
#include "ejs-number.h"
#include "ejs-regexp.h"
#include "ejs-date.h"
#include "ejs-console.h"
#include "ejs-process.h"
#include "ejs-function.h"
#include "ejs-uri.h"

ejsval _ejs_undefined;
ejsval _ejs_nan;
ejsval _ejs_null;
ejsval _ejs_true;
ejsval _ejs_false;

ejsval _ejs_global;

void
_ejs_init(int argc, char** argv)
{
  // initialize our constants before anything else
  _ejs_null = BUILD_EJSVAL(EJSVAL_TAG_NULL, 0);
  _ejs_undefined = BUILD_EJSVAL(EJSVAL_TAG_UNDEFINED, 0);
  _ejs_nan = NUMBER_TO_EJSVAL(nan("7734"));
  _ejs_true = BOOLEAN_TO_EJSVAL(EJS_TRUE);
  _ejs_false = BOOLEAN_TO_EJSVAL(EJS_FALSE);

  START_SHADOW_STACK_FRAME;

  _ejs_gc_init();

  _ejs_gc_add_named_root (_ejs_global);

  _ejs_global = OBJECT_TO_EJSVAL(_ejs_object_alloc_instance());
  EJSObject *global = EJSVAL_TO_OBJECT(_ejs_global);
  _ejs_init_object (global, _ejs_null, &_ejs_object_specops);

  _ejs_object_setprop_utf8 (_ejs_global, "undefined", _ejs_undefined);
  _ejs_object_setprop_utf8 (_ejs_global, "NaN", _ejs_nan);
  _ejs_object_setprop_utf8 (_ejs_global, "__ejs", _ejs_true);

  _ejs_function_init(_ejs_global);
  _ejs_object_init(_ejs_global);
  _ejs_arguments_init(_ejs_global);
  _ejs_array_init(_ejs_global);
  _ejs_string_init(_ejs_global);
  _ejs_number_init(_ejs_global);
  _ejs_regexp_init(_ejs_global);
  _ejs_date_init(_ejs_global);
  _ejs_require_init(_ejs_global);
  _ejs_console_init(_ejs_global);
  _ejs_process_init(_ejs_global, argc, argv);

#define GLOBAL_METHOD(x) do { ADD_STACK_ROOT(ejsval, name, _ejs_string_new_utf8 (#x)); _ejs_object_setprop (_ejs_global, name, _ejs_function_new (_ejs_null, name, (EJSClosureFunc)_ejs_##x)); } while (0)

  GLOBAL_METHOD(isNaN);
  GLOBAL_METHOD(isFinite);
  GLOBAL_METHOD(parseInt);
  GLOBAL_METHOD(parseFloat);

  GLOBAL_METHOD(decodeURI);
  GLOBAL_METHOD(decodeURIComponent);
  GLOBAL_METHOD(encodeURI);
  GLOBAL_METHOD(encodeURIComponent);

#undef GLOBAL_METHOD
}
