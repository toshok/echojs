#ifndef _ejs_number_h_
#define _ejs_number_h_

#include "ejs-object.h"

typedef struct {
  /* object header */
  EJSObject obj;

  /* number specific data */
  double number;
} EJSNumber;

EJS_BEGIN_DECLS

extern EJSValue* _ejs_Number;
extern EJSSpecOps _ejs_number_specops;

EJSValue* _ejs_number_get_prototype();
EJSObject* _ejs_number_alloc_instance();

void _ejs_number_init(EJSValue* global);

EJS_END_DECLS

#endif /* _ejs_number_h */
