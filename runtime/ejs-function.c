/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

//#define DEBUG_FUNCTIONS 1

#include <assert.h>

#include "ejs-value.h"
#include "ejs-object.h"
#include "ejs-function.h"
#include "ejs-array.h"

static ejsval  _ejs_function_specop_get (ejsval obj, ejsval propertyName, EJSBool isCStr);
static EJSPropertyDesc* _ejs_function_specop_get_own_property (ejsval obj, ejsval propertyName);
static EJSPropertyDesc* _ejs_function_specop_get_property (ejsval obj, ejsval propertyName);
static void    _ejs_function_specop_put (ejsval obj, ejsval propertyName, ejsval val, EJSBool flag);
static EJSBool _ejs_function_specop_can_put (ejsval obj, ejsval propertyName);
static EJSBool _ejs_function_specop_has_property (ejsval obj, ejsval propertyName);
static EJSBool _ejs_function_specop_delete (ejsval obj, ejsval propertyName, EJSBool flag);
static ejsval  _ejs_function_specop_default_value (ejsval obj, const char *hint);
static EJSBool _ejs_function_specop_define_own_property (ejsval obj, ejsval propertyName, EJSPropertyDesc* propertyDescriptor, EJSBool flag);
static EJSObject* _ejs_function_specop_allocate ();
static void    _ejs_function_specop_finalize (EJSObject* obj);
static void    _ejs_function_specop_scan (EJSObject* obj, EJSValueFunc scan_func);

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

    _ejs_function_specop_allocate,
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
    
    _ejs_init_object ((EJSObject*)rv, _ejs_Function__proto__, &_ejs_function_specops);

    rv->name = name;
    rv->func = func;
    rv->env = env;

    ejsval fun = OBJECT_TO_EJSVAL((EJSObject*)rv);

    // ECMA262: 15.3.2.1
    ejsval fun_proto = _ejs_object_new (_ejs_Object_prototype, &_ejs_object_specops);

    _ejs_object_setprop (fun_proto, _ejs_atom_constructor,  fun);
    _ejs_object_define_value_property (fun, _ejs_atom_prototype, fun_proto, EJS_FALSE, EJS_FALSE, EJS_FALSE);

    return fun;
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


ejsval _ejs_Function__proto__;
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
    ejsval func = _this;

    /* 1. If IsCallable(func) is false, then throw a TypeError exception. */
    if (!EJSVAL_IS_FUNCTION(_this)) {
        printf ("throw TypeError, func is not callable\n");
        EJS_NOT_IMPLEMENTED();
    }

    ejsval thisArg = _ejs_undefined;
    ejsval argArray = _ejs_undefined;
    if (argc > 0) thisArg = args[0];
    if (argc > 1) argArray = args[1];

    /* 2. If argArray is null or undefined, then */
    if (EJSVAL_IS_UNDEFINED(argArray) || EJSVAL_IS_NULL(argArray)) {
        /*    a. Return the result of calling the [[Call]] internal method of func, providing thisArg as the this value */
        /*       and an empty list of arguments. */
        return _ejs_invoke_closure_0 (func, thisArg, 0);
    }
    /* 3. If Type(argArray) is not Object, then throw a TypeError exception. */
    if (!EJSVAL_IS_OBJECT(argArray)) {
        printf ("throw TypeError, argArray is not an object\n");
        EJS_NOT_IMPLEMENTED();
    }
    EJSObject* argArray_ = EJSVAL_TO_OBJECT(argArray);

    /* 4. Let len be the result of calling the [[Get]] internal method of argArray with argument "length". */
    ejsval len = OP(argArray_,get) (argArray, _ejs_atom_length, EJS_FALSE);

    /* 5. Let n be ToUint32(len). */
    uint32_t n = (uint32_t)EJSVAL_TO_NUMBER(len);

    /* 6. Let argList be an empty List. */
    ejsval* argList = (ejsval*)malloc(sizeof(ejsval) * n);

    /* 7. Let index be 0. */
    int index = 0;

    /* 8. Repeat while index < n */
    while (index < n) {
        /*    a. Let indexName be ToString(index). */
        ejsval indexName = NUMBER_TO_EJSVAL(index);
        /*    b. Let nextArg be the result of calling the [[Get]] internal method of argArray with indexName as the  */
        /*       argument. */
        ejsval nextArg = OP(argArray_,get)(argArray, indexName, EJS_FALSE);
        /*    c. Append nextArg as the last element of argList. */
        argList[index] = nextArg;
        /*    d. Set index to index + 1. */
        ++index;
    }

    /* 9. Return the result of calling the [[Call]] internal method of func, providing thisArg as the this value and */
    /*    argList as the list of arguments. */
    ejsval rv = EJSVAL_TO_FUNC(func) (EJSVAL_TO_ENV(func), thisArg, n, argList);

    free (argList);
    return rv;
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
    EJS_NOT_IMPLEMENTED();
}

ejsval
_ejs_Function_empty (ejsval env, ejsval _this, int argc, ejsval *args)
{
    return _ejs_undefined;
}

static void
_ejs_function_init_proto()
{
    _ejs_gc_add_named_root (_ejs_Function__proto__);

    // Function.__proto__ = function () { return undefined; }

    EJSFunction* __proto__ = _ejs_gc_new(EJSFunction);
    __proto__->name = _ejs_atom_Empty;
    __proto__->func = _ejs_Function_empty;
    __proto__->env = _ejs_null;

    _ejs_init_object ((EJSObject*)__proto__, _ejs_Object_prototype, &_ejs_function_specops);

    _ejs_Function__proto__ = OBJECT_TO_EJSVAL((EJSObject*)__proto__);
}

void
_ejs_function_init(ejsval global)
{
    START_SHADOW_STACK_FRAME;

    _ejs_function_init_proto();

    ADD_STACK_ROOT(ejsval, tmpobj, _ejs_function_new (_ejs_null, _ejs_atom_Function, (EJSClosureFunc)_ejs_Function_impl));
    _ejs_Function = tmpobj;

    // ECMA262 15.3.3.1
    _ejs_object_define_value_property (_ejs_Function, _ejs_atom_prototype, _ejs_Function__proto__, EJS_FALSE, EJS_FALSE, EJS_FALSE);
    // ECMA262 15.3.3.2
    _ejs_object_define_value_property (_ejs_Function, _ejs_atom_length, NUMBER_TO_EJSVAL(1), EJS_FALSE, EJS_FALSE, EJS_FALSE);

#define PROTO_METHOD(x) EJS_INSTALL_FUNCTION(_ejs_Function__proto__, EJS_STRINGIFY(x), _ejs_Function_prototype_##x)

    PROTO_METHOD(toString);
    PROTO_METHOD(apply);
    PROTO_METHOD(call);
    PROTO_METHOD(bind);

#undef PROTOTYPE_METHOD

    _ejs_object_setprop (global, _ejs_atom_Function, _ejs_Function);

    END_SHADOW_STACK_FRAME;
}


ejsval
_ejs_invoke_closure_0 (ejsval closure, ejsval _this, int argc)
{
    if (!EJSVAL_IS_FUNCTION(closure)) {
        printf ("TypeError, object not a function\n");
        EJS_NOT_IMPLEMENTED();
    }

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
    if (!EJSVAL_IS_FUNCTION(closure)) {
        printf ("TypeError, object not a function\n");
        EJS_NOT_IMPLEMENTED();
    }

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
    if (!EJSVAL_IS_FUNCTION(closure)) {
        printf ("TypeError, object not a function\n");
        EJS_NOT_IMPLEMENTED();
    }

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
    if (!EJSVAL_IS_FUNCTION(closure)) {
        printf ("TypeError, object not a function\n");
        EJS_NOT_IMPLEMENTED();
    }

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
    if (!EJSVAL_IS_FUNCTION(closure)) {
        printf ("TypeError, object not a function\n");
        EJS_NOT_IMPLEMENTED();
    }

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
    if (!EJSVAL_IS_FUNCTION(closure)) {
        printf ("TypeError, object not a function\n");
        EJS_NOT_IMPLEMENTED();
    }

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
    if (!EJSVAL_IS_FUNCTION(closure)) {
        printf ("TypeError, object not a function\n");
        EJS_NOT_IMPLEMENTED();
    }

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
    if (!EJSVAL_IS_FUNCTION(closure)) {
        printf ("TypeError, object not a function\n");
        EJS_NOT_IMPLEMENTED();
    }

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
    if (!EJSVAL_IS_FUNCTION(closure)) {
        printf ("TypeError, object not a function\n");
        EJS_NOT_IMPLEMENTED();
    }

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
    if (!EJSVAL_IS_FUNCTION(closure)) {
        printf ("TypeError, object not a function\n");
        EJS_NOT_IMPLEMENTED();
    }

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
    if (!EJSVAL_IS_FUNCTION(closure)) {
        printf ("TypeError, object not a function\n");
        EJS_NOT_IMPLEMENTED();
    }

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

static EJSPropertyDesc*
_ejs_function_specop_get_own_property (ejsval obj, ejsval propertyName)
{
    return _ejs_object_specops.get_own_property (obj, propertyName);
}

static EJSPropertyDesc*
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

static EJSBool
_ejs_function_specop_define_own_property (ejsval obj, ejsval propertyName, EJSPropertyDesc* propertyDescriptor, EJSBool flag)
{
    return _ejs_object_specops.define_own_property (obj, propertyName, propertyDescriptor, flag);
}

static EJSObject*
_ejs_function_specop_allocate ()
{
    EJS_NOT_IMPLEMENTED();
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
