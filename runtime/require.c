#include "ejs.h"
#include "object.h"
#include "require.h"

extern EJSRequire _ejs_require_map[];

EJSValue* _ejs_require;
EJSValue*
_ejs_require_impl (EJSValue* env, EJSValue* _this, int argc, EJSValue *val)
{
  if (!EJSVAL_IS_STRING(val)) {
    printf ("required called with non-string\n");
    return NULL;
  }

  int i = 0;
  while (1) {
    if (!_ejs_require_map[i].name)
      break;
    if (!strcmp (_ejs_require_map[i].name, EJSVAL_TO_STRING(val))) {
      if (!_ejs_require_map[i].cached_exports) {
	_ejs_require_map[i].cached_exports = _ejs_object_new(NULL);
	_ejs_require_map[i].func (NULL, _ejs_undefined, 1, _ejs_require_map[i].cached_exports);
      }
      return _ejs_require_map[i].cached_exports;
    }
    i++;
  }

  return NULL;
}
