
#ifndef _ejs_function_h_
#define _ejs_function_h_

#include "ejs.h"
#include "ejs-object.h"

// for now we just build environments out of EJS objects
typedef ejsval EJSClosureEnv;

typedef ejsval (*EJSClosureFunc) (ejsval env, ejsval _this, int argc, ejsval* args);



typedef struct {
  /* object header */
  EJSObject obj;

  ejsval name;
  EJSClosureFunc func;
  EJSClosureEnv env;
  EJSBool bound_this;
  ejsval _this;
} EJSFunction;


EJS_BEGIN_DECLS

ejsval _ejs_invoke_closure_0 (ejsval closure, ejsval _this, int argc);
ejsval _ejs_invoke_closure_1 (ejsval closure, ejsval _this, int argc, ejsval arg1);
ejsval _ejs_invoke_closure_2 (ejsval closure, ejsval _this, int argc, ejsval arg1, ejsval arg2);
ejsval _ejs_invoke_closure_3 (ejsval closure, ejsval _this, int argc, ejsval arg1, ejsval arg2, ejsval arg3);
ejsval _ejs_invoke_closure_4 (ejsval closure, ejsval _this, int argc, ejsval arg1, ejsval arg2, ejsval arg3, ejsval arg4);
ejsval _ejs_invoke_closure_5 (ejsval closure, ejsval _this, int argc, ejsval arg1, ejsval arg2, ejsval arg3, ejsval arg4, ejsval arg5);
ejsval _ejs_invoke_closure_6 (ejsval closure, ejsval _this, int argc, ejsval arg1, ejsval arg2, ejsval arg3, ejsval arg4, ejsval arg5, ejsval arg6);
ejsval _ejs_invoke_closure_7 (ejsval closure, ejsval _this, int argc, ejsval arg1, ejsval arg2, ejsval arg3, ejsval arg4, ejsval arg5, ejsval arg6, ejsval arg7);
ejsval _ejs_invoke_closure_8 (ejsval closure, ejsval _this, int argc, ejsval arg1, ejsval arg2, ejsval arg3, ejsval arg4, ejsval arg5, ejsval arg6, ejsval arg7, ejsval arg8);
ejsval _ejs_invoke_closure_9 (ejsval closure, ejsval _this, int argc, ejsval arg1, ejsval arg2, ejsval arg3, ejsval arg4, ejsval arg5, ejsval arg6, ejsval arg7, ejsval arg8, ejsval arg9);
ejsval _ejs_invoke_closure_10 (ejsval closure, ejsval _this, int argc, ejsval arg1, ejsval arg2, ejsval arg3, ejsval arg4, ejsval arg5, ejsval arg6, ejsval arg7, ejsval arg8, ejsval arg9, ejsval arg10);

extern ejsval _ejs_function_new (EJSClosureEnv env, ejsval name, EJSClosureFunc func);
extern ejsval _ejs_function_new_utf8 (EJSClosureEnv env, const char* name, EJSClosureFunc func);

extern ejsval _ejs_Function;
extern ejsval _ejs_Function_proto;
extern void _ejs_function_init(ejsval global);

EJS_END_DECLS

#endif
