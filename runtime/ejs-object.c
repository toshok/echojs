#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "ejs-value.h"
#include "ejs-object.h"
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
_ejs_propertymap_lookup (EJSPropertyMap *map, char *name, EJSBool add_if_not_found)
{
  int i;
  for (i = 0; i < map->num; i ++) {
    if (!strcmp (map->names[i], name))
      return i;
  }
  
  int idx = -1;
  if (add_if_not_found) {
    idx = map->num++;
    map->names[idx] = strdup (name);
  }

  return idx;
}

EJSValue*
_ejs_object_new (EJSValue *proto)
{
  EJSValue* rv = (EJSValue*)calloc(1, sizeof (EJSValue));
  rv->type = EJSValueTypeObject;
  rv->u.a.proto = proto ? proto : _ejs_object_get_prototype();
  rv->u.o.map = _ejs_propertymap_new (8);
  rv->u.o.fields = (EJSValue**)calloc(8, sizeof (EJSValue*));
  return rv;
}

EJSValue*
_ejs_array_new (int numElements)
{
  EJSValue* rv = _ejs_object_new(NULL/* XXX should be array prototype*/);
  rv->type = EJSValueTypeArray;
  rv->u.a.proto = _ejs_array_get_prototype();
  rv->u.a.map = _ejs_propertymap_new (8);
  rv->u.a.fields = (EJSValue**)calloc(8, sizeof (EJSValue*));
  rv->u.a.array_length = 0;
  rv->u.a.array_alloc = numElements;
  rv->u.a.elements = (EJSValue**)calloc(rv->u.a.array_alloc, sizeof (EJSValue*));
  return rv;
}

EJSValue*
_ejs_string_new_utf8 (const char* str)
{
  int str_len = strlen(str);
  int value_size = sizeof (EJSValueType) + sizeof (int) + str_len + 1;

  EJSValue* rv = (EJSValue*)calloc(1, value_size);
  rv->type = EJSValueTypeString;
  rv->u.s.len = str_len;
  memmove (&rv->u.s.data, str, str_len + 1);
  return rv;
}

EJSValue*
_ejs_number_new (double value)
{
  EJSValue* rv = (EJSValue*)calloc(1, sizeof (EJSValue));
  rv->type = EJSValueTypeNumber;
  rv->u.n.data = value;
  return rv;
}

EJSValue*
_ejs_boolean_new (EJSBool value)
{
  EJSValue* rv = (EJSValue*)calloc(1, sizeof (EJSValue));
  rv->type = EJSValueTypeBoolean;
  rv->u.b.data = value;
  return rv;
}

EJSValue*
_ejs_closure_new (EJSClosureEnv* env, EJSClosureFunc func)
{
  EJSValue* rv = (EJSValue*)calloc(1, sizeof (EJSValue));
  rv->type = EJSValueTypeFunction;
  rv->u.closure.map = _ejs_propertymap_new (8);
  rv->u.closure.fields = (EJSValue**)calloc(8, sizeof (EJSValue*));
  rv->u.closure.func = func;
  rv->u.closure.env = env;
  return rv;
}

EJSValue*
_ejs_undefined_new ()
{
  EJSValue* rv = (EJSValue*)calloc(1, sizeof (EJSValue));
  rv->type = EJSValueTypeUndefined;
  return rv;
}

EJSValue*
_ejs_object_setprop (EJSValue* obj, EJSValue* key, EJSValue* value)
{
  if (EJSVAL_IS_PRIMITIVE(obj)) {
    printf ("setprop on primitive.  ignoring\n");
    return value;
  }

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
      // XXX we'll need to resize the array here
      obj->u.a.elements[idx] = value;
      obj->u.a.array_length = idx + 1;
      return value;
    }
    // if we fail there, we fall back to the object impl below
  }

  if (!EJSVAL_IS_STRING(key)) {
    printf ("key isn't a string\n");
    return NULL;
  }

  int prop_index = _ejs_propertymap_lookup (obj->u.o.map, key->u.s.data, TRUE);
  obj->u.o.fields[prop_index] = value;

  return NULL;
}

EJSValue*
_ejs_object_getprop (EJSValue* obj, EJSValue* key)
{
  if (EJSVAL_IS_PRIMITIVE(obj)) {
    printf ("getprop on primitive.  returning undefined\n");
    return _ejs_undefined;
  }

  if (EJSVAL_IS_ARRAY(obj)) {
    // check if key is an integer, or a string that we can convert to an int
    int idx = -1;
    if (EJSVAL_IS_NUMBER(key)) {
      double n = EJSVAL_TO_NUMBER(key);
      if (floor(n) == n) {
	idx = (int)n;
      }
    }

    if (idx != -1) {
      return obj->u.a.elements[idx];
    }

    if (EJSVAL_IS_STRING(key) && !strcmp ("length", EJSVAL_TO_STRING(key))) {
      return _ejs_number_new (obj->u.a.array_length);
    }

    // if we fail there, we fall back to the object impl below
  }

  if (!EJSVAL_IS_STRING(key)) {
    printf ("key isn't a string\n");
    return NULL;
  }

  int prop_index = _ejs_propertymap_lookup (obj->u.o.map, key->u.s.data, FALSE);

  if (prop_index == -1) {
    if (obj->u.o.proto)
      return _ejs_object_getprop (obj->u.o.proto, key);
    else
      return _ejs_undefined;
  }
  else {
    return obj->u.o.fields[prop_index];
  }
}

EJSValue*
_ejs_invoke_closure_0 (EJSValue* closure, EJSValue* _this, int argc)
{
  assert (EJSVAL_IS_CLOSURE(closure));
  return closure->u.closure.func (closure->u.closure.env, _this, argc, NULL);
}

EJSValue*
_ejs_invoke_closure_1 (EJSValue* closure, EJSValue* _this, int argc, EJSValue* arg1)
{
  assert (EJSVAL_IS_CLOSURE(closure));
  EJSValue *args[] = { arg1 };
  return closure->u.closure.func (closure->u.closure.env, _this, argc, args);
}

EJSValue*
_ejs_invoke_closure_2 (EJSValue* closure, EJSValue* _this, int argc, EJSValue* arg1, EJSValue* arg2)
{
  assert (EJSVAL_IS_CLOSURE(closure));
  EJSValue *args[] = { arg1, arg2 };
  return closure->u.closure.func (closure->u.closure.env, _this, argc, args);
}

EJSValue*
_ejs_invoke_closure_3 (EJSValue* closure, EJSValue* _this, int argc, EJSValue* arg1, EJSValue* arg2, EJSValue* arg3)
{
  assert (EJSVAL_IS_CLOSURE(closure));
  EJSValue *args[] = { arg1, arg2, arg3 };
  return closure->u.closure.func (closure->u.closure.env, _this, argc, args);
}

EJSValue*
_ejs_invoke_closure_4 (EJSValue* closure, EJSValue* _this, int argc, EJSValue* arg1, EJSValue* arg2, EJSValue* arg3, EJSValue* arg4)
{
  assert (EJSVAL_IS_CLOSURE(closure));
  EJSValue *args[] = { arg1, arg2, arg3, arg4 };
  return closure->u.closure.func (closure->u.closure.env, _this, argc, args);
}

EJSValue*
_ejs_invoke_closure_5 (EJSValue* closure, EJSValue* _this, int argc, EJSValue* arg1, EJSValue* arg2, EJSValue* arg3, EJSValue* arg4, EJSValue* arg5)
{
  assert (EJSVAL_IS_CLOSURE(closure));
  EJSValue *args[] = { arg1, arg2, arg3, arg4, arg5 };
  return closure->u.closure.func (closure->u.closure.env, _this, argc, args);
}

EJSValue*
_ejs_invoke_closure_6 (EJSValue* closure, EJSValue* _this, int argc, EJSValue* arg1, EJSValue* arg2, EJSValue* arg3, EJSValue* arg4, EJSValue* arg5, EJSValue* arg6)
{
  assert (EJSVAL_IS_CLOSURE(closure));
  EJSValue *args[] = { arg1, arg2, arg3, arg4, arg5, arg6 };
  return closure->u.closure.func (closure->u.closure.env, _this, argc, args);
}

EJSValue*
_ejs_invoke_closure_7 (EJSValue* closure, EJSValue* _this, int argc, EJSValue* arg1, EJSValue* arg2, EJSValue* arg3, EJSValue* arg4, EJSValue* arg5, EJSValue* arg6, EJSValue* arg7)
{
  assert (EJSVAL_IS_CLOSURE(closure));
  EJSValue *args[] = { arg1, arg2, arg3, arg4, arg5, arg6, arg7 };
  return closure->u.closure.func (closure->u.closure.env, _this, argc, args);
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
  else if (EJSVAL_IS_CLOSURE(val)) {
    printf ("<closure>\n");
  }
}

EJSValue* _ejs_Object;
static EJSValue*
_ejs_Object_impl (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  if (EJSVAL_IS_UNDEFINED(_this)) {
    // called as a function
    printf ("called Object() as a function!\n");
    return NULL;
  }
  else {
    // called as a constructor
    printf ("called Object() as a constructor!\n");
    return NULL;
  }
}


static EJSValue*
_ejs_Object_prototype_toString (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  return _ejs_string_new_utf8 ("[object Object]");
}

static EJSValue* _ejs_Object_proto;
EJSValue*
_ejs_object_get_prototype()
{
  return _ejs_Object_proto;
}

void
_ejs_object_init(EJSValue *global)
{
  _ejs_Object = _ejs_closure_new (NULL, (EJSClosureFunc)_ejs_Object_impl);
  _ejs_Object_proto = _ejs_object_new(NULL);

  _ejs_object_setprop (_ejs_Object,       _ejs_string_new_utf8("prototype"),  _ejs_Object_proto);
  _ejs_object_setprop (_ejs_Object_proto, _ejs_string_new_utf8("prototype"),  NULL);
  _ejs_object_setprop (_ejs_Object_proto, _ejs_string_new_utf8("toString"),   _ejs_closure_new (NULL, (EJSClosureFunc)_ejs_Object_prototype_toString));

  _ejs_object_setprop (global, _ejs_string_new_utf8("Object"), _ejs_Object);
}
