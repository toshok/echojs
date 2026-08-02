/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include <stdlib.h>

#include "ejs-resources.h"
#include "ejs-error.h"
#include "ejs-function.h"
#include "ejs-gc.h"
#include "ejs-ops.h"
#include "ejs-promise.h"
#include "ejs-string.h"
#include "ejs-symbol.h"

ejsval _ejs_DisposableStack EJSVAL_ALIGNMENT;
ejsval _ejs_DisposableStack_prototype EJSVAL_ALIGNMENT;

ejsval _ejs_AsyncDisposableStack EJSVAL_ALIGNMENT;
ejsval _ejs_AsyncDisposableStack_prototype EJSVAL_ALIGNMENT;

ejsval _ejs_SuppressedError EJSVAL_ALIGNMENT;
ejsval _ejs_SuppressedError_prototype EJSVAL_ALIGNMENT;

ejsval _ejs_FinalizationRegistry EJSVAL_ALIGNMENT;
ejsval _ejs_FinalizationRegistry_prototype EJSVAL_ALIGNMENT;

ejsval _ejs_WeakRef EJSVAL_ALIGNMENT;
ejsval _ejs_WeakRef_prototype EJSVAL_ALIGNMENT;

// ------------------------------------------------------------------------
// SuppressedError
// ------------------------------------------------------------------------

// ES2026 20.5.8.1 SuppressedError ( error, suppressed, message )
static EJS_NATIVE_FUNC(_ejs_SuppressedError_impl) {
    ejsval error      = _ejs_undefined;
    ejsval suppressed = _ejs_undefined;
    ejsval message    = _ejs_undefined;
    if (argc > 0) error      = args[0];
    if (argc > 1) suppressed = args[1];
    if (argc > 2) message    = args[2];

    // 1. If NewTarget is undefined, let newTarget be the active function object.
    if (EJSVAL_IS_UNDEFINED(newTarget))
        newTarget = _ejs_SuppressedError;

    // 2. Let O be OrdinaryCreateFromConstructor(newTarget, "%SuppressedError.prototype%", «[[ErrorData]]»).
    ejsval O = OrdinaryCreateFromConstructor(newTarget, _ejs_SuppressedError_prototype, &_ejs_Error_specops);
    *_this = O;

    // 3. If message is not undefined, define O.message.
    if (!EJSVAL_IS_UNDEFINED(message)) {
        ejsval msg = ToString(message);
        _ejs_object_define_value_property (O, _ejs_atom_message, msg,
                                           EJS_PROP_NOT_ENUMERABLE | EJS_PROP_CONFIGURABLE | EJS_PROP_WRITABLE);
    }

    // 4-5. Define O.error and O.suppressed.
    _ejs_object_define_value_property (O, _ejs_atom_error, error,
                                       EJS_PROP_NOT_ENUMERABLE | EJS_PROP_CONFIGURABLE | EJS_PROP_WRITABLE);
    _ejs_object_define_value_property (O, _ejs_atom_suppressed, suppressed,
                                       EJS_PROP_NOT_ENUMERABLE | EJS_PROP_CONFIGURABLE | EJS_PROP_WRITABLE);

    // 6. Return O.
    return O;
}

static ejsval
_ejs_suppressed_error_new (ejsval error, ejsval suppressed)
{
    ejsval O = _ejs_undefined;
    ejsval err_args[3];
    err_args[0] = error;
    err_args[1] = suppressed;
    err_args[2] = _ejs_string_new_utf8("An error was suppressed during disposal");
    return _ejs_SuppressedError_impl (_ejs_null, &O, 3, err_args, _ejs_SuppressedError);
}

// ------------------------------------------------------------------------
// DisposableStack / AsyncDisposableStack
// ------------------------------------------------------------------------

static ejsval
_ejs_disposablestack_new (ejsval proto, EJSSpecOps* ops)
{
    EJSDisposableStack *stack = _ejs_gc_new (EJSDisposableStack);
    _ejs_init_object ((EJSObject*)stack, proto, ops);
    return OBJECT_TO_EJSVAL((EJSObject*)stack);
}

static void
_ejs_disposablestack_push (ejsval stackv, ejsval value, ejsval method, EJSBool pass_value_as_arg)
{
    EJSDisposableStack* stack = (EJSDisposableStack*)EJSVAL_TO_OBJECT(stackv);

    EJSDisposeResource* res = calloc (1, sizeof (EJSDisposeResource));
    res->value = value;
    res->method = method;
    res->pass_value_as_arg = pass_value_as_arg;
    _ejs_gc_remember (stack, res->value);
    _ejs_gc_remember (stack, res->method);

    // prepend: dispose() walks the list front-to-back to get reverse
    // insertion order
    res->next = stack->resources;
    stack->resources = res;
}

// runs every queued callback (reverse insertion order), chaining
// multiple thrown errors through SuppressedError.  returns EJS_TRUE on
// success; on failure returns EJS_FALSE with the aggregated error in
// *error_out.
static EJSBool
_ejs_disposablestack_run_callbacks (ejsval stackv, ejsval* error_out)
{
    EJSDisposableStack* stack = (EJSDisposableStack*)EJSVAL_TO_OBJECT(stackv);

    EJSBool have_error = EJS_FALSE;
    ejsval pending = _ejs_undefined;

    EJSDisposeResource* res = stack->resources;
    stack->resources = NULL;
    while (res) {
        EJSDisposeResource* next = res->next;

        ejsval rv;
        EJSBool ok;
        if (res->pass_value_as_arg) {
            ejsval thisv = _ejs_undefined;
            ejsval call_args[1];
            call_args[0] = res->value;
            ok = _ejs_invoke_closure_catch (&rv, res->method, &thisv, 1, call_args, _ejs_undefined);
        }
        else {
            ejsval thisv = res->value;
            ok = _ejs_invoke_closure_catch (&rv, res->method, &thisv, 0, NULL, _ejs_undefined);
        }

        if (!ok) {
            if (have_error)
                pending = _ejs_suppressed_error_new (rv, pending);
            else {
                pending = rv;
                have_error = EJS_TRUE;
            }
        }

        free (res);
        res = next;
    }

    *error_out = pending;
    return !have_error;
}

// 12.3.1.1 DisposableStack ( )
static EJS_NATIVE_FUNC(_ejs_DisposableStack_impl) {
    if (EJSVAL_IS_UNDEFINED(newTarget))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "DisposableStack constructor must be called with new");

    ejsval O = OrdinaryCreateFromConstructor(newTarget, _ejs_DisposableStack_prototype, &_ejs_DisposableStack_specops);
    *_this = O;
    return O;
}

static EJSDisposableStack*
_ejs_disposablestack_check (ejsval v, const char* method)
{
    if (!EJSVAL_IS_DISPOSABLESTACK(v))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, method);
    return (EJSDisposableStack*)EJSVAL_TO_OBJECT(v);
}

// 12.3.3.6 DisposableStack.prototype.use ( value )
static EJS_NATIVE_FUNC(_ejs_DisposableStack_prototype_use) {
    ejsval value = _ejs_undefined;
    if (argc > 0) value = args[0];

    EJSDisposableStack* stack = _ejs_disposablestack_check (*_this, "DisposableStack.prototype.use called with incompatible this");
    if (stack->disposed)
        _ejs_throw_nativeerror_utf8 (EJS_REFERENCE_ERROR, "DisposableStack already disposed");

    if (EJSVAL_IS_NULL_OR_UNDEFINED(value))
        return value;

    if (!EJSVAL_IS_OBJECT(value))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "DisposableStack.prototype.use requires an object");

    ejsval method = Get (value, _ejs_Symbol_dispose);
    if (!IsCallable(method))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "object is not disposable (no callable Symbol.dispose method)");

    _ejs_disposablestack_push (*_this, value, method, EJS_FALSE);
    return value;
}

// 12.3.3.1 DisposableStack.prototype.adopt ( value, onDispose )
static EJS_NATIVE_FUNC(_ejs_DisposableStack_prototype_adopt) {
    ejsval value = _ejs_undefined;
    ejsval onDispose = _ejs_undefined;
    if (argc > 0) value = args[0];
    if (argc > 1) onDispose = args[1];

    EJSDisposableStack* stack = _ejs_disposablestack_check (*_this, "DisposableStack.prototype.adopt called with incompatible this");
    if (stack->disposed)
        _ejs_throw_nativeerror_utf8 (EJS_REFERENCE_ERROR, "DisposableStack already disposed");

    if (!IsCallable(onDispose))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "DisposableStack.prototype.adopt onDispose isn't a function");

    _ejs_disposablestack_push (*_this, value, onDispose, EJS_TRUE);
    return value;
}

// 12.3.3.2 DisposableStack.prototype.defer ( onDispose )
static EJS_NATIVE_FUNC(_ejs_DisposableStack_prototype_defer) {
    ejsval onDispose = _ejs_undefined;
    if (argc > 0) onDispose = args[0];

    EJSDisposableStack* stack = _ejs_disposablestack_check (*_this, "DisposableStack.prototype.defer called with incompatible this");
    if (stack->disposed)
        _ejs_throw_nativeerror_utf8 (EJS_REFERENCE_ERROR, "DisposableStack already disposed");

    if (!IsCallable(onDispose))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "DisposableStack.prototype.defer onDispose isn't a function");

    _ejs_disposablestack_push (*_this, _ejs_undefined, onDispose, EJS_FALSE);
    return _ejs_undefined;
}

// 12.3.3.5 DisposableStack.prototype.move ( )
static EJS_NATIVE_FUNC(_ejs_DisposableStack_prototype_move) {
    EJSDisposableStack* stack = _ejs_disposablestack_check (*_this, "DisposableStack.prototype.move called with incompatible this");
    if (stack->disposed)
        _ejs_throw_nativeerror_utf8 (EJS_REFERENCE_ERROR, "DisposableStack already disposed");

    ejsval newStackv = _ejs_disposablestack_new (_ejs_DisposableStack_prototype, &_ejs_DisposableStack_specops);
    EJSDisposableStack* newStack = (EJSDisposableStack*)EJSVAL_TO_OBJECT(newStackv);

    newStack->resources = stack->resources;
    stack->resources = NULL;
    stack->disposed = EJS_TRUE;

    return newStackv;
}

// 12.3.3.3 DisposableStack.prototype.dispose ( )
static EJS_NATIVE_FUNC(_ejs_DisposableStack_prototype_dispose) {
    EJSDisposableStack* stack = _ejs_disposablestack_check (*_this, "DisposableStack.prototype.dispose called with incompatible this");
    if (stack->disposed)
        return _ejs_undefined;
    stack->disposed = EJS_TRUE;

    ejsval error;
    if (!_ejs_disposablestack_run_callbacks (*_this, &error))
        _ejs_throw (error);

    return _ejs_undefined;
}

// 12.3.3.4 get DisposableStack.prototype.disposed
static EJS_NATIVE_FUNC(_ejs_DisposableStack_prototype_get_disposed) {
    EJSDisposableStack* stack = _ejs_disposablestack_check (*_this, "get DisposableStack.prototype.disposed called with incompatible this");
    return BOOLEAN_TO_EJSVAL(stack->disposed);
}

// 12.4.1.1 AsyncDisposableStack ( )
static EJS_NATIVE_FUNC(_ejs_AsyncDisposableStack_impl) {
    if (EJSVAL_IS_UNDEFINED(newTarget))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "AsyncDisposableStack constructor must be called with new");

    ejsval O = OrdinaryCreateFromConstructor(newTarget, _ejs_AsyncDisposableStack_prototype, &_ejs_AsyncDisposableStack_specops);
    *_this = O;
    return O;
}

static EJSDisposableStack*
_ejs_asyncdisposablestack_check (ejsval v, const char* method)
{
    if (!EJSVAL_IS_ASYNCDISPOSABLESTACK(v))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, method);
    return (EJSDisposableStack*)EJSVAL_TO_OBJECT(v);
}

// 12.4.3.6 AsyncDisposableStack.prototype.use ( value )
static EJS_NATIVE_FUNC(_ejs_AsyncDisposableStack_prototype_use) {
    ejsval value = _ejs_undefined;
    if (argc > 0) value = args[0];

    EJSDisposableStack* stack = _ejs_asyncdisposablestack_check (*_this, "AsyncDisposableStack.prototype.use called with incompatible this");
    if (stack->disposed)
        _ejs_throw_nativeerror_utf8 (EJS_REFERENCE_ERROR, "AsyncDisposableStack already disposed");

    if (EJSVAL_IS_NULL_OR_UNDEFINED(value))
        return value;

    if (!EJSVAL_IS_OBJECT(value))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "AsyncDisposableStack.prototype.use requires an object");

    // async hint: prefer Symbol.asyncDispose, fall back to Symbol.dispose
    ejsval method = Get (value, _ejs_Symbol_asyncDispose);
    if (EJSVAL_IS_NULL_OR_UNDEFINED(method))
        method = Get (value, _ejs_Symbol_dispose);
    if (!IsCallable(method))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "object is not async disposable (no callable Symbol.asyncDispose or Symbol.dispose method)");

    _ejs_disposablestack_push (*_this, value, method, EJS_FALSE);
    return value;
}

// 12.4.3.1 AsyncDisposableStack.prototype.adopt ( value, onDisposeAsync )
static EJS_NATIVE_FUNC(_ejs_AsyncDisposableStack_prototype_adopt) {
    ejsval value = _ejs_undefined;
    ejsval onDispose = _ejs_undefined;
    if (argc > 0) value = args[0];
    if (argc > 1) onDispose = args[1];

    EJSDisposableStack* stack = _ejs_asyncdisposablestack_check (*_this, "AsyncDisposableStack.prototype.adopt called with incompatible this");
    if (stack->disposed)
        _ejs_throw_nativeerror_utf8 (EJS_REFERENCE_ERROR, "AsyncDisposableStack already disposed");

    if (!IsCallable(onDispose))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "AsyncDisposableStack.prototype.adopt onDisposeAsync isn't a function");

    _ejs_disposablestack_push (*_this, value, onDispose, EJS_TRUE);
    return value;
}

// 12.4.3.2 AsyncDisposableStack.prototype.defer ( onDisposeAsync )
static EJS_NATIVE_FUNC(_ejs_AsyncDisposableStack_prototype_defer) {
    ejsval onDispose = _ejs_undefined;
    if (argc > 0) onDispose = args[0];

    EJSDisposableStack* stack = _ejs_asyncdisposablestack_check (*_this, "AsyncDisposableStack.prototype.defer called with incompatible this");
    if (stack->disposed)
        _ejs_throw_nativeerror_utf8 (EJS_REFERENCE_ERROR, "AsyncDisposableStack already disposed");

    if (!IsCallable(onDispose))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "AsyncDisposableStack.prototype.defer onDisposeAsync isn't a function");

    _ejs_disposablestack_push (*_this, _ejs_undefined, onDispose, EJS_FALSE);
    return _ejs_undefined;
}

// 12.4.3.5 AsyncDisposableStack.prototype.move ( )
static EJS_NATIVE_FUNC(_ejs_AsyncDisposableStack_prototype_move) {
    EJSDisposableStack* stack = _ejs_asyncdisposablestack_check (*_this, "AsyncDisposableStack.prototype.move called with incompatible this");
    if (stack->disposed)
        _ejs_throw_nativeerror_utf8 (EJS_REFERENCE_ERROR, "AsyncDisposableStack already disposed");

    ejsval newStackv = _ejs_disposablestack_new (_ejs_AsyncDisposableStack_prototype, &_ejs_AsyncDisposableStack_specops);
    EJSDisposableStack* newStack = (EJSDisposableStack*)EJSVAL_TO_OBJECT(newStackv);

    newStack->resources = stack->resources;
    stack->resources = NULL;
    stack->disposed = EJS_TRUE;

    return newStackv;
}

static ejsval
_ejs_promise_settled (ejsval settle_atom, ejsval value)
{
    ejsval settle = Get (_ejs_Promise, settle_atom);
    ejsval thisv = _ejs_Promise;
    return _ejs_invoke_closure (settle, &thisv, 1, &value, _ejs_undefined);
}

// 12.4.3.3 AsyncDisposableStack.prototype.disposeAsync ( )
//
// dispose callbacks run synchronously (their promise results are not
// awaited); the aggregate outcome is delivered through the returned
// promise.
static EJS_NATIVE_FUNC(_ejs_AsyncDisposableStack_prototype_disposeAsync) {
    if (!EJSVAL_IS_ASYNCDISPOSABLESTACK(*_this)) {
        ejsval err = _ejs_nativeerror_new_utf8 (EJS_TYPE_ERROR, "AsyncDisposableStack.prototype.disposeAsync called with incompatible this");
        return _ejs_promise_settled (_ejs_atom_reject, err);
    }

    EJSDisposableStack* stack = (EJSDisposableStack*)EJSVAL_TO_OBJECT(*_this);
    if (stack->disposed)
        return _ejs_promise_settled (_ejs_atom_resolve, _ejs_undefined);
    stack->disposed = EJS_TRUE;

    ejsval error;
    if (!_ejs_disposablestack_run_callbacks (*_this, &error))
        return _ejs_promise_settled (_ejs_atom_reject, error);

    return _ejs_promise_settled (_ejs_atom_resolve, _ejs_undefined);
}

// 12.4.3.4 get AsyncDisposableStack.prototype.disposed
static EJS_NATIVE_FUNC(_ejs_AsyncDisposableStack_prototype_get_disposed) {
    EJSDisposableStack* stack = _ejs_asyncdisposablestack_check (*_this, "get AsyncDisposableStack.prototype.disposed called with incompatible this");
    return BOOLEAN_TO_EJSVAL(stack->disposed);
}

// ------------------------------------------------------------------------
// FinalizationRegistry
// ------------------------------------------------------------------------

// object or symbol (CanBeHeldWeakly; registered symbols are not
// excluded here)
static EJSBool
_ejs_can_be_held_weakly (ejsval v)
{
    return EJSVAL_IS_OBJECT(v) || EJSVAL_IS_SYMBOL(v);
}

// 26.2.1.1 FinalizationRegistry ( cleanupCallback )
static EJS_NATIVE_FUNC(_ejs_FinalizationRegistry_impl) {
    ejsval cleanupCallback = _ejs_undefined;
    if (argc > 0) cleanupCallback = args[0];

    if (EJSVAL_IS_UNDEFINED(newTarget))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "FinalizationRegistry constructor must be called with new");

    if (!IsCallable(cleanupCallback))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "FinalizationRegistry cleanup callback isn't a function");

    ejsval O = OrdinaryCreateFromConstructor(newTarget, _ejs_FinalizationRegistry_prototype, &_ejs_FinalizationRegistry_specops);
    *_this = O;

    EJSFinalizationRegistry* registry = (EJSFinalizationRegistry*)EJSVAL_TO_OBJECT(O);
    registry->cleanup_callback = cleanupCallback;
    _ejs_gc_remember (registry, cleanupCallback);

    return O;
}

static EJSFinalizationRegistry*
_ejs_finalizationregistry_check (ejsval v, const char* method)
{
    if (!EJSVAL_IS_FINALIZATIONREGISTRY(v))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, method);
    return (EJSFinalizationRegistry*)EJSVAL_TO_OBJECT(v);
}

// 26.2.3.2 FinalizationRegistry.prototype.register ( target, heldValue [ , unregisterToken ] )
static EJS_NATIVE_FUNC(_ejs_FinalizationRegistry_prototype_register) {
    ejsval target = _ejs_undefined;
    ejsval heldValue = _ejs_undefined;
    ejsval unregisterToken = _ejs_undefined;
    if (argc > 0) target = args[0];
    if (argc > 1) heldValue = args[1];
    if (argc > 2) unregisterToken = args[2];

    EJSFinalizationRegistry* registry = _ejs_finalizationregistry_check (*_this, "FinalizationRegistry.prototype.register called with incompatible this");

    if (!_ejs_can_be_held_weakly(target))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "FinalizationRegistry.prototype.register target must be an object or symbol");

    // target can be held weakly (object or symbol), so identity is
    // raw-ejsval equality; SameValue's number/string cases can't apply
    if (EJSVAL_EQ (target, heldValue))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "FinalizationRegistry.prototype.register target and heldValue must not be the same");

    if (!EJSVAL_IS_UNDEFINED(unregisterToken) && !_ejs_can_be_held_weakly(unregisterToken))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "FinalizationRegistry.prototype.register unregisterToken must be an object or symbol");

    EJSFinalizationCell* cell = calloc (1, sizeof (EJSFinalizationCell));
    cell->target = target;
    cell->held = heldValue;
    cell->token = unregisterToken;
    _ejs_gc_remember (registry, cell->target);
    _ejs_gc_remember (registry, cell->held);
    _ejs_gc_remember (registry, cell->token);

    cell->next = registry->cells;
    registry->cells = cell;

    return _ejs_undefined;
}

// 26.2.3.3 FinalizationRegistry.prototype.unregister ( unregisterToken )
static EJS_NATIVE_FUNC(_ejs_FinalizationRegistry_prototype_unregister) {
    ejsval unregisterToken = _ejs_undefined;
    if (argc > 0) unregisterToken = args[0];

    EJSFinalizationRegistry* registry = _ejs_finalizationregistry_check (*_this, "FinalizationRegistry.prototype.unregister called with incompatible this");

    if (!_ejs_can_be_held_weakly(unregisterToken))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "FinalizationRegistry.prototype.unregister token must be an object or symbol");

    EJSBool removed = EJS_FALSE;
    EJSFinalizationCell** link = &registry->cells;
    while (*link) {
        EJSFinalizationCell* cell = *link;
        if (!EJSVAL_IS_UNDEFINED(cell->token) && EJSVAL_EQ (cell->token, unregisterToken)) {
            *link = cell->next;
            free (cell);
            removed = EJS_TRUE;
        }
        else {
            link = &cell->next;
        }
    }

    return BOOLEAN_TO_EJSVAL(removed);
}

// FinalizationRegistry.prototype.cleanupSome ( [ callback ] )
//
// cleanup callbacks never run: the collector never reclaims registered
// targets, and the spec permits a GC that never collects.
static EJS_NATIVE_FUNC(_ejs_FinalizationRegistry_prototype_cleanupSome) {
    ejsval callback = _ejs_undefined;
    if (argc > 0) callback = args[0];

    _ejs_finalizationregistry_check (*_this, "FinalizationRegistry.prototype.cleanupSome called with incompatible this");

    if (!EJSVAL_IS_UNDEFINED(callback) && !IsCallable(callback))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "FinalizationRegistry.prototype.cleanupSome callback isn't a function");

    return _ejs_undefined;
}

// ------------------------------------------------------------------------
// WeakRef
// ------------------------------------------------------------------------

// 26.1.1.1 WeakRef ( target )
static EJS_NATIVE_FUNC(_ejs_WeakRef_impl) {
    ejsval target = _ejs_undefined;
    if (argc > 0) target = args[0];

    if (EJSVAL_IS_UNDEFINED(newTarget))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "WeakRef constructor must be called with new");

    if (!_ejs_can_be_held_weakly(target))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "WeakRef target must be an object or symbol");

    ejsval O = OrdinaryCreateFromConstructor(newTarget, _ejs_WeakRef_prototype, &_ejs_WeakRef_specops);
    *_this = O;

    EJSWeakRef* weakref = (EJSWeakRef*)EJSVAL_TO_OBJECT(O);
    weakref->target = target;
    _ejs_gc_remember (weakref, target);

    return O;
}

// 26.1.3.2 WeakRef.prototype.deref ( )
//
// the target is held strongly, so deref always returns it — permitted,
// since the spec allows a GC that never collects.
static EJS_NATIVE_FUNC(_ejs_WeakRef_prototype_deref) {
    if (!EJSVAL_IS_WEAKREF(*_this))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "WeakRef.prototype.deref called with incompatible this");

    return ((EJSWeakRef*)EJSVAL_TO_OBJECT(*_this))->target;
}

// ------------------------------------------------------------------------
// init
// ------------------------------------------------------------------------

void
_ejs_resources_init (ejsval global)
{
    _ejs_Class_initialize (&_ejs_DisposableStack_specops, &_ejs_Object_specops);
    _ejs_Class_initialize (&_ejs_AsyncDisposableStack_specops, &_ejs_Object_specops);
    _ejs_Class_initialize (&_ejs_FinalizationRegistry_specops, &_ejs_Object_specops);
    _ejs_Class_initialize (&_ejs_WeakRef_specops, &_ejs_Object_specops);

    // SuppressedError: a native error whose prototype chains to
    // Error.prototype, matching the layout ejs-error.c gives the other
    // NativeError types
    _ejs_SuppressedError = _ejs_function_new_without_proto (_ejs_null, _ejs_atom_SuppressedError, _ejs_SuppressedError_impl);
    _ejs_object_setprop (global, _ejs_atom_SuppressedError, _ejs_SuppressedError);

    _ejs_gc_add_root (&_ejs_SuppressedError_prototype);
    _ejs_SuppressedError_prototype = _ejs_object_new (_ejs_Error_prototype, &_ejs_Object_specops);
    _ejs_object_define_value_property (_ejs_SuppressedError, _ejs_atom_prototype, _ejs_SuppressedError_prototype,
                                       EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_CONFIGURABLE | EJS_PROP_NOT_WRITABLE);
    _ejs_object_define_value_property (_ejs_SuppressedError_prototype, _ejs_atom_constructor, _ejs_SuppressedError,
                                       EJS_PROP_NOT_ENUMERABLE | EJS_PROP_CONFIGURABLE | EJS_PROP_WRITABLE);
    _ejs_object_define_value_property (_ejs_SuppressedError_prototype, _ejs_atom_name, _ejs_atom_SuppressedError,
                                       EJS_PROP_NOT_ENUMERABLE | EJS_PROP_CONFIGURABLE | EJS_PROP_WRITABLE);
    _ejs_object_define_value_property (_ejs_SuppressedError_prototype, _ejs_atom_message, _ejs_atom_empty,
                                       EJS_PROP_NOT_ENUMERABLE | EJS_PROP_CONFIGURABLE | EJS_PROP_WRITABLE);

    // DisposableStack
    _ejs_DisposableStack = _ejs_function_new_without_proto (_ejs_null, _ejs_atom_DisposableStack, _ejs_DisposableStack_impl);
    _ejs_object_setprop (global, _ejs_atom_DisposableStack, _ejs_DisposableStack);

    _ejs_gc_add_root (&_ejs_DisposableStack_prototype);
    _ejs_DisposableStack_prototype = _ejs_object_new (_ejs_Object_prototype, &_ejs_Object_specops);
    _ejs_object_define_value_property (_ejs_DisposableStack, _ejs_atom_prototype, _ejs_DisposableStack_prototype,
                                       EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_CONFIGURABLE | EJS_PROP_NOT_WRITABLE);
    _ejs_object_define_value_property (_ejs_DisposableStack_prototype, _ejs_atom_constructor, _ejs_DisposableStack,
                                       EJS_PROP_NOT_ENUMERABLE | EJS_PROP_CONFIGURABLE | EJS_PROP_WRITABLE);

#define PROTO_METHOD(o,x) EJS_INSTALL_ATOM_FUNCTION_FLAGS(o, x, _ejs_DisposableStack_prototype_##x, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_WRITABLE | EJS_PROP_CONFIGURABLE)

    PROTO_METHOD(_ejs_DisposableStack_prototype, use);
    PROTO_METHOD(_ejs_DisposableStack_prototype, adopt);
    PROTO_METHOD(_ejs_DisposableStack_prototype, defer);
    PROTO_METHOD(_ejs_DisposableStack_prototype, move);

    // accessor with spec attributes (non-enumerable, configurable)
    ejsval _disposed_getter = _ejs_function_new_native (_ejs_null, _ejs_atom_disposed, _ejs_DisposableStack_prototype_get_disposed);
    _ejs_object_define_accessor_property (_ejs_DisposableStack_prototype, _ejs_atom_disposed, _disposed_getter, _ejs_undefined,
                                          EJS_PROP_FLAGS_GETTER_SET | EJS_PROP_NOT_ENUMERABLE | EJS_PROP_CONFIGURABLE);

    // dispose is also installed as @@dispose, so expand PROTO_METHOD
    ejsval _dispose = _ejs_function_new_native (_ejs_null, _ejs_atom_dispose, _ejs_DisposableStack_prototype_dispose);
    _ejs_object_define_value_property (_ejs_DisposableStack_prototype, _ejs_atom_dispose, _dispose,
                                       EJS_PROP_NOT_ENUMERABLE | EJS_PROP_WRITABLE | EJS_PROP_CONFIGURABLE);
    _ejs_object_define_value_property (_ejs_DisposableStack_prototype, _ejs_Symbol_dispose, _dispose,
                                       EJS_PROP_NOT_ENUMERABLE | EJS_PROP_WRITABLE | EJS_PROP_CONFIGURABLE);
    _ejs_object_define_value_property (_ejs_DisposableStack_prototype, _ejs_Symbol_toStringTag, _ejs_atom_DisposableStack,
                                       EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_WRITABLE | EJS_PROP_CONFIGURABLE);

#undef PROTO_METHOD

    // AsyncDisposableStack
    _ejs_AsyncDisposableStack = _ejs_function_new_without_proto (_ejs_null, _ejs_atom_AsyncDisposableStack, _ejs_AsyncDisposableStack_impl);
    _ejs_object_setprop (global, _ejs_atom_AsyncDisposableStack, _ejs_AsyncDisposableStack);

    _ejs_gc_add_root (&_ejs_AsyncDisposableStack_prototype);
    _ejs_AsyncDisposableStack_prototype = _ejs_object_new (_ejs_Object_prototype, &_ejs_Object_specops);
    _ejs_object_define_value_property (_ejs_AsyncDisposableStack, _ejs_atom_prototype, _ejs_AsyncDisposableStack_prototype,
                                       EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_CONFIGURABLE | EJS_PROP_NOT_WRITABLE);
    _ejs_object_define_value_property (_ejs_AsyncDisposableStack_prototype, _ejs_atom_constructor, _ejs_AsyncDisposableStack,
                                       EJS_PROP_NOT_ENUMERABLE | EJS_PROP_CONFIGURABLE | EJS_PROP_WRITABLE);

#define PROTO_METHOD(o,x) EJS_INSTALL_ATOM_FUNCTION_FLAGS(o, x, _ejs_AsyncDisposableStack_prototype_##x, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_WRITABLE | EJS_PROP_CONFIGURABLE)

    PROTO_METHOD(_ejs_AsyncDisposableStack_prototype, use);
    PROTO_METHOD(_ejs_AsyncDisposableStack_prototype, adopt);
    PROTO_METHOD(_ejs_AsyncDisposableStack_prototype, defer);
    PROTO_METHOD(_ejs_AsyncDisposableStack_prototype, move);

    ejsval _adisposed_getter = _ejs_function_new_native (_ejs_null, _ejs_atom_disposed, _ejs_AsyncDisposableStack_prototype_get_disposed);
    _ejs_object_define_accessor_property (_ejs_AsyncDisposableStack_prototype, _ejs_atom_disposed, _adisposed_getter, _ejs_undefined,
                                          EJS_PROP_FLAGS_GETTER_SET | EJS_PROP_NOT_ENUMERABLE | EJS_PROP_CONFIGURABLE);

    // disposeAsync is also installed as @@asyncDispose
    ejsval _disposeAsync = _ejs_function_new_native (_ejs_null, _ejs_atom_disposeAsync, _ejs_AsyncDisposableStack_prototype_disposeAsync);
    _ejs_object_define_value_property (_ejs_AsyncDisposableStack_prototype, _ejs_atom_disposeAsync, _disposeAsync,
                                       EJS_PROP_NOT_ENUMERABLE | EJS_PROP_WRITABLE | EJS_PROP_CONFIGURABLE);
    _ejs_object_define_value_property (_ejs_AsyncDisposableStack_prototype, _ejs_Symbol_asyncDispose, _disposeAsync,
                                       EJS_PROP_NOT_ENUMERABLE | EJS_PROP_WRITABLE | EJS_PROP_CONFIGURABLE);
    _ejs_object_define_value_property (_ejs_AsyncDisposableStack_prototype, _ejs_Symbol_toStringTag, _ejs_atom_AsyncDisposableStack,
                                       EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_WRITABLE | EJS_PROP_CONFIGURABLE);

#undef PROTO_METHOD

    // FinalizationRegistry
    _ejs_FinalizationRegistry = _ejs_function_new_without_proto (_ejs_null, _ejs_atom_FinalizationRegistry, _ejs_FinalizationRegistry_impl);
    _ejs_object_setprop (global, _ejs_atom_FinalizationRegistry, _ejs_FinalizationRegistry);

    _ejs_gc_add_root (&_ejs_FinalizationRegistry_prototype);
    _ejs_FinalizationRegistry_prototype = _ejs_object_new (_ejs_Object_prototype, &_ejs_Object_specops);
    _ejs_object_define_value_property (_ejs_FinalizationRegistry, _ejs_atom_prototype, _ejs_FinalizationRegistry_prototype,
                                       EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_CONFIGURABLE | EJS_PROP_NOT_WRITABLE);
    _ejs_object_define_value_property (_ejs_FinalizationRegistry_prototype, _ejs_atom_constructor, _ejs_FinalizationRegistry,
                                       EJS_PROP_NOT_ENUMERABLE | EJS_PROP_CONFIGURABLE | EJS_PROP_WRITABLE);

#define PROTO_METHOD(x) EJS_INSTALL_ATOM_FUNCTION_FLAGS(_ejs_FinalizationRegistry_prototype, x, _ejs_FinalizationRegistry_prototype_##x, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_WRITABLE | EJS_PROP_CONFIGURABLE)

    PROTO_METHOD(register);
    PROTO_METHOD(unregister);
    PROTO_METHOD(cleanupSome);

#undef PROTO_METHOD

    _ejs_object_define_value_property (_ejs_FinalizationRegistry_prototype, _ejs_Symbol_toStringTag, _ejs_atom_FinalizationRegistry,
                                       EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_WRITABLE | EJS_PROP_CONFIGURABLE);

    // WeakRef
    _ejs_WeakRef = _ejs_function_new_without_proto (_ejs_null, _ejs_atom_WeakRef, _ejs_WeakRef_impl);
    _ejs_object_setprop (global, _ejs_atom_WeakRef, _ejs_WeakRef);

    _ejs_gc_add_root (&_ejs_WeakRef_prototype);
    _ejs_WeakRef_prototype = _ejs_object_new (_ejs_Object_prototype, &_ejs_Object_specops);
    _ejs_object_define_value_property (_ejs_WeakRef, _ejs_atom_prototype, _ejs_WeakRef_prototype,
                                       EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_CONFIGURABLE | EJS_PROP_NOT_WRITABLE);
    _ejs_object_define_value_property (_ejs_WeakRef_prototype, _ejs_atom_constructor, _ejs_WeakRef,
                                       EJS_PROP_NOT_ENUMERABLE | EJS_PROP_CONFIGURABLE | EJS_PROP_WRITABLE);

    EJS_INSTALL_ATOM_FUNCTION_FLAGS(_ejs_WeakRef_prototype, deref, _ejs_WeakRef_prototype_deref, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_WRITABLE | EJS_PROP_CONFIGURABLE);

    _ejs_object_define_value_property (_ejs_WeakRef_prototype, _ejs_Symbol_toStringTag, _ejs_atom_WeakRef,
                                       EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_WRITABLE | EJS_PROP_CONFIGURABLE);
}

// ------------------------------------------------------------------------
// specops
// ------------------------------------------------------------------------

static EJSObject*
_ejs_disposablestack_specop_allocate ()
{
    return (EJSObject*)_ejs_gc_new (EJSDisposableStack);
}

static void
_ejs_disposablestack_specop_finalize (EJSObject* obj)
{
    EJSDisposableStack* stack = (EJSDisposableStack*)obj;

    EJSDisposeResource* res = stack->resources;
    while (res) {
        EJSDisposeResource* next = res->next;
        free (res);
        res = next;
    }

    _ejs_Object_specops.Finalize (obj);
}

static void
_ejs_disposablestack_specop_scan (EJSObject* obj, EJSValueFunc scan_func)
{
    EJSDisposableStack* stack = (EJSDisposableStack*)obj;

    for (EJSDisposeResource* res = stack->resources; res; res = res->next) {
        scan_func (&(res->value));
        scan_func (&(res->method));
    }

    _ejs_Object_specops.Scan (obj, scan_func);
}

EJS_DEFINE_CLASS(DisposableStack,
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
                 _ejs_disposablestack_specop_allocate,
                 _ejs_disposablestack_specop_finalize,
                 _ejs_disposablestack_specop_scan
                 )

EJS_DEFINE_CLASS(AsyncDisposableStack,
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
                 _ejs_disposablestack_specop_allocate,
                 _ejs_disposablestack_specop_finalize,
                 _ejs_disposablestack_specop_scan
                 )

static EJSObject*
_ejs_finalizationregistry_specop_allocate ()
{
    return (EJSObject*)_ejs_gc_new (EJSFinalizationRegistry);
}

static void
_ejs_finalizationregistry_specop_finalize (EJSObject* obj)
{
    EJSFinalizationRegistry* registry = (EJSFinalizationRegistry*)obj;

    EJSFinalizationCell* cell = registry->cells;
    while (cell) {
        EJSFinalizationCell* next = cell->next;
        free (cell);
        cell = next;
    }

    _ejs_Object_specops.Finalize (obj);
}

static void
_ejs_finalizationregistry_specop_scan (EJSObject* obj, EJSValueFunc scan_func)
{
    EJSFinalizationRegistry* registry = (EJSFinalizationRegistry*)obj;

    scan_func (&registry->cleanup_callback);
    for (EJSFinalizationCell* cell = registry->cells; cell; cell = cell->next) {
        scan_func (&(cell->target));
        scan_func (&(cell->held));
        scan_func (&(cell->token));
    }

    _ejs_Object_specops.Scan (obj, scan_func);
}

EJS_DEFINE_CLASS(FinalizationRegistry,
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
                 _ejs_finalizationregistry_specop_allocate,
                 _ejs_finalizationregistry_specop_finalize,
                 _ejs_finalizationregistry_specop_scan
                 )

static EJSObject*
_ejs_weakref_specop_allocate ()
{
    return (EJSObject*)_ejs_gc_new (EJSWeakRef);
}

static void
_ejs_weakref_specop_scan (EJSObject* obj, EJSValueFunc scan_func)
{
    EJSWeakRef* weakref = (EJSWeakRef*)obj;
    scan_func (&weakref->target);
    _ejs_Object_specops.Scan (obj, scan_func);
}

EJS_DEFINE_CLASS(WeakRef,
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
                 _ejs_weakref_specop_allocate,
                 OP_INHERIT, // finalize: no out-of-object state
                 _ejs_weakref_specop_scan
                 )
