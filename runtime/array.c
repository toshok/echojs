#include <assert.h>

#include "object.h"

EJSValue* _ejs_Array;
static EJSValue*
_ejs_Array_impl (EJSValue* env, EJSValue* _this, int argc, EJSValue *val)
{
  if (EJSVAL_IS_UNDEFINED(_this)) {
    // called as a function
    printf ("called Array() as a function!\n");
    return NULL;
  }
  else {
    // called as a constructor
    printf ("called Array() as a constructor!\n");
    return NULL;
  }
}

#define ARRAY_LEN(a) (a->u.a.array_length)
#define ARRAY_ELEMENTS(a) (a->u.a.elements)

static EJSValue*
_ejs_Array_push (EJSValue* env, EJSValue* _this, int argc, EJSValue *val)
{
  assert (EJSVAL_IS_ARRAY(_this));
  ARRAY_ELEMENTS(_this)[ARRAY_LEN(_this)++] = val;
  return _ejs_number_new (ARRAY_LEN(_this));
}

static EJSValue*
_ejs_Array_pop (EJSValue* env, EJSValue* _this, int argc, EJSValue *val)
{
  assert (EJSVAL_IS_ARRAY(_this));
  if (ARRAY_LENGTH(_this) == 0)
    return _ejs_undefined;
  return ARRAY_ELEMENTS(_this)[--ARRAY_LENGTH(_this)];
}

static EJSValue* _ejs_Array_proto;
EJSValue*
_ejs_array_get_prototype()
{
  return _ejs_Array_proto;
}

void
_ejs_array_init()
{
  _ejs_Array = _ejs_closure_new (NULL, (EJSClosureFunc0)_ejs_Array_impl);
  _ejs_Array_proto = _ejs_object_new(NULL);

  _ejs_object_setprop (_ejs_Array,       _ejs_string_new_utf8("prototype"),  _ejs_Array_proto);
  _ejs_object_setprop (_ejs_Array_proto, _ejs_string_new_utf8("push"),       _ejs_closure_new (NULL, (EJSClosureFunc0)_ejs_Array_push));
  _ejs_object_setprop (_ejs_Array_proto, _ejs_string_new_utf8("pop"),        _ejs_closure_new (NULL, (EJSClosureFunc0)_ejs_Array_pop));
}
