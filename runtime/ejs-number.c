#include <assert.h>

#include "ejs-ops.h"
#include "ejs-value.h"
#include "ejs-number.h"

static EJSValue* _ejs_number_specop_get (EJSValue* obj, void* propertyName, EJSBool isCStr);
static EJSValue* _ejs_number_specop_get_own_property (EJSValue* obj, EJSValue* propertyName);
static EJSValue* _ejs_number_specop_get_property (EJSValue* obj, EJSValue* propertyName);
static void      _ejs_number_specop_put (EJSValue *obj, EJSValue* propertyName, EJSValue* val, EJSBool flag);
static EJSBool   _ejs_number_specop_can_put (EJSValue *obj, EJSValue* propertyName);
static EJSBool   _ejs_number_specop_has_property (EJSValue *obj, EJSValue* propertyName);
static EJSBool   _ejs_number_specop_delete (EJSValue *obj, EJSValue* propertyName, EJSBool flag);
static EJSValue* _ejs_number_specop_default_value (EJSValue *obj, const char *hint);
static void      _ejs_number_specop_define_own_property (EJSValue *obj, EJSValue* propertyName, EJSValue* propertyDescriptor, EJSBool flag);

extern EJSSpecOps _ejs_object_specops;

EJSSpecOps _ejs_number_specops = {
  "Number",
  _ejs_number_specop_get,
  _ejs_number_specop_get_own_property,
  _ejs_number_specop_get_property,
  _ejs_number_specop_put,
  _ejs_number_specop_can_put,
  _ejs_number_specop_has_property,
  _ejs_number_specop_delete,
  _ejs_number_specop_default_value,
  _ejs_number_specop_define_own_property
};

EJSObject* _ejs_number_alloc_instance()
{
  return (EJSObject*)_ejs_gc_new (EJSNumber);
}

EJSValue* _ejs_Number;
static EJSValue*
_ejs_Number_impl (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  if (EJSVAL_IS_UNDEFINED(_this)) {
    // called as a function
    double num = 0;

    if (argc > 0) {
      num = ToDouble(args[0]);
    }

    EJSNumber* rv = _ejs_gc_new (EJSNumber);

    _ejs_init_object ((EJSObject*)rv, _ejs_number_get_prototype());

    rv->number = num;

    return (EJSValue*)rv;
  }
  else {
    // called as a constructor
    _this->o.ops = &_ejs_number_specops;

    EJSNumber* str = (EJSNumber*)&_this->o;
    if (argc > 0) {
      str->number = ToDouble(args[0]);
    }
    else {
      str->number = 0;
    }
    return _this;
  }
}

static EJSValue* _ejs_Number_proto;
EJSValue*
_ejs_number_get_prototype()
{
  return _ejs_Number_proto;
}

static EJSValue*
_ejs_Number_prototype_toString (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  EJSNumber *num = (EJSNumber*)_this;

  return NumberToString(num->number);
}

void
_ejs_number_init(EJSValue *global)
{
  START_SHADOW_STACK_FRAME;

  _ejs_gc_add_named_root (_ejs_Number_proto);

  ADD_STACK_ROOT(EJSValue*, _ejs_Number, _ejs_function_new_utf8 (NULL, "Number", (EJSClosureFunc)_ejs_Number_impl));
  _ejs_Number_proto = _ejs_object_new(NULL);

  _ejs_object_setprop_utf8 (_ejs_Number,       "prototype",  _ejs_Number_proto);

#define OBJ_METHOD(x) do { ADD_STACK_ROOT(EJSValue*, funcname, _ejs_string_new_utf8(#x)); ADD_STACK_ROOT(EJSValue*, tmpfunc, _ejs_function_new (NULL, funcname, (EJSClosureFunc)_ejs_Number_##x)); _ejs_object_setprop (_ejs_Number, funcname, tmpfunc); } while (0)
#define PROTO_METHOD(x) do { ADD_STACK_ROOT(EJSValue*, funcname, _ejs_string_new_utf8(#x)); ADD_STACK_ROOT(EJSValue*, tmpfunc, _ejs_function_new (NULL, funcname, (EJSClosureFunc)_ejs_Number_prototype_##x)); _ejs_object_setprop (_ejs_Number_proto, funcname, tmpfunc); } while (0)

  PROTO_METHOD(toString);

#undef OBJ_METHOD
#undef PROTO_METHOD

  _ejs_object_setprop_utf8 (global, "Number", _ejs_Number);

  END_SHADOW_STACK_FRAME;
}


static EJSValue*
_ejs_number_specop_get (EJSValue* obj, void* propertyName, EJSBool isCStr)
{
  return _ejs_object_specops.get (obj, propertyName, isCStr);
}

static EJSValue*
_ejs_number_specop_get_own_property (EJSValue* obj, EJSValue* propertyName)
{
  return _ejs_object_specops.get_own_property (obj, propertyName);
}

static EJSValue*
_ejs_number_specop_get_property (EJSValue* obj, EJSValue* propertyName)
{
  return _ejs_object_specops.get_property (obj, propertyName);
}

static void
_ejs_number_specop_put (EJSValue *obj, EJSValue* propertyName, EJSValue* val, EJSBool flag)
{
  _ejs_object_specops.put (obj, propertyName, val, flag);
}

static EJSBool
_ejs_number_specop_can_put (EJSValue *obj, EJSValue* propertyName)
{
  return _ejs_object_specops.can_put (obj, propertyName);
}

static EJSBool
_ejs_number_specop_has_property (EJSValue *obj, EJSValue* propertyName)
{
  return _ejs_object_specops.has_property (obj, propertyName);
}

static EJSBool
_ejs_number_specop_delete (EJSValue *obj, EJSValue* propertyName, EJSBool flag)
{
  return _ejs_object_specops._delete (obj, propertyName, flag);
}

static EJSValue*
_ejs_number_specop_default_value (EJSValue *obj, const char *hint)
{
  return _ejs_object_specops.default_value (obj, hint);
}

static void
_ejs_number_specop_define_own_property (EJSValue *obj, EJSValue* propertyName, EJSValue* propertyDescriptor, EJSBool flag)
{
  _ejs_object_specops.define_own_property (obj, propertyName, propertyDescriptor, flag);
}
