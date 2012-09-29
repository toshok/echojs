#include <stdio.h>
#include <libgen.h>

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "ejs-value.h"
#include "ejs-builtin-modules.h"


////
/// path module
///

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

  _ejs_object_setprop_utf8 (exports, "basename", _ejs_function_new_utf8 (NULL, "basename", _ejs_path_basename));

  return _ejs_undefined;
}

////
/// fs module
///

static EJSValue*
_ejs_fs_readFileSync (EJSValue* env, EJSValue* _this, int argc, EJSValue** args)
{
  // FIXME we currently ignore the encoding and just slam the entire thing into a buffer and return a utf8 string...
  char* path = EJSVAL_TO_STRING(args[0]);

  int fd = open (path, O_RDONLY);
  struct stat fd_stat;

  fstat (fd, &fd_stat);

  char *buf = (char*)malloc (fd_stat.st_size);
  read(fd, buf, fd_stat.st_size);
  close(fd);

  return _ejs_string_new_utf8(buf);
}

EJSValue*
_ejs_fs_module_func (EJSValue* env, EJSValue* _this, int argc, EJSValue** args)
{
  EJSValue* exports = args[0];

  _ejs_object_setprop_utf8 (exports, "readFileSync", _ejs_function_new_utf8 (NULL, "readFileSync", _ejs_fs_readFileSync));

  return _ejs_undefined;
}

EJSValue*
_ejs_child_process_module_func (EJSValue* env, EJSValue* _this, int argc, EJSValue** args)
{
  return _ejs_undefined;
}
