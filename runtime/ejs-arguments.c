#include <assert.h>
#include <math.h>

#include "ejs-ops.h"
#include "ejs-value.h"
#include "ejs-arguments.h"

static EJSValue* _ejs_arguments_specop_get (EJSValue* obj, void* propertyName, EJSBool isCStr);
static EJSValue* _ejs_arguments_specop_get_own_property (EJSValue* obj, EJSValue* propertyName);
static EJSValue* _ejs_arguments_specop_get_property (EJSValue* obj, EJSValue* propertyName);
static void      _ejs_arguments_specop_put (EJSValue *obj, EJSValue* propertyName, EJSValue* val, EJSBool flag);
static EJSBool   _ejs_arguments_specop_can_put (EJSValue *obj, EJSValue* propertyName);
static EJSBool   _ejs_arguments_specop_has_property (EJSValue *obj, EJSValue* propertyName);
static EJSBool   _ejs_arguments_specop_delete (EJSValue *obj, EJSValue* propertyName, EJSBool flag);
static EJSValue* _ejs_arguments_specop_default_value (EJSValue *obj, const char *hint);
static void      _ejs_arguments_specop_define_own_property (EJSValue *obj, EJSValue* propertyName, EJSValue* propertyDescriptor, EJSBool flag);
static void      _ejs_arguments_specop_finalize (EJSValue* obj);
static void      _ejs_arguments_specop_scan (EJSValue* obj, EJSValueFunc scan_func);

EJSSpecOps _ejs_arguments_specops = {
  "Arguments",
  _ejs_arguments_specop_get,
  _ejs_arguments_specop_get_own_property,
  _ejs_arguments_specop_get_property,
  _ejs_arguments_specop_put,
  _ejs_arguments_specop_can_put,
  _ejs_arguments_specop_has_property,
  _ejs_arguments_specop_delete,
  _ejs_arguments_specop_default_value,
  _ejs_arguments_specop_define_own_property,
  _ejs_arguments_specop_finalize,
  _ejs_arguments_specop_scan
};


#define EJSOBJ_IS_ARGUMENTS(obj) (((EJSObject*)obj)->proto == _ejs_Arguments_proto)

EJSObject* _ejs_arguments_alloc_instance()
{
  return (EJSObject*)_ejs_gc_new (EJSArguments);
}

EJSValue*
_ejs_arguments_new (int numElements, EJSValue** args)
{
  EJSArguments* rv = _ejs_gc_new (EJSArguments);

  _ejs_init_object ((EJSObject*)rv, _ejs_arguments_get_prototype(), &_ejs_arguments_specops);

  rv->argc = numElements;
  rv->args = (EJSValue**)calloc(numElements, sizeof (EJSValue*));
  memmove (rv->args, args, sizeof(EJSValue*) * rv->argc);
  return (EJSValue*)rv;
}


static EJSValue* _ejs_Arguments_proto;
EJSValue*
_ejs_arguments_get_prototype()
{
  return _ejs_Arguments_proto;
}

void
_ejs_arguments_init(EJSValue *global)
{
  _ejs_Arguments_proto = _ejs_object_new(NULL);
  _ejs_gc_add_named_root (_ejs_Arguments_proto);
}

static EJSValue*
_ejs_arguments_specop_get (EJSValue* obj, void* propertyName, EJSBool isCStr)
{
  EJSArguments* arguments = (EJSArguments*)&obj->o;

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
    if (idx < 0 || idx > arguments->argc) {
      printf ("getprop(%d) on an arguments, returning undefined\n", idx);
      return _ejs_undefined;
    }
    return arguments->args[idx];
  }

  // we also handle the length getter here
  if ((isCStr && !strcmp("length", (char*)propertyName))
      || (!isCStr && EJSVAL_IS_STRING(propertyName) && !strcmp ("length", EJSVAL_TO_STRING(propertyName)))) {
    return _ejs_number_new (arguments->argc);
  }

  // otherwise we fallback to the object implementation
  return _ejs_object_specops.get (obj, propertyName, isCStr);
}

static EJSValue*
_ejs_arguments_specop_get_own_property (EJSValue* obj, EJSValue* propertyName)
{
  return _ejs_object_specops.get_own_property (obj, propertyName);
}

static EJSValue*
_ejs_arguments_specop_get_property (EJSValue* obj, EJSValue* propertyName)
{
  return _ejs_object_specops.get_property (obj, propertyName);
}

static void
_ejs_arguments_specop_put (EJSValue *obj, EJSValue* propertyName, EJSValue* val, EJSBool flag)
{
  _ejs_object_specops.put (obj, propertyName, val, flag);
}

static EJSBool
_ejs_arguments_specop_can_put (EJSValue *obj, EJSValue* propertyName)
{
  return _ejs_object_specops.can_put (obj, propertyName);
}

static EJSBool
_ejs_arguments_specop_has_property (EJSValue *obj, EJSValue* propertyName)
{
  return _ejs_object_specops.has_property (obj, propertyName);
}

static EJSBool
_ejs_arguments_specop_delete (EJSValue *obj, EJSValue* propertyName, EJSBool flag)
{
  return _ejs_object_specops._delete (obj, propertyName, flag);
}

static EJSValue*
_ejs_arguments_specop_default_value (EJSValue *obj, const char *hint)
{
  return _ejs_object_specops.default_value (obj, hint);
}

static void
_ejs_arguments_specop_define_own_property (EJSValue *obj, EJSValue* propertyName, EJSValue* propertyDescriptor, EJSBool flag)
{
  _ejs_object_specops.define_own_property (obj, propertyName, propertyDescriptor, flag);
}

static void
_ejs_arguments_specop_finalize (EJSValue *obj)
{
  _ejs_object_specops.finalize (obj);
}

static void
_ejs_arguments_specop_scan (EJSValue* obj, EJSValueFunc scan_func)
{
  _ejs_object_specops.scan (obj, scan_func);
}
