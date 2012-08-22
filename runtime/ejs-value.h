
#ifndef _ejs_value_h
#define _ejs_value_h

#include "ejs.h"
#include "ejs-object.h"
#include "ejs-array.h"
#include "ejs-function.h"

typedef enum {
  // the primitives
  EJSValueTypeBoolean,
  EJSValueTypeNumber,
  EJSValueTypeString,
  EJSValueTypeUndefined,

  // the object types
  EJSValueTypeObject,
  EJSValueTypeFunction,
  EJSValueTypeArray
} EJSValueType;

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

#define EJSVAL_IS_OBJECT(v)    (v->type == EJSValueTypeObject)
#define EJSVAL_IS_ARRAY(v)     (v->type == EJSValueTypeArray)
#define EJSVAL_IS_NUMBER(v)    (v->type == EJSValueTypeNumber)
#define EJSVAL_IS_STRING(v)    (v->type == EJSValueTypeString)
#define EJSVAL_IS_BOOLEAN(v)   (v->type == EJSValueTypeBoolean)
#define EJSVAL_IS_CLOSURE(v)   (v->type == EJSValueTypeFunction)
#define EJSVAL_IS_UNDEFINED(v) (v->type == EJSValueTypeUndefined)

#define EJSVAL_TO_STRING(v)       ((char*)v->u.s.data)
#define EJSVAL_TO_NUMBER(v)       (v->u.n.data)
#define EJSVAL_TO_BOOLEAN(v)      (v->u.b.data)
#define EJSVAL_TO_CLOSURE_FUNC(v) (v->u.closure.func)
#define EJSVAL_TO_CLOSURE_ENV(v)  (v->u.closure.env)

#define EJS_NUMBER_FORMAT "%g"

void _ejs_dump_value (EJSValue* val);

EJSValue* _ejs_string_new_utf8 (const char* str);
EJSValue* _ejs_number_new (double value);
EJSValue* _ejs_boolean_new (EJSBool value);

EJSValue* _ejs_undefined_new ();


#endif /* _ejs_value_h */
