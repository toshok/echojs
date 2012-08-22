#include <assert.h>

#include "ejs-value.h"
#include "ejs-string.h"

EJSValue* _ejs_String;
static EJSValue*
_ejs_String_impl (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  if (EJSVAL_IS_UNDEFINED(_this)) {
    // called as a function
    printf ("called String() as a function!\n");
    return NULL;
  }
  else {
    // called as a constructor
    printf ("called String() as a constructor!\n");
    return NULL;
  }
}

static EJSValue* _ejs_String_proto;
EJSValue*
_ejs_string_get_prototype()
{
  return _ejs_String_proto;
}

void
_ejs_string_init(EJSValue *global)
{
  _ejs_String = _ejs_closure_new (NULL, (EJSClosureFunc)_ejs_String_impl);
  _ejs_String_proto = _ejs_object_new(NULL);

  _ejs_object_setprop (_ejs_String,       _ejs_string_new_utf8("prototype"),  _ejs_String_proto);
  _ejs_object_setprop (_ejs_String_proto, _ejs_string_new_utf8("prototype"),  _ejs_string_get_prototype());

  _ejs_object_setprop (global, _ejs_string_new_utf8("String"), _ejs_String);
}
