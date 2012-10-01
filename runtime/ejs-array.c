#include <assert.h>
#include <math.h>

#include "ejs-ops.h"
#include "ejs-value.h"
#include "ejs-array.h"

static EJSValue* _ejs_array_specop_get (EJSValue* obj, EJSValue* propertyName);
static EJSValue* _ejs_array_specop_get_own_property (EJSValue* obj, EJSValue* propertyName);
static EJSValue* _ejs_array_specop_get_property (EJSValue* obj, EJSValue* propertyName);
static void      _ejs_array_specop_put (EJSValue *obj, EJSValue* propertyName, EJSValue* val, EJSBool flag);
static EJSBool   _ejs_array_specop_can_put (EJSValue *obj, EJSValue* propertyName);
static EJSBool   _ejs_array_specop_has_property (EJSValue *obj, EJSValue* propertyName);
static EJSBool   _ejs_array_specop_delete (EJSValue *obj, EJSValue* propertyName, EJSBool flag);
static EJSValue* _ejs_array_specop_default_value (EJSValue *obj, const char *hint);
static void      _ejs_array_specop_define_own_property (EJSValue *obj, EJSValue* propertyName, EJSValue* propertyDescriptor, EJSBool flag);

extern EJSSpecOps _ejs_object_specops;

EJSSpecOps _ejs_array_specops = {
  "Array",
  _ejs_array_specop_get,
  _ejs_array_specop_get_own_property,
  _ejs_array_specop_get_property,
  _ejs_array_specop_put,
  _ejs_array_specop_can_put,
  _ejs_array_specop_has_property,
  _ejs_array_specop_delete,
  _ejs_array_specop_default_value,
  _ejs_array_specop_define_own_property
};


#define EJSOBJ_IS_ARRAY(obj) (((EJSObject*)obj)->proto == _ejs_Array_proto)

EJSObject* _ejs_array_alloc_instance()
{
  return (EJSObject*)calloc(1, sizeof (EJSArray));
}

EJSValue*
_ejs_array_new (int numElements)
{
  EJSArray* rv = (EJSArray*)calloc(1, sizeof(EJSArray));

  _ejs_init_object ((EJSObject*)rv, _ejs_array_get_prototype());
  rv->obj.ops = &_ejs_array_specops;

  rv->array_length = 0;
  rv->array_alloc = numElements + 40;
  rv->elements = (EJSValue**)calloc(rv->array_alloc, sizeof (EJSValue*));
  return (EJSValue*)rv;
}


static EJSValue* _ejs_Array_proto;
EJSValue*
_ejs_array_get_prototype()
{
  return _ejs_Array_proto;
}

EJSValue* _ejs_Array;
static EJSValue*
_ejs_Array_impl (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  if (EJSVAL_IS_UNDEFINED(_this)) {
    // called as a function

    if (argc == 0) {
      return _ejs_array_new(10);
    }
    else if (argc == 1 && EJSVAL_IS_NUMBER(args[0])) {
      return _ejs_array_new((int)EJSVAL_TO_NUMBER(args[0]));
    }
    else {
      EJSValue* rv = _ejs_array_new(argc);
      int i;

      for (i = 0; i < argc; i ++) {
	_ejs_object_setprop (rv, _ejs_number_new (i), args[i]);
      }

      return rv;
    }
  }
  else {
    // called as a constructor
    return _this;
  }
}

static EJSValue*
_ejs_Array_prototype_push (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  int i;
  assert (EJSOBJ_IS_ARRAY(_this));
  for (i = 0; i < argc; i ++)
    EJS_ARRAY_ELEMENTS(_this)[EJS_ARRAY_LEN(_this)++] = args[i];
  return _ejs_number_new (EJS_ARRAY_LEN(_this));
}

static EJSValue*
_ejs_Array_prototype_pop (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  assert (EJSOBJ_IS_ARRAY(_this));
  if (EJS_ARRAY_LEN(_this) == 0)
    return _ejs_undefined;
  return EJS_ARRAY_ELEMENTS(_this)[--EJS_ARRAY_LEN(_this)];
}

static EJSValue*
_ejs_Array_prototype_slice (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  int len = EJS_ARRAY_LEN(_this);
  int begin = argc > 0 ? (int)EJSVAL_TO_NUMBER(args[0]) : 0;
  int end = argc > 1 ? (int)EJSVAL_TO_NUMBER(args[1]) : len;

  begin = MIN(begin, len);
  end = MIN(end, len);

  EJSValue* rv = _ejs_array_new(end-begin);
  int i, rv_i;

  rv_i = 0;
  for (i = begin; i < end; i ++, rv_i++) {
    _ejs_object_setprop (rv, _ejs_number_new (rv_i), EJS_ARRAY_ELEMENTS(_this)[i]);
  }

  return rv;
}

static EJSValue*
_ejs_Array_prototype_join (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  if (EJS_ARRAY_LEN(_this) == 0)
    return _ejs_string_new_utf8 ("");

  char* separator;
  int separator_len;

  if (argc > 0) {
    EJSValue* sepToString = ToString(args[0]);
    separator_len = EJSVAL_TO_STRLEN(sepToString);
    separator = EJSVAL_TO_STRING(sepToString);
  }
  else {
    separator = ",";
    separator_len = 1;
  }
  
  char* result;
  int result_len = 0;

  EJSValue** strings = (EJSValue**)malloc (sizeof (EJSValue*) * EJS_ARRAY_LEN(_this));
  int i;

  for (i = 0; i < EJS_ARRAY_LEN(_this); i ++) {
    strings[i] = ToString(EJS_ARRAY_ELEMENTS(_this)[i]);
    result_len += EJSVAL_TO_STRLEN(strings[i]);
  }

  result_len += separator_len * (EJS_ARRAY_LEN(_this)-1) + 1/* \0 terminator */;

  result = (char*)malloc (result_len);
  int offset = 0;
  for (i = 0; i < EJS_ARRAY_LEN(_this); i ++) {
    int slen = EJSVAL_TO_STRLEN(strings[i]);
    memmove (result + offset, EJSVAL_TO_STRING(strings[i]), slen);
    offset += slen;
    if (i < EJS_ARRAY_LEN(_this)-1) {
      memmove (result + offset, separator, separator_len);
      offset += separator_len;
    }
  }
  result[result_len-1] = 0;

  EJSValue* rv = _ejs_string_new_utf8(result);

  free (result);
  free (strings);

  return rv;
}

static EJSValue*
_ejs_Array_prototype_forEach (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  if (argc < 1)
    NOT_IMPLEMENTED();

  EJSValue *fun = args[0];

  int i;
  for (i = 0; i < EJS_ARRAY_LEN(_this); i ++) {
    _ejs_invoke_closure_1 (fun, NULL, 1, EJS_ARRAY_ELEMENTS(_this)[i]);
  }
  return _ejs_undefined;
}

static EJSValue*
_ejs_Array_prototype_splice (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  NOT_IMPLEMENTED();
}

static EJSValue*
_ejs_Array_prototype_indexOf (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  if (argc != 1)
    return _ejs_number_new (-1);

  int i;
  int rv = -1;
  EJSValue *needle = args[0];
  if (needle == NULL) {
    for (i = 0; i < EJS_ARRAY_LEN(_this); i ++) {
      if (EJS_ARRAY_ELEMENTS(_this)[i] == NULL) {
	rv = i;
	break;
      }
    }
  }
  else {
    for (i = 0; i < EJS_ARRAY_LEN(_this); i ++) {
      if (EJSVAL_TO_BOOLEAN(_ejs_op_strict_eq (needle, EJS_ARRAY_ELEMENTS(_this)[i]))) {
	rv = i;
	break;
      }
    }
  }

  return _ejs_number_new (rv);
}

void
_ejs_array_init(EJSValue *global)
{
  _ejs_Array = _ejs_function_new_utf8 (NULL, "Array", (EJSClosureFunc)_ejs_Array_impl);
  _ejs_Array_proto = _ejs_object_new(NULL);

  _ejs_object_setprop_utf8 (_ejs_Array,       "prototype",  _ejs_Array_proto);

#define PROTO_METHOD(x) _ejs_object_setprop_utf8 (_ejs_Array_proto, #x,       _ejs_function_new_utf8 (NULL, #x, (EJSClosureFunc)_ejs_Array_prototype_##x))
  PROTO_METHOD(push);
  PROTO_METHOD(pop);
  PROTO_METHOD(slice);
  PROTO_METHOD(splice);
  PROTO_METHOD(indexOf);
  PROTO_METHOD(join);
  PROTO_METHOD(forEach);

  _ejs_object_setprop_utf8 (global,           "Array",      _ejs_Array);
}

static EJSValue*
_ejs_array_specop_get (EJSValue* obj, EJSValue* propertyName)
{
  EJSArray* arr = (EJSArray*)&obj->o;

  // check if propertyName is an integer, or a string that we can convert to an int
  EJSBool is_index = FALSE;
  int idx = 0;
  if (EJSVAL_IS_NUMBER(propertyName)) {
    double n = EJSVAL_TO_NUMBER(propertyName);
    if (floor(n) == n) {
      idx = (int)n;
      is_index = TRUE;
    }
  }

  if (is_index) {
    if (idx < 0 || idx > EJS_ARRAY_LEN(arr)) {
      printf ("getprop(%d) on an array, returning undefined\n", idx);
      return _ejs_undefined;
    }
    return EJS_ARRAY_ELEMENTS(arr)[idx];
  }

  // we also handle the length getter here
  if (EJSVAL_IS_STRING(propertyName) && !strcmp ("length", EJSVAL_TO_STRING(propertyName))) {
    return _ejs_number_new (EJS_ARRAY_LEN(arr));
  }

  // otherwise we fallback to the object implementation
  return _ejs_object_specops.get (obj, propertyName);
}

static EJSValue*
_ejs_array_specop_get_own_property (EJSValue* obj, EJSValue* propertyName)
{
  return _ejs_object_specops.get_own_property (obj, propertyName);
}

static EJSValue*
_ejs_array_specop_get_property (EJSValue* obj, EJSValue* propertyName)
{
  return _ejs_object_specops.get_property (obj, propertyName);
}

static void
_ejs_array_specop_put (EJSValue *obj, EJSValue* propertyName, EJSValue* val, EJSBool flag)
{
  _ejs_object_specops.put (obj, propertyName, val, flag);
}

static EJSBool
_ejs_array_specop_can_put (EJSValue *obj, EJSValue* propertyName)
{
  return _ejs_object_specops.can_put (obj, propertyName);
}

static EJSBool
_ejs_array_specop_has_property (EJSValue *obj, EJSValue* propertyName)
{
  return _ejs_object_specops.has_property (obj, propertyName);
}

static EJSBool
_ejs_array_specop_delete (EJSValue *obj, EJSValue* propertyName, EJSBool flag)
{
  return _ejs_object_specops._delete (obj, propertyName, flag);
}

static EJSValue*
_ejs_array_specop_default_value (EJSValue *obj, const char *hint)
{
  return _ejs_object_specops.default_value (obj, hint);
}

static void
_ejs_array_specop_define_own_property (EJSValue *obj, EJSValue* propertyName, EJSValue* propertyDescriptor, EJSBool flag)
{
  _ejs_object_specops.define_own_property (obj, propertyName, propertyDescriptor, flag);
}
