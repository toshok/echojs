
#ifndef _ejs_object_h_
#define _ejs_object_h_

#include "ejs.h"
#include "ejs-object.h"

typedef struct _EJSPropertyMap EJSPropertyMap;
typedef struct _EJSPropertyIterator EJSPropertyIterator;

typedef struct {
  EJSBool configurable;
  EJSBool writable;
  EJSBool enumerable;
} EJSPropertyDesc;

typedef EJSValue* (*SpecOpGet) (EJSValue* obj, EJSValue* propertyName);
typedef EJSValue* (*SpecOpGetOwnProperty) (EJSValue* obj, EJSValue* propertyName);
typedef EJSValue* (*SpecOpGetProperty) (EJSValue* obj, EJSValue* propertyName);
typedef void      (*SpecOpPut) (EJSValue* obj, EJSValue* propertyName, EJSValue* val, EJSBool flag);
typedef EJSBool   (*SpecOpCanPut) (EJSValue* obj, EJSValue* propertyName);
typedef EJSBool   (*SpecOpHasProperty) (EJSValue* obj, EJSValue* propertyName);
typedef EJSBool   (*SpecOpDelete) (EJSValue* obj, EJSValue* propertyName, EJSBool flag);
typedef EJSValue* (*SpecOpDefaultValue) (EJSValue* obj, const char *hint);
typedef void      (*SpecOpDefineOwnProperty) (EJSValue* obj, EJSValue* propertyName, EJSValue* propertyDescriptor, EJSBool flag);

typedef struct {
  const char* class_name;
  SpecOpGet get;
  SpecOpGetOwnProperty get_own_property;
  SpecOpGetProperty get_property;
  SpecOpPut put;
  SpecOpCanPut can_put;
  SpecOpHasProperty has_property;
  SpecOpDelete _delete;
  SpecOpDefaultValue default_value;
  SpecOpDefineOwnProperty define_own_property;
} EJSSpecOps;

typedef struct {
  EJSValueTag tag;
  EJSValue *proto;
  EJSSpecOps *ops;
  EJSPropertyMap* map;
} EJSObject;

EJS_BEGIN_DECLS

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
EJSObject* _ejs_object_alloc_instance();
void      _ejs_init_object (EJSObject *obj, EJSValue *proto);

EJSValue* _ejs_object_get_prototype();

void _ejs_object_init(EJSValue *global);

EJS_END_DECLS

#endif // _ejs_object_h_
