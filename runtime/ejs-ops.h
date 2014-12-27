
#ifndef _ejs_ops_h
#define _ejs_ops_h

#include "ejs.h"
#include "ejs-object.h"

EJS_BEGIN_DECLS

/* returns an EJSPrimString* */
ejsval NumberToString(double d);
ejsval ToString(ejsval exp);
ejsval ToNumber(ejsval exp);
double ToDouble(ejsval exp);
int64_t ToInteger(ejsval exp);
int64_t ToLength(ejsval exp);
uint16_t ToUint16(ejsval exp);
uint32_t ToUint32(ejsval exp);
ejsval ToObject(ejsval exp);
ejsval ToBoolean(ejsval exp);
EJSBool ToEJSBool(ejsval exp);
EJSBool IsConcatSpreadable (ejsval O);

EJSBool SameValue(ejsval x, ejsval y);
EJSBool SameValueZero(ejsval x, ejsval y);

ejsval _ejs_op_typeof_is_object(ejsval exp);
ejsval _ejs_op_typeof_is_function(ejsval exp);
ejsval _ejs_op_typeof_is_string(ejsval exp);
ejsval _ejs_op_typeof_is_symbol(ejsval exp);
ejsval _ejs_op_typeof_is_number(ejsval exp);
ejsval _ejs_op_typeof_is_undefined(ejsval exp);
ejsval _ejs_op_typeof_is_null(ejsval exp);
ejsval _ejs_op_typeof_is_boolean(ejsval exp);

ejsval _ejs_op_neg (ejsval exp);
ejsval _ejs_op_not (ejsval exp);
ejsval _ejs_op_void (ejsval exp);
ejsval _ejs_op_typeof (ejsval exp);
ejsval _ejs_op_delete (ejsval obj, ejsval prop);
ejsval _ejs_op_bitwise_xor (ejsval lhs, ejsval rhs);
ejsval _ejs_op_bitwise_not (ejsval val);
ejsval _ejs_op_bitwise_and (ejsval lhs, ejsval rhs);
ejsval _ejs_op_bitwise_or (ejsval lhs, ejsval rhs);
ejsval _ejs_op_rsh (ejsval lhs, ejsval rhs);
ejsval _ejs_op_ursh (ejsval lhs, ejsval rhs);
ejsval _ejs_op_lsh (ejsval lhs, ejsval rhs);
ejsval _ejs_op_ulsh (ejsval lhs, ejsval rhs);
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

void _ejs_throw (ejsval exp) __attribute__ ((noreturn));

extern ejsval _ejs_isNaN;
extern ejsval _ejs_isFinite;
extern ejsval _ejs_parseInt;
extern ejsval _ejs_parseFloat;

ejsval _ejs_isNaN_impl (ejsval env, ejsval _this, uint32_t argc, ejsval* args);
ejsval _ejs_isFinite_impl (ejsval env, ejsval _this, uint32_t argc, ejsval* args);
ejsval _ejs_parseInt_impl (ejsval env, ejsval _this, uint32_t argc, ejsval* args);
ejsval _ejs_parseFloat_impl (ejsval env, ejsval _this, uint32_t argc, ejsval* args);

ejsval CheckIterableObject (ejsval obj);
ejsval GetIterator (ejsval obj, ejsval method);
ejsval IteratorNext (ejsval iterator, ejsval value);
ejsval IteratorComplete (ejsval iterResult);
ejsval IteratorValue (ejsval iterResult);
ejsval IteratorStep (ejsval iterator);
ejsval _ejs_create_iter_result (ejsval value, ejsval done);

EJS_END_DECLS

#endif // _ejs_ops_h
