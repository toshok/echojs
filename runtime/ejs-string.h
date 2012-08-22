#ifndef _ejs_string_h_
#define _ejs_string_h_

#include "ejs-object.h"

typedef struct {
  /* object header */
  EJSValue *proto;
  EJSPropertyMap* map;
  EJSValue **fields;


} EJSString;


extern EJSValue* _ejs_String;

EJSValue* _ejs_string_get_prototype();

void _ejs_string_init(EJSValue* global);

#endif /* _ejs_string_h */
