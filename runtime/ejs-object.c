#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "ejs-value.h"
#include "ejs-ops.h"
#include "ejs-object.h"
#include "ejs-string.h"
#include "ejs-regexp.h"
#include "ejs-date.h"
#include "ejs-number.h"
#include "ejs-array.h"

// really terribly performing property maps
struct _EJSPropertyMap {
  char **names;
  int allocated;
  int num;
};

EJSPropertyMap*
_ejs_propertymap_new (int initial_allocation)
{
  EJSPropertyMap* rv = (EJSPropertyMap*)calloc(1, sizeof (EJSPropertyMap));
  rv->names = (char**)malloc(sizeof(char*) * initial_allocation);
  rv->num = 0;
  rv->allocated = initial_allocation;
  return rv;
}

int
_ejs_propertymap_lookup (EJSPropertyMap *map, const char *name, EJSBool add_if_not_found)
{
  int i;
  for (i = 0; i < map->num; i ++) {
    if (!strcmp (map->names[i], name))
      return i;
  }
  
  int idx = -1;
  if (add_if_not_found) {
    idx = map->num++;
    if (idx == map->allocated - 1) {
      int new_allocated = map->allocated + 20;
      char **new_names = (char**)malloc(sizeof(char*) * new_allocated);
      memmove (new_names, map->names, map->allocated * sizeof(char*));
      free (map->names);
      map->names = new_names;
      map->allocated = new_allocated;
    }
    map->names[idx] = strdup (name);
  }

  return idx;
}

/* property iterators */
struct _EJSPropertyIterator {
  EJSObject *forObj;
  char **names;
  int num;
  int current;
};

EJSPropertyIterator*
_ejs_property_iterator_new (EJSValue *forVal)
{
  if (EJSVAL_IS_PRIMITIVE(forVal))
    abort();

  EJSObject* forObj = (EJSObject*)forVal;

  // totally broken, only iterator over forObj's property map, not prototype properties
  EJSPropertyIterator* iterator = (EJSPropertyIterator*)calloc(1, sizeof (EJSPropertyIterator));

  EJSPropertyMap *map = forObj->map;

  if (map) {
    int i;

    iterator->forObj = forObj;
    iterator->num = map->num;
    iterator->names = (char**)malloc(sizeof(char*) * iterator->num);
    for (i = 0; i < iterator->num; i ++)
      iterator->names[i] = strdup(map->names[i]);
  }
  return iterator;
}

char *
_ejs_property_iterator_current (EJSPropertyIterator* iterator)
{
  return iterator->current < iterator->num ? iterator->names[iterator->current] : NULL;
}

void
_ejs_property_iterator_next (EJSPropertyIterator* iterator)
{
  iterator->current ++;
}

void
_ejs_property_iterator_free (EJSPropertyIterator *iterator)
{
  int i;
  for (i = 0; i < iterator->num; i ++)
    free(iterator->names[i]);
  free (iterator->names);
  free (iterator);
}


///

void
_ejs_init_object (EJSObject *obj, EJSValue *proto)
{
  obj->tag = EJSValueTagObject;
  obj->proto = proto;
  obj->map = _ejs_propertymap_new (40);
  obj->fields = (EJSValue**)calloc(40, sizeof (EJSValue*));
}

EJSValue*
_ejs_object_new (EJSValue *proto)
{
  if (proto == NULL) proto = _ejs_object_get_prototype();

  EJSObject *obj;

  if (proto == _ejs_array_get_prototype())
    obj = _ejs_array_alloc_instance();
  else if (proto == _ejs_string_get_prototype())
    obj = _ejs_string_alloc_instance();
  else if (proto == _ejs_number_get_prototype())
    obj = _ejs_number_alloc_instance();
  else if (proto == _ejs_regexp_get_prototype())
    obj = _ejs_regexp_alloc_instance();
  else if (proto == _ejs_date_get_prototype())
    obj = _ejs_date_alloc_instance();
  else
    obj = _ejs_object_alloc_instance();

  _ejs_init_object (obj, proto);
  return (EJSValue*)obj;
}

EJSObject* _ejs_object_alloc_instance()
{
  return (EJSObject*)calloc(1, sizeof (EJSValue));
}

EJSValue*
_ejs_array_new (int numElements)
{
  EJSArray* rv = (EJSArray*)calloc(1, sizeof(EJSArray));

  _ejs_init_object ((EJSObject*)rv, _ejs_array_get_prototype());

  rv->array_length = 0;
  rv->array_alloc = numElements + 40;
  rv->elements = (EJSValue**)calloc(rv->array_alloc, sizeof (EJSValue*));
  return (EJSValue*)rv;
}

#define offsetof(t,f) (int)(long)(&((t*)NULL)->f)

EJSValue*
_ejs_string_new_utf8 (const char* str)
{
  int str_len = strlen(str);
  int value_size = sizeof(EJSPrimString) + str_len; /* no +1 here since PrimString already includes 1 byte */

  EJSPrimString* rv = (EJSPrimString*)calloc(1, value_size);
  rv->tag = EJSValueTagString;
  rv->len = str_len;
  strcpy (EJSVAL_TO_STRING(rv), str);
  return (EJSValue*)rv;
}

EJSValue*
_ejs_number_new (double value)
{
  EJSPrimNumber* rv = (EJSPrimNumber*)calloc(1, sizeof (EJSPrimNumber));
  rv->tag = EJSValueTagNumber;
  rv->data = value;
  return (EJSValue*)rv;
}

EJSValue*
_ejs_boolean_new_internal (EJSBool value)
{
  EJSPrimBool* rv = (EJSPrimBool*)calloc(1, sizeof (EJSPrimBool));
  rv->tag = EJSValueTagBoolean;
  rv->data = value;
  return (EJSValue*)rv;
}

EJSValue*
_ejs_boolean_new (EJSBool value)
{
  return value ? _ejs_true : _ejs_false;
}

EJSValue*
_ejs_undefined_new ()
{
  EJSValueTag* rv = (EJSValueTag*)malloc(sizeof (EJSValueTag));
  *rv = EJSValueTagUndefined;
  return (EJSValue*)rv;
}

EJSValue*
_ejs_object_setprop (EJSValue* val, EJSValue* key, EJSValue* value)
{
  if (EJSVAL_IS_PRIMITIVE(val)) {
    printf ("setprop on primitive.  ignoring\n" );
    abort();
  }

  EJSObject* obj = (EJSObject*)val;

  if (EJSVAL_IS_ARRAY(obj)) {
    // check if key is a uint32, or a string that we can convert to an uint32
    int idx = -1;
    if (EJSVAL_IS_NUMBER(key)) {
      double n = EJSVAL_TO_NUMBER(key);
      if (floor(n) == n) {
	idx = (int)n;
      }
    }

    if (idx != -1) {
      if (idx >= EJS_ARRAY_ALLOC(obj)) {
	int new_alloc = idx + 10;
	EJSValue** new_elements = malloc (sizeof(EJSValue**) * new_alloc);
	memmove (new_elements, EJS_ARRAY_ELEMENTS(obj), EJS_ARRAY_ALLOC(obj) * sizeof(EJSValue*));
	free (EJS_ARRAY_ELEMENTS(obj));
	EJS_ARRAY_ELEMENTS(obj) = new_elements;
	EJS_ARRAY_ALLOC(obj) = new_alloc;
      }
      EJS_ARRAY_ELEMENTS(obj)[idx] = value;
      EJS_ARRAY_LEN(obj) = idx + 1;
      if (EJS_ARRAY_LEN(obj) >= EJS_ARRAY_ALLOC(obj))
	abort();
      return value;
    }
    // if we fail there, we fall back to the object impl below
  }

  if (!EJSVAL_IS_STRING(key)) {
    if (EJSVAL_IS_NUMBER(key)) {
      char buf[128];
      snprintf(buf, sizeof(buf), EJS_NUMBER_FORMAT, EJSVAL_TO_NUMBER(key));
      key = _ejs_string_new_utf8(buf);
    }
    else {
      printf ("key isn't a string\n");
      return NULL;
    }
  }

  int prop_index = _ejs_propertymap_lookup (obj->map, EJSVAL_TO_STRING(key), TRUE);
  obj->fields[prop_index] = value;

  return value;
}

EJSValue*
_ejs_object_getprop (EJSValue* obj_, EJSValue* key)
{
  if (!obj_) {
    printf ("attempt to get property '%s' on null object.\n", EJSVAL_TO_STRING(key));
    abort();
  }

  if (EJSVAL_IS_STRING(obj_)) {
    // check if key is an integer, or a string that we can convert to an int
    EJSBool is_index = FALSE;
    int idx = 0;
    if (EJSVAL_IS_NUMBER(key)) {
      double n = EJSVAL_TO_NUMBER(key);
      if (floor(n) == n) {
	idx = (int)n;
	is_index = TRUE;
      }
    }

    if (is_index) {
      if (idx < 0 || idx >  EJSVAL_TO_STRLEN(obj_))
	return _ejs_undefined;
      char c[2];
      c[1] = EJSVAL_TO_STRING(obj_)[idx];
      c[2] = '\0';
      return _ejs_string_new_utf8 (c);
    }

    if (EJSVAL_IS_STRING(key) && !strcmp ("length", EJSVAL_TO_STRING(key))) {
      return _ejs_number_new (EJSVAL_TO_STRLEN(obj_));
    }
  }

  EJSValue* valToObject = ToObject(obj_);

  if (EJSVAL_IS_PRIMITIVE(valToObject)) {
    printf ("getprop on primitive.  returning undefined\n");
    return _ejs_undefined;
  }

  EJSObject* obj = (EJSObject*)valToObject;

  if (EJSVAL_IS_ARRAY(obj)) {
    // check if key is an integer, or a string that we can convert to an int
    EJSBool is_index = FALSE;
    int idx = 0;
    if (EJSVAL_IS_NUMBER(key)) {
      double n = EJSVAL_TO_NUMBER(key);
      if (floor(n) == n) {
	idx = (int)n;
	is_index = TRUE;
      }
    }

    if (is_index) {
      if (idx < 0 || idx > EJS_ARRAY_LEN(obj))
	return _ejs_undefined;
      return EJS_ARRAY_ELEMENTS(obj)[idx];
    }

    if (EJSVAL_IS_STRING(key) && !strcmp ("length", EJSVAL_TO_STRING(key))) {
      return _ejs_number_new (EJS_ARRAY_LEN(obj));
    }

    // if we fail there, we fall back to the object impl below
  }

  if (!EJSVAL_IS_STRING(key)) {
    if (EJSVAL_IS_NUMBER(key)) {
      char buf[128];
      snprintf(buf, sizeof(buf), EJS_NUMBER_FORMAT, EJSVAL_TO_NUMBER(key));
      key = _ejs_string_new_utf8(buf);
    }
    else {
      printf ("key isn't a string\n");
      return _ejs_undefined;
    }
  }

  int prop_index = _ejs_propertymap_lookup (obj->map, EJSVAL_TO_STRING(key), FALSE);

  if (prop_index == -1) {
    if (EJSVAL_IS_STRING(key) && !strcmp("prototype", EJSVAL_TO_STRING(key)))
      return obj->proto;

    if (obj->proto)
      return _ejs_object_getprop (obj->proto, key);
    else
      return _ejs_undefined;
  }
  else {
    return obj->fields[prop_index];
  }
}

EJSValue*
_ejs_object_setprop_utf8 (EJSValue* val, const char *key, EJSValue* value)
{
  if (EJSVAL_IS_PRIMITIVE(val)) {
    printf ("setprop on primitive.  ignoring\n");
    abort();
  }

  EJSObject *obj = (EJSObject*)val;
  int prop_index = _ejs_propertymap_lookup (obj->map, key, TRUE);
  obj->fields[prop_index] = value;

  return value;
}

EJSValue*
_ejs_object_getprop_utf8 (EJSValue* val, const char *key)
{
  if (!val) {
    printf ("attempt to get property '%s' on null object.\n", key);
    return _ejs_undefined; // XXX this should really throw an exception
  }

  EJSValue* valToObject = ToObject(val);

  if (EJSVAL_IS_PRIMITIVE(valToObject)) {
    printf ("getprop_utf8(%s) on primitive\n", key);
    abort();
  }

  EJSObject *obj = (EJSObject*)valToObject;

  if (EJSVAL_IS_ARRAY(obj)) {
    if (!strcmp ("length", key)) {
      return _ejs_number_new (EJS_ARRAY_LEN(obj));
    }

    // if we fail there, we fall back to the object impl below
  }

  int prop_index = _ejs_propertymap_lookup (obj->map, key, FALSE);

  if (prop_index == -1) {
    if (!strcmp(key, "prototype"))
      return obj->proto;

    if (obj->proto)
      return _ejs_object_getprop_utf8 (obj->proto, key);
    else
      return _ejs_undefined;
  }
  else {
    return obj->fields[prop_index];
  }
}

void
_ejs_dump_value (EJSValue* val)
{
  if (EJSVAL_IS_UNDEFINED(val)) {
    printf ("undefined\n");
  }
  else if (EJSVAL_IS_NUMBER(val)) {
    printf (EJS_NUMBER_FORMAT "\n", EJSVAL_TO_NUMBER(val));
  }
  else if (EJSVAL_IS_BOOLEAN(val)) {
    printf ("%s\n", EJSVAL_TO_BOOLEAN(val) ? "true" : "false");
  }
  else if (EJSVAL_IS_STRING(val)) {
    printf ("'%s'\n", EJSVAL_TO_STRING(val));
  }
  else if (EJSVAL_IS_ARRAY(val)) {
    printf ("<array>\n");
  }
  else if (EJSVAL_IS_OBJECT(val)) {
    printf ("<object>\n");
  }
  else if (EJSVAL_IS_FUNCTION(val)) {
    printf ("<closure>\n");
  }
}

EJSValue* _ejs_Object;
static EJSValue*
_ejs_Object_impl (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  if (EJSVAL_IS_UNDEFINED(_this)) {
    // ECMA262: 15.2.1.1
    
    printf ("called Object() as a function!\n");
    return NULL;
  }
  else {
    // ECMA262: 15.2.2

    printf ("called Object() as a constructor!\n");
    return NULL;
  }
}

// ECMA262: 15.2.3.2
static EJSValue*
_ejs_Object_getPrototypeOf (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  abort();
}

// ECMA262: 15.2.3.3
static EJSValue*
_ejs_Object_getOwnPropertyDescriptor (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  abort();
}

// ECMA262: 15.2.3.4
static EJSValue*
_ejs_Object_getOwnPropertyNames (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  abort();
}

// FIXME ECMA262: 15.2.3.5
static EJSValue*
_ejs_Object_create (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  /* Object.create ( O [, Properties] ) */
  /* 1. If Type(O) is not Object or Null throw a TypeError exception. */
  /* 2. Let obj be the result of creating a new object as if by the expression new Object() where Object is the  */
  /* standard built-in constructor with that name */
  EJSValue* obj = _ejs_object_new(NULL);
  /* 3. Set the [[Prototype]] internal property of obj to O. */
  ((EJSObject*)obj)->proto = args[0];
  /* 4. If the argument Properties is present and not undefined, add own properties to obj as if by calling the  */
  /* standard built-in function Object.defineProperties with arguments obj and Properties. */
  /* 5. Return obj. */
  return obj;
}

// ECMA262: 15.2.3.6
static EJSValue*
_ejs_Object_defineProperty (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  abort();
}

// ECMA262: 15.2.3.7
static EJSValue*
_ejs_Object_defineProperties (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  abort();
}

// ECMA262: 15.2.3.8
static EJSValue*
_ejs_Object_seal (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  return args[0];
}

// FIXME ECMA262: 15.2.3.9
static EJSValue*
_ejs_Object_freeze (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  return args[0];
}

// ECMA262: 15.2.3.10
static EJSValue*
_ejs_Object_preventExtensions (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  abort();
}

// ECMA262: 15.2.3.11
static EJSValue*
_ejs_Object_isSealed (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  abort();
}

// ECMA262: 15.2.3.12
static EJSValue*
_ejs_Object_isFrozen (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  abort();
}

// ECMA262: 15.2.3.13
static EJSValue*
_ejs_Object_isExtensible (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  abort();
}

// ECMA262: 15.2.3.14
static EJSValue*
_ejs_Object_keys (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  abort();
}

// ECMA262: 15.2.4.2
static EJSValue*
_ejs_Object_prototype_toString (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  return _ejs_string_new_utf8 ("[object Object]");
}

// ECMA262: 15.2.4.3
static EJSValue*
_ejs_Object_prototype_toLocaleString (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  abort();
}

// ECMA262: 15.2.4.4
static EJSValue*
_ejs_Object_prototype_valueOf (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  abort();
}

// ECMA262: 15.2.4.5
static EJSValue*
_ejs_Object_prototype_hasOwnProperty (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  EJSObject* obj = (EJSObject*)_this;

  if (argc == 0)
    return _ejs_boolean_new(FALSE);

  EJSValue* needle = args[0];
  if (EJSVAL_IS_NUMBER(needle)) {
    // if the oject is an array, we look up the index.
  }

  EJSValue* needle_str = ToString(needle);

  int idx = _ejs_propertymap_lookup (obj->map, EJSVAL_TO_STRING(needle_str), FALSE);
  return _ejs_boolean_new (idx != -1);
}

// ECMA262: 15.2.4.6
static EJSValue*
_ejs_Object_prototype_isPrototypeOf (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  abort();
}

// ECMA262: 15.2.4.7
static EJSValue*
_ejs_Object_prototype_propertyIsEnumerable (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  abort();
}

static EJSValue* _ejs_Object_proto = NULL;
EJSValue*
_ejs_object_get_prototype()
{
  return _ejs_Object_proto;
}

void
_ejs_object_init (EJSValue *global)
{
  // FIXME ECMA262 15.2.4
  _ejs_Object = _ejs_function_new (NULL, (EJSClosureFunc)_ejs_Object_impl);
  _ejs_Object_proto = _ejs_object_new(NULL);

  // ECMA262 15.2.3.1
  _ejs_object_setprop_utf8 (_ejs_Object,       "prototype",    _ejs_Object_proto); // FIXME: {[[Writable]]: false, [[Enumerable]]: false, [[Configurable]]: false }
  // ECMA262: 15.2.4.1
  _ejs_object_setprop_utf8 (_ejs_Object_proto, "constructor",  _ejs_Object);

#define OBJ_METHOD(x) _ejs_object_setprop_utf8 (_ejs_Object, #x, _ejs_function_new (NULL, (EJSClosureFunc)_ejs_Object_##x))
#define PROTO_METHOD(x) _ejs_object_setprop_utf8 (_ejs_Object_proto, #x, _ejs_function_new (NULL, (EJSClosureFunc)_ejs_Object_prototype_##x))

  OBJ_METHOD(getPrototypeOf);
  OBJ_METHOD(getOwnPropertyDescriptor);
  OBJ_METHOD(getOwnPropertyNames);
  OBJ_METHOD(create);
  OBJ_METHOD(defineProperty);
  OBJ_METHOD(defineProperties);
  OBJ_METHOD(seal);
  OBJ_METHOD(freeze);
  OBJ_METHOD(preventExtensions);
  OBJ_METHOD(isSealed);
  OBJ_METHOD(isFrozen);
  OBJ_METHOD(isExtensible);
  OBJ_METHOD(keys);

  PROTO_METHOD(toString);
  PROTO_METHOD(toLocaleString);
  PROTO_METHOD(valueOf);
  PROTO_METHOD(hasOwnProperty);
  PROTO_METHOD(isPrototypeOf);
  PROTO_METHOD(propertyIsEnumerable);

#undef PROTOTYPE_METHOD
#undef OBJ_METHOD

  _ejs_object_setprop_utf8 (global, "Object", _ejs_Object);
}



