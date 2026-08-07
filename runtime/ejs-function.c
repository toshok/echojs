/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

//#define DEBUG_FUNCTIONS 1

#include <string.h>

#include "ejs-value.h"
#include "ejs-ops.h"
#include "ejs-gc.h"
#include "ejs-object.h"
#include "ejs-shapes.h"
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

// preinterned birth chains for the hottest allocation the runtime
// itself performs: every function gets {prototype(w,c), name(c)} and
// every fresh fun_proto gets {constructor(w,c)}.  Interning once and
// birthing in one step replaces three DefineOwnProperty round-trips
// (transition probe + slot growth each) per closure.
static uint32_t fn_birth_shape;    // root -> prototype -> name
static uint32_t proto_birth_shape; // root -> constructor
static EJSBool birth_shapes_ready;

static void
ensure_birth_shapes (void)
{
    if (birth_shapes_ready)
        return;
    birth_shapes_ready = EJS_TRUE;
    uint32_t s = _ejs_shape_intern_edge (EJS_SHAPE_ROOT, _ejs_atom_prototype,
                                         EJS_SHAPE_REPR_BOXED,
                                         EJS_SHAPE_ATTR_WRITABLE | EJS_SHAPE_ATTR_CONFIGURABLE);
    if (s != EJS_SHAPE_DICT)
        s = _ejs_shape_intern_edge (s, _ejs_atom_name, EJS_SHAPE_REPR_BOXED,
                                    EJS_SHAPE_ATTR_CONFIGURABLE);
    fn_birth_shape = s;
    proto_birth_shape = _ejs_shape_intern_edge (EJS_SHAPE_ROOT, _ejs_atom_constructor,
                                                EJS_SHAPE_REPR_BOXED,
                                                EJS_SHAPE_ATTR_WRITABLE | EJS_SHAPE_ATTR_CONFIGURABLE);
}

ejsval
_ejs_function_new (ejsval env, ejsval name, EJSClosureFunc func)
{
    EJSFunction *rv = _ejs_gc_new(EJSFunction);

    _ejs_init_object ((EJSObject*)rv, _ejs_Function_prototype, &_ejs_Function_specops);

    rv->func = func;
    rv->env = env;

    ejsval fun = OBJECT_TO_EJSVAL(rv);

    rv->constructor_kind = CONSTRUCTOR_KIND_BASE;

    // ECMA262: 15.3.2.1
    ejsval fun_proto = _ejs_object_new (_ejs_Object_prototype, &_ejs_Object_specops);

    ensure_birth_shapes();
    if (fn_birth_shape != EJS_SHAPE_DICT && proto_birth_shape != EJS_SHAPE_DICT
        && EJSVAL_IS_STRING(name)) {
        ejsval fn_vals[2] = { fun_proto, name };
        _ejs_object_birth_shaped (fun, fn_birth_shape, 2, fn_vals);
        _ejs_object_birth_shaped (fun_proto, proto_birth_shape, 1, &fun);
        return fun;
    }

    _ejs_object_define_value_property (fun, _ejs_atom_prototype, fun_proto, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_CONFIGURABLE | EJS_PROP_WRITABLE);

    _ejs_object_define_value_property (fun_proto, _ejs_atom_constructor, fun, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_CONFIGURABLE | EJS_PROP_WRITABLE);
    _ejs_object_define_value_property (fun, _ejs_atom_name, name, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_CONFIGURABLE | EJS_PROP_NOT_WRITABLE);
    return fun;
}

// closure creation for compiled JS functions: same as _ejs_function_new
// plus the spec .length own property (ES6 19.2.4.1: non-writable,
// non-enumerable, configurable)
ejsval
_ejs_function_new_closure (ejsval env, ejsval name, EJSClosureFunc func, uint32_t len)
{
    ejsval fun = _ejs_function_new (env, name, func);
    _ejs_object_define_value_property (fun, _ejs_atom_length, NUMBER_TO_EJSVAL(len),
                                       EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_WRITABLE | EJS_PROP_CONFIGURABLE);
    return fun;
}

ejsval
_ejs_function_new_without_env (ejsval name, EJSClosureFunc func)
{
    return _ejs_function_new (_ejs_undefined, name, func);
}

ejsval
_ejs_function_new_without_proto (ejsval env, ejsval name, EJSClosureFunc func)
{
    EJSFunction *rv = _ejs_gc_new(EJSFunction);
    
    _ejs_init_object ((EJSObject*)rv, _ejs_Function_prototype, &_ejs_Function_specops);

    rv->func = func;
    rv->env = env;
    rv->constructor_kind = CONSTRUCTOR_KIND_BASE;

    ejsval fun = OBJECT_TO_EJSVAL(rv);

    _ejs_object_define_value_property (fun, _ejs_atom_name, name, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_CONFIGURABLE | EJS_PROP_NOT_WRITABLE);
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
    rv->constructor_kind = CONSTRUCTOR_KIND_BASE;

    ejsval fun = OBJECT_TO_EJSVAL(rv);

    _ejs_object_define_value_property (fun, _ejs_atom_name, function_name, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_CONFIGURABLE | EJS_PROP_NOT_WRITABLE);
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
    // builtin methods/accessors are not constructors (spec: no
    // [[Construct]]); real native constructors go through
    // _ejs_function_new_without_proto
    rv->constructor_kind = CONSTRUCTOR_KIND_NONE;

    ejsval fun = OBJECT_TO_EJSVAL(rv);

    _ejs_object_define_value_property (fun, _ejs_atom_name, name, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_CONFIGURABLE | EJS_PROP_NOT_WRITABLE);
    // ES6 19.2.4.1: builtins have an own .length; the spec arity for
    // table-known builtins is stamped later by
    // _ejs_install_builtin_arities
    _ejs_object_define_value_property (fun, _ejs_atom_length, NUMBER_TO_EJSVAL(0),
                                       EJS_PROP_NOT_ENUMERABLE | EJS_PROP_CONFIGURABLE | EJS_PROP_NOT_WRITABLE);

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

static EJS_NATIVE_FUNC(_ejs_Function_impl) {
    printf ("Function() called either as a function or a constructor is not supported in ejs\n");
    abort();
}


// ECMA262 15.3.4.2
static EJS_NATIVE_FUNC(_ejs_Function_prototype_toString) {
    char terrible_fixed_buffer[256];

    if (!EJSVAL_IS_FUNCTION(*_this))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Function.prototype.toString is not generic.");

    ejsval func_name = _ejs_object_getprop (*_this, _ejs_atom_name);

    char *utf8_funcname = ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(func_name));
    
    snprintf (terrible_fixed_buffer, sizeof (terrible_fixed_buffer), "function %s() {}", utf8_funcname);

    free (utf8_funcname);

    return _ejs_string_new_utf8(terrible_fixed_buffer);
}

// ECMA262 15.3.4.3
static EJS_NATIVE_FUNC(_ejs_Function_prototype_apply) {
    ejsval func = *_this;

    /* 1. If IsCallable(func) is false, then throw a TypeError exception. */
    if (!IsCallable(*_this)) {
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "func is not callable");
    }

    ejsval thisArg = _ejs_undefined;
    ejsval argArray = _ejs_undefined;
    if (argc > 0) thisArg = args[0];
    if (argc > 1) argArray = args[1];

    /* 2. If argArray is null or undefined, then */
    if (EJSVAL_IS_UNDEFINED(argArray) || EJSVAL_IS_NULL(argArray)) {
        /*    a. Return the result of calling the [[Call]] internal method of func, providing thisArg as the this value */
        /*       and an empty list of arguments. */
        return _ejs_invoke_closure (func, &thisArg, 0, NULL, _ejs_undefined);
    }
    /* 3. If Type(argArray) is not Object, then throw a TypeError exception. */
    if (!EJSVAL_IS_OBJECT(argArray)) {
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "argArray is not an object");
    }
    EJSObject* argArray_ = EJSVAL_TO_OBJECT(argArray);

    /* 4. Let len be the result of calling the [[Get]] internal method of argArray with argument "length". */
    ejsval len = OP(argArray_,Get) (argArray, _ejs_atom_length, argArray);

    /* 5. Let n be ToUint32(len). */
    uint32_t n = (uint32_t)EJSVAL_TO_NUMBER(len);

    ejsval* argList;
    EJSBool argList_allocated = EJS_FALSE;

    if (EJSVAL_IS_DENSE_ARRAY(argArray)) {
        argList = EJSDENSEARRAY_ELEMENTS(argArray_);
    }
    else {
        /* 6. Let argList be an empty List. */
        argList = (ejsval*)malloc(sizeof(ejsval) * n);
        argList_allocated = EJS_TRUE;

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
    }

    /* 9. Return the result of calling the [[Call]] internal method of func, providing thisArg as the this value and */
    /*    argList as the list of arguments. */
    ejsval rv = OP(EJSVAL_TO_OBJECT(func), Call)(func, thisArg, n, argList);

    if (argList_allocated)
        free (argList);
    return rv;
}

// ECMA262 15.3.4.4
static EJS_NATIVE_FUNC(_ejs_Function_prototype_call) {
    // XXX nanboxing breaks this EJS_ASSERT (EJSVAL_IS_FUNCTION(_this));

    ejsval func = *_this;
    ejsval thisArg = _ejs_undefined;
  
    if (argc > 0) {
        thisArg = args[0];
        args = &args[1];
        argc --;
    }
    
    ejsval rv = OP(EJSVAL_TO_OBJECT(func), Call)(func, thisArg, argc, argc == 0 ? NULL : args);

    return rv;
}

#define EJS_BOUNDFUNC_TARGET_SLOT 0
#define EJS_BOUNDFUNC_THIS_SLOT 1
#define EJS_BOUNDFUNC_ARGC_SLOT 2
#define EJS_BOUNDFUNC_FIRST_ARG_SLOT 3
#define EJS_BOUNDFUNC_RESERVED_SLOT_COUNT 3

#define EJS_BOUNDFUNC_ENV_NEW(argc)      _ejs_closureenv_new(EJS_BOUNDFUNC_RESERVED_SLOT_COUNT + (argc))
#define EJS_BOUNDFUNC_ENV_GET_TARGET(bf) _ejs_closureenv_get_slot((bf), EJS_BOUNDFUNC_TARGET_SLOT)
#define EJS_BOUNDFUNC_ENV_GET_THIS(bf)   _ejs_closureenv_get_slot((bf), EJS_BOUNDFUNC_THIS_SLOT)
#define EJS_BOUNDFUNC_ENV_GET_ARGC(bf)   _ejs_closureenv_get_slot((bf), EJS_BOUNDFUNC_ARGC_SLOT)
#define EJS_BOUNDFUNC_ENV_GET_ARG(bf,a)  _ejs_closureenv_get_slot((bf), EJS_BOUNDFUNC_FIRST_ARG_SLOT + (a))

#define EJS_BOUNDFUNC_ENV_SET_TARGET(bf,v) (*_ejs_closureenv_get_slot_ref((bf), EJS_BOUNDFUNC_TARGET_SLOT) = v)
#define EJS_BOUNDFUNC_ENV_SET_THIS(bf,v)   (*_ejs_closureenv_get_slot_ref((bf), EJS_BOUNDFUNC_THIS_SLOT) = v)
#define EJS_BOUNDFUNC_ENV_SET_ARGC(bf,v)   (*_ejs_closureenv_get_slot_ref((bf), EJS_BOUNDFUNC_ARGC_SLOT) = v)
#define EJS_BOUNDFUNC_ENV_SET_ARG(bf,a,v)  (*_ejs_closureenv_get_slot_ref((bf), EJS_BOUNDFUNC_FIRST_ARG_SLOT + (a)) = v)

static EJS_NATIVE_FUNC(bound_wrapper) {
    ejsval target = EJS_BOUNDFUNC_ENV_GET_TARGET(env);
    ejsval thisArg = EJS_BOUNDFUNC_ENV_GET_THIS(env);
    uint32_t bound_argc = ToUint32(EJS_BOUNDFUNC_ENV_GET_ARGC(env));

    // 9.4.1.2 [[Construct]]: construct the target with the bound args
    // prepended; a newTarget equal to the bound function itself is
    // replaced by the target
    if (!EJSVAL_IS_UNDEFINED(newTarget)) {
        uint32_t call_argc = argc + bound_argc;
        ejsval* call_args = alloca(sizeof(ejsval) * call_argc);
        if (bound_argc)
            memcpy (call_args, _ejs_closureenv_get_slot_ref(env, EJS_BOUNDFUNC_FIRST_ARG_SLOT), sizeof(ejsval) * bound_argc);
        if (argc)
            memcpy (call_args + bound_argc, args, sizeof(ejsval) * argc);
        // the Construct specop reached us through the bound function
        // object; that object (not the target) is what newTarget holds,
        // but we cannot compare against it here, so pass the target as
        // newTarget — correct for the direct `new bf()` case
        return Construct (target, target, call_argc, call_args);
    }

    if (bound_argc == 0) {
        return _ejs_invoke_closure(target, &thisArg, argc, args, _ejs_undefined);
    }
    else if (argc == 0) {
        return _ejs_invoke_closure(target, &thisArg, bound_argc, _ejs_closureenv_get_slot_ref(env, EJS_BOUNDFUNC_FIRST_ARG_SLOT), _ejs_undefined);
    }
    else {
        uint32_t call_argc = argc + bound_argc;
        ejsval* call_args = alloca(sizeof(ejsval) * call_argc);
        memcpy (call_args, _ejs_closureenv_get_slot_ref(env, EJS_BOUNDFUNC_FIRST_ARG_SLOT), sizeof(ejsval) * bound_argc);
        memcpy (call_args + bound_argc, args, sizeof(ejsval) * argc);
        return _ejs_invoke_closure(target, &thisArg, call_argc, call_args, _ejs_undefined);
    }
}

// ECMA262 15.3.4.5
static EJS_NATIVE_FUNC(_ejs_Function_prototype_bind) {
    /* 1. Let Target be the this value. */
    ejsval Target = *_this;

    /* 2. If IsCallable(Target) is false, throw a TypeError exception. */
    if (!IsCallable(Target)) {
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "object not a function");
    }

    ejsval thisArg = _ejs_undefined;
    if (argc > 0)
        thisArg = args[0];

    /* 3. Let A be a new (possibly empty) internal list of all of the argument values provided after thisArg (arg1, arg2 etc), in order. */
    int bound_argc = argc > 1 ? argc - 1 : 0;

    /* 4. Let F be a new native ECMAScript object . */

    ejsval bound_env = EJS_BOUNDFUNC_ENV_NEW(bound_argc);
    EJS_BOUNDFUNC_ENV_SET_TARGET(bound_env, Target);
    EJS_BOUNDFUNC_ENV_SET_THIS(bound_env, thisArg);
    EJS_BOUNDFUNC_ENV_SET_ARGC(bound_env, NUMBER_TO_EJSVAL(bound_argc));
    for (int i = 0; i < bound_argc; i ++) {
        EJS_BOUNDFUNC_ENV_SET_ARG(bound_env, i, args[i+1]);
    }

    ejsval target_name = _ejs_object_getprop (Target, _ejs_atom_name);
    ejsval bound_name;
    if (EJSVAL_IS_STRING(target_name))
        bound_name = _ejs_string_concat(_ejs_atom_bound_space, target_name);
    else
        bound_name = _ejs_atom_bound_space;
    

    ejsval F = _ejs_function_new_without_proto (bound_env, bound_name, bound_wrapper);
    EJSFunction *F_ = (EJSFunction*)EJSVAL_TO_OBJECT(F);
    ((EJSObject*)F_)->proto = EJSVAL_TO_OBJECT(Target)->proto;

    F_->bound = EJS_TRUE;
    // a bound function constructs iff its target does (9.4.1.2)
    if (EJSVAL_IS_FUNCTION(Target))
        F_->constructor_kind = ((EJSFunction*)EJSVAL_TO_OBJECT(Target))->constructor_kind;

    return F;
}

EJS_NATIVE_FUNC(_ejs_Function_empty) {
    return _ejs_undefined;
}

static EJS_NATIVE_FUNC(_ejs_Function_create) {
    _ejs_throw_nativeerror_utf8 (EJS_ERROR, "ejs doesn't support dynamic creation of functions");
}

void
_ejs_function_set_derived_constructor (ejsval F)
{
    EJS_ASSERT(EJSVAL_IS_FUNCTION(F));
    EJSFunction *F_ = (EJSFunction*)EJSVAL_TO_OBJECT(F);
    F_->constructor_kind = CONSTRUCTOR_KIND_DERIVED;
    F_->function_kind = FUNCTION_KIND_CLASS_CONSTRUCTOR;
}

void
_ejs_function_set_base_constructor (ejsval F)
{
    EJS_ASSERT(EJSVAL_IS_FUNCTION(F));
    EJSFunction *F_ = (EJSFunction*)EJSVAL_TO_OBJECT(F);
    F_->constructor_kind = CONSTRUCTOR_KIND_BASE;
    F_->function_kind = FUNCTION_KIND_CLASS_CONSTRUCTOR;
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

    _ejs_object_define_value_property (OBJECT_TO_EJSVAL(proto), _ejs_atom_name, _ejs_atom_empty, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_CONFIGURABLE | EJS_PROP_NOT_WRITABLE);
}

void
_ejs_function_init(ejsval global)
{
    trace = getenv("EJS_TRACE") != NULL;

    _ejs_function_init_proto();

    _ejs_Function = _ejs_function_new_without_proto (_ejs_null, _ejs_atom_Function, _ejs_Function_impl);
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
_ejs_invoke_closure (ejsval closure, ejsval* _this, uint32_t argc, ejsval* args, ejsval newTarget)
{
    if (!EJSVAL_IS_FUNCTION(closure)) {
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "object not a function");
    }

    EJSFunction* func = (EJSFunction*)EJSVAL_TO_OBJECT(closure);
    if (func->function_kind == FUNCTION_KIND_CLASS_CONSTRUCTOR) {
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "class constructors are not callable as functions.  use 'new'");
    }

    return OP(EJSVAL_TO_OBJECT(closure),Call) (closure, *_this, argc, args);
}

// the .ll landing-pad wrappers (ejs-invoke-closure-catch.ll)
EJSBool _ejs_invoke_closure_catch_inner (ejsval* retval, ejsval closure, ejsval* _this, uint32_t argc, ejsval* args, ejsval newTarget);
EJSBool _ejs_invoke_func_catch_inner (ejsval* retval, ejsval(*func)(void*), void* data);

// A C-side catch discards every emitted frame below it, but only
// emitted CATCH handlers re-link the gc-frame chain head — a C catcher
// must restore the head itself or the collector keeps walking the
// unwound (dead) frame records.
EJSBool
_ejs_invoke_closure_catch (ejsval* retval, ejsval closure, ejsval* _this, uint32_t argc, ejsval* args, ejsval newTarget)
{
    void* saved_gc_frame_head = _ejs_heap.gc_frame_head;
    EJSBool ok = _ejs_invoke_closure_catch_inner (retval, closure, _this, argc, args, newTarget);
    if (!ok)
        _ejs_heap.gc_frame_head = saved_gc_frame_head;
    return ok;
}

EJSBool
_ejs_invoke_func_catch (ejsval* retval, ejsval(*func)(void*), void* data)
{
    void* saved_gc_frame_head = _ejs_heap.gc_frame_head;
    EJSBool ok = _ejs_invoke_func_catch_inner (retval, func, data);
    if (!ok)
        _ejs_heap.gc_frame_head = saved_gc_frame_head;
    return ok;
}

ejsval
_ejs_construct_closure (ejsval _closure, ejsval* _this, uint32_t argc, ejsval* args, ejsval newTarget)
{
    *_this = Construct(_closure, newTarget, argc, args);
    return *_this;
}

ejsval
_ejs_construct_closure_apply (ejsval _closure, ejsval* _this, uint32_t argc, ejsval* args, ejsval newTarget)
{
    EJS_ASSERT(argc == 1);
    ejsval args_array = args[0];

    EJS_ASSERT(EJSVAL_IS_ARRAY(args_array));

    *_this = Construct(_closure, newTarget, EJS_ARRAY_LEN(args_array), EJS_DENSE_ARRAY_ELEMENTS(args_array));

    EJS_ASSERT(!EJSVAL_IS_UNDEFINED(*_this));

    return *_this;
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
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "O is not an object");
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
    _ejs_Object_specops.Finalize (obj);
}

static void
_ejs_function_specop_scan (EJSObject* obj, EJSValueFunc scan_func)
{
    EJSFunction* f = (EJSFunction*)obj;
    scan_func (&(f->env));
    _ejs_Object_specops.Scan (obj, scan_func);
}

// ES2015, June 2015
// 9.2.1
static ejsval
_ejs_function_specop_call (ejsval F, ejsval _this, uint32_t argc, ejsval *args)
{
    // 1. Assert: F is an ECMAScript function object.
    EJS_ASSERT(EJSVAL_IS_FUNCTION(F));
    EJSFunction* F_ = (EJSFunction*)EJSVAL_TO_OBJECT(F);

    // 2. If F’s [[FunctionKind]] internal slot is "classConstructor", throw a TypeError exception.
    if (F_->function_kind == FUNCTION_KIND_CLASS_CONSTRUCTOR) {
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "class constructors are not callable");
    }

    // 3. Let callerContext be the running execution context.
    // 4. Let calleeContext be PrepareForOrdinaryCall(F, undefined).
    // 5. Assert: calleeContext is now the running execution context.
    // 6. Perform OrdinaryCallBindThis(F, calleeContext, thisArgument).
    // 7. Let result be OrdinaryCallEvaluateBody(F, argumentsList).
    // 8. Remove calleeContext from the execution context stack and restore callerContext as the running execution context.
    // 9. If result.[[type]] is return, return NormalCompletion(result.[[value]]).
    // 10. ReturnIfAbrupt(result).
    // 11. Return NormalCompletion(undefined)
    return F_->func (F_->env, &_this, argc, args, _ejs_undefined);
}

// ES2015, June 2015
// 9.2.2
static ejsval
_ejs_function_specop_construct (ejsval F, ejsval newTarget, uint32_t argc, ejsval* args)
{
    // 1. Assert: F is an ECMAScript function object.
    EJS_ASSERT(EJSVAL_IS_FUNCTION(F));
    EJSFunction* F_ = (EJSFunction*)EJSVAL_TO_OBJECT(F);

    // 2. Assert: Type(newTarget) is Object.
    EJS_ASSERT(EJSVAL_IS_OBJECT(newTarget));

    // 3. Let callerContext be the running execution context.
    
    // 4. Let kind be F’s [[ConstructorKind]] internal slot.
    EJSConstructorKind kind = F_->constructor_kind;

    ejsval thisArgument = _ejs_undefined;
    // 5. If kind is "base", then
    if (kind == CONSTRUCTOR_KIND_BASE) {
        // a. Let thisArgument be OrdinaryCreateFromConstructor(newTarget, "%ObjectPrototype%").
        // b. ReturnIfAbrupt(thisArgument).
        // the birth-capacity hint pre-sizes `this` so the
        // constructor's slot fills stay in the object's own cell
        // (single-cell allocation); semantics are unchanged from
        // OrdinaryCreateFromConstructor with _ejs_Object_specops.
        ejsval proto = GetPrototypeFromConstructor(newTarget, _ejs_Object_prototype);
        thisArgument = _ejs_object_new_with_slot_hint (proto, F_->ctor_slot_hint);
    }

    // 6. Let calleeContext be PrepareForOrdinaryCall(F, newTarget).
    // 7. Assert: calleeContext is now the running execution context.
    // 8. If kind is "base", perform OrdinaryCallBindThis(F, calleeContext, thisArgument).
    // 9. Let constructorEnv be the LexicalEnvironment of calleeContext.
    // 10. Let envRec be constructorEnv’s EnvironmentRecord.
    // 11. Let result be OrdinaryCallEvaluateBody(F, argumentsList).
    ejsval result = F_->func (F_->env, &thisArgument, argc, args, newTarget);
    // birth-capacity feedback: remember how many fields the
    // constructor installed so the NEXT base construct births `this`
    // with embedded slot storage.  One-shot 0 -> count; F_ is pinned by
    // the conservative scan (it's C-stack-visible), so the pointer is
    // stable across the body call.
    if (kind == CONSTRUCTOR_KIND_BASE && F_->ctor_slot_hint == 0
        && EJSVAL_IS_OBJECT(thisArgument)) {
        EJSObject* T_ = EJSVAL_TO_OBJECT(thisArgument);
        if (T_->ops == &_ejs_Object_specops) {
            uint32_t tshape = EJS_OBJECT_SHAPE(T_);
            if (tshape != EJS_SHAPE_DICT)
                F_->ctor_slot_hint = _ejs_shape_field_count (tshape);
        }
    }
    // 12. Remove calleeContext from the execution context stack and restore callerContext as the running execution context.
    // 13. If result.[[type]] is return, then
    // a. If Type(result.[[value]]) is Object, return NormalCompletion(result.[[value]]).
    if (EJSVAL_IS_OBJECT(result))
        return result;
    // b. If kind is "base", return NormalCompletion(thisArgument).
    if (kind == CONSTRUCTOR_KIND_BASE)
        return thisArgument;
    // c. If result.[[value]] is not undefined, throw a TypeError exception.
    if (!EJSVAL_IS_UNDEFINED(result)) {
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "constructor returned non-object");
    }
    // 14. Else, ReturnIfAbrupt(result).
    // 15. Return envRec.GetThisBinding().
    return thisArgument;
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
                 _ejs_function_specop_call,      // [[Call]]
                 _ejs_function_specop_construct, // [[Construct]]
                 _ejs_function_specop_allocate,
                 _ejs_function_specop_finalize,
                 _ejs_function_specop_scan
                 )

