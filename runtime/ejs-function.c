/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

//#define DEBUG_FUNCTIONS 1

#include <assert.h>

#include "ejs-value.h"
#include "ejs-object.h"
#include "ejs-function.h"
#include "ejs-array.h"

static ejsval _ejs_function_specop_get (ejsval obj, ejsval propertyName, EJSBool isCStr);
static ejsval _ejs_function_specop_get_own_property (ejsval obj, ejsval propertyName);
static ejsval _ejs_function_specop_get_property (ejsval obj, ejsval propertyName);
static void      _ejs_function_specop_put (ejsval obj, ejsval propertyName, ejsval val, EJSBool flag);
static EJSBool   _ejs_function_specop_can_put (ejsval obj, ejsval propertyName);
static EJSBool   _ejs_function_specop_has_property (ejsval obj, ejsval propertyName);
static EJSBool   _ejs_function_specop_delete (ejsval obj, ejsval propertyName, EJSBool flag);
static ejsval _ejs_function_specop_default_value (ejsval obj, const char *hint);
static void      _ejs_function_specop_define_own_property (ejsval obj, ejsval propertyName, ejsval propertyDescriptor, EJSBool flag);
static void      _ejs_function_specop_finalize (EJSObject* obj);
static void      _ejs_function_specop_scan (EJSObject* obj, EJSValueFunc scan_func);

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
    _ejs_function_specop_define_own_property,
    _ejs_function_specop_finalize,
    _ejs_function_specop_scan
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

ejsval
_ejs_function_new (EJSClosureEnv env, ejsval name, EJSClosureFunc func)
{
    EJSFunction *rv = _ejs_gc_new(EJSFunction);

    _ejs_init_object ((EJSObject*)rv, _ejs_Function_proto, &_ejs_function_specops);

    rv->name = name;
    rv->func = func;
    rv->env = env;

    return OBJECT_TO_EJSVAL((EJSObject*)rv);
}

ejsval
_ejs_function_new_utf8 (EJSClosureEnv env, const char *name, EJSClosureFunc func)
{
    START_SHADOW_STACK_FRAME;

    ADD_STACK_ROOT(ejsval, function_name, _ejs_string_new_utf8 (name));

    ADD_STACK_ROOT(ejsval, rv, _ejs_function_new (env, function_name, func));

    END_SHADOW_STACK_FRAME;
    return rv;
}


ejsval _ejs_Function_proto;
ejsval _ejs_Function;

static ejsval
_ejs_Function_impl (ejsval env, ejsval _this, int argc, ejsval *args)
{
    printf ("Function() called either as a function or a constructor is not supported in ejs\n");
    abort();
}


// ECMA262 15.3.4.2
static ejsval
_ejs_Function_prototype_toString (ejsval env, ejsval _this, int argc, ejsval *args)
{
    char terrible_fixed_buffer[256];

    // XXX nanboxing breaks this assert (EJSVAL_IS_FUNCTION(_this));
    EJSFunction* func = (EJSFunction*)EJSVAL_TO_OBJECT(_this);

    snprintf (terrible_fixed_buffer, sizeof (terrible_fixed_buffer), "[Function: %s]", EJSVAL_TO_FLAT_STRING(func->name));
    return _ejs_string_new_utf8(terrible_fixed_buffer);
}

// ECMA262 15.3.4.3
static ejsval
_ejs_Function_prototype_apply (ejsval env, ejsval _this, int argc, ejsval *args)
{
    // XXX nanboxing breaks this assert (EJSVAL_IS_FUNCTION(_this));
    ejsval thisArg = args[0];
    ejsval argArray = args[1];

    int apply_argc = EJS_ARRAY_LEN(argArray);
    ejsval* apply_args = NULL;
    if (argc) {
        apply_args = (ejsval*)malloc(sizeof(ejsval) * argc);
        int i;
        for (i = 0; i < argc; i ++) {
            apply_args[i] = EJS_ARRAY_ELEMENTS(argArray)[i];
        }
    }

    return EJSVAL_TO_FUNC(_this) (EJSVAL_TO_ENV(_this), thisArg, apply_argc, apply_args);
}

// ECMA262 15.3.4.4
static ejsval
_ejs_Function_prototype_call (ejsval env, ejsval _this, int argc, ejsval *args)
{
    // XXX nanboxing breaks this assert (EJSVAL_IS_FUNCTION(_this));

    START_SHADOW_STACK_FRAME;

    ADD_STACK_ROOT(ejsval, thisArg, _ejs_undefined);
  
    if (argc > 0) {
        thisArg = args[0];
        args = &args[1];
        argc --;
    }

    ADD_STACK_ROOT(ejsval, rv, EJSVAL_TO_FUNC(_this) (EJSVAL_TO_ENV(_this), thisArg, argc, argc == 0 ? NULL : args));

    END_SHADOW_STACK_FRAME;
    return rv;
}

// ECMA262 15.3.4.5
static ejsval
_ejs_Function_prototype_bind (ejsval env, ejsval _this, int argc, ejsval *args)
{
    NOT_IMPLEMENTED();
}

void
_ejs_function_init(ejsval global)
{
    START_SHADOW_STACK_FRAME;

    _ejs_gc_add_named_root (_ejs_Function_proto);
    _ejs_Function_proto = _ejs_object_new(_ejs_Object_proto);

    ADD_STACK_ROOT(ejsval, tmpobj, _ejs_function_new_utf8 (_ejs_null, "Function", (EJSClosureFunc)_ejs_Function_impl));
    _ejs_Function = tmpobj;

    // ECMA262 15.3.3.1
    _ejs_object_setprop (_ejs_Function,       _ejs_atom_prototype,  _ejs_Function_proto); // FIXME:  { [[Writable]]: false, [[Enumerable]]: false, [[Configurable]]: false }.
    // ECMA262 15.3.3.2
    _ejs_object_setprop (_ejs_Function,       _ejs_atom_length,     NUMBER_TO_EJSVAL(1)); // FIXME:  { [[Writable]]: false, [[Enumerable]]: false, [[Configurable]]: false }.

#define OBJ_METHOD(x) EJS_MACRO_START ADD_STACK_ROOT(ejsval, funcname, _ejs_string_new_utf8(#x)); ADD_STACK_ROOT(ejsval, tmpfunc, _ejs_function_new (_ejs_null, funcname, (EJSClosureFunc)_ejs_Function_##x)); _ejs_object_setprop (_ejs_Function, funcname, tmpfunc); EJS_MACRO_END
#define PROTO_METHOD(x) EJS_MACRO_START ADD_STACK_ROOT(ejsval, funcname, _ejs_string_new_utf8(#x)); ADD_STACK_ROOT(ejsval, tmpfunc, _ejs_function_new (_ejs_null, funcname, (EJSClosureFunc)_ejs_Function_prototype_##x)); _ejs_object_setprop (_ejs_Function_proto, funcname, tmpfunc); EJS_MACRO_END

    PROTO_METHOD(toString);
    PROTO_METHOD(apply);
    PROTO_METHOD(call);
    PROTO_METHOD(bind);

#undef PROTOTYPE_METHOD
#undef OBJ_METHOD

    _ejs_object_setprop_utf8 (global, "Function", _ejs_Function);

    END_SHADOW_STACK_FRAME;
}


ejsval
_ejs_invoke_closure_0 (ejsval closure, ejsval _this, int argc)
{
    // XXX nanboxing breaks this assert (EJSVAL_IS_FUNCTION(closre));
#if DEBUG_FUNCTIONS
    ejsval closure_name = _ejs_Function_prototype_toString (_ejs_null, closure, 0, NULL);
    indent('*');
    printf ("invoking %s\n", EJSVAL_TO_STRING(closure_name));
    indent_level += INDENT_AMOUNT;
#endif
    ejsval rv = EJSVAL_TO_FUNC(closure) (EJSVAL_TO_ENV(closure), _this, argc, NULL);
#if DEBUG_FUNCTIONS
    indent_level -= INDENT_AMOUNT;
    indent(' ');
    printf ("returning from %s\n", EJSVAL_TO_STRING(closure_name));
#endif
    return rv;
}

ejsval
_ejs_invoke_closure_1 (ejsval closure, ejsval _this, int argc, ejsval arg1)
{
    // XXX nanboxing breaks this assert (EJSVAL_IS_FUNCTION(closre));
    ejsval args[] = { arg1 };
#if DEBUG_FUNCTIONS
    ejsval closure_name = _ejs_Function_prototype_toString (_ejs_null, closure, 0, NULL);
    indent('*');
    printf ("invoking %s\n", EJSVAL_TO_STRING(closure_name));
    indent_level += INDENT_AMOUNT;
#endif
    ejsval rv = EJSVAL_TO_FUNC(closure) (EJSVAL_TO_ENV(closure), _this, argc, args);
#if DEBUG_FUNCTIONS
    indent_level -= INDENT_AMOUNT;
    indent(' ');
    printf ("returning from %s\n", EJSVAL_TO_STRING(closure_name));
#endif
    return rv;
}

ejsval
_ejs_invoke_closure_2 (ejsval closure, ejsval _this, int argc, ejsval arg1, ejsval arg2)
{
    // XXX nanboxing breaks this assert (EJSVAL_IS_FUNCTION(closre));
    ejsval args[] = { arg1, arg2 };
#if DEBUG_FUNCTIONS
    ejsval closure_name = _ejs_Function_prototype_toString (_ejs_null, closure, 0, NULL);
    indent('*');
    printf ("invoking %s\n", EJSVAL_TO_STRING(closure_name));
    indent_level += INDENT_AMOUNT;
#endif
    ejsval rv = EJSVAL_TO_FUNC(closure) (EJSVAL_TO_ENV(closure), _this, argc, args);
#if DEBUG_FUNCTIONS
    indent_level -= INDENT_AMOUNT;
    indent(' ');
    printf ("returning from %s\n", EJSVAL_TO_STRING(closure_name));
#endif
    return rv;
}

ejsval
_ejs_invoke_closure_3 (ejsval closure, ejsval _this, int argc, ejsval arg1, ejsval arg2, ejsval arg3)
{
    // XXX nanboxing breaks this assert (EJSVAL_IS_FUNCTION(closre));
    ejsval args[] = { arg1, arg2, arg3 };
#if DEBUG_FUNCTIONS
    ejsval closure_name = _ejs_Function_prototype_toString (_ejs_null, closure, 0, NULL);
    indent('*');
    printf ("invoking %s\n", EJSVAL_TO_STRING(closure_name));
    indent_level += INDENT_AMOUNT;
#endif
    ejsval rv = EJSVAL_TO_FUNC(closure) (EJSVAL_TO_ENV(closure), _this, argc, args);
#if DEBUG_FUNCTIONS
    indent_level -= INDENT_AMOUNT;
    indent(' ');
    printf ("returning from %s\n", EJSVAL_TO_STRING(closure_name));
#endif
    return rv;
}

ejsval
_ejs_invoke_closure_4 (ejsval closure, ejsval _this, int argc, ejsval arg1, ejsval arg2, ejsval arg3, ejsval arg4)
{
    // XXX nanboxing breaks this assert (EJSVAL_IS_FUNCTION(closre));
    ejsval args[] = { arg1, arg2, arg3, arg4 };
#if DEBUG_FUNCTIONS
    ejsval closure_name = _ejs_Function_prototype_toString (_ejs_null, closure, 0, NULL);
    indent('*');
    printf ("invoking %s\n", EJSVAL_TO_STRING(closure_name));
    indent_level += INDENT_AMOUNT;
#endif
    ejsval rv = EJSVAL_TO_FUNC(closure) (EJSVAL_TO_ENV(closure), _this, argc, args);
#if DEBUG_FUNCTIONS
    indent_level -= INDENT_AMOUNT;
    indent(' ');
    printf ("returning from %s\n", EJSVAL_TO_STRING(closure_name));
#endif
    return rv;
}

ejsval
_ejs_invoke_closure_5 (ejsval closure, ejsval _this, int argc, ejsval arg1, ejsval arg2, ejsval arg3, ejsval arg4, ejsval arg5)
{
    // XXX nanboxing breaks this assert (EJSVAL_IS_FUNCTION(closre));
    ejsval args[] = { arg1, arg2, arg3, arg4, arg5 };
#if DEBUG_FUNCTIONS
    ejsval closure_name = _ejs_Function_prototype_toString (_ejs_null, closure, 0, NULL);
    indent('*');
    printf ("invoking %s\n", EJSVAL_TO_STRING(closure_name));
    indent_level += INDENT_AMOUNT;
#endif
    ejsval rv = EJSVAL_TO_FUNC(closure) (EJSVAL_TO_ENV(closure), _this, argc, args);
#if DEBUG_FUNCTIONS
    indent_level -= INDENT_AMOUNT;
    indent(' ');
    printf ("returning from %s\n", EJSVAL_TO_STRING(closure_name));
#endif
    return rv;
}

ejsval
_ejs_invoke_closure_6 (ejsval closure, ejsval _this, int argc, ejsval arg1, ejsval arg2, ejsval arg3, ejsval arg4, ejsval arg5, ejsval arg6)
{
    // XXX nanboxing breaks this assert (EJSVAL_IS_FUNCTION(closre));
    ejsval args[] = { arg1, arg2, arg3, arg4, arg5, arg6 };
#if DEBUG_FUNCTIONS
    ejsval closure_name = _ejs_Function_prototype_toString (_ejs_null, closure, 0, NULL);
    indent('*');
    printf ("invoking %s\n", EJSVAL_TO_STRING(closure_name));
    indent_level += INDENT_AMOUNT;
#endif
    ejsval rv = EJSVAL_TO_FUNC(closure) (EJSVAL_TO_ENV(closure), _this, argc, args);
#if DEBUG_FUNCTIONS
    indent_level -= INDENT_AMOUNT;
    indent(' ');
    printf ("returning from %s\n", EJSVAL_TO_STRING(closure_name));
#endif
    return rv;
}

ejsval
_ejs_invoke_closure_7 (ejsval closure, ejsval _this, int argc, ejsval arg1, ejsval arg2, ejsval arg3, ejsval arg4, ejsval arg5, ejsval arg6, ejsval arg7)
{
    // XXX nanboxing breaks this assert (EJSVAL_IS_FUNCTION(closre));
    ejsval args[] = { arg1, arg2, arg3, arg4, arg5, arg6, arg7 };
#if DEBUG_FUNCTIONS
    ejsval closure_name = _ejs_Function_prototype_toString (_ejs_null, closure, 0, NULL);
    indent('*');
    printf ("invoking %s\n", EJSVAL_TO_STRING(closure_name));
    indent_level += INDENT_AMOUNT;
#endif
    ejsval rv = EJSVAL_TO_FUNC(closure) (EJSVAL_TO_ENV(closure), _this, argc, args);
#if DEBUG_FUNCTIONS
    indent_level -= INDENT_AMOUNT;
    indent(' ');
    printf ("returning from %s\n", EJSVAL_TO_STRING(closure_name));
#endif
    return rv;
}

ejsval
_ejs_invoke_closure_8 (ejsval closure, ejsval _this, int argc, ejsval arg1, ejsval arg2, ejsval arg3, ejsval arg4, ejsval arg5, ejsval arg6, ejsval arg7, ejsval arg8)
{
    // XXX nanboxing breaks this assert (EJSVAL_IS_FUNCTION(closre));
    ejsval args[] = { arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8 };
#if DEBUG_FUNCTIONS
    ejsval closure_name = _ejs_Function_prototype_toString (_ejs_null, closure, 0, NULL);
    indent('*');
    printf ("invoking %s\n", EJSVAL_TO_STRING(closure_name));
    indent_level += INDENT_AMOUNT;
#endif
    ejsval rv = EJSVAL_TO_FUNC(closure) (EJSVAL_TO_ENV(closure), _this, argc, args);
#if DEBUG_FUNCTIONS
    indent_level -= INDENT_AMOUNT;
    indent(' ');
    printf ("returning from %s\n", EJSVAL_TO_STRING(closure_name));
#endif
    return rv;
}

ejsval
_ejs_invoke_closure_9 (ejsval closure, ejsval _this, int argc, ejsval arg1, ejsval arg2, ejsval arg3, ejsval arg4, ejsval arg5, ejsval arg6, ejsval arg7, ejsval arg8, ejsval arg9)
{
    // XXX nanboxing breaks this assert (EJSVAL_IS_FUNCTION(closre));
    ejsval args[] = { arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9 };
#if DEBUG_FUNCTIONS
    ejsval closure_name = _ejs_Function_prototype_toString (_ejs_null, closure, 0, NULL);
    indent('*');
    printf ("invoking %s\n", EJSVAL_TO_STRING(closure_name));
    indent_level += INDENT_AMOUNT;
#endif
    ejsval rv = EJSVAL_TO_FUNC(closure) (EJSVAL_TO_ENV(closure), _this, argc, args);
#if DEBUG_FUNCTIONS
    indent_level -= INDENT_AMOUNT;
    indent(' ');
    printf ("returning from %s\n", EJSVAL_TO_STRING(closure_name));
#endif
    return rv;
}

ejsval
_ejs_invoke_closure_10 (ejsval closure, ejsval _this, int argc, ejsval arg1, ejsval arg2, ejsval arg3, ejsval arg4, ejsval arg5, ejsval arg6, ejsval arg7, ejsval arg8, ejsval arg9, ejsval arg10)
{
    // XXX nanboxing breaks this assert (EJSVAL_IS_FUNCTION(closre));
    ejsval args[] = { arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10 };
#if DEBUG_FUNCTIONS
    ejsval closure_name = _ejs_Function_prototype_toString (NULL, closure, 0, NULL);
    indent('*');
    printf ("invoking %s\n", EJSVAL_TO_STRING(closure_name));
    indent_level += INDENT_AMOUNT;
#endif
    ejsval rv = EJSVAL_TO_FUNC(closure) (EJSVAL_TO_ENV(closure), _this, argc, args);
#if DEBUG_FUNCTIONS
    indent_level -= INDENT_AMOUNT;
    indent(' ');
    printf ("returning from %s\n", EJSVAL_TO_STRING(closure_name));
#endif
    return rv;
}

static ejsval
_ejs_function_specop_get (ejsval obj, ejsval propertyName, EJSBool isCStr)
{
    return _ejs_object_specops.get (obj, propertyName, isCStr);
}

static ejsval
_ejs_function_specop_get_own_property (ejsval obj, ejsval propertyName)
{
    return _ejs_object_specops.get_own_property (obj, propertyName);
}

static ejsval
_ejs_function_specop_get_property (ejsval obj, ejsval propertyName)
{
    return _ejs_object_specops.get_property (obj, propertyName);
}

static void
_ejs_function_specop_put (ejsval obj, ejsval propertyName, ejsval val, EJSBool flag)
{
    _ejs_object_specops.put (obj, propertyName, val, flag);
}

static EJSBool
_ejs_function_specop_can_put (ejsval obj, ejsval propertyName)
{
    return _ejs_object_specops.can_put (obj, propertyName);
}

static EJSBool
_ejs_function_specop_has_property (ejsval obj, ejsval propertyName)
{
    return _ejs_object_specops.has_property (obj, propertyName);
}

static EJSBool
_ejs_function_specop_delete (ejsval obj, ejsval propertyName, EJSBool flag)
{
    return _ejs_object_specops._delete (obj, propertyName, flag);
}

static ejsval
_ejs_function_specop_default_value (ejsval obj, const char *hint)
{
    return _ejs_object_specops.default_value (obj, hint);
}

static void
_ejs_function_specop_define_own_property (ejsval obj, ejsval propertyName, ejsval propertyDescriptor, EJSBool flag)
{
    _ejs_object_specops.define_own_property (obj, propertyName, propertyDescriptor, flag);
}

static void
_ejs_function_specop_finalize (EJSObject* obj)
{
    _ejs_object_specops.finalize (obj);
}

static void
_ejs_function_specop_scan (EJSObject* obj, EJSValueFunc scan_func)
{
    EJSFunction* f = (EJSFunction*)obj;
    scan_func (f->name);
    scan_func (f->env);
    if (f->bound_this)
        scan_func (f->_this);
    _ejs_object_specops.scan (obj, scan_func);
}
