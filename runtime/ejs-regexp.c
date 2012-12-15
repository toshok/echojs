#include <assert.h>

#include "ejs-value.h"
#include "ejs-regexp.h"

static EJSValue* _ejs_regexp_specop_get (EJSValue* obj, void* propertyName, EJSBool isCStr);
static EJSValue* _ejs_regexp_specop_get_own_property (EJSValue* obj, EJSValue* propertyName);
static EJSValue* _ejs_regexp_specop_get_property (EJSValue* obj, EJSValue* propertyName);
static void      _ejs_regexp_specop_put (EJSValue *obj, EJSValue* propertyName, EJSValue* val, EJSBool flag);
static EJSBool   _ejs_regexp_specop_can_put (EJSValue *obj, EJSValue* propertyName);
static EJSBool   _ejs_regexp_specop_has_property (EJSValue *obj, EJSValue* propertyName);
static EJSBool   _ejs_regexp_specop_delete (EJSValue *obj, EJSValue* propertyName, EJSBool flag);
static EJSValue* _ejs_regexp_specop_default_value (EJSValue *obj, const char *hint);
static void      _ejs_regexp_specop_define_own_property (EJSValue *obj, EJSValue* propertyName, EJSValue* propertyDescriptor, EJSBool flag);

extern EJSSpecOps _ejs_object_specops;

EJSSpecOps _ejs_regexp_specops = {
  "RegExp",
  _ejs_regexp_specop_get,
  _ejs_regexp_specop_get_own_property,
  _ejs_regexp_specop_get_property,
  _ejs_regexp_specop_put,
  _ejs_regexp_specop_can_put,
  _ejs_regexp_specop_has_property,
  _ejs_regexp_specop_delete,
  _ejs_regexp_specop_default_value,
  _ejs_regexp_specop_define_own_property
};

EJSObject* _ejs_regexp_alloc_instance()
{
  return (EJSObject*)_ejs_gc_new(EJSRegexp);
}

EJSValue*
_ejs_regexp_new_utf8 (const char* str)
{
  int str_len = strlen(str);
  size_t value_size = sizeof (EJSRegexp) + str_len;

  EJSRegexp* rv = (EJSRegexp*)_ejs_gc_alloc (value_size);

  _ejs_init_object ((EJSObject*)rv, _ejs_regexp_get_prototype());
  ((EJSObject*)rv)->ops = &_ejs_regexp_specops;

  rv->pattern_len = str_len;
  rv->pattern = strdup(str);

  return (EJSValue*)rv;
}

EJSValue* _ejs_Regexp;
static EJSValue*
_ejs_Regexp_impl (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  if (EJSVAL_IS_UNDEFINED(_this)) {
    // called as a function
    printf ("called Regexp() as a function!\n");
    return _ejs_object_new(_ejs_regexp_get_prototype());
  }
  else {
    // called as a constructor
    printf ("called Regexp() as a constructor!\n");
    return _this;
  }
}

static EJSValue* _ejs_Regexp_proto;
EJSValue*
_ejs_regexp_get_prototype()
{
  return _ejs_Regexp_proto;
}

static EJSValue*
_ejs_Regexp_prototype_exec (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  NOT_IMPLEMENTED();
}

static EJSValue*
_ejs_Regexp_prototype_match (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  NOT_IMPLEMENTED();
}

static EJSValue*
_ejs_Regexp_prototype_test (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  NOT_IMPLEMENTED();
}

void
_ejs_regexp_init(EJSValue *global)
{
  _ejs_Regexp = _ejs_function_new_utf8 (NULL, "RegExp", (EJSClosureFunc)_ejs_Regexp_impl);
  _ejs_Regexp_proto = _ejs_object_new(NULL);

  _ejs_object_setprop_utf8 (_ejs_Regexp,       "prototype",  _ejs_Regexp_proto);

#define PROTO_METHOD(x) _ejs_object_setprop_utf8 (_ejs_Regexp_proto, #x, _ejs_function_new_utf8 (NULL, #x, (EJSClosureFunc)_ejs_Regexp_prototype_##x))

  PROTO_METHOD(exec);
  PROTO_METHOD(match);
  PROTO_METHOD(test);

#undef PROTO_METHOD

  _ejs_object_setprop_utf8 (global, "RegExp", _ejs_Regexp);
  _ejs_gc_add_named_root (_ejs_Regexp_proto);
}


static EJSValue*
_ejs_regexp_specop_get (EJSValue* obj, void* propertyName, EJSBool isCStr)
{
  return _ejs_object_specops.get (obj, propertyName, isCStr);
}

static EJSValue*
_ejs_regexp_specop_get_own_property (EJSValue* obj, EJSValue* propertyName)
{
  return _ejs_object_specops.get_own_property (obj, propertyName);
}

static EJSValue*
_ejs_regexp_specop_get_property (EJSValue* obj, EJSValue* propertyName)
{
  return _ejs_object_specops.get_property (obj, propertyName);
}

static void
_ejs_regexp_specop_put (EJSValue *obj, EJSValue* propertyName, EJSValue* val, EJSBool flag)
{
  _ejs_object_specops.put (obj, propertyName, val, flag);
}

static EJSBool
_ejs_regexp_specop_can_put (EJSValue *obj, EJSValue* propertyName)
{
  return _ejs_object_specops.can_put (obj, propertyName);
}

static EJSBool
_ejs_regexp_specop_has_property (EJSValue *obj, EJSValue* propertyName)
{
  return _ejs_object_specops.has_property (obj, propertyName);
}

static EJSBool
_ejs_regexp_specop_delete (EJSValue *obj, EJSValue* propertyName, EJSBool flag)
{
  return _ejs_object_specops._delete (obj, propertyName, flag);
}

static EJSValue*
_ejs_regexp_specop_default_value (EJSValue *obj, const char *hint)
{
  return _ejs_object_specops.default_value (obj, hint);
}

static void
_ejs_regexp_specop_define_own_property (EJSValue *obj, EJSValue* propertyName, EJSValue* propertyDescriptor, EJSBool flag)
{
  _ejs_object_specops.define_own_property (obj, propertyName, propertyDescriptor, flag);
}
