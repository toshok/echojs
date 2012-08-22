#include "ejs.h"
#include "ejs-object.h"
#include "ejs-require.h"
#include "ejs-value.h"

extern EJSRequire _ejs_require_map[];

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

  int i = 0;
  while (1) {
    if (!_ejs_require_map[i].name)
      break;
    if (!strcmp (_ejs_require_map[i].name, EJSVAL_TO_STRING(arg))) {
      if (!_ejs_require_map[i].cached_exports) {
	_ejs_require_map[i].cached_exports = _ejs_object_new(NULL);
	_ejs_require_map[i].func (NULL, _ejs_undefined, 1, &_ejs_require_map[i].cached_exports);
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
  _ejs_require = _ejs_closure_new (NULL, _ejs_require_impl);
  _ejs_object_setprop (global, _ejs_string_new_utf8("require"), _ejs_require);
}
