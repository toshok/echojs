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

EJS_BEGIN_DECLS

extern ejsval _ejs_Regexp;
extern ejsval _ejs_Regexp_proto;
extern EJSSpecOps _ejs_regexp_specops;


EJSObject* _ejs_regexp_alloc_instance();
void _ejs_regexp_init(ejsval global);

ejsval _ejs_regexp_new_utf8 (const char* str);

EJS_END_DECLS

#endif /* _ejs_regexp_h */