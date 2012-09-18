#include "ejs.h"
#include "ejs-object.h"
#include "ejs-require.h"
#include "ejs-value.h"
#include "ejs-builtin-modules.h"

extern EJSRequire _ejs_require_map[];

static EJSRequire builtin_module_map[] = {
  { "path", _ejs_path_module_func, 0 },
  { "fs", _ejs_fs_module_func, 0 },
  { "child_process", _ejs_child_process_module_func, 0 }
};
static int num_builtin_modules = sizeof(builtin_module_map) / sizeof(builtin_module_map[0]);

EJSValue* _ejs_require;
static EJSValue*
_ejs_require_impl (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  if (argc < 1)
    return _ejs_undefined;

  EJSValue* arg = args[0];

  if (!EJSVAL_IS_STRING(arg)) {
    printf ("required called with non-string\n");
    return NULL;
  }

  int i;
  for (i = 0; i < num_builtin_modules; i ++) {
    if (!strcmp (builtin_module_map[i].name, EJSVAL_TO_STRING(arg))) {
      if (!builtin_module_map[i].cached_exports) {
	//	printf ("require'ing %s.\n", EJSVAL_TO_STRING(arg));
	builtin_module_map[i].cached_exports = _ejs_object_new(NULL);
	builtin_module_map[i].func(NULL, _ejs_undefined, 1, &builtin_module_map[i].cached_exports);
      }
      return builtin_module_map[i].cached_exports;
    }
  }

  i = 0;
  while (1) {
    if (!_ejs_require_map[i].name) {
      printf ("require('%s') failed: module not included in build.\n", EJSVAL_TO_STRING(arg));
      break;
    }
    if (!strcmp (_ejs_require_map[i].name, EJSVAL_TO_STRING(arg))) {
      if (!_ejs_require_map[i].cached_exports) {
	//	printf ("require'ing %s.\n", EJSVAL_TO_STRING(arg));
	_ejs_require_map[i].cached_exports = _ejs_object_new(NULL);
	_ejs_object_setprop_utf8(_ejs_global, "exports", _ejs_require_map[i].cached_exports);
	_ejs_require_map[i].func (NULL, _ejs_undefined, 1, &_ejs_require_map[i].cached_exports);
	_ejs_object_setprop_utf8(_ejs_global, "exports", _ejs_undefined);
      }
      return _ejs_require_map[i].cached_exports;
    }
    i++;
  }

  return NULL;
}

void
_ejs_require_init(EJSValue* global)
{
  _ejs_require = _ejs_function_new (NULL, _ejs_require_impl);
  _ejs_object_setprop_utf8 (global, "require", _ejs_require);
}
