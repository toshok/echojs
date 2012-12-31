
#ifndef _ejs_ops_h
#define _ejs_ops_h

#include "ejs.h"
#include "ejs-object.h"

EJS_BEGIN_DECLS

/* returns an EJSPrimString* */
ejsval NumberToString(double d);
ejsval ToString(ejsval exp);
double ToDouble(ejsval exp);
int ToInteger(ejsval exp);
ejsval ToObject(ejsval exp);
ejsval ToBoolean(ejsval exp);

ejsval _ejs_op_neg (ejsval exp);
ejsval _ejs_op_not (ejsval exp);
ejsval _ejs_op_void (ejsval exp);
ejsval _ejs_op_typeof (ejsval exp);
ejsval _ejs_op_delete (ejsval obj, ejsval prop);
ejsval _ejs_op_bitwise_and (ejsval lhs, ejsval rhs);
ejsval _ejs_op_bitwise_or (ejsval lhs, ejsval rhs);
ejsval _ejs_op_rsh (ejsval lhs, ejsval rhs);
ejsval _ejs_op_ursh (ejsval lhs, ejsval rhs);
ejsval _ejs_op_mod (ejsval lhs, ejsval rhs);
ejsval _ejs_op_add (ejsval lhs, ejsval rhs);
ejsval _ejs_op_mult (ejsval lhs, ejsval rhs);
ejsval _ejs_op_lt (ejsval lhs, ejsval rhs);
EJSBool _ejs_op_lt_ejsbool (ejsval lhs, ejsval rhs);
ejsval _ejs_op_le (ejsval lhs, ejsval rhs);
ejsval _ejs_op_gt (ejsval lhs, ejsval rhs);
ejsval _ejs_op_ge (ejsval lhs, ejsval rhs);
ejsval _ejs_op_sub (ejsval lhs, ejsval rhs);
ejsval _ejs_op_strict_eq (ejsval lhs, ejsval rhs);
ejsval _ejs_op_strict_neq (ejsval lhs, ejsval rhs);

ejsval _ejs_op_eq (ejsval lhs, ejsval rhs);
ejsval _ejs_op_neq (ejsval lhs, ejsval rhs);

ejsval _ejs_op_instanceof (ejsval lhs, ejsval rhs);
ejsval _ejs_op_in (ejsval lhs, ejsval rhs);

EJSBool _ejs_truthy (ejsval val);

void _ejs_throw (ejsval exp);

ejsval _ejs_isNaN (ejsval env, ejsval _this, int argc, ejsval* args);
ejsval _ejs_isFinite (ejsval env, ejsval _this, int argc, ejsval* args);
ejsval _ejs_parseInt (ejsval env, ejsval _this, int argc, ejsval* args);
ejsval _ejs_parseFloat (ejsval env, ejsval _this, int argc, ejsval* args);

EJS_END_DECLS

#endif // _ejs_ops_h
