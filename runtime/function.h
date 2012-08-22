
#ifndef _ejs_function_h_
#define _ejs_function_h_

#include "ejs.h"

// for now we just build environments out of EJS objects
typedef struct _EJSValue EJSClosureEnv;

typedef EJSValue* (*EJSClosureFunc) (EJSValue* env, EJSValue* _this, int argc, EJSValue** args);



typedef struct {
  /* object header */
  EJSValue *proto;
  EJSPropertyMap* map;
  EJSValue **fields;

  EJSClosureFunc func;
  EJSClosureEnv* env;
  EJSBool bound_this;
  EJSValue *_this;
} EJSClosure;


EJSValue* _ejs_invoke_closure_0 (EJSValue* closure, EJSValue* _this, int argc);
EJSValue* _ejs_invoke_closure_1 (EJSValue* closure, EJSValue* _this, int argc, EJSValue *arg1);
EJSValue* _ejs_invoke_closure_2 (EJSValue* closure, EJSValue* _this, int argc, EJSValue *arg1, EJSValue *arg2);
EJSValue* _ejs_invoke_closure_3 (EJSValue* closure, EJSValue* _this, int argc, EJSValue *arg1, EJSValue *arg2, EJSValue *arg3);
EJSValue* _ejs_invoke_closure_4 (EJSValue* closure, EJSValue* _this, int argc, EJSValue *arg1, EJSValue *arg2, EJSValue *arg3, EJSValue *arg4);
EJSValue* _ejs_invoke_closure_5 (EJSValue* closure, EJSValue* _this, int argc, EJSValue *arg1, EJSValue *arg2, EJSValue *arg3, EJSValue *arg4, EJSValue *arg5);
EJSValue* _ejs_invoke_closure_6 (EJSValue* closure, EJSValue* _this, int argc, EJSValue *arg1, EJSValue *arg2, EJSValue *arg3, EJSValue *arg4, EJSValue *arg5, EJSValue *arg6);
EJSValue* _ejs_invoke_closure_7 (EJSValue* closure, EJSValue* _this, int argc, EJSValue *arg1, EJSValue *arg2, EJSValue *arg3, EJSValue *arg4, EJSValue *arg5, EJSValue *arg6, EJSValue *arg7);

EJSValue* _ejs_closure_new (EJSClosureEnv* env, EJSClosureFunc func);


#endif
