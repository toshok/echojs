/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

//#define DEBUG_FUNCTIONS 1

#include <assert.h>
#include <string.h>

#include "ejs-value.h"
#include "ejs-object.h"
#include "ejs-function.h"
#include "ejs-array.h"
#include "ejs-error.h"
#include "ejs-string.h"

static ejsval  _ejs_function_specop_get (ejsval obj, ejsval propertyName);
static EJSPropertyDesc* _ejs_function_specop_get_own_property (ejsval obj, ejsval propertyName);
static EJSPropertyDesc* _ejs_function_specop_get_property (ejsval obj, ejsval propertyName);
static void    _ejs_function_specop_put (ejsval obj, ejsval propertyName, ejsval val, EJSBool flag);
static EJSBool _ejs_function_specop_can_put (ejsval obj, ejsval propertyName);
static EJSBool _ejs_function_specop_has_property (ejsval obj, ejsval propertyName);
static EJSBool _ejs_function_specop_delete (ejsval obj, ejsval propertyName, EJSBool flag);
static ejsval  _ejs_function_specop_default_value (ejsval obj, const char *hint);
static EJSBool _ejs_function_specop_define_own_property (ejsval obj, ejsval propertyName, EJSPropertyDesc* propertyDescriptor, EJSBool flag);
static EJSBool _ejs_function_specop_has_instance (ejsval obj, ejsval lval);
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
    _ejs_function_specop_has_instance,

    _ejs_function_specop_allocate,
    _ejs_function_specop_finalize,
    _ejs_function_specop_scan
};

EJSBool trace = EJS_FALSE;

static int indent_level = 0;
#define INDENT_AMOUNT 1

static void indent(char ch)
{
    int i;
    for (i = 0; i < indent_level; i ++)
        putchar (ch);
}

ejsval
_ejs_function_new (ejsval env, ejsval name, EJSClosureFunc func)
{
    EJSFunction *rv = _ejs_gc_new(EJSFunction);
    
    _ejs_init_object ((EJSObject*)rv, _ejs_Function__proto__, &_ejs_function_specops);

    rv->func = func;
    rv->env = env;

    ejsval fun = OBJECT_TO_EJSVAL(rv);

    // ECMA262: 15.3.2.1
    ejsval fun_proto = _ejs_object_new (_ejs_Object_prototype, &_ejs_object_specops);

    _ejs_object_setprop (fun_proto, _ejs_atom_constructor,  fun);
    _ejs_object_define_value_property (fun, _ejs_atom_prototype, fun_proto, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_CONFIGURABLE | EJS_PROP_WRITABLE);
    _ejs_object_define_value_property (fun, _ejs_atom_name, name, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_CONFIGURABLE | EJS_PROP_NOT_WRITABLE);
    return fun;
}

ejsval
_ejs_function_new_native (ejsval env, ejsval name, EJSClosureFunc func)
{
    EJSFunction *rv = _ejs_gc_new(EJSFunction);
    
    _ejs_init_object ((EJSObject*)rv, _ejs_Function__proto__, &_ejs_function_specops);

    rv->func = func;
    rv->env = env;

    ejsval fun = OBJECT_TO_EJSVAL(rv);

    _ejs_object_define_value_property (fun, _ejs_atom_name, name, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_CONFIGURABLE | EJS_PROP_NOT_WRITABLE);

    return OBJECT_TO_EJSVAL(rv);
}

ejsval
_ejs_function_new_anon (ejsval env, EJSClosureFunc func)
{
    return _ejs_function_new (env, _ejs_atom_empty, func);
}

ejsval
_ejs_function_new_utf8 (ejsval env, const char *name, EJSClosureFunc func)
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
_ejs_Function_impl (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    printf ("Function() called either as a function or a constructor is not supported in ejs\n");
    abort();
}


// ECMA262 15.3.4.2
static ejsval
_ejs_Function_prototype_toString (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    char terrible_fixed_buffer[256];

    if (!EJSVAL_IS_FUNCTION(_this))
        _ejs_throw_nativeerror (EJS_TYPE_ERROR, "Function.prototype.toString is not generic.");

    START_SHADOW_STACK_FRAME;

    ADD_STACK_ROOT(ejsval, func_name, _ejs_object_getprop (_this, _ejs_atom_name));

    char *utf8_funcname = ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(func_name));
    
    snprintf (terrible_fixed_buffer, sizeof (terrible_fixed_buffer), "function %s() {}", utf8_funcname);

    free (utf8_funcname);

    END_SHADOW_STACK_FRAME;
    return _ejs_string_new_utf8(terrible_fixed_buffer);
}

// ECMA262 15.3.4.3
static ejsval
_ejs_Function_prototype_apply (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
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
    ejsval len = OP(argArray_,get) (argArray, _ejs_atom_length);

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
        ejsval nextArg = OP(argArray_,get)(argArray, indexName);
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
_ejs_Function_prototype_call (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
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
_ejs_Function_prototype_bind (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    /* 1. Let Target be the this value. */
    ejsval Target = _this;

    /* 2. If IsCallable(Target) is false, throw a TypeError exception. */
    if (!EJSVAL_IS_FUNCTION(Target)) {
        _ejs_throw_nativeerror (EJS_TYPE_ERROR, "object not a function");
    }

    if (argc == 0)
        return _this;

    ejsval thisArg = _ejs_undefined;
    if (argc >= 1)
        thisArg = args[0];
    if (argc >= 2)
        EJS_NOT_IMPLEMENTED();

    EJSFunction *TargetFunc = (EJSFunction*)EJSVAL_TO_OBJECT(Target);

    /* 3. Let A be a new (possibly empty) internal list of all of the argument values provided after thisArg (arg1, arg2 etc), in order. */
    // XXX

    /* 4. Let F be a new native ECMAScript object . */

    ejsval F = _ejs_function_new (TargetFunc->env, _ejs_atom_empty, TargetFunc->func);
    EJSFunction *F_ = (EJSFunction*)EJSVAL_TO_OBJECT(F);

    F_->bound = EJS_TRUE;

    /* 5. Set all the internal methods, except for [[Get]], of F as specified in 8.12. */
    /* 6. Set the [[Get]] internal property of F as specified in 15.3.5.4. */
    /* 7. Set the [[TargetFunction]] internal property of F to Target. */
    /* 8. Set the [[BoundThis]] internal property of F to the value of thisArg. */
    F_->bound_this = thisArg;

    /* 9. Set the [[BoundArgs]] internal property of F to A. */
    /* 10. Set the [[Class]] internal property of F to "Function". */
    /* 11. Set the [[Prototype]] internal property of F to the standard built-in Function prototype object as specified in 15.3.3.1. */
    /* 12. Set the [[Call]] internal property of F as described in 15.3.4.5.1. */
    /* 13. Set the [[Construct]] internal property of F as described in 15.3.4.5.2. */
    /* 14. Set the [[HasInstance]] internal property of F as described in 15.3.4.5.3. */
    /* 15. If the [[Class]] internal property of Target is "Function", then */
    /*     a. Let L be the length property of Target minus the length of A. */
    /*     b. Set the length own property of F to either 0 or L, whichever is larger.  */
    /* 16. Else set the length own property of F to 0. */
    /* 17. Set the attributes of the length own property of F to the values specified in 15.3.5.1. */
    /* 18. Set the [[Extensible]] internal property of F to true. */
    /* 19. Let thrower be the [[ThrowTypeError]] function Object (13.2.3). */
    /* 20. Call the [[DefineOwnProperty]] internal method of F with arguments "caller", PropertyDescriptor  */
    /*     {[[Get]]: thrower, [[Set]]: thrower, [[Enumerable]]: false, [[Configurable]]: false}, and false. */
    /* 21. Call the [[DefineOwnProperty]] internal method of F with arguments "arguments", PropertyDescriptor */
    /*     {[[Get]]: thrower, [[Set]]: thrower, [[Enumerable]]: false, [[Configurable]]: false}, and false. */
    /* 22. Return F. */
    return F;
}

ejsval
_ejs_Function_empty (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    return _ejs_undefined;
}

static void
_ejs_function_init_proto()
{
    _ejs_gc_add_named_root (_ejs_Function__proto__);

    // Function.__proto__ = function () { return undefined; }

    EJSFunction* __proto__ = _ejs_gc_new(EJSFunction);

    __proto__->func = _ejs_Function_empty;
    __proto__->env = _ejs_null;

    _ejs_init_object ((EJSObject*)__proto__, _ejs_Object_prototype, &_ejs_function_specops);

    _ejs_object_define_value_property (OBJECT_TO_EJSVAL(__proto__), _ejs_atom_name, _ejs_atom_empty, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_CONFIGURABLE | EJS_PROP_NOT_WRITABLE);

    _ejs_Function__proto__ = OBJECT_TO_EJSVAL(__proto__);
}

void
_ejs_function_init(ejsval global)
{
    trace = getenv("EJS_TRACE") != NULL;

    START_SHADOW_STACK_FRAME;

    _ejs_function_init_proto();

    ADD_STACK_ROOT(ejsval, tmpobj, _ejs_function_new (_ejs_null, _ejs_atom_Function, (EJSClosureFunc)_ejs_Function_impl));
    _ejs_Function = tmpobj;

    // ECMA262 15.3.3.1
    _ejs_object_define_value_property (_ejs_Function, _ejs_atom_prototype, _ejs_Function__proto__, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_CONFIGURABLE | EJS_PROP_NOT_WRITABLE);
    // ECMA262 15.3.3.2
    _ejs_object_define_value_property (_ejs_Function, _ejs_atom_length, NUMBER_TO_EJSVAL(1), EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_CONFIGURABLE | EJS_PROP_NOT_WRITABLE);

#define PROTO_METHOD(x) EJS_INSTALL_ATOM_FUNCTION(_ejs_Function__proto__, x, _ejs_Function_prototype_##x)

    PROTO_METHOD(toString);
    PROTO_METHOD(apply);
    PROTO_METHOD(call);
    PROTO_METHOD(bind);

#undef PROTO_METHOD

    _ejs_object_setprop (global, _ejs_atom_Function, _ejs_Function);

    END_SHADOW_STACK_FRAME;
}

#define DEBUG_FUNCTION_ENTER(x) EJS_MACRO_START                         \
    if (trace) {                                                        \
        ejsval closure_name = _ejs_Function_prototype_toString (_ejs_null, x, 0, NULL); \
        char *closure_utf8 = ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(closure_name)); \
        indent('*');                                                    \
        printf ("invoking %s\n", closure_utf8);                         \
        free (closure_utf8);                                            \
        indent_level += INDENT_AMOUNT;                                  \
    }                                                                   \
    EJS_MACRO_END
#define DEBUG_FUNCTION_EXIT(x) EJS_MACRO_START                          \
    if (trace) {                                                        \
        ejsval closure_name = _ejs_Function_prototype_toString (_ejs_null, x, 0, NULL); \
        char *closure_utf8 = ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(closure_name)); \
        indent_level -= INDENT_AMOUNT;                                  \
        indent(' ');                                                    \
        printf ("returning from %s\n", closure_utf8);                   \
        free (closure_utf8);                                            \
    }                                                                   \
    EJS_MACRO_END

#define BUILD_INVOKE_CLOSURE(_closure, _thisArg, _argc, ...) EJS_MACRO_START \
    ejsval args[] = { __VA_ARGS__ };                                    \
    return _ejs_invoke_closure (_closure, _thisArg, argc, args);        \
    EJS_MACRO_END

ejsval
_ejs_invoke_closure (ejsval closure, ejsval _this, uint32_t argc, ejsval* args)
{
    if (!EJSVAL_IS_FUNCTION(closure)) {
#if DEBUG_LAST_LOOKUP
        extern jschar* last_lookup;
        if (last_lookup) {
            char *last_utf8 = ucs2_to_utf8(last_lookup);
            fprintf (stderr, "last property lookup was for: %s\n", last_utf8);
            free (last_utf8);
        }
#endif
        
        _ejs_throw_nativeerror (EJS_TYPE_ERROR, "object not a function");
    }

    EJSFunction *fun = (EJSFunction*)EJSVAL_TO_OBJECT(closure);
    if (!fun->bound)
        return fun->func (fun->env, _this, argc, args);


    if (fun->bound_argc > 0) {
        ejsval* new_args = (ejsval*)malloc(sizeof(ejsval) * fun->bound_argc + argc);
        memmove (new_args, fun->bound_args, sizeof(ejsval) * fun->bound_argc);
        memmove (&new_args[fun->bound_argc], args, argc);
        args = new_args;
        argc += fun->bound_argc;
    }

    DEBUG_FUNCTION_ENTER (closure);
    ejsval rv = fun->func (fun->env, fun->bound_this, argc, args);
    DEBUG_FUNCTION_EXIT (closure);

    if (fun->bound_argc > 0)
        free (args);

    return rv;
}

EJSBool
_ejs_decompose_closure (ejsval closure, EJSClosureFunc* func, ejsval* env,
                        ejsval *_this)
{
    if (!EJSVAL_IS_FUNCTION(closure)) {
        printf ("TypeError, object not a function\n");
        EJS_NOT_IMPLEMENTED();
    }

    EJSFunction *fun = (EJSFunction*)EJSVAL_TO_OBJECT(closure);

    if (fun->bound_argc > 0)
        return EJS_FALSE;

    *func = fun->func;
    *env = fun->env;

    if (fun->bound)
        *_this = fun->bound_this;

    return EJS_TRUE;
}

ejsval
_ejs_invoke_closure_0 (ejsval closure, ejsval _this, uint32_t argc)
{
    return _ejs_invoke_closure (closure, _this, argc, NULL);
}

ejsval
_ejs_invoke_closure_1 (ejsval closure, ejsval _this, uint32_t argc, ejsval arg1)
{
    BUILD_INVOKE_CLOSURE(closure, _this, argc, arg1);
}

ejsval
_ejs_invoke_closure_2 (ejsval closure, ejsval _this, uint32_t argc, ejsval arg1, ejsval arg2)
{
    BUILD_INVOKE_CLOSURE(closure, _this, argc, arg1, arg2);
}

static ejsval
_ejs_function_specop_get (ejsval obj, ejsval propertyName)
{
    return _ejs_object_specops.get (obj, propertyName);
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

// ECMA262: 15.3.5.3
static EJSBool
_ejs_function_specop_has_instance (ejsval F, ejsval V)
{
    /* 1. If V is not an object, return false. */
    if (!EJSVAL_IS_OBJECT(V))
        return EJS_FALSE;

    /* 2. Let O be the result of calling the [[Get]] internal method of F with property name "prototype". */
    ejsval O = OP(EJSVAL_TO_OBJECT(F),get)(F, _ejs_atom_prototype);

    /* 3. If Type(O) is not Object, throw a TypeError exception. */
    if (!EJSVAL_IS_OBJECT(O)) {
        printf ("throw TypeError, O is not an object\n");
        EJS_NOT_IMPLEMENTED();
    }

    /* 4. Repeat */
    while (1) {
        /*    a. Let V be the value of the [[Prototype]] internal property of V. */
        V = EJSVAL_TO_OBJECT(V)->proto;
        /*    b. If V is null, return false. */
        if (EJSVAL_IS_NULL(V)) return EJS_FALSE;
        /*    c. If O and V refer to the same object, return true. */
        if (EJSVAL_EQ(O, V)) return EJS_TRUE;
    }
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
    scan_func (f->env);
    if (f->bound) {
        scan_func (f->bound_this);
        for (int i = 0; i < f->bound_argc; i ++)
            scan_func (f->bound_args[i]);
    }

    _ejs_object_specops.scan (obj, scan_func);
}
