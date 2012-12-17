#include <math.h>

#include "ejs.h"
#include "ejs-gc.h"
#include "ejs-ops.h"
#include "ejs-require.h"
#include "ejs-array.h"
#include "ejs-object.h"
#include "ejs-value.h"
#include "ejs-string.h"
#include "ejs-regexp.h"
#include "ejs-number.h"
#include "ejs-date.h"
#include "ejs-console.h"
#include "ejs-process.h"
#include "ejs-function.h"
#include "ejs-uri.h"

EJSValue* _ejs_undefined = NULL;
EJSValue* _ejs_nan = NULL;
EJSValue* _ejs_true = NULL;
EJSValue* _ejs_false = NULL;

EJSValue* _ejs_global = NULL;

extern EJSValue* _ejs_boolean_new_internal (EJSBool value);

void
_ejs_init(int argc, char** argv)
{
  START_SHADOW_STACK_FRAME;

  _ejs_gc_init();

  _ejs_gc_add_named_root (_ejs_global);
  _ejs_gc_add_named_root (_ejs_true);
  _ejs_gc_add_named_root (_ejs_false);

  _ejs_global = _ejs_object_new(NULL);

  _ejs_function_init(_ejs_global);
  _ejs_object_init(_ejs_global);
  _ejs_array_init(_ejs_global);
  _ejs_string_init(_ejs_global);
  _ejs_regexp_init(_ejs_global);
  _ejs_date_init(_ejs_global);
  _ejs_number_init(_ejs_global);
  _ejs_require_init(_ejs_global);
  _ejs_console_init(_ejs_global);
  _ejs_process_init(_ejs_global, argc, argv);

  ADD_STACK_ROOT(EJSValue*, tmpobj, _ejs_undefined_new ());
  _ejs_undefined = tmpobj;
  _ejs_object_setprop_utf8 (_ejs_global, "undefined", _ejs_undefined);

  ADD_STACK_ROOT(EJSValue*, tmpobj2, _ejs_number_new(nan("7734")));
  _ejs_nan = tmpobj2;

  _ejs_object_setprop_utf8 (_ejs_global, "NaN", _ejs_nan);

#define GLOBAL_METHOD(x) do { ADD_STACK_ROOT(EJSValue*, name, _ejs_string_new_utf8 (#x)); _ejs_object_setprop (_ejs_global, name, _ejs_function_new (NULL, name, (EJSClosureFunc)_ejs_##x)); } while (0)

  GLOBAL_METHOD(isNaN);
  GLOBAL_METHOD(isFinite);
  GLOBAL_METHOD(parseInt);
  GLOBAL_METHOD(parseFloat);

  GLOBAL_METHOD(decodeURI);
  GLOBAL_METHOD(decodeURIComponent);
  GLOBAL_METHOD(encodeURI);
  GLOBAL_METHOD(encodeURIComponent);

#undef GLOBAL_METHOD

  _ejs_true = _ejs_boolean_new_internal (TRUE);
  _ejs_false = _ejs_boolean_new_internal (FALSE);

  _ejs_object_setprop_utf8 (_ejs_global, "__ejs", _ejs_true);
}
