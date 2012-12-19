#include <assert.h>

#include "ejs-ops.h"
#include "ejs-value.h"
#include "ejs-number.h"
#include "ejs-function.h"

static ejsval _ejs_number_specop_get (ejsval obj, ejsval propertyName, EJSBool isCStr);
static ejsval _ejs_number_specop_get_own_property (ejsval obj, ejsval propertyName);
static ejsval _ejs_number_specop_get_property (ejsval obj, ejsval propertyName);
static void      _ejs_number_specop_put (ejsval obj, ejsval propertyName, ejsval val, EJSBool flag);
static EJSBool   _ejs_number_specop_can_put (ejsval obj, ejsval propertyName);
static EJSBool   _ejs_number_specop_has_property (ejsval obj, ejsval propertyName);
static EJSBool   _ejs_number_specop_delete (ejsval obj, ejsval propertyName, EJSBool flag);
static ejsval _ejs_number_specop_default_value (ejsval obj, const char *hint);
static void      _ejs_number_specop_define_own_property (ejsval obj, ejsval propertyName, ejsval propertyDescriptor, EJSBool flag);
static void      _ejs_number_specop_finalize (EJSObject* obj);
static void      _ejs_number_specop_scan (EJSObject* obj, EJSValueFunc scan_func);

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
  _ejs_number_specop_define_own_property,
  _ejs_number_specop_finalize,
  _ejs_number_specop_scan
};

EJSObject* _ejs_number_alloc_instance()
{
  return (EJSObject*)_ejs_gc_new (EJSNumber);
}

ejsval _ejs_Number;
ejsval _ejs_Number_proto;

static ejsval
_ejs_Number_impl (ejsval env, ejsval _this, int argc, ejsval *args)
{
  if (EJSVAL_IS_UNDEFINED(_this)) {
    // called as a function
    double num = 0;

    if (argc > 0) {
      num = ToDouble(args[0]);
    }

    EJSNumber* rv = _ejs_gc_new (EJSNumber);

    _ejs_init_object ((EJSObject*)rv, _ejs_Number_proto, &_ejs_number_specops);

    rv->number = num;

    return OBJECT_TO_EJSVAL((EJSObject*)rv);
  }
  else {
    EJSNumber* num = (EJSNumber*)EJSVAL_TO_OBJECT(_this);

    // called as a constructor
    ((EJSObject*)num)->ops = &_ejs_number_specops;

    if (argc > 0) {
      num->number = ToDouble(args[0]);
    }
    else {
      num->number = 0;
    }
    return _this;
  }
}

static ejsval
_ejs_Number_prototype_toString (ejsval env, ejsval _this, int argc, ejsval *args)
{
  EJSNumber *num = (EJSNumber*)EJSVAL_TO_OBJECT(_this);

  return NumberToString(num->number);
}

void
_ejs_number_init(ejsval global)
{
  START_SHADOW_STACK_FRAME;

  _ejs_gc_add_named_root (_ejs_Number_proto);
  _ejs_Number_proto = _ejs_object_new(_ejs_null);

  ADD_STACK_ROOT(ejsval, tmpobj, _ejs_function_new_utf8 (_ejs_null, "Number", (EJSClosureFunc)_ejs_Number_impl));
  _ejs_Number = tmpobj;

  _ejs_object_setprop_utf8 (_ejs_Number,       "prototype",  _ejs_Number_proto);

#define OBJ_METHOD(x) do { ADD_STACK_ROOT(ejsval, funcname, _ejs_string_new_utf8(#x)); ADD_STACK_ROOT(ejsval, tmpfunc, _ejs_function_new (_ejs_null, funcname, (EJSClosureFunc)_ejs_Number_##x)); _ejs_object_setprop (_ejs_Number, funcname, tmpfunc); } while (0)
#define PROTO_METHOD(x) do { ADD_STACK_ROOT(ejsval, funcname, _ejs_string_new_utf8(#x)); ADD_STACK_ROOT(ejsval, tmpfunc, _ejs_function_new (_ejs_null, funcname, (EJSClosureFunc)_ejs_Number_prototype_##x)); _ejs_object_setprop (_ejs_Number_proto, funcname, tmpfunc); } while (0)

  PROTO_METHOD(toString);

#undef OBJ_METHOD
#undef PROTO_METHOD

  _ejs_object_setprop_utf8 (global, "Number", _ejs_Number);

  END_SHADOW_STACK_FRAME;
}


static ejsval
_ejs_number_specop_get (ejsval obj, ejsval propertyName, EJSBool isCStr)
{
  return _ejs_object_specops.get (obj, propertyName, isCStr);
}

static ejsval
_ejs_number_specop_get_own_property (ejsval obj, ejsval propertyName)
{
  return _ejs_object_specops.get_own_property (obj, propertyName);
}

static ejsval
_ejs_number_specop_get_property (ejsval obj, ejsval propertyName)
{
  return _ejs_object_specops.get_property (obj, propertyName);
}

static void
_ejs_number_specop_put (ejsval obj, ejsval propertyName, ejsval val, EJSBool flag)
{
  _ejs_object_specops.put (obj, propertyName, val, flag);
}

static EJSBool
_ejs_number_specop_can_put (ejsval obj, ejsval propertyName)
{
  return _ejs_object_specops.can_put (obj, propertyName);
}

static EJSBool
_ejs_number_specop_has_property (ejsval obj, ejsval propertyName)
{
  return _ejs_object_specops.has_property (obj, propertyName);
}

static EJSBool
_ejs_number_specop_delete (ejsval obj, ejsval propertyName, EJSBool flag)
{
  return _ejs_object_specops._delete (obj, propertyName, flag);
}

static ejsval
_ejs_number_specop_default_value (ejsval obj, const char *hint)
{
  return _ejs_object_specops.default_value (obj, hint);
}

static void
_ejs_number_specop_define_own_property (ejsval obj, ejsval propertyName, ejsval propertyDescriptor, EJSBool flag)
{
  _ejs_object_specops.define_own_property (obj, propertyName, propertyDescriptor, flag);
}

static void
_ejs_number_specop_finalize (EJSObject* obj)
{
  _ejs_object_specops.finalize (obj);
}

static void
_ejs_number_specop_scan (EJSObject* obj, EJSValueFunc scan_func)
{
  _ejs_object_specops.scan (obj, scan_func);
}
