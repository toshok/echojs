/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include <string.h>

#include "ejs-array.h"
#include "ejs-ops.h"
#include "ejs-error.h"
#include "ejs-generator.h"
#include "ejs-function.h"
#include "ejs-string.h"
#include "ejs-symbol.h"

// a simple type that allows us to both iterate and return next value
typedef struct {
    EJSObject obj;
    ejsval iterator;
    EJSBool done;
} EJSIteratorWrapper;

static EJS_NATIVE_FUNC(_ejs_IteratorWrapper_prototype_getNextValue) {
    EJSIteratorWrapper* iter = (EJSIteratorWrapper*)EJSVAL_TO_OBJECT(*_this);
    if (iter->done)
        return _ejs_undefined;

    // 13.3.3.8 IteratorBindingInitialization: an abrupt completion from
    // next()/done/value propagates to the caller (marking the record
    // done so no close is attempted) — it must not fold to undefined
    iter->done = EJS_TRUE;
    ejsval iter_result = IteratorNext (iter->iterator, _ejs_undefined);
    iter->done = EJSVAL_TO_BOOLEAN (IteratorComplete (iter_result));
    if (iter->done)
        return _ejs_undefined;

    iter->done = EJS_TRUE;
    ejsval iter_value = IteratorValue (iter_result);
    iter->done = EJS_FALSE;
    return iter_value;
}

// normal-completion close: if the pattern finished before the iterator
// did, IteratorClose it (spec step 4 of the array-pattern bindings)
static EJS_NATIVE_FUNC(_ejs_IteratorWrapper_prototype_close) {
    EJSIteratorWrapper* iter = (EJSIteratorWrapper*)EJSVAL_TO_OBJECT(*_this);
    if (iter->done)
        return _ejs_undefined;
    iter->done = EJS_TRUE;
    IteratorClose (iter->iterator, _ejs_undefined, EJS_FALSE);
    return _ejs_undefined;
}

static EJS_NATIVE_FUNC(_ejs_IteratorWrapper_prototype_getRest) {
    EJSIteratorWrapper* iter = (EJSIteratorWrapper*)EJSVAL_TO_OBJECT(*_this);
    ejsval arr = _ejs_array_new(0, EJS_FALSE);

    while (!iter->done) {
        ejsval next_value = _ejs_IteratorWrapper_prototype_getNextValue(env, _this, 0, NULL, _ejs_undefined);
        if (!iter->done)
            _ejs_array_push_dense(arr, 1, &next_value);
    }

    return arr;
}

static EJSObject*
_ejs_iterator_wrapper_specop_allocate()
{
    return (EJSObject*)_ejs_gc_new (EJSIteratorWrapper);
}

static void
_ejs_iterator_wrapper_specop_scan (EJSObject* obj, EJSValueFunc scan_func)
{
    EJSIteratorWrapper* iter = (EJSIteratorWrapper*)obj;
    scan_func(&(iter->iterator));
    _ejs_Object_specops.Scan (obj, scan_func);
}

ejsval _ejs_IteratorWrapper_prototype EJSVAL_ALIGNMENT;

EJS_DEFINE_CLASS(IteratorWrapper,
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
                 OP_INHERIT, // [[Call]]
                 OP_INHERIT, // [[Construct]]
                 _ejs_iterator_wrapper_specop_allocate,
                 OP_INHERIT, // [[Finalize]]
                 _ejs_iterator_wrapper_specop_scan
                 )

void
_ejs_iterator_wrapper_init (ejsval global)
{
    _ejs_gc_add_root (&_ejs_IteratorWrapper_prototype);
    _ejs_IteratorWrapper_prototype = _ejs_object_new(_ejs_Object_prototype, &_ejs_Object_specops);

#define PROTO_METHOD(x) EJS_INSTALL_ATOM_FUNCTION_FLAGS (_ejs_IteratorWrapper_prototype, x, _ejs_IteratorWrapper_prototype_##x, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_WRITABLE | EJS_PROP_CONFIGURABLE)

    PROTO_METHOD(getNextValue);
    PROTO_METHOD(getRest);
    PROTO_METHOD(close);

#undef PROTO_METHOD
}

ejsval
_ejs_iterator_wrapper_new (ejsval iterator)
{
    EJSIteratorWrapper* rv = _ejs_gc_new (EJSIteratorWrapper);
    _ejs_init_object ((EJSObject*)rv, _ejs_IteratorWrapper_prototype, &_ejs_IteratorWrapper_specops);
    rv->iterator = iterator;
    return OBJECT_TO_EJSVAL(rv);
}

// ---- the state-machine resume driver ----------------------------------
// The body closure is a compiled resume-dispatch state machine; the
// driver below re-calls it as body(gen, mode, sent) and every yield
// suspends by returning (the compiled code sets eir_state and
// eir_suspended through _ejs_generator_eir_suspend first).  No machine
// stack, no contexts, no registry entry — the generator's entire
// suspended state is the precisely-scanned eir_env.

ejsval
_ejs_generator_new_eir (ejsval generator_body)
{
    EJSGenerator* rv = _ejs_gc_new(EJSGenerator);
    _ejs_init_object ((EJSObject*)rv, _ejs_Generator_prototype, &_ejs_Generator_specops);

    rv->body = generator_body;
    rv->eir_state = 0;
    rv->eir_suspended = EJS_FALSE;
    rv->eir_env = _ejs_undefined;
    rv->sent_value = _ejs_undefined;

    return OBJECT_TO_EJSVAL(rv);
}

ejsval
_ejs_generator_eir_state (ejsval generator)
{
    EJSGenerator* gen = (EJSGenerator*)EJSVAL_TO_OBJECT(generator);
    return NUMBER_TO_EJSVAL(gen->eir_state);
}

ejsval
_ejs_generator_eir_get_env (ejsval generator)
{
    EJSGenerator* gen = (EJSGenerator*)EJSVAL_TO_OBJECT(generator);
    return gen->eir_env;
}

void
_ejs_generator_eir_set_env (ejsval generator, ejsval env)
{
    EJSGenerator* gen = (EJSGenerator*)EJSVAL_TO_OBJECT(generator);
    gen->eir_env = env;
    _ejs_gc_remember (gen, env);
}

void
_ejs_generator_eir_suspend (ejsval generator, ejsval state)
{
    EJSGenerator* gen = (EJSGenerator*)EJSVAL_TO_OBJECT(generator);
    gen->eir_state = (int32_t)EJSVAL_TO_NUMBER(state);
    gen->eir_suspended = EJS_TRUE;
}

ejsval
_ejs_generator_eir_sentinel (void)
{
    return _ejs_generator_return_sentinel;
}

// modes match gen-lower.ts: 0 = next, 1 = throw, 2 = return
static ejsval
_ejs_generator_eir_resume (EJSGenerator* gen, int mode, ejsval arg)
{
    if (gen->running)
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "generator is already running");

    gen->started = EJS_TRUE;
    gen->eir_suspended = EJS_FALSE;
    // speculative: an uncaught throw out of the body must leave the
    // generator completed; a yield un-completes through eir_suspended
    gen->completed = EJS_TRUE;
    gen->running = EJS_TRUE;
    // the wrapper's sentinel catch reads the .return() value from here
    gen->sent_value = arg;
    _ejs_gc_remember (gen, gen->sent_value);

    ejsval genval = OBJECT_TO_EJSVAL(gen);
    ejsval body_args[3] = { genval, NUMBER_TO_EJSVAL(mode), arg };
    ejsval undef_this = _ejs_undefined;
    ejsval rv;
    EJSBool ok = _ejs_invoke_closure_catch (&rv, gen->body, &undef_this, 3, body_args, _ejs_undefined);
    gen->running = EJS_FALSE;
    if (!ok) {
        // uncaught throw: the generator is completed; rethrow on our
        // caller's stack
        _ejs_throw (rv);
    }
    if (gen->eir_suspended) {
        gen->completed = EJS_FALSE;
        return _ejs_create_iter_result (rv, _ejs_false);
    }
    return _ejs_create_iter_result (rv, _ejs_true);
}

// the unforgeable value .return() throws through the generator body to
// unwind it (running finally blocks); the desugared body's outermost
// catch converts it into a normal return
ejsval _ejs_generator_return_sentinel EJSVAL_ALIGNMENT;

ejsval
_ejs_generator_is_return_sentinel (ejsval exc)
{
    return BOOLEAN_TO_EJSVAL(EJSVAL_EQ(exc, _ejs_generator_return_sentinel));
}

ejsval
_ejs_generator_return_value (ejsval generator)
{
    EJSGenerator* gen = (EJSGenerator*)EJSVAL_TO_OBJECT(generator);
    return gen->sent_value;
}

static EJS_NATIVE_FUNC(_ejs_Generator_prototype_throw) {
    ejsval O = *_this;
    if (!EJSVAL_IS_OBJECT(O))
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, ".throw called on non-object");

    if (!EJSVAL_IS_GENERATOR(O))
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, ".throw called on non-generator");

    EJSGenerator* gen = (EJSGenerator*)EJSVAL_TO_OBJECT(O);
    // 25.3.3.4: throwing at a completed (or never-started) generator
    // just throws the exception in the caller
    if (gen->completed || !gen->started)
        _ejs_throw (argc > 0 ? args[0] : _ejs_undefined);

    return _ejs_generator_eir_resume (gen, 1, argc > 0 ? args[0] : _ejs_undefined);
}

static EJS_NATIVE_FUNC(_ejs_Generator_prototype_return) {
    ejsval O = *_this;
    if (!EJSVAL_IS_OBJECT(O))
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, ".return called on non-object");

    if (!EJSVAL_IS_GENERATOR(O))
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, ".return called on non-generator");

    EJSGenerator* gen = (EJSGenerator*)EJSVAL_TO_OBJECT(O);
    ejsval arg = argc > 0 ? args[0] : _ejs_undefined;

    // not yet started, or already done: complete without running the body
    if (!gen->started || gen->completed) {
        gen->completed = EJS_TRUE;
        return _ejs_create_iter_result(arg, _ejs_true);
    }

    // suspended at a yield: resume with the return sentinel.  finally
    // blocks run; unless one of them yields or overrides the completion,
    // the body's outer catch returns `arg` and the generator completes.
    return _ejs_generator_eir_resume (gen, 2, arg);
}

static EJS_NATIVE_FUNC(_ejs_Generator_prototype_next) {
    ejsval O = *_this;
    if (!EJSVAL_IS_OBJECT(O))
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, ".next called on non-object");

    if (!EJSVAL_IS_GENERATOR(O))
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, ".next called on non-generator");

    EJSGenerator* gen = (EJSGenerator*)EJSVAL_TO_OBJECT(O);
    // 25.3.3.3: a completed generator keeps answering { undefined, true }
    // (resuming the dead context would be undefined behavior)
    if (gen->completed)
        return _ejs_create_iter_result(_ejs_undefined, _ejs_true);

    return _ejs_generator_eir_resume (gen, 0, argc > 0 ? args[0] : _ejs_undefined);
}

static EJS_NATIVE_FUNC(_ejs_Iterator_prototype_iterator) {
    return *_this;
}

ejsval _ejs_Iterator_prototype EJSVAL_ALIGNMENT;
void
_ejs_iterator_init_proto()
{
    // _ejs_Generator_prototype is rooted by _ejs_generator_init, not
    // here.  the iterator prototype, though, needs this root: nothing
    // reachable references it until the other iterator protos are
    // created, and an unrooted prototype is freed by the first collection
    // out from under everything that later uses it as [[Prototype]].
    _ejs_gc_add_root (&_ejs_Iterator_prototype);
    _ejs_Iterator_prototype = _ejs_object_new(_ejs_Object_prototype, &_ejs_Object_specops);

    ejsval _iterator = _ejs_function_new_native (_ejs_null, _ejs_Symbol_iterator, _ejs_Iterator_prototype_iterator);
    _ejs_object_define_value_property (_ejs_Iterator_prototype, _ejs_Symbol_iterator, _iterator, EJS_PROP_NOT_ENUMERABLE);
}


ejsval _ejs_Generator_prototype EJSVAL_ALIGNMENT;

// ---- async generator surface -----------------------------------------
//
// async generators desugar to sync generators behind a driver object, so
// there is no dedicated instance class; these objects provide the spec's
// %AsyncIteratorPrototype% / %AsyncGeneratorPrototype% /
// %AsyncGeneratorFunction.prototype% chain, and %markAsyncGen (emitted by
// the async desugar at each async-generator definition) hangs a compiled
// wrapper function onto it.

ejsval _ejs_AsyncIteratorPrototype EJSVAL_ALIGNMENT;
ejsval _ejs_AsyncGeneratorPrototype EJSVAL_ALIGNMENT;
ejsval _ejs_AsyncGeneratorFunction_prototype EJSVAL_ALIGNMENT;

static EJS_NATIVE_FUNC(_ejs_AsyncIteratorPrototype_asyncIterator) {
    return *_this;
}

// AsyncGenerator.prototype's next/return/throw: the driver object carries
// own next/return/throw closures (which shadow these); the prototype
// methods dispatch to those so explicit .call() on the prototype works
static ejsval
agp_dispatch (ejsval atom, ejsval* _this, uint32_t argc, ejsval* args)
{
    if (!EJSVAL_IS_OBJECT(*_this))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "receiver is not an async generator");
    EJSPropertyDesc* own = OP(EJSVAL_TO_OBJECT(*_this),GetOwnProperty)(*_this, atom, NULL);
    if (!own || !_ejs_property_desc_has_value(own) || !IsCallable(_ejs_property_desc_get_value(own)))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "receiver is not an async generator");
    return _ejs_invoke_closure (_ejs_property_desc_get_value(own), _this, argc, args, _ejs_undefined);
}

static EJS_NATIVE_FUNC(_ejs_AsyncGeneratorPrototype_next) {
    return agp_dispatch (_ejs_atom_next, _this, argc, args);
}
static EJS_NATIVE_FUNC(_ejs_AsyncGeneratorPrototype_return) {
    return agp_dispatch (_ejs_atom_return, _this, argc, args);
}
static EJS_NATIVE_FUNC(_ejs_AsyncGeneratorPrototype_throw) {
    return agp_dispatch (_ejs_atom_throw, _this, argc, args);
}

static EJS_NATIVE_FUNC(_ejs_AsyncGeneratorFunction_impl) {
    _ejs_throw_nativeerror_utf8 (EJS_ERROR, "ejs doesn't support dynamic creation of functions");
}

// %markAsyncGen(fn): reparent the compiled async-generator wrapper into
// the AsyncGeneratorFunction chain and give it the spec .prototype
ejsval
_ejs_mark_async_generator (ejsval fn)
{
    if (!EJSVAL_IS_FUNCTION(fn))
        return fn;
    EJSObject* fn_ = EJSVAL_TO_OBJECT(fn);
    fn_->proto = _ejs_AsyncGeneratorFunction_prototype;
    _ejs_gc_remember (fn_, _ejs_AsyncGeneratorFunction_prototype);
    ejsval proto = _ejs_object_new (_ejs_AsyncGeneratorPrototype, &_ejs_Object_specops);
    _ejs_object_define_value_property (fn, _ejs_atom_prototype, proto,
                                       EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_CONFIGURABLE | EJS_PROP_WRITABLE);
    return fn;
}

void
_ejs_generator_init(ejsval global)
{
    _ejs_gc_add_root (&_ejs_Generator_prototype);
    _ejs_Generator_prototype = _ejs_object_new(_ejs_Iterator_prototype, &_ejs_Generator_specops);

    _ejs_gc_add_root (&_ejs_generator_return_sentinel);
    _ejs_generator_return_sentinel = _ejs_object_new(_ejs_null, &_ejs_Object_specops);

#define PROTO_METHOD(x) EJS_INSTALL_ATOM_FUNCTION_FLAGS (_ejs_Generator_prototype, x, _ejs_Generator_prototype_##x, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_WRITABLE | EJS_PROP_CONFIGURABLE)

    PROTO_METHOD(next);
    PROTO_METHOD(return);
    PROTO_METHOD(throw);

#undef PROTO_METHOD

    // %AsyncIteratorPrototype% (25.1.3)
    _ejs_gc_add_root (&_ejs_AsyncIteratorPrototype);
    _ejs_AsyncIteratorPrototype = _ejs_object_new (_ejs_Object_prototype, &_ejs_Object_specops);
    EJS_INSTALL_SYMBOL_FUNCTION_FLAGS (_ejs_AsyncIteratorPrototype, asyncIterator, _ejs_AsyncIteratorPrototype_asyncIterator,
                                       EJS_PROP_NOT_ENUMERABLE | EJS_PROP_WRITABLE | EJS_PROP_CONFIGURABLE);

    // %AsyncGeneratorPrototype% (27.6.1)
    _ejs_gc_add_root (&_ejs_AsyncGeneratorPrototype);
    _ejs_AsyncGeneratorPrototype = _ejs_object_new (_ejs_AsyncIteratorPrototype, &_ejs_Object_specops);
    // spec arity 1 for all three (the global arity table can't reach
    // this prototype by a dot-path)
#define AGP_METHOD(x) EJS_MACRO_START \
    ejsval __agp_fn = _ejs_function_new_native (_ejs_null, _ejs_atom_##x, _ejs_AsyncGeneratorPrototype_##x); \
    _ejs_object_define_value_property (__agp_fn, _ejs_atom_length, NUMBER_TO_EJSVAL(1), \
                                       EJS_PROP_NOT_ENUMERABLE | EJS_PROP_CONFIGURABLE | EJS_PROP_NOT_WRITABLE); \
    _ejs_object_define_value_property (_ejs_AsyncGeneratorPrototype, _ejs_atom_##x, __agp_fn, \
                                       EJS_PROP_NOT_ENUMERABLE | EJS_PROP_WRITABLE | EJS_PROP_CONFIGURABLE); \
    EJS_MACRO_END
    AGP_METHOD(next);
    AGP_METHOD(return);
    AGP_METHOD(throw);
#undef AGP_METHOD
    _ejs_object_define_value_property (_ejs_AsyncGeneratorPrototype, _ejs_Symbol_toStringTag, _ejs_string_new_utf8 ("AsyncGenerator"),
                                       EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_WRITABLE | EJS_PROP_CONFIGURABLE);

    // %AsyncGeneratorFunction.prototype% (27.4.3) and its constructor
    _ejs_gc_add_root (&_ejs_AsyncGeneratorFunction_prototype);
    _ejs_AsyncGeneratorFunction_prototype = _ejs_object_new (_ejs_Function_prototype, &_ejs_Object_specops);
    _ejs_object_define_value_property (_ejs_AsyncGeneratorFunction_prototype, _ejs_atom_prototype, _ejs_AsyncGeneratorPrototype,
                                       EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_WRITABLE | EJS_PROP_CONFIGURABLE);
    _ejs_object_define_value_property (_ejs_AsyncGeneratorFunction_prototype, _ejs_Symbol_toStringTag, _ejs_string_new_utf8 ("AsyncGeneratorFunction"),
                                       EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_WRITABLE | EJS_PROP_CONFIGURABLE);
    _ejs_object_define_value_property (_ejs_AsyncGeneratorPrototype, _ejs_atom_constructor, _ejs_AsyncGeneratorFunction_prototype,
                                       EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_WRITABLE | EJS_PROP_CONFIGURABLE);

    ejsval agf_ctor = _ejs_function_new_without_proto (_ejs_null, _ejs_string_new_utf8 ("AsyncGeneratorFunction"), _ejs_AsyncGeneratorFunction_impl);
    _ejs_object_define_value_property (agf_ctor, _ejs_atom_prototype, _ejs_AsyncGeneratorFunction_prototype,
                                       EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_CONFIGURABLE | EJS_PROP_NOT_WRITABLE);
    _ejs_object_define_value_property (_ejs_AsyncGeneratorFunction_prototype, _ejs_atom_constructor, agf_ctor,
                                       EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_WRITABLE | EJS_PROP_CONFIGURABLE);
}

static EJSObject*
_ejs_generator_specop_allocate()
{
    return (EJSObject*)_ejs_gc_new (EJSGenerator);
}

static void
_ejs_generator_specop_finalize (EJSObject* obj)
{
    // nothing beyond the object itself: the suspended state is the
    // heap env, collected like any other object
    (void)obj;
}

static void
_ejs_generator_specop_scan (EJSObject* obj, EJSValueFunc scan_func)
{
    EJSGenerator* gen = (EJSGenerator*)obj;
    scan_func(&(gen->body));
    scan_func(&(gen->sent_value));
    // the state machine's whole suspended state is the env — one
    // precise edge, nothing conservative
    scan_func(&(gen->eir_env));

    _ejs_Object_specops.Scan (obj, scan_func);
}

EJS_DEFINE_CLASS(Generator,
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
                 OP_INHERIT, // [[Call]]
                 OP_INHERIT, // [[Construct]]
                 _ejs_generator_specop_allocate,
                 _ejs_generator_specop_finalize,
                 _ejs_generator_specop_scan
                 )
