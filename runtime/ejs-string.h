#ifndef _ejs_string_h_
#define _ejs_string_h_

#include "ejs-object.h"

typedef struct {
  /* object header */
  EJSObject obj;

  /* string specific data */
  EJSValue *primStr;
} EJSString;

EJS_BEGIN_DECLS

extern EJSValue* _ejs_String;
extern EJSSpecOps _ejs_string_specops;

EJSValue* _ejs_string_get_prototype();
EJSObject* _ejs_string_alloc_instance();

void _ejs_string_finalize(EJSString* str);

void _ejs_string_init(EJSValue* global);

EJS_END_DECLS

#endif /* _ejs_string_h */
