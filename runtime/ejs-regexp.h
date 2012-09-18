#ifndef _ejs_regexp_h_
#define _ejs_regexp_h_

#include "ejs-object.h"

typedef struct {
  /* object header */
  EJSObject obj;

  /* regexp specific data */
  int pattern_len;
  char *pattern;
} EJSRegexp;


extern EJSValue* _ejs_Regexp;

EJSValue* _ejs_regexp_get_prototype();

EJSObject* _ejs_regexp_alloc_instance();
void _ejs_regexp_init(EJSValue* global);

EJSValue* _ejs_regexp_new_utf8 (const char* str);

#endif /* _ejs_regexp_h */
