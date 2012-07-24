
#ifndef _ejs_object_h_
#define _ejs_object_h_

#include "ejs.h"

typedef enum {
  EJSValueTypeObject,
  EJSValueTypeBoolean,
  EJSValueTypeNumber,
  EJSValueTypeString,
  EJSValueTypeClosure
} EJSValueType;

typedef struct _EJSContext* EJSContext;

typedef struct _EJSValue EJSValue;

// for now we just build environments out of EJS objects
typedef struct _EJSValue EJSClosureEnv;

typedef EJSBool (*EJSFunc) (EJSContext* context, EJSValue* this);

typedef EJSValue* (*EJSClosureFunc0) (EJSContext* context, EJSValue* closure, int argc);
typedef EJSValue* (*EJSClosureFunc1) (EJSContext* context, EJSValue* closure, int argc, EJSValue* arg1);
typedef EJSValue* (*EJSClosureFunc2) (EJSContext* context, EJSValue* closure, int argc, EJSValue* arg1, EJSValue* arg2);
typedef EJSValue* (*EJSClosureFunc3) (EJSContext* context, EJSValue* closure, int argc, EJSValue* arg1, EJSValue* arg2, EJSValue* arg3);

struct _EJSValue {
  EJSValueType type;

  union {
    // object members
    struct {
      EJSValue *proto;
      // field/property map
    } o;

    // number members
    struct {
      double data;
    } n;

    // string members
    struct {
      int len;
      char data[1]; // utf8 \0 terminated
    } s;

    // boolean members
    struct {
      EJSBool data;
    } b;

    // closure members
    struct {
      EJSClosureFunc0 func; /* this will be cast to the right arity */
      EJSClosureEnv* env;
    } closure;
  } u;
};

#define EJSVAL_IS_OBJECT(v)  (v->type == EJSValueTypeObject)
#define EJSVAL_IS_NUMBER(v)  (v->type == EJSValueTypeNumber)
#define EJSVAL_IS_STRING(v)  (v->type == EJSValueTypeString)
#define EJSVAL_IS_BOOLEAN(v) (v->type == EJSValueTypeBoolean)
#define EJSVAL_IS_CLOSURE(v) (v->type == EJSValueTypeClosure)

#define EJSVAL_TO_STRING(v) (v->u.s.data)
#define EJSVAL_TO_NUMBER(v) (v->u.n.data)
#define EJSVAL_TO_BOOLEAN(v) (v->u.b.data)
#define EJSVAL_TO_CLOSURE_FUNC(v) (v->u.closure.func)
#define EJSVAL_TO_CLOSURE_ENV(v) (v->u.closure.env)

#define EJS_NUMBER_FORMAT "%g"

EJSValue* _ejs_object_new (EJSValue *proto);
EJSValue* _ejs_string_new_utf8 (char* str);
EJSValue* _ejs_number_new (double value);
EJSValue* _ejs_boolean_new (EJSBool value);

EJSValue* _ejs_closure_new (EJSClosureFunc0 func, EJSClosureEnv* env);

EJSBool _ejs_object_setprop (EJSValue* obj, EJSValue* key, EJSValue* value);
EJSBool _ejs_object_getprop (EJSValue* obj, EJSValue* key, EJSValue** value);

EJSValue* _ejs_invoke_closure_0 (EJSContext* context, EJSValue* closure, int argc);
EJSValue* _ejs_invoke_closure_1 (EJSContext* context, EJSValue* closure, int argc, EJSValue *arg1);
EJSValue* _ejs_invoke_closure_2 (EJSContext* context, EJSValue* closure, int argc, EJSValue *arg1, EJSValue *arg2);
EJSValue* _ejs_invoke_closure_3 (EJSContext* context, EJSValue* closure, int argc, EJSValue *arg1, EJSValue *arg2, EJSValue *arg3);

#endif // _ejs_object_h_
