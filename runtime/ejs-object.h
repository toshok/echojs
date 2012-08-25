
#ifndef _ejs_object_h_
#define _ejs_object_h_

#include "ejs.h"

typedef struct _EJSPropertyMap EJSPropertyMap;
typedef struct _EJSPropertyIterator EJSPropertyIterator;

typedef struct {
  EJSBool configurable;
  EJSBool writable;
  EJSBool enumerable;
} EJSPropertyDesc;

typedef struct {
  EJSValue *proto;
  EJSPropertyMap* map;
  EJSValue **fields;
} EJSObject;

EJSPropertyMap* _ejs_propertymap_new (int initial_allocation);
int _ejs_propertymap_lookup (EJSPropertyMap *map, const char *name, EJSBool add_if_not_found);

EJSValue* _ejs_object_setprop (EJSValue* obj, EJSValue* key, EJSValue* value);
EJSValue* _ejs_object_getprop (EJSValue* obj, EJSValue* key);

EJSValue* _ejs_object_setprop_utf8 (EJSValue* obj, const char *key, EJSValue* value);
EJSValue* _ejs_object_getprop_utf8 (EJSValue* obj, const char *key);

EJSPropertyIterator* _ejs_property_iterator_new (EJSValue *forObj);
char *_ejs_property_iterator_current (EJSPropertyIterator* iterator);
void _ejs_property_iterator_next (EJSPropertyIterator* iterator);
void _ejs_property_iterator_free (EJSPropertyIterator *iterator);

extern EJSValue* _ejs_Object;

EJSValue* _ejs_object_new (EJSValue *proto);

EJSValue* _ejs_object_get_prototype();

void _ejs_object_init(EJSValue *global);

#endif // _ejs_object_h_
