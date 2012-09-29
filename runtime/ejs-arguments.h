#ifndef _ejs_arguments_h_
#define _ejs_arguments_h_

#include "ejs-object.h"

typedef struct {
  /* object header */
  EJSObject obj;

  /* arguments specific data */
  int argc;
  EJSValue** args;
} EJSArguments;

EJS_BEGIN_DECLS

extern EJSValue* _ejs_Arguments;

EJSValue* _ejs_arguments_get_prototype();
EJSObject* _ejs_arguments_alloc_instance();

void _ejs_arguments_init(EJSValue* global);

EJS_END_DECLS

#endif /* _ejs_arguments_h */
