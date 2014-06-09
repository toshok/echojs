/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

//#define DEBUG_FUNCTIONS 1

#include <string.h>

#include "ejs-value.h"
#include "ejs-ops.h"
#include "ejs-object.h"
#include "ejs-function.h"
#include "ejs-proxy.h"
#include "ejs-array.h"
#include "ejs-error.h"
#include "ejs-string.h"
#include "ejs-symbol.h"

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
    
    _ejs_init_object ((EJSObject*)rv, _ejs_Function_prototype, &_ejs_Function_specops);

    rv->func = func;
    rv->env = env;

    ejsval fun = OBJECT_TO_EJSVAL(rv);

    // ECMA262: 15.3.2.1
    ejsval fun_proto = _ejs_object_new (_ejs_Object_prototype, &_ejs_Object_specops);
    _ejs_object_define_value_property (fun, _ejs_atom_prototype, fun_proto, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_CONFIGURABLE | EJS_PROP_WRITABLE);

    _ejs_object_define_value_property (fun_proto, _ejs_atom_constructor, fun, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_CONFIGURABLE | EJS_PROP_WRITABLE);
    _ejs_object_define_value_property (fun, _ejs_atom_name, name, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_CONFIGURABLE | EJS_PROP_NOT_WRITABLE);
    return fun;
}

ejsval
_ejs_function_new_without_proto (ejsval env, ejsval name, EJSClosureFunc func)
{
    EJSFunction *rv = _ejs_gc_new(EJSFunction);
    
    _ejs_init_object ((EJSObject*)rv, _ejs_Function_prototype, &_ejs_Function_specops);

    rv->func = func;
    rv->env = env;

    ejsval fun = OBJECT_TO_EJSVAL(rv);

    _ejs_object_define_value_property (fun, _ejs_atom_name, name, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_CONFIGURABLE | EJS_PROP_NOT_WRITABLE);
    return fun;
}

ejsval
_ejs_function_new_utf8_with_proto (ejsval env, const char* name, EJSClosureFunc func, ejsval prototype)
{
    ejsval function_name = _ejs_string_new_utf8 (name);
    EJSFunction *rv = _ejs_gc_new(EJSFunction);
    
    _ejs_init_object ((EJSObject*)rv, _ejs_Function_prototype, &_ejs_Function_specops);

    rv->func = func;
    rv->env = env;

    ejsval fun = OBJECT_TO_EJSVAL(rv);

    _ejs_object_define_value_property (fun, _ejs_atom_name, function_name, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_CONFIGURABLE | EJS_PROP_NOT_WRITABLE);
    _ejs_object_define_value_property (fun, _ejs_atom_prototype, prototype, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_CONFIGURABLE | EJS_PROP_WRITABLE);

    return fun;
}


ejsval
_ejs_function_new_native (ejsval env, ejsval name, EJSClosureFunc func)
{
    EJSFunction *rv = _ejs_gc_new(EJSFunction);
    
    _ejs_init_object ((EJSObject*)rv, _ejs_Function_prototype, &_ejs_Function_specops);

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
    ejsval function_name = _ejs_string_new_utf8 (name);
    ejsval rv = _ejs_function_new (env, function_name, func);

    return rv;
}


ejsval _ejs_Function_prototype EJSVAL_ALIGNMENT;
ejsval _ejs_Function EJSVAL_ALIGNMENT;

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
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Function.prototype.toString is not generic.");

    ejsval func_name = _ejs_object_getprop (_this, _ejs_atom_name);

    char *utf8_funcname = ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(func_name));
    
    snprintf (terrible_fixed_buffer, sizeof (terrible_fixed_buffer), "function %s() {}", utf8_funcname);

    free (utf8_funcname);

    return _ejs_string_new_utf8(terrible_fixed_buffer);
}

// ECMA262 15.3.4.3
static ejsval
_ejs_Function_prototype_apply (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval func = _this;

    /* 1. If IsCallable(func) is false, then throw a TypeError exception. */
    if (!EJSVAL_IS_CALLABLE(_this)) {
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
        return _ejs_invoke_closure (func, thisArg, 0, NULL);
    }
    /* 3. If Type(argArray) is not Object, then throw a TypeError exception. */
    if (!EJSVAL_IS_OBJECT(argArray)) {
        printf ("throw TypeError, argArray is not an object\n");
        EJS_NOT_IMPLEMENTED();
    }
    EJSObject* argArray_ = EJSVAL_TO_OBJECT(argArray);

    /* 4. Let len be the result of calling the [[Get]] internal method of argArray with argument "length". */
    ejsval len = OP(argArray_,Get) (argArray, _ejs_atom_length, argArray);

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
        ejsval nextArg = OP(argArray_,Get)(argArray, indexName, argArray);
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
    // XXX nanboxing breaks this EJS_ASSERT (EJSVAL_IS_FUNCTION(_this));

    ejsval thisArg = _ejs_undefined;
  
    if (argc > 0) {
        thisArg = args[0];
        args = &args[1];
        argc --;
    }

    ejsval rv = EJSVAL_TO_FUNC(_this) (EJSVAL_TO_ENV(_this), thisArg, argc, argc == 0 ? NULL : args);

    return rv;
}

// ECMA262 15.3.4.5
static ejsval
_ejs_Function_prototype_bind (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    /* 1. Let Target be the this value. */
    ejsval Target = _this;

    /* 2. If IsCallable(Target) is false, throw a TypeError exception. */
    if (!EJSVAL_IS_CALLABLE(Target)) {
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "object not a function");
    }

    if (argc == 0)
        return _this;

    ejsval thisArg = _ejs_undefined;
    if (argc >= 1)
        thisArg = args[0];

    EJSFunction *TargetFunc = (EJSFunction*)EJSVAL_TO_OBJECT(Target);

    /* 3. Let A be a new (possibly empty) internal list of all of the argument values provided after thisArg (arg1, arg2 etc), in order. */
    int bound_argc = 0;
    ejsval *bound_args = NULL;
    if (argc > 1) {
        bound_argc = argc-1;
        bound_args = malloc(sizeof(ejsval) * bound_argc);
        memcpy (bound_args, args, sizeof(ejsval) * bound_argc);
    }

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
    F_->bound_argc = bound_argc;
    F_->bound_args = bound_args;

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
_ejs_Function_empty (ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
    return _ejs_undefined;
}

static ejsval
_ejs_Function_create(ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
    _ejs_throw_nativeerror_utf8 (EJS_ERROR, "ejs doesn't support dynamic creation of functions");
}

static ejsval
_ejs_Function_prototype_create(ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
    ejsval F = _this;

    if (!EJSVAL_IS_CONSTRUCTOR(F)) 
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "'this' in Function.prototype[Symbol.create] is not a constructor");
        
    EJSObject* F_ = EJSVAL_TO_OBJECT(F);

    ejsval proto = OP(F_,Get)(F, _ejs_atom_prototype, F);
    if (EJSVAL_IS_UNDEFINED(proto)) {
        proto = _ejs_Function_prototype;
    }

    if (!EJSVAL_IS_OBJECT(proto)) {
        EJS_NOT_IMPLEMENTED(); // cross-realm doesn't exist in ejs yet
    }

    EJSObject* obj = (EJSObject*)_ejs_gc_new (EJSObject);
    _ejs_init_object (obj, proto, &_ejs_Object_specops);
    return OBJECT_TO_EJSVAL(obj);
}

static void
_ejs_function_init_proto()
{
    _ejs_gc_add_root (&_ejs_Function_prototype);

    // Function.prototype = function () { return undefined; }

    EJSFunction* proto = _ejs_gc_new(EJSFunction);
    _ejs_Function_prototype = OBJECT_TO_EJSVAL(proto);
    _ejs_init_object ((EJSObject*)proto, _ejs_Object_prototype, &_ejs_Function_specops);
    proto->func = _ejs_Function_empty;
    proto->env = _ejs_null;

    _ejs_object_define_value_property (OBJECT_TO_EJSVAL(proto), _ejs_atom_name, _ejs_atom_empty, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_CONFIGURABLE | EJS_PROP_NOT_WRITABLE);
}

void
_ejs_function_init(ejsval global)
{
    trace = getenv("EJS_TRACE") != NULL;

    _ejs_function_init_proto();

    _ejs_Function = _ejs_function_new_without_proto (_ejs_null, _ejs_atom_Function, (EJSClosureFunc)_ejs_Function_impl);
    _ejs_object_setprop (global, _ejs_atom_Function, _ejs_Function);

    // ECMA262 15.3.3.1
    _ejs_object_define_value_property (_ejs_Function, _ejs_atom_prototype, _ejs_Function_prototype, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_CONFIGURABLE | EJS_PROP_NOT_WRITABLE);
    // ECMA262 15.3.3.2
    _ejs_object_define_value_property (_ejs_Function, _ejs_atom_length, NUMBER_TO_EJSVAL(1), EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_CONFIGURABLE | EJS_PROP_NOT_WRITABLE);

#define PROTO_METHOD(x) EJS_INSTALL_ATOM_FUNCTION_FLAGS(_ejs_Function_prototype, x, _ejs_Function_prototype_##x, EJS_PROP_NOT_ENUMERABLE)

    PROTO_METHOD(toString);
    PROTO_METHOD(apply);
    PROTO_METHOD(call);
    PROTO_METHOD(bind);

#undef PROTO_METHOD

}

void
_ejs_function_add_symbols()
{
    EJS_INSTALL_SYMBOL_FUNCTION_FLAGS (_ejs_Function, create, _ejs_Function_create, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_CONFIGURABLE | EJS_PROP_NOT_WRITABLE);

    EJS_INSTALL_SYMBOL_FUNCTION_FLAGS (_ejs_Function_prototype, create, _ejs_Function_prototype_create, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_CONFIGURABLE | EJS_PROP_NOT_WRITABLE);
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

ejsval
_ejs_invoke_closure (ejsval closure, ejsval _this, uint32_t argc, ejsval* args)
{
    if (!EJSVAL_IS_FUNCTION(closure)) {
#if DEBUG_LAST_LOOKUP
        extern jschar* last_lookup;
        if (last_lookup) {
            char *last_utf8 = ucs2_to_utf8(last_lookup);
            _ejs_log ("last property lookup was for: %s\n", last_utf8);
            free (last_utf8);
        }
#endif
        
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "object not a function");
    }

    if (!EJSVAL_IS_NULL_OR_UNDEFINED(_this) && EJSVAL_IS_PRIMITIVE(_this)) {
        _this = ToObject(_this);
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
_ejs_invoke_closure_catch(ejsval* retval, ejsval closure, ejsval _this, uint32_t argc, ejsval* args)
{
    // XXX this needs to be rewritten in llvm-ir to catch ejs exceptions thrown during the execution of @closure
    // see https://github.com/toshok/echo-js/issues/31
    *retval = _ejs_invoke_closure(closure, _this, argc, args);
    return EJS_TRUE;
}

EJSBool
_ejs_decompose_closure (ejsval closure, EJSClosureFunc* func, ejsval* env,
                        ejsval *_this)
{
    if (!EJSVAL_IS_FUNCTION(closure))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "object not a function");

    EJSFunction *fun = (EJSFunction*)EJSVAL_TO_OBJECT(closure);

    if (fun->bound_argc > 0)
        return EJS_FALSE;

    *func = fun->func;
    *env = fun->env;

    if (fun->bound)
        *_this = fun->bound_this;

    return EJS_TRUE;
}

// ECMA262: 15.3.5.3
static EJSBool
_ejs_function_specop_has_instance (ejsval F, ejsval V)
{
    /* 1. If V is not an object, return false. */
    if (!EJSVAL_IS_OBJECT(V))
        return EJS_FALSE;

    /* 2. Let O be the result of calling the [[Get]] internal method of F with property name "prototype". */
    ejsval O = OP(EJSVAL_TO_OBJECT(F),Get)(F, _ejs_atom_prototype, F);

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
    // we shouldn't ever get here..
    EJS_NOT_IMPLEMENTED();
}

static void
_ejs_function_specop_finalize (EJSObject* obj)
{
    EJSFunction* f = (EJSFunction*)obj;
    if (f->bound_args)
        free (f->bound_args);
    _ejs_Object_specops.Finalize (obj);
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

    _ejs_Object_specops.Scan (obj, scan_func);
}

EJS_DEFINE_CLASS(Function,
                 OP_INHERIT, // [[GetPrototypeOf]]
                 OP_INHERIT, // [[SetPrototypeOf]]
                 OP_INHERIT, // [[IsExtensible]]
                 OP_INHERIT, // [[PreventExtensions]]
                 OP_INHERIT, // [[GetOwnProperty]]
                 OP_INHERIT, // [[DefineOwnProperty]]
                 OP_INHERIT, // [[HasProperty]]
                 OP_INHERIT, // [[Get]]
                 OP_INHERIT, // [[Set]]
                 OP_INHERIT, // [[Delete]]
                 OP_INHERIT, // [[Enumerate]]
                 OP_INHERIT, // [[OwnPropertyKeys]]
                 _ejs_function_specop_allocate,
                 _ejs_function_specop_finalize,
                 _ejs_function_specop_scan
                 )

