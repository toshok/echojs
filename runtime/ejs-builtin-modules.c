#include <stdio.h>
#include <libgen.h>

#include "ejs-value.h"
#include "ejs-builtin-modules.h"

static EJSValue*
_ejs_path_basename (EJSValue* env, EJSValue* _this, int argc, EJSValue** args)
{
  EJSValue* path = args[0];
  // FIXME node's implementation allows a second arg to strip the extension, but the compiler doesn't use it.
  return _ejs_string_new_utf8(basename (EJSVAL_TO_STRING(path)));
}

EJSValue*
_ejs_path_module_func (EJSValue* env, EJSValue* _this, int argc, EJSValue** args)
{
  EJSValue* exports = args[0];

  _ejs_object_setprop (exports, _ejs_string_new_utf8("basename"), _ejs_path_basename);

  return _ejs_undefined;
}

EJSValue*
_ejs_fs_module_func (EJSValue* env, EJSValue* _this, int argc, EJSValue** args)
{
  EJSValue* exports = args[0];

  return _ejs_undefined;
}
