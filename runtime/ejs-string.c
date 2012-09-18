#include <assert.h>

#include "ejs-value.h"
#include "ejs-string.h"

EJSObject* _ejs_string_alloc_instance()
{
  return (EJSObject*)calloc(1, sizeof (EJSString));
}

EJSValue* _ejs_String;
static EJSValue*
_ejs_String_impl (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  if (EJSVAL_IS_UNDEFINED(_this)) {
    // called as a function
    char *str = "";
    int str_len = 0;

    if (argc > 0) {
      // XXX convert args[0] to a string, but we assume it already was one
      if (!EJSVAL_IS_STRING(args[0]))
	abort();

      str_len = EJSVAL_TO_STRLEN(args[0]);
      str = EJSVAL_TO_STRING(args[0]);
    }

    int value_size = sizeof (EJSString) + str_len;

    EJSString* rv = (EJSString*)calloc(1, value_size);

    _ejs_init_object ((EJSObject*)rv, _ejs_string_get_prototype());

    rv->len = str_len;
    rv->str = strdup(str);

    return (EJSValue*)rv;
  }
  else {
    // called as a constructor
    return NULL;
  }
}

static EJSValue* _ejs_String_proto;
EJSValue*
_ejs_string_get_prototype()
{
  return _ejs_String_proto;
}

static EJSValue*
_ejs_String_prototype_toString (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  EJSString *str = (EJSString*)_this;

  return _ejs_string_new_utf8 (str->str);
}

static EJSValue*
_ejs_String_prototype_split (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  // for now let's just not split anything at all, return the original string as element0 of the array.

  EJSValue* rv = _ejs_array_new (1);
  _ejs_object_setprop (rv, _ejs_number_new (0), _this);
  return rv;
}

void
_ejs_string_init(EJSValue *global)
{
  _ejs_String = _ejs_function_new (NULL, (EJSClosureFunc)_ejs_String_impl);
  _ejs_String_proto = _ejs_object_new(NULL);

  _ejs_object_setprop_utf8 (_ejs_String,       "prototype",  _ejs_String_proto);
  _ejs_object_setprop_utf8 (_ejs_String_proto, "toString",  _ejs_function_new (NULL, (EJSClosureFunc)_ejs_String_prototype_toString));
  _ejs_object_setprop_utf8 (_ejs_String_proto, "split",     _ejs_function_new (NULL, (EJSClosureFunc)_ejs_String_prototype_split));

  _ejs_object_setprop_utf8 (global, "String", _ejs_String);
}
