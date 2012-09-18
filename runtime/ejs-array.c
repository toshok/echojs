#include <assert.h>

#include "ejs-ops.h"
#include "ejs-value.h"
#include "ejs-array.h"

#define EJSOBJ_IS_ARRAY(obj) (((EJSObject*)obj)->proto == _ejs_Array_proto)

EJSObject* _ejs_array_alloc_instance()
{
  return (EJSObject*)calloc(1, sizeof (EJSArray));
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
_ejs_Array_prototype_splice (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  abort();
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
  _ejs_Array = _ejs_function_new (NULL, (EJSClosureFunc)_ejs_Array_impl);
  _ejs_Array_proto = _ejs_object_new(NULL);

  _ejs_object_setprop_utf8 (_ejs_Array,       "prototype",  _ejs_Array_proto);
  _ejs_object_setprop_utf8 (_ejs_Array_proto, "push",       _ejs_function_new (NULL, (EJSClosureFunc)_ejs_Array_prototype_push));
  _ejs_object_setprop_utf8 (_ejs_Array_proto, "pop",        _ejs_function_new (NULL, (EJSClosureFunc)_ejs_Array_prototype_pop));
  _ejs_object_setprop_utf8 (_ejs_Array_proto, "slice",      _ejs_function_new (NULL, (EJSClosureFunc)_ejs_Array_prototype_slice));
  _ejs_object_setprop_utf8 (_ejs_Array_proto, "splice",     _ejs_function_new (NULL, (EJSClosureFunc)_ejs_Array_prototype_splice));
  _ejs_object_setprop_utf8 (_ejs_Array_proto, "indexOf",    _ejs_function_new (NULL, (EJSClosureFunc)_ejs_Array_prototype_indexOf));
  _ejs_object_setprop_utf8 (_ejs_Array_proto, "join",       _ejs_function_new (NULL, (EJSClosureFunc)_ejs_Array_prototype_join));

  _ejs_object_setprop_utf8 (global,           "Array",      _ejs_Array);
}
