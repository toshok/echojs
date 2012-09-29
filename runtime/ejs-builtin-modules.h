#ifndef _ejs_builtin_modules_h_
#define _ejs_builtin_modules_h_

#include "ejs-function.h"

EJS_BEGIN_DECLS

extern EJSValue* _ejs_path_module_func (EJSValue* env, EJSValue* _this, int argc, EJSValue** args);
extern EJSValue* _ejs_fs_module_func (EJSValue* env, EJSValue* _this, int argc, EJSValue** args);
extern EJSValue* _ejs_child_process_module_func (EJSValue* env, EJSValue* _this, int argc, EJSValue** args);

EJS_END_DECLS

#endif /* _ejs_builtin_modules_h */
