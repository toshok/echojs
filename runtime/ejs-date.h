#ifndef _ejs_date_h_
#define _ejs_date_h_

#include <time.h>
#include "ejs-object.h"

typedef struct {
  /* object header */
  EJSObject obj;

  /* date specific data */
  struct tm tm;
} EJSDate;


extern EJSValue* _ejs_Date;

EJSValue* _ejs_date_get_prototype();

EJSValue* _ejs_date_new_unix (int timestamp);

EJSObject* _ejs_date_alloc_instance();
void _ejs_date_init(EJSValue* global);

#endif /* _ejs_date_h */
