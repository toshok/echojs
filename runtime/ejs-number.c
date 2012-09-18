#include <assert.h>

#include "ejs-ops.h"
#include "ejs-value.h"
#include "ejs-number.h"

EJSObject* _ejs_number_alloc_instance()
{
  return (EJSObject*)calloc(1, sizeof (EJSNumber));
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

    EJSNumber* rv = (EJSNumber*)calloc(1, sizeof(EJSNumber));

    _ejs_init_object ((EJSObject*)rv, _ejs_number_get_prototype());

    rv->number = num;

    return (EJSValue*)rv;
  }
  else {
    // called as a constructor
    return NULL;
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
  _ejs_Number = _ejs_function_new (NULL, (EJSClosureFunc)_ejs_Number_impl);
  _ejs_Number_proto = _ejs_object_new(NULL);

  _ejs_object_setprop_utf8 (_ejs_Number,       "prototype",  _ejs_Number_proto);
  _ejs_object_setprop_utf8 (_ejs_Number_proto, "toString",   _ejs_function_new (NULL, (EJSClosureFunc)_ejs_Number_prototype_toString));

  _ejs_object_setprop_utf8 (global, "Number", _ejs_Number);
}
