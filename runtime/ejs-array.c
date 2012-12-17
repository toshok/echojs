#include <assert.h>
#include <string.h>
#include <math.h>

#include "ejs-ops.h"
#include "ejs-value.h"
#include "ejs-array.h"

static EJSValue* _ejs_array_specop_get (EJSValue* obj, void* propertyName, EJSBool isCStr);
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
  return (EJSObject*)_ejs_gc_new (EJSArray);
}

EJSValue*
_ejs_array_new (int numElements)
{
  EJSArray* rv = _ejs_gc_new (EJSArray);

  _ejs_init_object ((EJSObject*)rv, _ejs_array_get_prototype());
  rv->obj.ops = &_ejs_array_specops;

  rv->array_length = 0;
  rv->array_alloc = numElements + 40;
  rv->elements = (EJSValue**)calloc(rv->array_alloc, sizeof (EJSValue*));
  return (EJSValue*)rv;
}

void
_ejs_array_finalize (EJSArray *array)
{
  free (array->elements);
  _ejs_object_finalize((EJSObject*)array);
}

void
_ejs_array_foreach_element (EJSArray* arr, EJSValueFunc foreach_func)
{
  for (int i = 0; i < EJS_ARRAY_LEN(arr); i ++)
    foreach_func (EJS_ARRAY_ELEMENTS(arr)[i]);
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
_ejs_Array_prototype_shift (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  // 1. Let O be the result of calling ToObject passing the this value as the argument.
  EJSValue* O = ToObject(_this);

  // EJS fast path for arrays
  if (!strcmp (CLASSNAME(O), "Array")) {
    int len = EJS_ARRAY_LEN(O);
    if (len == 0) {
      return _ejs_undefined;
    }
    EJSValue* first = EJS_ARRAY_ELEMENTS(O)[0];
    memmove (EJS_ARRAY_ELEMENTS(O), EJS_ARRAY_ELEMENTS(O) + 1, sizeof(EJSValue*) * len-1);
    EJS_ARRAY_LEN(O) --;
    return first;
  }

  NOT_IMPLEMENTED();
#if notyet
  // 2. Let lenVal be the result of calling the [[Get]] internal method of O with argument "length".
  EJSValue* lenVal = OP(O,get) (O, _ejs_string_new_utf8 ("length"));

  // 3. Let len be ToUint32(lenVal).
  int len = ToUint32(lenVal);

  // 4. If len is zero, then
  if (len == 0) {
    //   a. Call the [[Put]] internal method of O with arguments "length", 0, and true.
    OP(O,put) (O, _ejs_string_new_utf8 ("length"), 0, TRUE);
    //   b. Return undefined.
    return _ejs_undefined;
  }

  // 5. Let first be the result of calling the [[Get]] internal method of O with argument "0".
  EJSValue* first = OP(O,get) (O, _ejs_string_new_utf8 ("0"));

  // 6. Let k be 1.
  int k = 1;
  // 7. Repeat, while k < len
  while (k < len) {
    //   a. Let from be ToString(k).
    //   b. Let to be ToString(k–1).
    //   c. Let fromPresent be the result of calling the [[HasProperty]] internal method of O with argument from.
    //   d. If fromPresent is true, then
    //     i. Let fromVal be the result of calling the [[Get]] internal method of O with argument from.
    //     ii. Call the [[Put]] internal method of O with arguments to, fromVal, and true.
    //   e. Else, fromPresent is false
    //     i. Call the [[Delete]] internal method of O with arguments to and true.
    //   f. Increase k by 1.
    k++;
  }
  // 8. Call the [[Delete]] internal method of O with arguments ToString(len–1) and true.
  // 9. Call the [[Put]] internal method of O with arguments "length", (len–1) , and true.
  // 10. Return first.
#endif
}

static EJSValue*
_ejs_Array_prototype_unshift (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  // 1. Let O be the result of calling ToObject passing the this value as the argument.
  EJSValue* O = ToObject(_this);

  // EJS fast path for arrays
  if (!strcmp (CLASSNAME(O), "Array")) {
    int len = EJS_ARRAY_LEN(O);
    memmove (EJS_ARRAY_ELEMENTS(O) + argc, EJS_ARRAY_ELEMENTS(O), sizeof(EJSValue*) * len);
    memmove (EJS_ARRAY_ELEMENTS(O), args, sizeof(EJSValue*) * argc);
    EJS_ARRAY_LEN(O) += argc;
    return _ejs_number_new (len + argc);
  }

  NOT_IMPLEMENTED();

  // 2. Let lenVal be the result of calling the [[Get]] internal method of O with argument "length".
  // 3. Let len be ToUint32(lenVal).
  // 4. Let argCount be the number of actual arguments.
  // 5. Let k be len.
  // 6. Repeat, while k > 0,
  //   a. Let from be ToString(k–1).
  //   b. Let to be ToString(k+argCount –1).
  //   c. Let fromPresent be the result of calling the [[HasProperty]] internal method of O with argument from.
  //   d. If fromPresent is true, then
  //      i. Let fromValue be the result of calling the [[Get]] internal method of O with argument from.
  //      ii. Call the [[Put]] internal method of O with arguments to, fromValue, and true.
  //   e. Else, fromPresent is false
  //      i. Call the [[Delete]] internal method of O with arguments to, and true.
  //   f. Decrease k by 1.
  // 7. Let j be 0.
  // 8. Let items be an internal List whose elements are, in left to right order, the arguments that were passed to this function invocation.
  // 9. Repeat, while items is not empty
  //   a. Remove the first element from items and let E be the value of that element.
  //   b. Call the [[Put]] internal method of O with arguments ToString(j), E, and true.
  //   c. Increase j by 1.
  // 10. Call the [[Put]] internal method of O with arguments "length", len+argCount, and true.
  // 11. Return len+argCount.
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
_ejs_Array_prototype_concat (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  int numElements;

  // hacky fast path for everything being an array
  numElements = EJS_ARRAY_LEN(_this);
  int i;
  for (i = 0; i < argc; i ++) {
    numElements += EJS_ARRAY_LEN(args[i]);
  }

  EJSValue* rv = _ejs_array_new(numElements);
  numElements = 0;
  memmove (EJS_ARRAY_ELEMENTS(rv) + numElements, EJS_ARRAY_ELEMENTS(_this), sizeof(EJSValue*) * EJS_ARRAY_LEN(_this));
  numElements += EJS_ARRAY_LEN(_this);
  for (i = 0; i < argc; i ++) {
    memmove (EJS_ARRAY_ELEMENTS(rv) + numElements, EJS_ARRAY_ELEMENTS(args[i]), sizeof(EJSValue*) * EJS_ARRAY_LEN(args[i]));
    numElements += EJS_ARRAY_LEN(args[i]);
  }
  EJS_ARRAY_LEN(rv) = numElements;

  return rv;
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

static EJSValue*
_ejs_Array_isArray (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  if (argc == 0 || args[0] == NULL || EJSVAL_TAG(args[0]) != EJSValueTagObject)
    return _ejs_false;

  return _ejs_boolean_new (!strcmp (CLASSNAME(((EJSObject*)args[0])), "Array"));
}

void
_ejs_array_init(EJSValue *global)
{
  START_SHADOW_STACK_FRAME;

  _ejs_gc_add_named_root (_ejs_Array_proto);

  ADD_STACK_ROOT(EJSValue*, _ejs_Array, _ejs_function_new_utf8 (NULL, "Array", (EJSClosureFunc)_ejs_Array_impl));
  _ejs_Array_proto = _ejs_object_new(NULL);

  _ejs_object_setprop_utf8 (_ejs_Array,       "prototype",  _ejs_Array_proto);

#define OBJ_METHOD(x) do { ADD_STACK_ROOT(EJSValue*, funcname, _ejs_string_new_utf8(#x)); ADD_STACK_ROOT(EJSValue*, tmpfunc, _ejs_function_new (NULL, funcname, (EJSClosureFunc)_ejs_Array_##x)); _ejs_object_setprop (_ejs_Array, funcname, tmpfunc); } while (0)
#define PROTO_METHOD(x) do { ADD_STACK_ROOT(EJSValue*, funcname, _ejs_string_new_utf8(#x)); ADD_STACK_ROOT(EJSValue*, tmpfunc, _ejs_function_new (NULL, funcname, (EJSClosureFunc)_ejs_Array_prototype_##x)); _ejs_object_setprop (_ejs_Array_proto, funcname, tmpfunc); } while (0)

  OBJ_METHOD(isArray);

  PROTO_METHOD(push);
  PROTO_METHOD(pop);
  PROTO_METHOD(shift);
  PROTO_METHOD(unshift);
  PROTO_METHOD(concat);
  PROTO_METHOD(slice);
  PROTO_METHOD(splice);
  PROTO_METHOD(indexOf);
  PROTO_METHOD(join);
  PROTO_METHOD(forEach);

  _ejs_object_setprop_utf8 (global,           "Array",      _ejs_Array);

  END_SHADOW_STACK_FRAME;
}

static EJSValue*
_ejs_array_specop_get (EJSValue* obj, void* propertyName, EJSBool isCStr)
{
  EJSArray* arr = (EJSArray*)&obj->o;

  // check if propertyName is an integer, or a string that we can convert to an int
  EJSBool is_index = FALSE;
  int idx = 0;
  if (!isCStr && EJSVAL_IS_NUMBER(propertyName)) {
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
  if ((isCStr && !strcmp("length", (char*)propertyName))
      || (!isCStr && EJSVAL_IS_STRING(propertyName) && !strcmp ("length", EJSVAL_TO_STRING(propertyName)))) {
    return _ejs_number_new (EJS_ARRAY_LEN(arr));
  }

  // otherwise we fallback to the object implementation
  return _ejs_object_specops.get (obj, propertyName, isCStr);
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
