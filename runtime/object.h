
#ifndef _ejs_object_h_
#define _ejs_object_h_

#include "ejs.h"

typedef enum {
  EJSValueTypeObject,
  EJSValueTypeBoolean,
  EJSValueTypeNumber,
  EJSValueTypeString,
  EJSValueTypeClosure,
  EJSValueTypeUndefined,

  EJSValueTypeArray
} EJSValueType;


typedef struct _EJSContext* EJSContext;

typedef struct _EJSValue EJSValue;
typedef struct _EJSPropertyMap EJSPropertyMap;

// for now we just build environments out of EJS objects
typedef struct _EJSValue EJSClosureEnv;

typedef EJSValue* (*EJSClosureFunc) (EJSValue* env, EJSValue* _this, int argc, EJSValue** args);

typedef struct {
  EJSBool configurable;
  EJSBool writable;
  EJSBool enumerable;
} EJSPropertyDesc;

typedef struct {
  EJSValue *proto;
  EJSPropertyMap* map;
  EJSValue **fields;
} EJSObject;

typedef struct {
  /* object header */
  EJSValue *proto;
  EJSPropertyMap* map;
  EJSValue **fields;

  int array_length;
  int array_alloc;
  EJSValue **elements;
} EJSArray;

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

struct _EJSValue {
  EJSValueType type;

  union {
    EJSObject o;
    EJSArray a;
    EJSClosure closure;

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
  } u;
};

#define EJSVAL_IS_PRIMITIVE(v) (EJSVAL_IS_NUMBER(v) || EJSVAL_IS_STRING(v) || EJSVAL_IS_BOOLEAN(v) || EJSVAL_IS_UNDEFINED(v))

#define EJSVAL_IS_OBJECT(v)  (v->type == EJSValueTypeObject)
#define EJSVAL_IS_ARRAY(v)  (v->type == EJSValueTypeArray)
#define EJSVAL_IS_NUMBER(v)  (v->type == EJSValueTypeNumber)
#define EJSVAL_IS_STRING(v)  (v->type == EJSValueTypeString)
#define EJSVAL_IS_BOOLEAN(v) (v->type == EJSValueTypeBoolean)
#define EJSVAL_IS_CLOSURE(v) (v->type == EJSValueTypeClosure)
#define EJSVAL_IS_UNDEFINED(v) (v->type == EJSValueTypeUndefined)

#define EJSVAL_TO_STRING(v) ((char*)v->u.s.data)
#define EJSVAL_TO_NUMBER(v) (v->u.n.data)
#define EJSVAL_TO_BOOLEAN(v) (v->u.b.data)
#define EJSVAL_TO_CLOSURE_FUNC(v) (v->u.closure.func)
#define EJSVAL_TO_CLOSURE_ENV(v) (v->u.closure.env)

#define EJS_NUMBER_FORMAT "%g"

EJSValue* _ejs_string_new_utf8 (const char* str);
EJSValue* _ejs_number_new (double value);
EJSValue* _ejs_boolean_new (EJSBool value);

EJSValue* _ejs_undefined_new ();

EJSValue* _ejs_closure_new (EJSClosureEnv* env, EJSClosureFunc func);

EJSValue* _ejs_object_setprop (EJSValue* obj, EJSValue* key, EJSValue* value);
EJSValue* _ejs_object_getprop (EJSValue* obj, EJSValue* key);

EJSValue* _ejs_invoke_closure_0 (EJSValue* closure, EJSValue* _this, int argc);
EJSValue* _ejs_invoke_closure_1 (EJSValue* closure, EJSValue* _this, int argc, EJSValue *arg1);
EJSValue* _ejs_invoke_closure_2 (EJSValue* closure, EJSValue* _this, int argc, EJSValue *arg1, EJSValue *arg2);
EJSValue* _ejs_invoke_closure_3 (EJSValue* closure, EJSValue* _this, int argc, EJSValue *arg1, EJSValue *arg2, EJSValue *arg3);
EJSValue* _ejs_invoke_closure_4 (EJSValue* closure, EJSValue* _this, int argc, EJSValue *arg1, EJSValue *arg2, EJSValue *arg3, EJSValue *arg4);
EJSValue* _ejs_invoke_closure_5 (EJSValue* closure, EJSValue* _this, int argc, EJSValue *arg1, EJSValue *arg2, EJSValue *arg3, EJSValue *arg4, EJSValue *arg5);
EJSValue* _ejs_invoke_closure_6 (EJSValue* closure, EJSValue* _this, int argc, EJSValue *arg1, EJSValue *arg2, EJSValue *arg3, EJSValue *arg4, EJSValue *arg5, EJSValue *arg6);
EJSValue* _ejs_invoke_closure_7 (EJSValue* closure, EJSValue* _this, int argc, EJSValue *arg1, EJSValue *arg2, EJSValue *arg3, EJSValue *arg4, EJSValue *arg5, EJSValue *arg6, EJSValue *arg7);

extern EJSValue* _ejs_undefined;
extern EJSValue* _ejs_true;
extern EJSValue* _ejs_false;
extern EJSValue* _ejs_global;

void _ejs_dump_value (EJSValue* val);


extern EJSValue* _ejs_Object;

EJSValue* _ejs_object_new (EJSValue *proto);

EJSValue* _ejs_object_get_prototype();

void _ejs_object_init();

#endif // _ejs_object_h_
