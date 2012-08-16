#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "object.h"
#include "math.h"

// really terribly performing field maps
struct _EJSFieldMap {
  char **names;
  int allocated;
  int num;
};

EJSFieldMap*
_ejs_fieldmap_new (int initial_allocation)
{
  EJSFieldMap* rv = (EJSFieldMap*)calloc(1, sizeof (EJSFieldMap));
  rv->names = (char**)malloc(sizeof(char*) * initial_allocation);
  rv->num = 0;
  rv->allocated = initial_allocation;
  return rv;
}

int
_ejs_fieldmap_lookup (EJSFieldMap *map, char *name, EJSBool add_if_not_found)
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
  rv->u.o.proto = proto;
  rv->u.o.map = _ejs_fieldmap_new (8);
  rv->u.o.fields = (EJSValue**)calloc(8, sizeof (EJSValue*));
  return rv;
}

EJSValue*
_ejs_array_new (int numElements)
{
  EJSValue* rv = _ejs_object_new(NULL/* XXX should be array prototype*/);
  rv->type = EJSValueTypeArray;
  rv->u.a.array_length = 0;
  rv->u.a.array_alloc = numElements;
  rv->u.a.elements = (EJSValue**)calloc(rv->u.a.array_alloc, sizeof (EJSValue*));
  return rv;
}

EJSValue*
_ejs_string_new_utf8 (char* str)
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
_ejs_closure_new (EJSClosureEnv* env, EJSClosureFunc0 func)
{
  EJSValue* rv = (EJSValue*)calloc(1, sizeof (EJSValue));
  rv->type = EJSValueTypeClosure;
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

  int field_index = _ejs_fieldmap_lookup (obj->u.o.map, key->u.s.data, TRUE);
  obj->u.o.fields[field_index] = value;

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

  int field_index = _ejs_fieldmap_lookup (obj->u.o.map, key->u.s.data, FALSE);

  if (field_index == -1)
    return _ejs_undefined;
  else
    return obj->u.o.fields[field_index];
}

EJSValue*
_ejs_invoke_closure_0 (EJSValue* closure, int argc)
{
  assert (EJSVAL_IS_CLOSURE(closure));
  return ((EJSClosureFunc0)closure->u.closure.func) (closure->u.closure.env, argc);
}

EJSValue*
_ejs_invoke_closure_1 (EJSValue* closure, int argc, EJSValue* arg1)
{
  assert (EJSVAL_IS_CLOSURE(closure));
  return ((EJSClosureFunc1)closure->u.closure.func) (closure->u.closure.env, argc, arg1);
}

EJSValue*
_ejs_invoke_closure_2 (EJSValue* closure, int argc, EJSValue* arg1, EJSValue* arg2)
{
  assert (EJSVAL_IS_CLOSURE(closure));
  return ((EJSClosureFunc2)closure->u.closure.func) (closure->u.closure.env, argc, arg1, arg2);
}

EJSValue*
_ejs_invoke_closure_3 (EJSValue* closure, int argc, EJSValue* arg1, EJSValue* arg2, EJSValue* arg3)
{
  assert (EJSVAL_IS_CLOSURE(closure));
  return ((EJSClosureFunc3)closure->u.closure.func) (closure->u.closure.env, argc, arg1, arg2, arg3);
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
