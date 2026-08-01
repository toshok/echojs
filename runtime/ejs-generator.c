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

#define GENERATOR_STACK_SIZE 512 * 1024

static void
_ejs_generator_start(EJSGenerator* gen)
{
    _ejs_gc_push_generator(gen);
    ejsval undef_this = _ejs_undefined;
    // catch here: an uncaught throw out of the body must not unwind the
    // generator stack past this frame (there is nothing above it but the
    // makecontext trampoline).  The exception is parked in yielded_value
    // and rethrown by the resume site on the caller's stack.
    ejsval rv;
    EJSBool body_returned = _ejs_invoke_closure_catch(&rv, gen->body, &undef_this, 0, NULL, _ejs_undefined);

    // the body's return value is the final iteration result's value
    // (`function* g() { return 5; }` -> { value: 5, done: true }).
    // The iter result is allocated BEFORE the generator leaves the active
    // chain: we are still executing on the generator's stack here, and a
    // collection triggered by this allocation must know that
    // (mark_thread_stack's range depends on the chain).
    gen->completed = EJS_TRUE;
    if (body_returned) {
        gen->yielded_value = _ejs_create_iter_result(rv, _ejs_true);
    }
    else {
        gen->threw_out = EJS_TRUE;
        gen->yielded_value = rv;
    }
    _ejs_gc_remember(gen, gen->yielded_value);
    _ejs_gc_pop_generator();
}

// makecontext's variadic arguments are ints, so a 64-bit pointer passed
// directly gets truncated (which is how generators crashed on arm64
// macos: heap pointers there don't fit in 32 bits).  split the pointer
// across two int args, posix-style.
static void
_ejs_generator_trampoline(unsigned int gen_lo, unsigned int gen_hi)
{
    EJSGenerator* gen = (EJSGenerator*)(((uint64_t)gen_hi << 32) | gen_lo);
    _ejs_generator_start(gen);
}

ejsval
_ejs_generator_new (ejsval generator_body)
{
    EJSGenerator* rv = _ejs_gc_new(EJSGenerator);
    _ejs_init_object ((EJSObject*)rv, _ejs_Generator_prototype, &_ejs_Generator_specops);

    rv->body = generator_body;
    rv->started = EJS_FALSE;
    rv->completed = EJS_FALSE;
    rv->threw_out = EJS_FALSE;
    rv->throwing = EJS_FALSE;
    rv->returning = EJS_FALSE;
    rv->yielded_value = _ejs_undefined;
    rv->sent_value = _ejs_undefined;

    rv->stack = malloc(GENERATOR_STACK_SIZE);
    rv->stack_size = GENERATOR_STACK_SIZE;
    rv->caller_stack_top = NULL;
    rv->gc_frame_head = NULL;        // this stack's parked chain
    rv->caller_gc_frame_head = NULL;
    rv->reg_prev = NULL;
    rv->reg_next = _ejs_generator_registry;
    if (_ejs_generator_registry) _ejs_generator_registry->reg_prev = rv;
    _ejs_generator_registry = rv;
    getcontext(&rv->generator_context);
    rv->generator_context.uc_stack.ss_sp = rv->stack;
    rv->generator_context.uc_stack.ss_size = GENERATOR_STACK_SIZE;
    rv->generator_context.uc_link = &rv->caller_context;
    makecontext(&rv->generator_context, (void(*)(void))_ejs_generator_trampoline, 2,
                (unsigned int)(uint64_t)(uintptr_t)rv,
                (unsigned int)(((uint64_t)(uintptr_t)rv) >> 32));
    memset(&rv->caller_context, 0, sizeof(rv->caller_context));

    return OBJECT_TO_EJSVAL(rv);
}

ejsval
_ejs_generator_yield (ejsval generator, ejsval arg) {
    EJSGenerator* gen = (EJSGenerator*)EJSVAL_TO_OBJECT(generator);
    gen->yielded_value = _ejs_create_iter_result(arg, _ejs_false);
    _ejs_gc_remember(gen, gen->yielded_value);
    gen->sent_value = _ejs_undefined;

    _ejs_gc_pop_generator();
    swapcontext(&gen->generator_context, &gen->caller_context);
    _ejs_gc_push_generator(gen);

    if (gen->throwing) {
        gen->throwing = EJS_FALSE;
        _ejs_throw (gen->sent_value);
    }

    if (gen->returning) {
        gen->returning = EJS_FALSE;
        // unwind the generator body: finally blocks run; the desugared
        // body's outer catch recognizes the sentinel and returns
        // gen->sent_value (see DesugarGeneratorFunctions)
        _ejs_throw (_ejs_generator_return_sentinel);
    }

    return gen->sent_value;
}

// every swap back from the generator lands here: if the body ended in
// an uncaught throw, rethrow it now — on the caller's stack
static ejsval
_ejs_generator_resume_result (EJSGenerator* gen)
{
    if (gen->threw_out) {
        gen->threw_out = EJS_FALSE;
        ejsval exc = gen->yielded_value;
        gen->yielded_value = _ejs_undefined;
        _ejs_throw (exc);
    }
    return gen->yielded_value;
}

static ejsval
_ejs_generator_send (ejsval generator, ejsval arg) {
    EJSGenerator* gen = (EJSGenerator*)EJSVAL_TO_OBJECT(generator);
    gen->started = EJS_TRUE;
    gen->yielded_value = _ejs_undefined;
    gen->sent_value = arg;
    _ejs_gc_remember(gen, gen->sent_value);
    gen->caller_stack_top = (void*)&gen; // GC: the suspended segment starts here
    swapcontext(&gen->caller_context, &gen->generator_context);
    return _ejs_generator_resume_result(gen);
}

static ejsval
_ejs_generator_throw (ejsval generator, ejsval arg) {
    EJSGenerator* gen = (EJSGenerator*)EJSVAL_TO_OBJECT(generator);
    gen->yielded_value = _ejs_undefined;
    gen->sent_value = arg;
    gen->throwing = EJS_TRUE;
    gen->caller_stack_top = (void*)&gen; // GC: the suspended segment starts here
    swapcontext(&gen->caller_context, &gen->generator_context);
    return _ejs_generator_resume_result(gen);
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

    return _ejs_generator_throw(O, argc > 0 ? args[0] : _ejs_undefined);
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
    gen->returning = EJS_TRUE;
    gen->yielded_value = _ejs_undefined;
    gen->sent_value = arg;
    _ejs_gc_remember(gen, gen->sent_value);
    gen->caller_stack_top = (void*)&gen; // GC: the suspended segment starts here
    swapcontext(&gen->caller_context, &gen->generator_context);
    return _ejs_generator_resume_result(gen);
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

    return _ejs_generator_send(O, argc > 0 ? args[0] : _ejs_undefined);
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
// async generators desugar to sync coroutines behind a driver object, so
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

// the live-generator registry — every generator's suspended
// stack must be conservatively scanned BEFORE a minor collection starts
// evacuating (see ejs-gc.c minor step 1)
EJSGenerator* _ejs_generator_registry;

static void
_ejs_generator_specop_finalize (EJSObject* obj)
{
    EJSGenerator* gen = (EJSGenerator*)obj;
    if (gen->reg_next) gen->reg_next->reg_prev = gen->reg_prev;
    if (gen->reg_prev) gen->reg_prev->reg_next = gen->reg_next;
    if (_ejs_generator_registry == gen) _ejs_generator_registry = gen->reg_next;
    free (gen->stack);
}

// the conservative half of the generator scan: both saved register
// files (the ucontexts) and the live suspended stack segment.  Shared
// by the specop scan and the minor collection's pre-evacuation registry
// walk (conservative ranges must all be seen before any
// object moves).
void
_ejs_generator_scan_conservative (EJSGenerator* gen)
{
    _ejs_gc_mark_conservative_range(&gen->generator_context, (char*)&gen->generator_context + sizeof(ucontext_t));
    _ejs_gc_mark_conservative_range(&gen->caller_context, (char*)&gen->caller_context + sizeof(ucontext_t));

    if (gen->stack) {
        void* stack_end = gen->stack + gen->stack_size;
        void* saved_sp =
#if __APPLE__
#if TARGET_CPU_AMD64
                         (void*)gen->generator_context.__mcontext_data.__ss.__rsp
#elif TARGET_CPU_X86
                         (void*)gen->generator_context.__mcontext_data.__ss.__esp
#elif TARGET_CPU_ARM
                         (void*)gen->generator_context.__mcontext_data.__ss.__sp
#elif TARGET_CPU_ARM64
                         (void*)gen->generator_context.__mcontext_data.__ss.__sp
#else
#error "unimplemented darwin cpu arch"
#endif
#elif linux
#if TARGET_CPU_AMD64
                         (void*)gen->generator_context.uc_mcontext.gregs[REG_RSP]
#elif TARGET_CPU_ARM64
                         (void*)gen->generator_context.uc_mcontext.sp
#else
#error "unimplemented linux cpu arch"
#endif
#else
#error "unimplemented platform"
#endif
                         ;
        // The stack grows DOWN: the live suspended frames sit between the
        // suspension SP and the stack's END — [stack, sp) is the DEAD
        // region.  An SP outside the range (never-started context,
        // garbage) degrades to scanning the whole stack, which is merely
        // conservative.
        if (saved_sp < gen->stack || saved_sp > stack_end)
            saved_sp = gen->stack;
        _ejs_gc_mark_conservative_range(saved_sp, stack_end);
    }
}

static void
_ejs_generator_specop_scan (EJSObject* obj, EJSValueFunc scan_func)
{
    EJSGenerator* gen = (EJSGenerator*)obj;
    scan_func(&(gen->body));
    scan_func(&(gen->yielded_value));
    scan_func(&(gen->sent_value));

    _ejs_generator_scan_conservative (gen);

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
