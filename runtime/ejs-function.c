//#define DEBUG_FUNCTIONS 1

#include <assert.h>

#include "ejs-value.h"
#include "ejs-object.h"
#include "ejs-function.h"

static EJSValue* _ejs_function_specop_get (EJSValue* obj, EJSValue* propertyName);
static EJSValue* _ejs_function_specop_get_own_property (EJSValue* obj, EJSValue* propertyName);
static EJSValue* _ejs_function_specop_get_property (EJSValue* obj, EJSValue* propertyName);
static void      _ejs_function_specop_put (EJSValue *obj, EJSValue* propertyName, EJSValue* val, EJSBool flag);
static EJSBool   _ejs_function_specop_can_put (EJSValue *obj, EJSValue* propertyName);
static EJSBool   _ejs_function_specop_has_property (EJSValue *obj, EJSValue* propertyName);
static EJSBool   _ejs_function_specop_delete (EJSValue *obj, EJSValue* propertyName, EJSBool flag);
static EJSValue* _ejs_function_specop_default_value (EJSValue *obj, const char *hint);
static void      _ejs_function_specop_define_own_property (EJSValue *obj, EJSValue* propertyName, EJSValue* propertyDescriptor, EJSBool flag);

extern EJSSpecOps _ejs_object_specops;

EJSSpecOps _ejs_function_specops = {
  "Function",
  _ejs_function_specop_get,
  _ejs_function_specop_get_own_property,
  _ejs_function_specop_get_property,
  _ejs_function_specop_put,
  _ejs_function_specop_can_put,
  _ejs_function_specop_has_property,
  _ejs_function_specop_delete,
  _ejs_function_specop_default_value,
  _ejs_function_specop_define_own_property
};

#if DEBUG_FUNCTIONS
static int indent_level = 0;
#define INDENT_AMOUNT 1

static void indent(char ch)
{
  int i;
  for (i = 0; i < indent_level; i ++)
    putchar (ch);
}

#endif

EJSValue*
_ejs_function_new (EJSClosureEnv* env, EJSValue *name, EJSClosureFunc func)
{
  EJSFunction *rv = (EJSFunction*)calloc (1, sizeof(EJSFunction));

  _ejs_init_object ((EJSObject*)rv, _ejs_function_get_prototype());
  rv->obj.ops = &_ejs_function_specops;

  rv->name = name;
  rv->func = func;
  rv->env = env;

  return (EJSValue*)rv;
}

EJSValue*
_ejs_function_new_utf8 (EJSClosureEnv* env, const char *name, EJSClosureFunc func)
{
  return _ejs_function_new (env, _ejs_string_new_utf8 (name), func);
}


EJSValue* _ejs_Function;
static EJSValue*
_ejs_Function_impl (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  printf ("Function() called either as a function or a constructor is not supported in ejs\n");
  abort();
}

static EJSValue* _ejs_Function_proto;
EJSValue*
_ejs_function_get_prototype()
{
  return _ejs_Function_proto;
}

// ECMA262 15.3.4.2
static EJSValue*
_ejs_Function_prototype_toString (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  char terrible_fixed_buffer[256];

  assert (EJSVAL_IS_FUNCTION(_this));
  EJSFunction* func = (EJSFunction*)_this;

  snprintf (terrible_fixed_buffer, sizeof (terrible_fixed_buffer), "[Function: %s]", EJSVAL_TO_STRING(func->name));
  return _ejs_string_new_utf8(terrible_fixed_buffer);
}

// ECMA262 15.3.4.3
static EJSValue*
_ejs_Function_prototype_apply (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  assert (EJSVAL_IS_FUNCTION(_this));
  EJSValue* thisArg = args[0];
  EJSArray* argArray = (EJSArray*)args[1];

  int apply_argc = EJS_ARRAY_LEN(argArray);
  EJSValue** apply_args = NULL;
  if (argc) {
    apply_args = malloc(sizeof(EJSValue*) * argc);
    int i;
    for (i = 0; i < argc; i ++) {
      apply_args[i] = EJS_ARRAY_ELEMENTS(argArray)[i];
    }
  }

  return EJSVAL_TO_FUNC(_this) (EJSVAL_TO_ENV(_this), thisArg, apply_argc, apply_args);
}

// ECMA262 15.3.4.4
static EJSValue*
_ejs_Function_prototype_call (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  assert (EJSVAL_IS_FUNCTION(_this));
  EJSValue* thisArg = _ejs_undefined;
  
  if (argc > 0) {
    thisArg = args[0];
    args = &args[1];
    argc --;
  }

  return EJSVAL_TO_FUNC(_this) (EJSVAL_TO_ENV(_this), thisArg, argc, argc == 0 ? NULL : args);
}

// ECMA262 15.3.4.5
static EJSValue*
_ejs_Function_prototype_bind (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  NOT_IMPLEMENTED();
}

void
_ejs_function_init(EJSValue *global)
{
  _ejs_Function = _ejs_function_new_utf8 (NULL, "Function", (EJSClosureFunc)_ejs_Function_impl);
  _ejs_Function_proto = _ejs_object_new(_ejs_object_get_prototype());

  // ECMA262 15.3.3.1
  _ejs_object_setprop_utf8 (_ejs_Function,       "prototype",  _ejs_Function_proto); // FIXME:  { [[Writable]]: false, [[Enumerable]]: false, [[Configurable]]: false }.
  // ECMA262 15.3.3.2
  _ejs_object_setprop_utf8 (_ejs_Function,       "length",     _ejs_number_new(1)); // FIXME:  { [[Writable]]: false, [[Enumerable]]: false, [[Configurable]]: false }.

#define OBJ_METHOD(x) _ejs_object_setprop_utf8 (_ejs_Function, #x, _ejs_function_new_utf8 (NULL, #x, (EJSClosureFunc)_ejs_Function_##x))
#define PROTO_METHOD(x) _ejs_object_setprop_utf8 (_ejs_Function_proto, #x, _ejs_function_new_utf8 (NULL, #x, (EJSClosureFunc)_ejs_Function_prototype_##x))

  PROTO_METHOD(toString);
  PROTO_METHOD(apply);
  PROTO_METHOD(call);
  PROTO_METHOD(bind);

#undef PROTOTYPE_METHOD
#undef OBJ_METHOD

  _ejs_object_setprop_utf8 (global, "Function", _ejs_Function);
}


EJSValue*
_ejs_invoke_closure_0 (EJSValue* closure, EJSValue* _this, int argc)
{
  assert (EJSVAL_IS_FUNCTION(closure));
#if DEBUG_FUNCTIONS
  EJSValue* closure_name = _ejs_Function_prototype_toString (NULL, closure, 0, NULL);
  indent('*');
  printf ("invoking %s\n", EJSVAL_TO_STRING(closure_name));
  indent_level += INDENT_AMOUNT;
#endif
  EJSValue* rv = EJSVAL_TO_FUNC(closure) (EJSVAL_TO_ENV(closure), _this, argc, NULL);
#if DEBUG_FUNCTIONS
  indent_level -= INDENT_AMOUNT;
  indent(' ');
  printf ("returning from %s\n", EJSVAL_TO_STRING(closure_name));
#endif
  return rv;
}

EJSValue*
_ejs_invoke_closure_1 (EJSValue* closure, EJSValue* _this, int argc, EJSValue* arg1)
{
  assert (EJSVAL_IS_FUNCTION(closure));
  EJSValue *args[] = { arg1 };
#if DEBUG_FUNCTIONS
  EJSValue* closure_name = _ejs_Function_prototype_toString (NULL, closure, 0, NULL);
  indent('*');
  printf ("invoking %s\n", EJSVAL_TO_STRING(closure_name));
  indent_level += INDENT_AMOUNT;
#endif
  EJSValue* rv = EJSVAL_TO_FUNC(closure) (EJSVAL_TO_ENV(closure), _this, argc, args);
#if DEBUG_FUNCTIONS
  indent_level -= INDENT_AMOUNT;
  indent(' ');
  printf ("returning from %s\n", EJSVAL_TO_STRING(closure_name));
#endif
  return rv;
}

EJSValue*
_ejs_invoke_closure_2 (EJSValue* closure, EJSValue* _this, int argc, EJSValue* arg1, EJSValue* arg2)
{
  assert (EJSVAL_IS_FUNCTION(closure));
  EJSValue *args[] = { arg1, arg2 };
#if DEBUG_FUNCTIONS
  EJSValue* closure_name = _ejs_Function_prototype_toString (NULL, closure, 0, NULL);
  indent('*');
  printf ("invoking %s\n", EJSVAL_TO_STRING(closure_name));
  indent_level += INDENT_AMOUNT;
#endif
  EJSValue* rv = EJSVAL_TO_FUNC(closure) (EJSVAL_TO_ENV(closure), _this, argc, args);
#if DEBUG_FUNCTIONS
  indent_level -= INDENT_AMOUNT;
  indent(' ');
  printf ("returning from %s\n", EJSVAL_TO_STRING(closure_name));
#endif
  return rv;
}

EJSValue*
_ejs_invoke_closure_3 (EJSValue* closure, EJSValue* _this, int argc, EJSValue* arg1, EJSValue* arg2, EJSValue* arg3)
{
  assert (EJSVAL_IS_FUNCTION(closure));
  EJSValue *args[] = { arg1, arg2, arg3 };
#if DEBUG_FUNCTIONS
  EJSValue* closure_name = _ejs_Function_prototype_toString (NULL, closure, 0, NULL);
  indent('*');
  printf ("invoking %s\n", EJSVAL_TO_STRING(closure_name));
  indent_level += INDENT_AMOUNT;
#endif
  EJSValue* rv = EJSVAL_TO_FUNC(closure) (EJSVAL_TO_ENV(closure), _this, argc, args);
#if DEBUG_FUNCTIONS
  indent_level -= INDENT_AMOUNT;
  indent(' ');
  printf ("returning from %s\n", EJSVAL_TO_STRING(closure_name));
#endif
  return rv;
}

EJSValue*
_ejs_invoke_closure_4 (EJSValue* closure, EJSValue* _this, int argc, EJSValue* arg1, EJSValue* arg2, EJSValue* arg3, EJSValue* arg4)
{
  assert (EJSVAL_IS_FUNCTION(closure));
  EJSValue *args[] = { arg1, arg2, arg3, arg4 };
#if DEBUG_FUNCTIONS
  EJSValue* closure_name = _ejs_Function_prototype_toString (NULL, closure, 0, NULL);
  indent('*');
  printf ("invoking %s\n", EJSVAL_TO_STRING(closure_name));
  indent_level += INDENT_AMOUNT;
#endif
  EJSValue* rv = EJSVAL_TO_FUNC(closure) (EJSVAL_TO_ENV(closure), _this, argc, args);
#if DEBUG_FUNCTIONS
  indent_level -= INDENT_AMOUNT;
  indent(' ');
  printf ("returning from %s\n", EJSVAL_TO_STRING(closure_name));
#endif
  return rv;
}

EJSValue*
_ejs_invoke_closure_5 (EJSValue* closure, EJSValue* _this, int argc, EJSValue* arg1, EJSValue* arg2, EJSValue* arg3, EJSValue* arg4, EJSValue* arg5)
{
  assert (EJSVAL_IS_FUNCTION(closure));
  EJSValue *args[] = { arg1, arg2, arg3, arg4, arg5 };
#if DEBUG_FUNCTIONS
  EJSValue* closure_name = _ejs_Function_prototype_toString (NULL, closure, 0, NULL);
  indent('*');
  printf ("invoking %s\n", EJSVAL_TO_STRING(closure_name));
  indent_level += INDENT_AMOUNT;
#endif
  EJSValue* rv = EJSVAL_TO_FUNC(closure) (EJSVAL_TO_ENV(closure), _this, argc, args);
#if DEBUG_FUNCTIONS
  indent_level -= INDENT_AMOUNT;
  indent(' ');
  printf ("returning from %s\n", EJSVAL_TO_STRING(closure_name));
#endif
  return rv;
}

EJSValue*
_ejs_invoke_closure_6 (EJSValue* closure, EJSValue* _this, int argc, EJSValue* arg1, EJSValue* arg2, EJSValue* arg3, EJSValue* arg4, EJSValue* arg5, EJSValue* arg6)
{
  assert (EJSVAL_IS_FUNCTION(closure));
  EJSValue *args[] = { arg1, arg2, arg3, arg4, arg5, arg6 };
#if DEBUG_FUNCTIONS
  EJSValue* closure_name = _ejs_Function_prototype_toString (NULL, closure, 0, NULL);
  indent('*');
  printf ("invoking %s\n", EJSVAL_TO_STRING(closure_name));
  indent_level += INDENT_AMOUNT;
#endif
  EJSValue* rv = EJSVAL_TO_FUNC(closure) (EJSVAL_TO_ENV(closure), _this, argc, args);
#if DEBUG_FUNCTIONS
  indent_level -= INDENT_AMOUNT;
  indent(' ');
  printf ("returning from %s\n", EJSVAL_TO_STRING(closure_name));
#endif
  return rv;
}

EJSValue*
_ejs_invoke_closure_7 (EJSValue* closure, EJSValue* _this, int argc, EJSValue* arg1, EJSValue* arg2, EJSValue* arg3, EJSValue* arg4, EJSValue* arg5, EJSValue* arg6, EJSValue* arg7)
{
  assert (EJSVAL_IS_FUNCTION(closure));
  EJSValue *args[] = { arg1, arg2, arg3, arg4, arg5, arg6, arg7 };
#if DEBUG_FUNCTIONS
  EJSValue* closure_name = _ejs_Function_prototype_toString (NULL, closure, 0, NULL);
  indent('*');
  printf ("invoking %s\n", EJSVAL_TO_STRING(closure_name));
  indent_level += INDENT_AMOUNT;
#endif
  EJSValue* rv = EJSVAL_TO_FUNC(closure) (EJSVAL_TO_ENV(closure), _this, argc, args);
#if DEBUG_FUNCTIONS
  indent_level -= INDENT_AMOUNT;
  indent(' ');
  printf ("returning from %s\n", EJSVAL_TO_STRING(closure_name));
#endif
  return rv;
}

EJSValue*
_ejs_invoke_closure_8 (EJSValue* closure, EJSValue* _this, int argc, EJSValue* arg1, EJSValue* arg2, EJSValue* arg3, EJSValue* arg4, EJSValue* arg5, EJSValue* arg6, EJSValue* arg7, EJSValue* arg8)
{
  assert (EJSVAL_IS_FUNCTION(closure));
  EJSValue *args[] = { arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8 };
#if DEBUG_FUNCTIONS
  EJSValue* closure_name = _ejs_Function_prototype_toString (NULL, closure, 0, NULL);
  indent('*');
  printf ("invoking %s\n", EJSVAL_TO_STRING(closure_name));
  indent_level += INDENT_AMOUNT;
#endif
  EJSValue* rv = EJSVAL_TO_FUNC(closure) (EJSVAL_TO_ENV(closure), _this, argc, args);
#if DEBUG_FUNCTIONS
  indent_level -= INDENT_AMOUNT;
  indent(' ');
  printf ("returning from %s\n", EJSVAL_TO_STRING(closure_name));
#endif
  return rv;
}

EJSValue*
_ejs_invoke_closure_9 (EJSValue* closure, EJSValue* _this, int argc, EJSValue* arg1, EJSValue* arg2, EJSValue* arg3, EJSValue* arg4, EJSValue* arg5, EJSValue* arg6, EJSValue* arg7, EJSValue* arg8, EJSValue* arg9)
{
  assert (EJSVAL_IS_FUNCTION(closure));
  EJSValue *args[] = { arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9 };
#if DEBUG_FUNCTIONS
  EJSValue* closure_name = _ejs_Function_prototype_toString (NULL, closure, 0, NULL);
  indent('*');
  printf ("invoking %s\n", EJSVAL_TO_STRING(closure_name));
  indent_level += INDENT_AMOUNT;
#endif
  EJSValue* rv = EJSVAL_TO_FUNC(closure) (EJSVAL_TO_ENV(closure), _this, argc, args);
#if DEBUG_FUNCTIONS
  indent_level -= INDENT_AMOUNT;
  indent(' ');
  printf ("returning from %s\n", EJSVAL_TO_STRING(closure_name));
#endif
  return rv;
}

EJSValue*
_ejs_invoke_closure_10 (EJSValue* closure, EJSValue* _this, int argc, EJSValue* arg1, EJSValue* arg2, EJSValue* arg3, EJSValue* arg4, EJSValue* arg5, EJSValue* arg6, EJSValue* arg7, EJSValue* arg8, EJSValue* arg9, EJSValue* arg10)
{
  assert (EJSVAL_IS_FUNCTION(closure));
  EJSValue *args[] = { arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10 };
#if DEBUG_FUNCTIONS
  EJSValue* closure_name = _ejs_Function_prototype_toString (NULL, closure, 0, NULL);
  indent('*');
  printf ("invoking %s\n", EJSVAL_TO_STRING(closure_name));
  indent_level += INDENT_AMOUNT;
#endif
  EJSValue* rv = EJSVAL_TO_FUNC(closure) (EJSVAL_TO_ENV(closure), _this, argc, args);
#if DEBUG_FUNCTIONS
  indent_level -= INDENT_AMOUNT;
  indent(' ');
  printf ("returning from %s\n", EJSVAL_TO_STRING(closure_name));
#endif
  return rv;
}



static EJSValue*
_ejs_function_specop_get (EJSValue* obj, EJSValue* propertyName)
{
  return _ejs_object_specops.get (obj, propertyName);
}

static EJSValue*
_ejs_function_specop_get_own_property (EJSValue* obj, EJSValue* propertyName)
{
  return _ejs_object_specops.get_own_property (obj, propertyName);
}

static EJSValue*
_ejs_function_specop_get_property (EJSValue* obj, EJSValue* propertyName)
{
  return _ejs_object_specops.get_property (obj, propertyName);
}

static void
_ejs_function_specop_put (EJSValue *obj, EJSValue* propertyName, EJSValue* val, EJSBool flag)
{
  _ejs_object_specops.put (obj, propertyName, val, flag);
}

static EJSBool
_ejs_function_specop_can_put (EJSValue *obj, EJSValue* propertyName)
{
  return _ejs_object_specops.can_put (obj, propertyName);
}

static EJSBool
_ejs_function_specop_has_property (EJSValue *obj, EJSValue* propertyName)
{
  return _ejs_object_specops.has_property (obj, propertyName);
}

static EJSBool
_ejs_function_specop_delete (EJSValue *obj, EJSValue* propertyName, EJSBool flag)
{
  return _ejs_object_specops._delete (obj, propertyName, flag);
}

static EJSValue*
_ejs_function_specop_default_value (EJSValue *obj, const char *hint)
{
  return _ejs_object_specops.default_value (obj, hint);
}

static void
_ejs_function_specop_define_own_property (EJSValue *obj, EJSValue* propertyName, EJSValue* propertyDescriptor, EJSBool flag)
{
  _ejs_object_specops.define_own_property (obj, propertyName, propertyDescriptor, flag);
}
