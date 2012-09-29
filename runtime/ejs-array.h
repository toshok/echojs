#ifndef _ejs_array_h_
#define _ejs_array_h_

#include "ejs-object.h"

typedef struct {
  /* object header */
  EJSObject obj;

  /* array data */
  int array_length;
  int array_alloc;
  EJSValue **elements;
} EJSArray;

#define EJS_ARRAY_ALLOC(obj) (((EJSArray*)obj)->array_alloc)
#define EJS_ARRAY_LEN(obj) (((EJSArray*)obj)->array_length)
#define EJS_ARRAY_ELEMENTS(obj) (((EJSArray*)obj)->elements)

EJS_BEGIN_DECLS

extern EJSValue* _ejs_Array;

EJSObject* _ejs_array_alloc_instance();
EJSValue* _ejs_array_new (int numElements);
EJSValue* _ejs_array_get_prototype();

void _ejs_array_init(EJSValue *global);

EJS_END_DECLS

#endif /* _ejs_array_h */
