
#ifndef _ejs_array_h_
#define _ejs_array_h_

#include "ejs.h"

extern EJSValue* _ejs_Array;
extern EJSValue* _ejs_Array_impl (EJSValue* env, EJSValue* _this, int argc, EJSValue *val);

EJSValue* _ejs_array_new (int numElements);
EJSValue* _ejs_array_get_prototype();

void _ejs_array_init();

#endif /* _ejs_array_h */
