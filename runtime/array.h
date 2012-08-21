#ifndef _ejs_array_h_
#define _ejs_array_h_

#include "object.h"

extern EJSValue* _ejs_Array;

EJSValue* _ejs_array_new (int numElements);
EJSValue* _ejs_array_get_prototype();

void _ejs_array_init();

#endif /* _ejs_array_h */
