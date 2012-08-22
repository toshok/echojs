#ifndef _ejs_array_h_
#define _ejs_array_h_

#include "object.h"

typedef struct {
  /* object header */
  EJSValue *proto;
  EJSPropertyMap* map;
  EJSValue **fields;

  int array_length;
  int array_alloc;
  EJSValue **elements;
} EJSArray;


extern EJSValue* _ejs_Array;

EJSValue* _ejs_array_new (int numElements);
EJSValue* _ejs_array_get_prototype();

void _ejs_array_init(EJSValue *global);

#endif /* _ejs_array_h */
