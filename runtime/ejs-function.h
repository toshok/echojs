
#ifndef _ejs_function_h_
#define _ejs_function_h_

#include "ejs.h"
#include "ejs-object.h"

// for now we just build environments out of EJS objects
typedef union _EJSValue EJSClosureEnv;

typedef EJSValue* (*EJSClosureFunc) (EJSValue* env, EJSValue* _this, int argc, EJSValue** args);



typedef struct {
  /* object header */
  EJSObject obj;

  EJSClosureFunc func;
  EJSClosureEnv* env;
  EJSBool bound_this;
  EJSValue *_this;
} EJSFunction;


EJSValue* _ejs_invoke_closure_0 (EJSValue* closure, EJSValue* _this, int argc);
EJSValue* _ejs_invoke_closure_1 (EJSValue* closure, EJSValue* _this, int argc, EJSValue *arg1);
EJSValue* _ejs_invoke_closure_2 (EJSValue* closure, EJSValue* _this, int argc, EJSValue *arg1, EJSValue *arg2);
EJSValue* _ejs_invoke_closure_3 (EJSValue* closure, EJSValue* _this, int argc, EJSValue *arg1, EJSValue *arg2, EJSValue *arg3);
EJSValue* _ejs_invoke_closure_4 (EJSValue* closure, EJSValue* _this, int argc, EJSValue *arg1, EJSValue *arg2, EJSValue *arg3, EJSValue *arg4);
EJSValue* _ejs_invoke_closure_5 (EJSValue* closure, EJSValue* _this, int argc, EJSValue *arg1, EJSValue *arg2, EJSValue *arg3, EJSValue *arg4, EJSValue *arg5);
EJSValue* _ejs_invoke_closure_6 (EJSValue* closure, EJSValue* _this, int argc, EJSValue *arg1, EJSValue *arg2, EJSValue *arg3, EJSValue *arg4, EJSValue *arg5, EJSValue *arg6);
EJSValue* _ejs_invoke_closure_7 (EJSValue* closure, EJSValue* _this, int argc, EJSValue *arg1, EJSValue *arg2, EJSValue *arg3, EJSValue *arg4, EJSValue *arg5, EJSValue *arg6, EJSValue *arg7);
EJSValue* _ejs_invoke_closure_8 (EJSValue* closure, EJSValue* _this, int argc, EJSValue *arg1, EJSValue *arg2, EJSValue *arg3, EJSValue *arg4, EJSValue *arg5, EJSValue *arg6, EJSValue *arg7, EJSValue *arg8);
EJSValue* _ejs_invoke_closure_9 (EJSValue* closure, EJSValue* _this, int argc, EJSValue *arg1, EJSValue *arg2, EJSValue *arg3, EJSValue *arg4, EJSValue *arg5, EJSValue *arg6, EJSValue *arg7, EJSValue *arg8, EJSValue *arg9);
EJSValue* _ejs_invoke_closure_10 (EJSValue* closure, EJSValue* _this, int argc, EJSValue *arg1, EJSValue *arg2, EJSValue *arg3, EJSValue *arg4, EJSValue *arg5, EJSValue *arg6, EJSValue *arg7, EJSValue *arg8, EJSValue *arg9, EJSValue *arg10);

extern EJSValue* _ejs_function_new (EJSClosureEnv* env, EJSClosureFunc func);

extern EJSValue* _ejs_Function;
extern EJSValue* _ejs_function_get_prototype();
extern void _ejs_function_init(EJSValue *global);

#endif
