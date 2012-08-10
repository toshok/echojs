
#ifndef _ejs_ops_h
#define _ejs_ops_h

#include "ejs.h"
#include "object.h"

EJSBool _ejs_op_not (EJSValue* exp, EJSValue** result);
EJSBool _ejs_op_add (EJSValue* lhs, EJSValue* rhs, EJSValue** result);
EJSBool _ejs_op_sub (EJSValue* lhs, EJSValue* rhs, EJSValue** result);
EJSBool _ejs_op_strict_eq (EJSValue* lhs, EJSValue* rhs, EJSValue** result);

EJSBool _ejs_truthy (EJSValue* val, EJSBool *result);

extern EJSValue* _ejs_print;
extern void _ejs_print_impl (EJSValue* env, int argc, EJSValue *val);


#endif // _ejs_ops_h
