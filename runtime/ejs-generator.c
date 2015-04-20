/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include <string.h>

#include "ejs-ops.h"
#include "ejs-error.h"
#include "ejs-generator.h"
#include "ejs-function.h"

#define GENERATOR_STACK_SIZE 64 * 1024

static void
_ejs_generator_start(EJSGenerator* gen)
{
    _ejs_invoke_closure(gen->body, _ejs_undefined, 0, NULL);
    gen->yielded_value = _ejs_create_iter_result(_ejs_undefined, _ejs_true);
}

ejsval
_ejs_generator_new (ejsval generator_body)
{
    EJSGenerator* rv = _ejs_gc_new(EJSGenerator);
    _ejs_init_object ((EJSObject*)rv, _ejs_Generator_prototype, &_ejs_Generator_specops);

    rv->body = generator_body;
    rv->started = EJS_FALSE;
    rv->yielded_value = _ejs_undefined;
    rv->sent_value = _ejs_undefined;

    getcontext(&rv->generator_context);
    rv->generator_context.uc_stack.ss_sp = malloc(GENERATOR_STACK_SIZE);
    rv->generator_context.uc_stack.ss_size = GENERATOR_STACK_SIZE;
    rv->generator_context.uc_link = &rv->caller_context;
    makecontext(&rv->generator_context, _ejs_generator_start, 1, rv);
    memset(&rv->caller_context, 0, sizeof(rv->caller_context));

    return OBJECT_TO_EJSVAL(rv);
}

ejsval
_ejs_generator_yield (ejsval generator, ejsval arg) {
    EJSGenerator* gen = (EJSGenerator*)EJSVAL_TO_OBJECT(generator);
    gen->yielded_value = _ejs_create_iter_result(arg, _ejs_false);
    gen->sent_value = _ejs_undefined;
    swapcontext(&gen->generator_context, &gen->caller_context);

    if (gen->throwing) {
        gen->throwing = EJS_FALSE;
        _ejs_throw (gen->sent_value);
    }

    return gen->sent_value;
}

static ejsval
_ejs_generator_send (ejsval generator, ejsval arg) {
    EJSGenerator* gen = (EJSGenerator*)EJSVAL_TO_OBJECT(generator);
    gen->yielded_value = _ejs_undefined;
    gen->sent_value = arg;
    swapcontext(&gen->caller_context, &gen->generator_context);
    return gen->yielded_value;
}

static ejsval
_ejs_generator_throw (ejsval generator, ejsval arg) {
    EJSGenerator* gen = (EJSGenerator*)EJSVAL_TO_OBJECT(generator);
    gen->yielded_value = _ejs_undefined;
    gen->sent_value = arg;
    gen->throwing = EJS_TRUE;
    swapcontext(&gen->caller_context, &gen->generator_context);
    return gen->yielded_value;
}

static ejsval
_ejs_Generator_prototype_throw (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval O = _this;
    if (!EJSVAL_IS_OBJECT(O))
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, ".throw called on non-object");

    if (!EJSVAL_IS_GENERATOR(O))
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, ".throw called on non-generator");

    return _ejs_generator_throw(O, argc > 0 ? args[0] : _ejs_undefined);
}

static ejsval
_ejs_Generator_prototype_next (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval O = _this;
    if (!EJSVAL_IS_OBJECT(O))
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, ".next called on non-object");

    if (!EJSVAL_IS_GENERATOR(O))
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, ".next called on non-generator");

    return _ejs_generator_send(O, argc > 0 ? args[0] : _ejs_undefined);
}

ejsval _ejs_Generator_prototype EJSVAL_ALIGNMENT;

void
_ejs_generator_init(ejsval global)
{
    _ejs_gc_add_root (&_ejs_Generator_prototype);
    _ejs_Generator_prototype = _ejs_object_new(_ejs_Object_prototype, &_ejs_Generator_specops);

#define PROTO_METHOD(x) EJS_INSTALL_ATOM_FUNCTION_FLAGS (_ejs_Generator_prototype, x, _ejs_Generator_prototype_##x, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_WRITABLE | EJS_PROP_CONFIGURABLE)

    PROTO_METHOD(next);
    PROTO_METHOD(throw);
}

static void
_ejs_generator_specop_scan (EJSObject* obj, EJSValueFunc scan_func)
{
    EJSGenerator* gen = (EJSGenerator*)obj;
    scan_func(gen->body);
    scan_func(gen->yielded_value);
    scan_func(gen->sent_value);
    //  XXX we need to conservatively scan the generator stack
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
                 OP_INHERIT, // allocate.  shouldn't ever be used
                 OP_INHERIT, // finalize.  also shouldn't ever be used
                 _ejs_generator_specop_scan
                 )
