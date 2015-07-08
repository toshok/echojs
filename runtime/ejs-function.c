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

static void indent(char ch) {
    int i;
    for (i = 0; i < indent_level; i++)
        putchar(ch);
}

ejsval _ejs_function_new(ejsval env, ejsval name, EJSClosureFunc func) {
    EJSFunction *rv = _ejs_gc_new(EJSFunction);

    _ejs_init_object((EJSObject *)rv, _ejs_Function_prototype,
                     &_ejs_Function_specops);

    rv->func = func;
    rv->env = env;
    rv->is_constructor = EJS_TRUE;

    ejsval fun = OBJECT_TO_EJSVAL(rv);

    // ECMA262: 15.3.2.1
    ejsval fun_proto =
        _ejs_object_new(_ejs_Object_prototype, &_ejs_Object_specops);
    _ejs_object_define_value_property(
        fun, _ejs_atom_prototype, fun_proto,
        EJS_PROP_NOT_ENUMERABLE | EJS_PROP_CONFIGURABLE | EJS_PROP_WRITABLE);

    _ejs_object_define_value_property(
        fun_proto, _ejs_atom_constructor, fun,
        EJS_PROP_NOT_ENUMERABLE | EJS_PROP_CONFIGURABLE | EJS_PROP_WRITABLE);
    _ejs_object_define_value_property(fun, _ejs_atom_name, name,
                                      EJS_PROP_NOT_ENUMERABLE |
                                          EJS_PROP_NOT_CONFIGURABLE |
                                          EJS_PROP_NOT_WRITABLE);
    return fun;
}

ejsval _ejs_function_new_without_env(ejsval name, EJSClosureFunc func) {
    return _ejs_function_new(_ejs_undefined, name, func);
}

ejsval _ejs_function_new_without_proto(ejsval env, ejsval name,
                                       EJSClosureFunc func) {
    EJSFunction *rv = _ejs_gc_new(EJSFunction);

    _ejs_init_object((EJSObject *)rv, _ejs_Function_prototype,
                     &_ejs_Function_specops);

    rv->func = func;
    rv->env = env;
    rv->is_constructor = EJS_TRUE;

    ejsval fun = OBJECT_TO_EJSVAL(rv);

    _ejs_object_define_value_property(fun, _ejs_atom_name, name,
                                      EJS_PROP_NOT_ENUMERABLE |
                                          EJS_PROP_NOT_CONFIGURABLE |
                                          EJS_PROP_NOT_WRITABLE);
    return fun;
}

ejsval _ejs_function_new_utf8_with_proto(ejsval env, const char *name,
                                         EJSClosureFunc func,
                                         ejsval prototype) {
    ejsval function_name = _ejs_string_new_utf8(name);
    EJSFunction *rv = _ejs_gc_new(EJSFunction);

    _ejs_init_object((EJSObject *)rv, _ejs_Function_prototype,
                     &_ejs_Function_specops);

    rv->func = func;
    rv->env = env;
    rv->is_constructor = EJS_TRUE;

    ejsval fun = OBJECT_TO_EJSVAL(rv);

    _ejs_object_define_value_property(fun, _ejs_atom_name, function_name,
                                      EJS_PROP_NOT_ENUMERABLE |
                                          EJS_PROP_NOT_CONFIGURABLE |
                                          EJS_PROP_NOT_WRITABLE);
    _ejs_object_define_value_property(
        fun, _ejs_atom_prototype, prototype,
        EJS_PROP_NOT_ENUMERABLE | EJS_PROP_CONFIGURABLE | EJS_PROP_WRITABLE);

    return fun;
}

ejsval _ejs_function_new_native(ejsval env, ejsval name, EJSClosureFunc func) {
    EJSFunction *rv = _ejs_gc_new(EJSFunction);

    _ejs_init_object((EJSObject *)rv, _ejs_Function_prototype,
                     &_ejs_Function_specops);

    rv->func = func;
    rv->env = env;
    rv->is_constructor = EJS_TRUE;

    ejsval fun = OBJECT_TO_EJSVAL(rv);

    _ejs_object_define_value_property(fun, _ejs_atom_name, name,
                                      EJS_PROP_NOT_ENUMERABLE |
                                          EJS_PROP_NOT_CONFIGURABLE |
                                          EJS_PROP_NOT_WRITABLE);

    return OBJECT_TO_EJSVAL(rv);
}

ejsval _ejs_function_new_anon(ejsval env, EJSClosureFunc func) {
    return _ejs_function_new(env, _ejs_atom_empty, func);
}

ejsval _ejs_function_new_utf8(ejsval env, const char *name,
                              EJSClosureFunc func) {
    ejsval function_name = _ejs_string_new_utf8(name);
    ejsval rv = _ejs_function_new(env, function_name, func);

    return rv;
}

ejsval _ejs_Function_prototype EJSVAL_ALIGNMENT;
ejsval _ejs_Function EJSVAL_ALIGNMENT;

static ejsval _ejs_Function_impl(ejsval env, ejsval _this, uint32_t argc,
                                 ejsval *args) {
    printf("Function() called either as a function or a constructor is not "
           "supported in ejs\n");
    abort();
}

// ECMA262 15.3.4.2
static ejsval _ejs_Function_prototype_toString(ejsval env, ejsval _this,
                                               uint32_t argc, ejsval *args) {
    char terrible_fixed_buffer[256];

    if (!EJSVAL_IS_FUNCTION(_this))
        _ejs_throw_nativeerror_utf8(
            EJS_TYPE_ERROR, "Function.prototype.toString is not generic.");

    ejsval func_name = _ejs_object_getprop(_this, _ejs_atom_name);

    char *utf8_funcname = ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(func_name));

    snprintf(terrible_fixed_buffer, sizeof(terrible_fixed_buffer),
             "function %s() {}", utf8_funcname);

    free(utf8_funcname);

    return _ejs_string_new_utf8(terrible_fixed_buffer);
}

// ECMA262 15.3.4.3
static ejsval _ejs_Function_prototype_apply(ejsval env, ejsval _this,
                                            uint32_t argc, ejsval *args) {
    ejsval func = _this;

    /* 1. If IsCallable(func) is false, then throw a TypeError exception. */
    if (!EJSVAL_IS_CALLABLE(_this)) {
        printf("throw TypeError, func is not callable\n");
        EJS_NOT_IMPLEMENTED();
    }

    ejsval thisArg = _ejs_undefined;
    ejsval argArray = _ejs_undefined;
    if (argc > 0)
        thisArg = args[0];
    if (argc > 1)
        argArray = args[1];

    /* 2. If argArray is null or undefined, then */
    if (EJSVAL_IS_UNDEFINED(argArray) || EJSVAL_IS_NULL(argArray)) {
        /*    a. Return the result of calling the [[Call]] internal method of
         * func, providing thisArg as the this value */
        /*       and an empty list of arguments. */
        return _ejs_invoke_closure(func, thisArg, 0, NULL);
    }
    /* 3. If Type(argArray) is not Object, then throw a TypeError exception. */
    if (!EJSVAL_IS_OBJECT(argArray)) {
        printf("throw TypeError, argArray is not an object\n");
        EJS_NOT_IMPLEMENTED();
    }
    EJSObject *argArray_ = EJSVAL_TO_OBJECT(argArray);

    /* 4. Let len be the result of calling the [[Get]] internal method of
     * argArray with argument "length". */
    ejsval len = OP(argArray_, Get)(argArray, _ejs_atom_length, argArray);

    /* 5. Let n be ToUint32(len). */
    uint32_t n = (uint32_t)EJSVAL_TO_NUMBER(len);

    ejsval *argList;
    EJSBool argList_allocated = EJS_FALSE;

    if (EJSVAL_IS_DENSE_ARRAY(argArray)) {
        argList = EJSDENSEARRAY_ELEMENTS(argArray_);
    } else {
        /* 6. Let argList be an empty List. */
        argList = (ejsval *)malloc(sizeof(ejsval) * n);
        argList_allocated = EJS_TRUE;

        /* 7. Let index be 0. */
        int index = 0;

        /* 8. Repeat while index < n */
        while (index < n) {
            /*    a. Let indexName be ToString(index). */
            ejsval indexName = NUMBER_TO_EJSVAL(index);
            /*    b. Let nextArg be the result of calling the [[Get]] internal
             * method of argArray with indexName as the  */
            /*       argument. */
            ejsval nextArg = OP(argArray_, Get)(argArray, indexName, argArray);
            /*    c. Append nextArg as the last element of argList. */
            argList[index] = nextArg;
            /*    d. Set index to index + 1. */
            ++index;
        }
    }

    /* 9. Return the result of calling the [[Call]] internal method of func,
     * providing thisArg as the this value and */
    /*    argList as the list of arguments. */
    ejsval rv = EJSVAL_TO_FUNC(func)(EJSVAL_TO_ENV(func), thisArg, n, argList);

    if (argList_allocated)
        free(argList);
    return rv;
}

// ECMA262 15.3.4.4
static ejsval _ejs_Function_prototype_call(ejsval env, ejsval _this,
                                           uint32_t argc, ejsval *args) {
    // XXX nanboxing breaks this EJS_ASSERT (EJSVAL_IS_FUNCTION(_this));

    ejsval thisArg = _ejs_undefined;

    if (argc > 0) {
        thisArg = args[0];
        args = &args[1];
        argc--;
    }

    ejsval rv = EJSVAL_TO_FUNC(_this)(EJSVAL_TO_ENV(_this), thisArg, argc,
                                      argc == 0 ? NULL : args);

    return rv;
}

#define EJS_BOUNDFUNC_TARGET_SLOT 0
#define EJS_BOUNDFUNC_THIS_SLOT 1
#define EJS_BOUNDFUNC_ARGC_SLOT 2
#define EJS_BOUNDFUNC_FIRST_ARG_SLOT 3
#define EJS_BOUNDFUNC_RESERVED_SLOT_COUNT 3

#define EJS_BOUNDFUNC_ENV_NEW(argc)                                            \
    _ejs_closureenv_new(EJS_BOUNDFUNC_RESERVED_SLOT_COUNT + (argc))
#define EJS_BOUNDFUNC_ENV_GET_TARGET(bf)                                       \
    _ejs_closureenv_get_slot((bf), EJS_BOUNDFUNC_TARGET_SLOT)
#define EJS_BOUNDFUNC_ENV_GET_THIS(bf)                                         \
    _ejs_closureenv_get_slot((bf), EJS_BOUNDFUNC_THIS_SLOT)
#define EJS_BOUNDFUNC_ENV_GET_ARGC(bf)                                         \
    _ejs_closureenv_get_slot((bf), EJS_BOUNDFUNC_ARGC_SLOT)
#define EJS_BOUNDFUNC_ENV_GET_ARG(bf, a)                                       \
    _ejs_closureenv_get_slot((bf), EJS_BOUNDFUNC_FIRST_ARG_SLOT + (a))

#define EJS_BOUNDFUNC_ENV_SET_TARGET(bf, v)                                    \
    (*_ejs_closureenv_get_slot_ref((bf), EJS_BOUNDFUNC_TARGET_SLOT) = v)
#define EJS_BOUNDFUNC_ENV_SET_THIS(bf, v)                                      \
    (*_ejs_closureenv_get_slot_ref((bf), EJS_BOUNDFUNC_THIS_SLOT) = v)
#define EJS_BOUNDFUNC_ENV_SET_ARGC(bf, v)                                      \
    (*_ejs_closureenv_get_slot_ref((bf), EJS_BOUNDFUNC_ARGC_SLOT) = v)
#define EJS_BOUNDFUNC_ENV_SET_ARG(bf, a, v)                                    \
    (*_ejs_closureenv_get_slot_ref((bf), EJS_BOUNDFUNC_FIRST_ARG_SLOT + (a)) = \
         v)

static ejsval bound_wrapper(ejsval env, ejsval _this, uint32_t argc,
                            ejsval *args) {
    ejsval target = EJS_BOUNDFUNC_ENV_GET_TARGET(env);
    ejsval thisArg = EJS_BOUNDFUNC_ENV_GET_THIS(env);
    uint32_t bound_argc = ToUint32(EJS_BOUNDFUNC_ENV_GET_ARGC(env));

    if (bound_argc == 0) {
        return _ejs_invoke_closure(target, thisArg, argc, args);
    } else if (argc == 0) {
        return _ejs_invoke_closure(
            target, thisArg, bound_argc,
            _ejs_closureenv_get_slot_ref(env, EJS_BOUNDFUNC_FIRST_ARG_SLOT));
    } else {
        uint32_t call_argc = argc + bound_argc;
        ejsval *call_args = alloca(sizeof(ejsval) * call_argc);
        memcpy(call_args,
               _ejs_closureenv_get_slot_ref(env, EJS_BOUNDFUNC_FIRST_ARG_SLOT),
               sizeof(ejsval) * bound_argc);
        memcpy(call_args + bound_argc, args, sizeof(ejsval) * argc);
        return _ejs_invoke_closure(target, thisArg, call_argc, call_args);
    }
}

// ECMA262 15.3.4.5
static ejsval _ejs_Function_prototype_bind(ejsval env, ejsval _this,
                                           uint32_t argc, ejsval *args) {
    /* 1. Let Target be the this value. */
    ejsval Target = _this;

    /* 2. If IsCallable(Target) is false, throw a TypeError exception. */
    if (!EJSVAL_IS_CALLABLE(Target)) {
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "object not a function");
    }

    ejsval thisArg = _ejs_undefined;
    if (argc > 0)
        thisArg = args[0];

    /* 3. Let A be a new (possibly empty) internal list of all of the argument
     * values provided after thisArg (arg1, arg2 etc), in order. */
    int bound_argc = argc > 1 ? argc - 1 : 0;

    /* 4. Let F be a new native ECMAScript object . */

    ejsval bound_env = EJS_BOUNDFUNC_ENV_NEW(bound_argc);
    EJS_BOUNDFUNC_ENV_SET_TARGET(bound_env, Target);
    EJS_BOUNDFUNC_ENV_SET_THIS(bound_env, thisArg);
    EJS_BOUNDFUNC_ENV_SET_ARGC(bound_env, NUMBER_TO_EJSVAL(bound_argc));
    for (int i = 0; i < bound_argc; i++) {
        EJS_BOUNDFUNC_ENV_SET_ARG(bound_env, i, args[i + 1]);
    }

    ejsval target_name = _ejs_object_getprop(Target, _ejs_atom_name);
    ejsval bound_name;
    if (EJSVAL_IS_STRING(target_name))
        bound_name = _ejs_string_concat(_ejs_atom_bound_space, target_name);
    else
        bound_name = _ejs_atom_bound_space;

    ejsval F = _ejs_function_new(bound_env, bound_name, bound_wrapper);
    EJSFunction *F_ = (EJSFunction *)EJSVAL_TO_OBJECT(F);

    F_->bound = EJS_TRUE;

    return F;
}

ejsval _ejs_Function_empty(ejsval env, ejsval _this, uint32_t argc,
                           ejsval *args) {
    return _ejs_undefined;
}

static ejsval _ejs_Function_create(ejsval env, ejsval _this, uint32_t argc,
                                   ejsval *args) {
    _ejs_throw_nativeerror_utf8(
        EJS_ERROR, "ejs doesn't support dynamic creation of functions");
}

static ejsval _ejs_Function_prototype_create(ejsval env, ejsval _this,
                                             uint32_t argc, ejsval *args) {
    ejsval F = _this;

    if (!EJSVAL_IS_CONSTRUCTOR(F))
        _ejs_throw_nativeerror_utf8(
            EJS_TYPE_ERROR,
            "'this' in Function.prototype[Symbol.create] is not a constructor");

    EJSObject *F_ = EJSVAL_TO_OBJECT(F);

    ejsval proto = OP(F_, Get)(F, _ejs_atom_prototype, F);
    if (EJSVAL_IS_UNDEFINED(proto)) {
        proto = _ejs_Function_prototype;
    }

    if (!EJSVAL_IS_OBJECT(proto)) {
        EJS_NOT_IMPLEMENTED(); // cross-realm doesn't exist in ejs yet
    }

    EJSObject *obj = (EJSObject *)_ejs_gc_new(EJSObject);
    _ejs_init_object(obj, proto, &_ejs_Object_specops);
    return OBJECT_TO_EJSVAL(obj);
}

static void _ejs_function_init_proto() {
    _ejs_gc_add_root(&_ejs_Function_prototype);

    // Function.prototype = function () { return undefined; }

    EJSFunction *proto = _ejs_gc_new(EJSFunction);
    _ejs_Function_prototype = OBJECT_TO_EJSVAL(proto);
    _ejs_init_object((EJSObject *)proto, _ejs_Object_prototype,
                     &_ejs_Function_specops);
    proto->func = _ejs_Function_empty;
    proto->env = _ejs_null;

    _ejs_object_define_value_property(
        OBJECT_TO_EJSVAL(proto), _ejs_atom_name, _ejs_atom_empty,
        EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_CONFIGURABLE |
            EJS_PROP_NOT_WRITABLE);
}

void _ejs_function_init(ejsval global) {
    trace = getenv("EJS_TRACE") != NULL;

    _ejs_function_init_proto();

    _ejs_Function = _ejs_function_new_without_proto(
        _ejs_null, _ejs_atom_Function, (EJSClosureFunc)_ejs_Function_impl);
    _ejs_object_setprop(global, _ejs_atom_Function, _ejs_Function);

    // ECMA262 15.3.3.1
    _ejs_object_define_value_property(
        _ejs_Function, _ejs_atom_prototype, _ejs_Function_prototype,
        EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_CONFIGURABLE |
            EJS_PROP_NOT_WRITABLE);
    // ECMA262 15.3.3.2
    _ejs_object_define_value_property(
        _ejs_Function, _ejs_atom_length, NUMBER_TO_EJSVAL(1),
        EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_CONFIGURABLE |
            EJS_PROP_NOT_WRITABLE);

#define PROTO_METHOD(x)                                                        \
    EJS_INSTALL_ATOM_FUNCTION_FLAGS(_ejs_Function_prototype, x,                \
                                    _ejs_Function_prototype_##x,               \
                                    EJS_PROP_NOT_ENUMERABLE)

    PROTO_METHOD(toString);
    PROTO_METHOD(apply);
    PROTO_METHOD(call);
    PROTO_METHOD(bind);

#undef PROTO_METHOD
}

void _ejs_function_add_symbols() {
    EJS_INSTALL_SYMBOL_FUNCTION_FLAGS(
        _ejs_Function, create, _ejs_Function_create, EJS_PROP_NOT_ENUMERABLE |
                                                         EJS_PROP_CONFIGURABLE |
                                                         EJS_PROP_NOT_WRITABLE);

    EJS_INSTALL_SYMBOL_FUNCTION_FLAGS(
        _ejs_Function_prototype, create, _ejs_Function_prototype_create,
        EJS_PROP_NOT_ENUMERABLE | EJS_PROP_CONFIGURABLE |
            EJS_PROP_NOT_WRITABLE);
}

#define DEBUG_FUNCTION_ENTER(x)                                                \
    EJS_MACRO_START                                                            \
    if (trace) {                                                               \
        ejsval closure_name =                                                  \
            _ejs_Function_prototype_toString(_ejs_null, x, 0, NULL);           \
        char *closure_utf8 =                                                   \
            ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(closure_name));                 \
        indent('*');                                                           \
        printf("invoking %s\n", closure_utf8);                                 \
        free(closure_utf8);                                                    \
        indent_level += INDENT_AMOUNT;                                         \
    }                                                                          \
    EJS_MACRO_END
#define DEBUG_FUNCTION_EXIT(x)                                                 \
    EJS_MACRO_START                                                            \
    if (trace) {                                                               \
        ejsval closure_name =                                                  \
            _ejs_Function_prototype_toString(_ejs_null, x, 0, NULL);           \
        char *closure_utf8 =                                                   \
            ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(closure_name));                 \
        indent_level -= INDENT_AMOUNT;                                         \
        indent(' ');                                                           \
        printf("returning from %s\n", closure_utf8);                           \
        free(closure_utf8);                                                    \
    }                                                                          \
    EJS_MACRO_END

ejsval _ejs_invoke_closure(ejsval closure, ejsval _this, uint32_t argc,
                           ejsval *args) {
    if (!EJSVAL_IS_FUNCTION(closure)) {
#if DEBUG_LAST_LOOKUP
        extern jschar *last_lookup;
        if (last_lookup) {
            char *last_utf8 = ucs2_to_utf8(last_lookup);
            _ejs_log("last property lookup was for: %s\n", last_utf8);
            free(last_utf8);
        }
#endif

        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "object not a function");
    }

#if 0
    if (!EJSVAL_IS_NULL_OR_UNDEFINED(_this) && EJSVAL_IS_PRIMITIVE(_this)) {
        _this = ToObject(_this);
    }
#endif

    EJSFunction *fun = (EJSFunction *)EJSVAL_TO_OBJECT(closure);
    return fun->func(fun->env, _this, argc, args);
}

// ES6 Draft rev32 Feb 2, 2015
// 9.2.3
// [[Construct]] ( argumentsList, newTarget)
ejsval _ejs_construct_closure(ejsval _closure, ejsval newTarget, uint32_t argc,
                              ejsval *args) {
    // 1. Assert: F is an ECMAScript function object.
    EJSFunction *F = (EJSFunction *)EJSVAL_TO_OBJECT(_closure);

    // 2. Assert: Type(newTarget) is Object.
    // 3. Let callerContext be the running execution context.

    // 4. Let kind be F’s [[ConstructorKind]] internal slot.
    EJSConstructorKind kind = F->constructor_kind;

    ejsval thisArgument = _ejs_undefined;
    // 5. If kind is "base", then
    if (kind == CONSTRUCTOR_KIND_BASE) {
        //    a. Let thisArgument be OrdinaryCreateFromConstructor(newTarget,
        //    "%ObjectPrototype%").
        //    b. ReturnIfAbrupt(thisArgument).
    }
    // 6. Let calleeContext be PrepareForOrdinaryCall(F, newTarget).
    // 7. ReturnIfAbrupt(calleeContext).
    // 8. Assert: calleeContext is now the active execution context.
    // 9. If kind is "base", then
    if (kind == CONSTRUCTOR_KIND_BASE) {
        //    a. Let status be OrdinaryCallBindThis(F, calleeContext,
        //    thisArgument).
        //    b. If status is an abrupt completion, then
        //       i. Remove calleeContext from the execution context stack and
        //       restore callerContext as the running execution context.
        //       ii. Return status.
    }
    // 10. Let constructorEnv be the LexicalEnvironment of calleeContext.
    // 11. Let envRec be constructorEnv’s environment record.
    // 12. Let result be OrdinaryCallEvaluateBody(F, calleeContext,
    // argumentsList).
    ejsval result = _ejs_invoke_closure(_closure, thisArgument, argc, args);
    // 13. Remove calleeContext from the execution context stack and restore
    // callerContext as the running execution context.
    // 14. If result.[[type]] is return, then
    //     a. If Type(result.[[value]]) is Object, return
    //     NormalCompletion(result.[[value]]).
    if (EJSVAL_IS_OBJECT(result))
        return result;
    //     b. If kind is "base", return NormalCompletion(thisArgument).
    if (kind == CONSTRUCTOR_KIND_BASE)
        return thisArgument;
    //     c. Throw a TypeError exception.
    _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "9.2.3/14.c");
    // 15. ReturnIfAbrupt(result).
    // 16. Return the result of calling the GetThisBinding concrete method of
    // envRec’s with no arguments
    EJS_NOT_IMPLEMENTED();
}

// ECMA262: 15.3.5.3
static EJSBool _ejs_function_specop_has_instance(ejsval F, ejsval V) {
    /* 1. If V is not an object, return false. */
    if (!EJSVAL_IS_OBJECT(V))
        return EJS_FALSE;

    /* 2. Let O be the result of calling the [[Get]] internal method of F with
     * property name "prototype". */
    ejsval O = OP(EJSVAL_TO_OBJECT(F), Get)(F, _ejs_atom_prototype, F);

    /* 3. If Type(O) is not Object, throw a TypeError exception. */
    if (!EJSVAL_IS_OBJECT(O)) {
        printf("throw TypeError, O is not an object\n");
        EJS_NOT_IMPLEMENTED();
    }

    /* 4. Repeat */
    while (1) {
        /*    a. Let V be the value of the [[Prototype]] internal property of V.
         */
        V = EJSVAL_TO_OBJECT(V)->proto;
        /*    b. If V is null, return false. */
        if (EJSVAL_IS_NULL(V))
            return EJS_FALSE;
        /*    c. If O and V refer to the same object, return true. */
        if (EJSVAL_EQ(O, V))
            return EJS_TRUE;
    }
}

static EJSObject *_ejs_function_specop_allocate() {
    // we shouldn't ever get here..
    EJS_NOT_IMPLEMENTED();
}

static void _ejs_function_specop_finalize(EJSObject *obj) {
    _ejs_Object_specops.Finalize(obj);
}

static void _ejs_function_specop_scan(EJSObject *obj, EJSValueFunc scan_func) {
    EJSFunction *f = (EJSFunction *)obj;
    scan_func(f->env);
    _ejs_Object_specops.Scan(obj, scan_func);
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
                 _ejs_function_specop_allocate, _ejs_function_specop_finalize,
                 _ejs_function_specop_scan)
