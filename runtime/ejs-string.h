#ifndef _ejs_string_h_
#define _ejs_string_h_

#include "ejs-object.h"

typedef struct {
  /* object header */
  EJSObject obj;

  /* string specific data */
  int len;
  char *str;
} EJSString;

extern EJSValue* _ejs_String;

EJSValue* _ejs_string_get_prototype();
EJSObject* _ejs_string_alloc_instance();

void _ejs_string_init(EJSValue* global);

#endif /* _ejs_string_h */
