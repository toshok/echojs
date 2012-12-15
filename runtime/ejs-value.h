
#ifndef _ejs_value_h
#define _ejs_value_h

#include "ejs.h"
#include "ejs-gc.h"
#include "ejs-object.h"
#include "ejs-array.h"
#include "ejs-function.h"

typedef struct {
  GCObjectHeader header;
  double data;
} EJSPrimNumber;

typedef struct {
  GCObjectHeader header;
  int len;
  char data[1]; // utf8 \0 terminated
} EJSPrimString;

typedef struct {
  GCObjectHeader header;
  EJSBool data;
} EJSPrimBool;

union _EJSValue {
  GCObjectHeader header;

  EJSObject o;

  EJSPrimNumber n;
  EJSPrimString s;
  EJSPrimBool b;
};

#define EJSVAL_IS_PRIMITIVE(v) (EJSVAL_IS_NUMBER(v) || EJSVAL_IS_STRING(v) || EJSVAL_IS_BOOLEAN(v) || EJSVAL_IS_UNDEFINED(v))

#define EJSVAL_TAG(v) (((GCObjectHeader*)v)->tag & 0xffff)
#define EJSVAL_SET_TAG(v,t) ((GCObjectHeader*)v)->tag = (((GCObjectHeader*)v)->tag & ~0xffff) | t

#define EJSVAL_IS_OBJECT(v)    (EJSVAL_TAG(v) == EJSValueTagObject)
#define EJSVAL_IS_ARRAY(v)     (EJSVAL_IS_OBJECT(v) && ((EJSObject*)v)->proto == _ejs_array_get_prototype())
#define EJSVAL_IS_NUMBER(v)    (EJSVAL_TAG(v) == EJSValueTagNumber)
#define EJSVAL_IS_STRING(v)    (EJSVAL_TAG(v) == EJSValueTagString)
#define EJSVAL_IS_BOOLEAN(v)   (EJSVAL_TAG(v) == EJSValueTagBoolean)
#define EJSVAL_IS_FUNCTION(v)     (EJSVAL_IS_OBJECT(v) && ((EJSObject*)v)->proto == _ejs_function_get_prototype())
#define EJSVAL_IS_UNDEFINED(v) (EJSVAL_TAG(v) == EJSValueTagUndefined)

#define EJSVAL_TO_STRING(v)       ((char*)((EJSPrimString*)v)->data)
#define EJSVAL_TO_STRLEN(v)       (((EJSPrimString*)v)->len)
#define EJSVAL_TO_NUMBER(v)       (((EJSPrimNumber*)v)->data)
#define EJSVAL_TO_BOOLEAN(v)      (((EJSPrimBool*)v)->data)
#define EJSVAL_TO_FUNC(v)         (((EJSFunction*)v)->func)
#define EJSVAL_TO_ENV(v)          (((EJSFunction*)v)->env)

#define EJS_NUMBER_FORMAT "%g"

EJS_BEGIN_DECLS

void _ejs_dump_value (EJSValue* val);

EJSValue* _ejs_string_new_utf8 (const char* str);
EJSValue* _ejs_string_new_utf8_len (const char* str, int len);

EJSValue* _ejs_number_new (double value);
EJSValue* _ejs_boolean_new (EJSBool value);

EJSValue* _ejs_undefined_new ();

EJS_END_DECLS

#endif /* _ejs_value_h */
