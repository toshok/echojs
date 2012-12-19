#ifndef _ejs_builtin_modules_h_
#define _ejs_builtin_modules_h_

#include "ejs-function.h"

EJS_BEGIN_DECLS

extern ejsval _ejs_path_module_func (ejsval env, ejsval _this, int argc, ejsval* args);
extern ejsval _ejs_fs_module_func (ejsval env, ejsval _this, int argc, ejsval* args);
extern ejsval _ejs_child_process_module_func (ejsval env, ejsval _this, int argc, ejsval* args);

EJS_END_DECLS

#endif /* _ejs_builtin_modules_h */
