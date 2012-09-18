#ifndef _ejs_number_h_
#define _ejs_number_h_

#include "ejs-object.h"

typedef struct {
  /* object header */
  EJSObject obj;

  /* number specific data */
  double number;
} EJSNumber;

extern EJSValue* _ejs_Number;

EJSValue* _ejs_number_get_prototype();
EJSObject* _ejs_number_alloc_instance();

void _ejs_number_init(EJSValue* global);

#endif /* _ejs_number_h */
