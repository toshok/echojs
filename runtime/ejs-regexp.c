#include <assert.h>

#include "ejs-value.h"
#include "ejs-regexp.h"

EJSValue*
_ejs_regexp_new_utf8 (const char* str)
{
  int str_len = strlen(str);
  int value_size = sizeof (EJSValueType) + sizeof (int) + str_len + 1;

  EJSValue* rv = (EJSValue*)calloc(1, value_size);
  rv->type = EJSValueTypeString;
  rv->u.s.len = str_len;
  memmove (&rv->u.s.data, str, str_len + 1);
  return rv;
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

void
_ejs_regexp_init(EJSValue *global)
{
  _ejs_Regexp = _ejs_closure_new (NULL, (EJSClosureFunc)_ejs_Regexp_impl);
  _ejs_Regexp_proto = _ejs_object_new(NULL);

  _ejs_object_setprop_utf8 (_ejs_Regexp,       "prototype",  _ejs_Regexp_proto);
  _ejs_object_setprop_utf8 (_ejs_Regexp_proto, "prototype",  _ejs_regexp_get_prototype());

  _ejs_object_setprop_utf8 (global, "RegExp", _ejs_Regexp);
}
