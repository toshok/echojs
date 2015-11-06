/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include "ejs-ops.h"
#include "ejs-array.h"
#include "ejs-value.h"
#include "ejs-function.h"
#include "ejs-promise.h"
#include "ejs-error.h"
#include "ejs-string.h"
#include "ejs-proxy.h"
#include "ejs-symbol.h"
#include "ejs-closureenv.h"
#include "ejs-runloop.h"

#include <string.h>

ejsval _ejs_Promise EJSVAL_ALIGNMENT;
ejsval _ejs_Promise_prototype EJSVAL_ALIGNMENT;

ejsval _ejs_identity_function EJSVAL_ALIGNMENT;
ejsval _ejs_thrower_function EJSVAL_ALIGNMENT;

// ECMA262 25.4.5.4.0 Identity Functions 
static EJS_NATIVE_FUNC(identity) {
    ejsval x = _ejs_undefined;
    if (argc > 0) x = args[0];
    return x;
}

// ECMA262 25.4.5.25.0 Thrower Functions 
static EJS_NATIVE_FUNC(thrower) {
    ejsval x = _ejs_undefined;
    if (argc > 0) x = args[0];
    _ejs_throw(x);
    EJS_NOT_REACHED();
}

static void PromiseReactionTask (EJSPromiseReaction* reaction, ejsval argument);
static void PromiseResolveThenableTask (ejsval promiseToResolve, ejsval thenable, ejsval then);

// PromiseReaction tasks
typedef struct {
    EJSPromiseReaction* reaction;
    ejsval arg;
} ReactionTaskArg;

static void
call_promise_reaction_task(void* data)
{
    ReactionTaskArg* arg = (ReactionTaskArg*)data;
    PromiseReactionTask(arg->reaction, arg->arg);
}

static void
reaction_arg_dtor(void* data)
{
    ReactionTaskArg* arg = (ReactionTaskArg*)data;
    /* unroot the arg's ejsvals */
    _ejs_gc_remove_root(&arg->reaction->handler);
    _ejs_gc_remove_root(&arg->reaction->capabilities);
    _ejs_gc_remove_root(&arg->arg);
    free (arg->reaction);
    free (arg);
}

static void
EnqueuePromiseReactionTask (EJSPromiseReaction* reaction, ejsval value)
{
    ReactionTaskArg *arg = malloc(sizeof(ReactionTaskArg));
    arg->reaction = reaction;
    arg->arg = value;
    /* root the ejsvals in EJSPromiseReaction and our value here */
    _ejs_gc_add_root(&arg->reaction->handler);
    _ejs_gc_add_root(&arg->reaction->capabilities);
    _ejs_gc_add_root(&arg->arg);
    _ejs_runloop_add_task(call_promise_reaction_task, arg, reaction_arg_dtor);
}

// PromiseResolveThenable tasks
typedef struct {
    ejsval promiseToResolve;
    ejsval thenable;
    ejsval then;
} ResolveThenableTaskArg;

static void
call_promise_resolve_thenable_task(void* data)
{
    ResolveThenableTaskArg* arg = (ResolveThenableTaskArg*)data;
    PromiseResolveThenableTask(arg->promiseToResolve, arg->thenable, arg->then);
}

static void
resolve_thenable_arg_dtor(void* data)
{
    ResolveThenableTaskArg* arg = (ResolveThenableTaskArg*)data;
    /* unroot the arg's ejsvals */
    _ejs_gc_remove_root(&arg->promiseToResolve);
    _ejs_gc_remove_root(&arg->thenable);
    _ejs_gc_remove_root(&arg->then);
    free (arg);
}

static void
EnqueuePromiseResolveThenableTask (ejsval promiseToResolve, ejsval thenable, ejsval then)
{
    ResolveThenableTaskArg *arg = malloc(sizeof(ResolveThenableTaskArg));
    arg->promiseToResolve = promiseToResolve;
    arg->thenable = thenable;
    arg->then = then;
    /* root the ejsvals */
    _ejs_gc_add_root(&arg->promiseToResolve);
    _ejs_gc_add_root(&arg->thenable);
    _ejs_gc_add_root(&arg->then);
    _ejs_runloop_add_task(call_promise_resolve_thenable_task, arg, resolve_thenable_arg_dtor);
}

// ECMA262 25.4.1.8 RejectPromise ( promise, reason) 
static ejsval RejectPromise (ejsval promise, ejsval reason)
{
    EJSPromise* _promise = EJSVAL_TO_PROMISE(promise);
    // 1. Assert: the value of promise's [[PromiseState]] internal slot is "pending". 
    // 2. Let reactions be the value of promise's [[PromiseRejectReactions]] internal slot. 
    EJSPromiseReaction* reactions = _promise->rejectReactions;

    // 3. Set the value of promise's [[PromiseResult]] internal slot to reason. 
    _promise->result = reason;

    // 4. Set the value of promise's [[PromiseFulfillReactions]] internal slot to undefined. 
    // XXX we need to free our listnodes
    _promise->fulfillReactions = NULL;

    // 5. Set the value of promise's [[PromiseRejectReactions]] internal slot to undefined. 
    // XXX we need to free our listnodes
    _promise->fulfillReactions = NULL;

    // 6. Set the value of promise's [[PromiseState]] internal slot to "rejected". 
    _promise->state = PROMISE_STATE_REJECTED;

    // 7. Return TriggerPromiseReactions(reactions, reason). 
    //    1. Repeat for each reaction in reactions, in original insertion order 
    for (EJSPromiseReaction* reaction = reactions; reaction; reaction = reaction->next) {
        //       a. Perform EnqueueTask("PromiseTasks", PromiseReactionTask, (reaction, reason). 
        EnqueuePromiseReactionTask (reaction, reason);
    }
    //    2. Return undefined.
    return _ejs_undefined;
}

// ECMA262 25.4.1.5 FulfillPromise ( promise, resolutionValue) 
static ejsval FulfillPromise (ejsval promise, ejsval resolutionValue)
{
    EJSPromise* _promise = EJSVAL_TO_PROMISE(promise);
    // 1. Assert: the value of promise's [[PromiseState]] internal slot is "pending". 
    // 2. Let reactions be the value of promise's [[PromiseFulfillReactions]] internal slot. 
    EJSPromiseReaction* reactions = _promise->fulfillReactions;
    // 3. Set the value of promise's [[PromiseResult]] internal slot to resolutionvalue. 
    _promise->result = resolutionValue;

    // 4. Set the value of promise's [[PromiseFulfullReactions]] internal slot to undefined. 
    // XXX we need to free our listnodes
    _promise->fulfillReactions = NULL;

    // 5. Set the value of promise's [[PromiseRejectReactions]] internal slot to undefined. 
    // XXX we need to free our listnodes
    _promise->fulfillReactions = NULL;

    // 6. Set the value of promise's [[PromiseState]] internal slot to "fulfilled". 
    _promise->state = PROMISE_STATE_FULFILLED;

    // 7. Return TriggerPromiseReactions(reactions, resolutionValue). 
    //    1. Repeat for each reaction in reactions, in original insertion order 
    for (EJSPromiseReaction* reaction = reactions; reaction; reaction = reaction->next) {
        //       a. Perform EnqueueTask("PromiseTasks", PromiseReactionTask, (reaction, resolutionValue). 
        EnqueuePromiseReactionTask (reaction, resolutionValue);
    }
    //    2. Return undefined.
    return _ejs_undefined;
}

// ECMA262 25.4.1.3.1 Promise Reject Functions 
static EJS_NATIVE_FUNC(reject) {
    ejsval reason = _ejs_undefined;
    if (argc > 0) reason = args[0];

    // 1. Assert: F has a [[Promise]] internal slot whose value is an Object. 
    // 2. Let promise be the value of F's [[Promise]] internal slot. 
    ejsval promise = _ejs_closureenv_get_slot(env, 1);
    // 3. Let alreadyResolved by be the value of F's [[AlreadyResolved]] internal slot. 
    ejsval alreadyResolved = _ejs_closureenv_get_slot(env, 0);
    // 4. If alreadyResolved.[[value]] is true, then return undefined. 
    if (EJSVAL_TO_BOOLEAN(alreadyResolved))
        return _ejs_undefined;

    // 5. Set alreadyResolved.[[value]] to true. 
    *_ejs_closureenv_get_slot_ref(env, 0) = _ejs_true;

    // 6. Return RejectPromise(promise, reason). 
    return RejectPromise(promise, reason);
}

// ECMA262 25.4.1.4 Promise Resolve Functions 
static EJS_NATIVE_FUNC(resolve) {
    ejsval resolution = _ejs_undefined;
    if (argc > 0) resolution = args[0];

    // 1. Assert: F has a [[Promise]] internal slot whose value is an Object. 
    // 2. Let promise be the value of F's [[Promise]] internal slot. 
    ejsval promise = _ejs_closureenv_get_slot(env, 1);

    // 3. Let alreadyResolved by be the value of F's [[AlreadyResolved]] internal slot. 
    ejsval alreadyResolved = _ejs_closureenv_get_slot(env, 0);
    // 4. If alreadyResolved. [[value]] is true, then return undefined. 
    if (EJSVAL_TO_BOOLEAN(alreadyResolved)) {
        return _ejs_undefined;
    }
    // 5. Set alreadyResolved.[[value]] to true. 
    *_ejs_closureenv_get_slot_ref(env, 0) = _ejs_true;

    // 6. If SameValue(resolution, promise) is true, then 
    if (SameValue(resolution, promise)) {
        //    a. Let selfResolutionError be a newly-created TypeError object. 
        ejsval selfResolutionError = _ejs_nativeerror_new_utf8(EJS_TYPE_ERROR, ""); // XXX
        //    b. Return RejectPromise(promise, selfResolutionError). 
        return RejectPromise(promise, selfResolutionError);
    }
    // 7. If Type(resolution) is not Object, then 
    if (!EJSVAL_IS_OBJECT(resolution)) {
        //    a. Return FulfillPromise(promise, resolution). 
        return FulfillPromise(promise, resolution);
    }
    // 8. Let then be Get(resolution, "then"). 
    ejsval then = Get(resolution, _ejs_atom_then);
    // 9. If then is an abrupt completion, then 
    //    a. Return RejectPromise(promise, then.[[value]]). 
    
    // XXX

    // 10. Let then be then.[[value]]. 
    // 11. If IsCallable(then) is false, then 
    if (!IsCallable(then)) {
        //     a. Return FulfillPromise(promise, resolution). 
        return FulfillPromise(promise, resolution);
    }
    // 12. Perform EnqueueTask ("PromiseTasks", PromiseResolveThenableTask, (promise, resolution, then))
    EnqueuePromiseResolveThenableTask (promise, resolution, then);

    // 13. Return undefined. 
    return _ejs_undefined;
}

// ECMA262 25.4.1.6.2 GetCapabilitiesExecutor Functions 
static EJS_NATIVE_FUNC(capabilitiesExecutor) {
    ejsval resolve = _ejs_undefined;
    if (argc > 0) resolve = args[0];
    ejsval reject = _ejs_undefined;
    if (argc > 1) reject = args[1];

    // 1. Assert: F has a [[Capability]] internal slot whose value is a PromiseCapability Record. 
    // 2. Let promiseCapability be the value of F's [[Capability]] internal slot. 
    ejsval promiseCapability = env;

    // 3. If promiseCapability.[[Resolve]] is not undefined, then throw a TypeError exception. 
    if (!EJSVAL_IS_UNDEFINED(EJS_CAPABILITY_GET_RESOLVE(promiseCapability)))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, ""); // XXX

    // 4. If promiseCapability.[[Reject]] is not undefined, then throw a TypeError exception. 
    if (!EJSVAL_IS_UNDEFINED(EJS_CAPABILITY_GET_REJECT(promiseCapability)))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, ""); // XXX

    // 5. Set promiseCapability.[[Resolve]] to resolve.
    EJS_CAPABILITY_SET_RESOLVE(promiseCapability, resolve);

    // 6. Set promiseCapability.[[Reject]] to reject. 
    EJS_CAPABILITY_SET_REJECT(promiseCapability, reject);

    // 7. Return undefined.
    return _ejs_undefined;
}

// ECMA262 25.4.1.3 CreateResolvingFunctions ( promise ) 
static void
CreateResolvingFunctions(ejsval promise, ejsval* out_resolve, ejsval* out_reject)
{
    // 1. Let alreadyResolved be a new Record { [[value]]: false }.
    ejsval resolvingFunctions_env = _ejs_closureenv_new(2);
    *_ejs_closureenv_get_slot_ref(resolvingFunctions_env, 0) = _ejs_false;
    *_ejs_closureenv_get_slot_ref(resolvingFunctions_env, 1) = promise;

    // 2. Let resolve be a new built-in function object as defined in Promise Resolve Functions (25.4.1.4). 
    // 3. Set the [[Promise]] internal slot of resolve to promise. 
    // 4. Set the [[AlreadyResolved]] internal slot of resolve to alreadyResolved. 
    *out_resolve = _ejs_function_new_anon(resolvingFunctions_env, resolve);

    // 5. Let reject be a new built-in function object as defined in Promise Reject Functions (25.4.1.3.1). 
    // 6. Set the [[Promise]] internal slot of reject to promise. 
    // 7. Set the [[AlreadyResolved]] internal slot of reject to alreadyResolved. 
    *out_reject = _ejs_function_new_anon(resolvingFunctions_env, reject);

    // 8. Return a new Record { [[Resolve]]: resolve, [[Reject]]: reject }. 
}


// ECMA262 25.4.1.6.1 CreatePromiseCapabilityRecord( promise, constructor ) 
static ejsval
CreatePromiseCapabilityRecord (ejsval promise, ejsval constructor)
{
    // 1. Assert: promise is an uninitialized object created as if by invoking @@create on constructor.
    // 2. Assert: IsConstructor(constructor) is true.
    // 3. Let promiseCapability be a new PromiseCapability { [[Promise]]: promise, [[Resolve]]: undefined, [[Reject]]: undefined }.
    ejsval promiseCapability = EJS_CAPABILITY_NEW();
    EJS_CAPABILITY_SET_PROMISE(promiseCapability, promise);
    EJS_CAPABILITY_SET_RESOLVE(promiseCapability, _ejs_undefined);
    EJS_CAPABILITY_SET_REJECT(promiseCapability, _ejs_undefined);

    // 4. Let executor be a new built-in function object as defined in GetCapabilitiesExecutor Functions (25.4.1.5.1).
    ejsval executor = _ejs_function_new_anon (promiseCapability, capabilitiesExecutor);

    // 5. Set the [[Capability]] internal slot of executor to promiseCapability.

    // 6. Let constructorResult be the result of calling the [[Call]] internal method of constructor, passing promise and (executor) as the arguments.
    // 7. ReturnIfAbrupt(constructorResult).
    ejsval constructorResult = _ejs_invoke_closure (constructor, &promise, 1, &executor, EJS_CALL_FLAGS_CALL, _ejs_undefined);

    // 8. If IsCallable(promiseCapability.[[Resolve]]) is false, then throw a TypeError exception. 
    if (!IsCallable(EJS_CAPABILITY_GET_RESOLVE(promiseCapability)))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, ""); // XXX
    // 9. If IsCallable(promiseCapability.[[Reject]]) is false, then throw a TypeError exception. 
    if (!IsCallable(EJS_CAPABILITY_GET_REJECT(promiseCapability)))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, ""); // XXX
    // 10. If Type(constructorResult) is Object and SameValue(promise, constructorResult) is false, then throw a TypeError exception. 
    if (EJSVAL_IS_OBJECT(constructorResult) && !SameValue(promise, constructorResult))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "1"); // XXX
        
    // 11. Return promiseCapability. 
    return promiseCapability;
}

// ES2015, June 2015
// 25.4.1.5 NewPromiseCapability ( C )
static ejsval
NewPromiseCapability(ejsval C)
{
    // 1. If IsConstructor(C) is false, throw a TypeError exception.
    if (!IsConstructor(C))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "1"); // XXX

    // 2. NOTE C is assumed to be a constructor function that supports the parameter conventions of the Promise constructor (see 25.4.3.1).
    // 3. Let promiseCapability be a new PromiseCapability { [[Promise]]: undefined, [[Resolve]]: undefined, [[Reject]]: undefined }.
    ejsval promiseCapability = EJS_CAPABILITY_NEW();
    EJS_CAPABILITY_SET_PROMISE(promiseCapability, _ejs_undefined);
    EJS_CAPABILITY_SET_RESOLVE(promiseCapability, _ejs_undefined);
    EJS_CAPABILITY_SET_REJECT(promiseCapability, _ejs_undefined);

    // 4. Let executor be a new built-in function object as defined in GetCapabilitiesExecutor Functions (25.4.1.5.1).
    // 5. Set the [[Capability]] internal slot of executor to promiseCapability.
    ejsval executor = _ejs_function_new_anon (promiseCapability, capabilitiesExecutor);

    // 6. Let promise be Construct(C, «executor»).
    // 7. ReturnIfAbrupt(promise).
    ejsval promise = Construct(C, C, 1, &executor);

    // 8. If IsCallable(promiseCapability.[[Resolve]]) is false, throw a TypeError exception.
    if (!IsCallable(EJS_CAPABILITY_GET_RESOLVE(promiseCapability)))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "1"); // XXX

    // 9. If IsCallable(promiseCapability.[[Reject]]) is false, throw a TypeError exception.
    if (!IsCallable(EJS_CAPABILITY_GET_REJECT(promiseCapability)))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "1"); // XXX

    // 10. Set promiseCapability.[[Promise]] to promise.
    EJS_CAPABILITY_SET_PROMISE(promiseCapability, promise);
    
    // 11. Return promiseCapability.
    return promiseCapability;
#if false
    // 1. If IsConstructor(C) is false, throw a TypeError exception. 
    // 2. Assert: C is a constructor function that supports the parameter conventions of the Promise constructor (see 25.4.3.1). 
    // 3. Let promise be CreateFromConstructor(C). 
    ejsval creator = Get(C, _ejs_Symbol_create);
    // 4. ReturnIfAbrupt(promise). 
    ejsval promise = _ejs_invoke_closure (creator, &_ejs_undefined, 0, NULL);

    // 5. If Type(promise) is not Object, then throw a TypeError exception. 
    if (!EJSVAL_IS_OBJECT(promise))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "promise constructor returned non-object");

    // 6. Return CreatePromiseCapabilityRecord(promise, C). 
    return CreatePromiseCapabilityRecord(promise, C);
#endif
}

// 25.4.2.1 PromiseReactionTask ( reaction, argument ) 
static void
PromiseReactionTask (EJSPromiseReaction* reaction, ejsval argument)
{
    // 1. Assert: reaction is a PromiseReaction Record. 
    // 2. Let promiseCapability be reaction.[[Capabilities]]. 
    ejsval promiseCapability = reaction->capabilities;

    // 3. Let handler be reaction.[[Handler]]. 
    ejsval handler = reaction->handler;

    EJSBool success;
    ejsval handlerResult = _ejs_undefined;

    // 4. If handler is "Identity", then let handlerResult be NormalCompletion(argument). 
    if (SameValue(handler, _ejs_identity_function)) {
        success = EJS_TRUE;
        handlerResult = argument;
    }
    // 5. Else If handler is "Thrower", then let handlerResult be Completion{[[type]]: throw, [[value]]: argument, [[target]]: empty}. 
    if (SameValue(handler, _ejs_thrower_function)) {
        success = EJS_FALSE;
        handlerResult = argument;
    }
    // 6. Else, Let let handlerResult be the result of calling the [[Call]] internal method of handler passing undefined as thisArgument and (argument) as argumentsList. 
    else {
        ejsval undef_this = _ejs_undefined;
        success = _ejs_invoke_closure_catch(&handlerResult, handler, &undef_this, 1, &argument, EJS_CALL_FLAGS_CALL, _ejs_undefined);
    }

    ejsval status;

    // 7. If handlerResult is an abrupt completion, then 
    if (!success) {
        ejsval undef_this = _ejs_undefined;
        //    a. Let status be the result of calling the [[Call]] internal method of promiseCapability.[[Reject]] passing undefined as thisArgument and (handlerResult.[[value]]) as argumentsList. 
        success = _ejs_invoke_closure_catch(&status, EJS_CAPABILITY_GET_REJECT(promiseCapability), &undef_this, 1, &handlerResult, EJS_CALL_FLAGS_CALL, _ejs_undefined);

        //    b. NextTask status. 
        return;//EJS_NOT_IMPLEMENTED();
    }
    // 8. Let handlerResult be handlerResult.[[value]]. 
    // 9. Let status be the result of calling the [[Call]] internal method of promiseCapability.[[Resolve]] passing undefined as thisArgument and (handlerResult) as argumentsList. 
    ejsval undef_this = _ejs_undefined;
    success = _ejs_invoke_closure_catch(&status, EJS_CAPABILITY_GET_RESOLVE(promiseCapability), &undef_this, 1, &handlerResult, EJS_CALL_FLAGS_CALL, _ejs_undefined);
    
    // 10. NextTask status. 
}

// 25.4.2.2 PromiseResolveThenableTask ( promiseToResolve, thenable, then) 
static void
PromiseResolveThenableTask (ejsval promiseToResolve, ejsval thenable, ejsval then)
{
    // 1. Let resolvingFunctions be CreateResolvingFunctions(promiseToResolve).
    ejsval resolvingFunctions_resolve;
    ejsval resolvingFunctions_reject;
    CreateResolvingFunctions(promiseToResolve, &resolvingFunctions_resolve, &resolvingFunctions_reject);

    // 2. Let thenCallResult be the result of calling the [[Call]] internal method of then passing thenable as the thisArgument and (resolvingFunctions.[[Resolve]], resolvingFunctions.[[Reject]]) as argumentsList. 
    ejsval thenCallResult;
    ejsval args[] = { resolvingFunctions_resolve, resolvingFunctions_reject };
    EJSBool success = _ejs_invoke_closure_catch(&thenCallResult, then, &thenable, 2, args, EJS_CALL_FLAGS_CALL, _ejs_undefined);

    // 3. If thenCallResult is an abrupt completion, 
    if (!success) {
        //    a. Let status be the result of calling the [[Call]] internal method of resolvingFunctions.[[Reject]] passing undefined as the thisArgument and (thenCallResult.[[value]]) as argumentsList. 
        ejsval status;
        ejsval undef_this = _ejs_undefined;
        success = _ejs_invoke_closure_catch(&status, resolvingFunctions_reject, &undef_this, 1, &thenCallResult, EJS_CALL_FLAGS_CALL, _ejs_undefined);
        
        //    b. NextTask status. 
        EJS_NOT_IMPLEMENTED();
    }
    // 4. NextTask thenCallResult. 
    EJS_NOT_IMPLEMENTED();
}

// ES2015, June 2015
// 25.4.3.1 Promise ( executor ) 
static EJS_NATIVE_FUNC(_ejs_Promise_impl) {
    ejsval executor = _ejs_undefined;
    if (argc > 0) executor = args[0];

    // 1. If NewTarget is undefined, throw a TypeError exception.
    if (EJSVAL_IS_UNDEFINED(newTarget))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Promise constructor must be invoked with 'new'");
        
    // 2. If IsCallable(executor) is false, throw a TypeError exception.
    if (!IsCallable(executor))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "executor is not callable");

    // 3. Let promise be OrdinaryCreateFromConstructor(NewTarget, "%PromisePrototype%", «[[PromiseState]], [[PromiseResult]], [[PromiseFulfillReactions]], [[PromiseRejectReactions]]» ).
    // 4. ReturnIfAbrupt(promise).
    ejsval promise = OrdinaryCreateFromConstructor(newTarget, _ejs_Promise_prototype, &_ejs_Promise_specops);
    *_this = promise;
    EJSPromise* _promise = EJSVAL_TO_PROMISE(promise);

    // 5. Set promise's [[PromiseState]] internal slot to "pending".
    _promise->state = PROMISE_STATE_PENDING;

    // 6. Set promise's [[PromiseFulfillReactions]] internal slot to a new empty List.
    _promise->fulfillReactions = NULL;

    // 7. Set promise's [[PromiseRejectReactions]] internal slot to a new empty List.
    _promise->rejectReactions = NULL;

    // 8. Let resolvingFunctions be CreateResolvingFunctions(promise).
    ejsval resolvingFunctions_resolve;
    ejsval resolvingFunctions_reject;

    CreateResolvingFunctions(promise, &resolvingFunctions_resolve, &resolvingFunctions_reject);

    // 9. Let completion be Call(executor, undefined, «resolvingFunctions.[[Resolve]], resolvingFunctions.[[Reject]]»).
    ejsval completion;
    ejsval executor_args[] = { resolvingFunctions_resolve, resolvingFunctions_reject };

    ejsval undef_this = _ejs_undefined;
    EJSBool success = _ejs_invoke_closure_catch(&completion, executor, &undef_this, 2, executor_args, EJS_CALL_FLAGS_CALL, _ejs_undefined);

    // 10. If completion is an abrupt completion, then
    if (!success) {
        // a. Let status be Call(resolvingFunctions.[[Reject]], undefined, «completion.[[value]]»).
        // b. ReturnIfAbrupt(status).
        ejsval undef_this = _ejs_undefined;
        _ejs_invoke_closure(resolvingFunctions_reject, &undef_this, 1, &completion, EJS_CALL_FLAGS_CALL, _ejs_undefined);
    }
    // 11. Return promise.
    return promise;
}

// ECMA262 25.4.5.1 Promise.prototype.catch ( onRejected ) 
static EJS_NATIVE_FUNC(_ejs_Promise_prototype_catch) {
    ejsval onRejected = _ejs_undefined;
    if (argc > 0) onRejected = args[0];

    // 1. Let promise be the this value. 
    ejsval promise = *_this;

    // 2. Return Invoke(promise, "then", (undefined, onRejected)). 
    ejsval thenargs[] = { _ejs_undefined, onRejected };
    return _ejs_invoke_closure(Get(promise, _ejs_atom_then), &promise, 2, thenargs, EJS_CALL_FLAGS_CALL, _ejs_undefined);
}

static EJSPromiseReaction*
_ejs_promise_reaction_new (ejsval capability, ejsval handler)
{
    EJSPromiseReaction* reaction = (EJSPromiseReaction*)malloc(sizeof(EJSPromiseReaction));
    reaction->capabilities = capability;
    reaction->handler = handler;
    return reaction;
}

// ECMA262 25.4.5.3 Promise.prototype.then ( onFulfilled , onRejected ) 
static EJS_NATIVE_FUNC(_ejs_Promise_prototype_then) {
    ejsval onFulfilled = _ejs_undefined;
    ejsval onRejected = _ejs_undefined;
    if (argc > 0) onFulfilled = args[0];
    if (argc > 1) onRejected  = args[1];

    // 1. Let promise be the this value. 
    ejsval promise = *_this;

    // 2. If IsPromise(promise) is false, throw a TypeError exception. 
    if (!EJSVAL_IS_PROMISE(promise))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "this is not a promise");
        
    EJSPromise* _promise = EJSVAL_TO_PROMISE(promise);

    // 3. If IsCallable(onFulfilled) is undefined or nullfalse, then 
    if (!IsCallable(onFulfilled)) {
        //    a. Let onFulfilled be "Identity" a new Identity Function (see 25.4.5.3.1).
        onFulfilled = _ejs_identity_function; // XXX does it really need to be a newly instantiated one? realm-specific?  we don't instantiate a new one
    }

    // 4. If IsCallable(onRejected) is undefined or nullfalse, then 
    if (!IsCallable(onRejected)) {
        //    a. Let onRejected be "Thrower"a new Thrower Function (see 25.4.5.3.3). 
        onRejected = _ejs_thrower_function; // XXX does it really need to be a newly instantiated one?  realm-specific?  we don't instantiate a new one
    }
    // 5. If IsCallable(onFulfilled) is false or if IsCallable(onRejected) is false, then throw a TypeError exception. 
    if (!IsCallable(onFulfilled) || !IsCallable(onRejected)) {
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "1: shouldn't happen.  runtime error?");
    }

    // 6. Let C be Get(promise, "constructor"). 
    // 7. ReturnIfAbrupt(C). 
    ejsval C = Get(promise, _ejs_atom_constructor);

    // 8. Let promiseCapability be NewPromiseCapability(C). 
    // 9. ReturnIfAbrupt(promiseCapability). 
    ejsval promiseCapability = NewPromiseCapability(C);

    // 12. If the value of promise's [[PromiseState]] internal slot is "pending", 
    if (_promise->state == PROMISE_STATE_PENDING) {
        // 10. Let fulfillReaction be the PromiseReaction { [[Capabilities]]: promiseCapability, [[Handler]]: onFulfilled }.
        EJSPromiseReaction* fulfillReaction = _ejs_promise_reaction_new (promiseCapability, onFulfilled);
        // 11. Let rejectReaction  be the PromiseReaction { [[Capabilities]]: promiseCapability, [[Handler]]: onRejected  }.
        EJSPromiseReaction* rejectReaction  = _ejs_promise_reaction_new (promiseCapability, onRejected);

        //        a. Append fulfillReaction as the last element of the List that is the value of promise's [[PromiseFulfillReactions]] internal slot. 
        EJS_LIST_APPEND(EJSPromiseReaction, fulfillReaction, _promise->fulfillReactions);
        //        b. Append rejectReaction as the last element of the List that is the value of promise's [[PromiseRejectReactions]] internal slot. 
        EJS_LIST_APPEND(EJSPromiseReaction, rejectReaction, _promise->rejectReactions);
    }
    // 13. Else if the value of promise's [[PromiseState]] internal slot is "fulfilled", 
    else if (_promise->state == PROMISE_STATE_FULFILLED) {
        // 10. Let fulfillReaction be the PromiseReaction { [[Capabilities]]: promiseCapability, [[Handler]]: onFulfilled }.
        EJSPromiseReaction* fulfillReaction = _ejs_promise_reaction_new (promiseCapability, onFulfilled);

        //     a. Let value be the value of promise's [[PromiseResult]] internal slot. 
        ejsval value = _promise->result;
        //     b. Perform EnqueueTask("PromiseTasks", PromiseReactionTask, (fulfillReaction, value)). 
        EnqueuePromiseReactionTask (fulfillReaction, value);
    }
    // 14. Else if the value of promise's [[PromiseState]] internal slot is "rejected", 
    else if (_promise->state == PROMISE_STATE_REJECTED) {
        // 11. Let rejectReaction  be the PromiseReaction { [[Capabilities]]: promiseCapability, [[Handler]]: onRejected  }.
        EJSPromiseReaction* rejectReaction  = _ejs_promise_reaction_new (promiseCapability, onRejected);

        //     a. Let reason be the value of promise's [[PromiseResult]] internal slot. 
        ejsval reason = _promise->result;
        //     b. Perform EnqueueTask("PromiseTasks", PromiseReactionTask, (rejectReaction, reason)). 
        EnqueuePromiseReactionTask (rejectReaction, reason);
    }
    else {
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "2: shouldn't happen.  runtime error?");
    }
    // 15. Return promiseCapability.[[Promise]]. 
    return EJS_CAPABILITY_GET_PROMISE(promiseCapability);
}

static EJS_NATIVE_FUNC(_ejs_Promise_get_species) {
    return _ejs_Promise;
}

// ECMA262 25.4.4.1.1 Promise.all Resolve Element Functions
static EJS_NATIVE_FUNC(resolve_element) {
    // 1. If the value of F's [[AlreadyCalled]] internal slot is true, then return undefined. 
    if (EJSVAL_TO_BOOLEAN(EJS_RESOLVEELEMENT_GET_ALREADY_CALLED(env)))
        return _ejs_undefined;

    // 2. Set the value of F's [[AlreadyCalled]] internal slot to true. 
    EJS_RESOLVEELEMENT_SET_ALREADY_CALLED(env, _ejs_true);

    // 3. Let index be the value of F's [[Index]] internal slot. 
    ejsval index = EJS_RESOLVEELEMENT_GET_INDEX(env);

    // 4. Let values be the value of F's [[Values]] internal slot. 
    ejsval values = EJS_RESOLVEELEMENT_GET_VALUES(env);

    // 5. Let promiseCapability be the value of F's [[Capabilities]] internal slot. 
    ejsval promiseCapability = EJS_RESOLVEELEMENT_GET_CAPABILITIES(env);

    // 6. Let remainingElementsCount be the value of F's [[RemainingElements]] internal slot. 

    // XXX remainingElementsCount needs to be a boxed value so that resolve_element functions can update it
    // XXX maybe an EJSNumber?  supposed to be immutable, but we could fudge.. or another closure.

    // 7. Let result be CreateDataProperty(values, ToString(index), x). 
    // 8. IfAbruptRejectPromise(result, promiseCapability). 
    // 9. Set remainingElementsCount.[[value]] to remainingElementsCount.[[value]] - 1. 
    // 10. If remainingElementsCount.[[value]] is 0, 
    //     a. Return the result of calling the [[Call]] internal method of promiseCapability.[[Resolve]] with undefined as thisArgument and (values) as argumentsList. 
    // 11. Return undefined.
    return _ejs_undefined;
}

// ECMA262 25.4.4.1 Promise.all ( iterable )
static EJS_NATIVE_FUNC(_ejs_Promise_all) {
    EJSBool success;

    ejsval iterable = _ejs_undefined;
    if (argc > 0) iterable = args[0];

    // 1. Let C be the this value. 
    ejsval C = *_this;

    // 2. Let promiseCapability be NewPromiseCapability(C). 
    // 3. ReturnIfAbrupt(promiseCapability). 
    ejsval promiseCapability = NewPromiseCapability(C);

    // 4. Let iterator be GetIterator(iterable). 
    ejsval iterator;
    success = GetIterator_internal(&iterator, iterable);

    // 5. IfAbruptRejectPromise(iterator, promiseCapability). 
    if (!success) {
        ejsval undef_this = _ejs_undefined;
        _ejs_invoke_closure(EJS_CAPABILITY_GET_REJECT(promiseCapability), &undef_this, 1, &iterator, EJS_CALL_FLAGS_CALL, _ejs_undefined);
        return EJS_CAPABILITY_GET_PROMISE(promiseCapability);
    }

    // 6. Let values be ArrayCreate(0). 
    ejsval values = _ejs_array_new(0, EJS_FALSE);

    // 7. Let remainingElementsCount be a new Record { [[value]]: 1 }. 
    int remainingElementsCount = 1;

    // 8. Let index be 0. 
    int index = 0;

    // 9. Repeat 
    while (EJS_TRUE) {
        //    a. Let next be IteratorStep(iterator). 
        ejsval next;
        success = IteratorStep_internal(&next, iterator);

        //    b. IfAbruptRejectPromise(next, promiseCapability). 
        if (!success) {
            ejsval undef_this = _ejs_undefined;
            _ejs_invoke_closure(EJS_CAPABILITY_GET_REJECT(promiseCapability), &undef_this, 1, &next, EJS_CALL_FLAGS_CALL, _ejs_undefined);
            return EJS_CAPABILITY_GET_PROMISE(promiseCapability);
        }
        //    c. If next is false, 
        if (EJSVAL_IS_BOOLEAN(next) && !EJSVAL_TO_BOOLEAN(next)) {
            //       i. Set remainingElementsCount.[[value]] to remainingElementsCount.[[value]] - 1. 
            remainingElementsCount --;
            //       ii. If remainingElementsCount.[[value]] is 0, 
            if (remainingElementsCount == 0) {
                //           1. Let resolveResult be the result of calling the [[Call]] internal method of promiseCapability.[[Resolve]] with undefined as thisArgument and (values) as argumentsList. 
                ejsval resolveResult;
                ejsval undef_this = _ejs_undefined;
                success = _ejs_invoke_closure_catch(&resolveResult, EJS_CAPABILITY_GET_RESOLVE(promiseCapability), &undef_this, 1, &values, EJS_CALL_FLAGS_CALL, _ejs_undefined);
                //           2. ReturnIfAbrupt(resolveResult). 
                if (!success) {
                    ejsval undef_this = _ejs_undefined;
                    _ejs_invoke_closure(EJS_CAPABILITY_GET_REJECT(promiseCapability), &undef_this, 1, &resolveResult, EJS_CALL_FLAGS_CALL, _ejs_undefined);
                    return EJS_CAPABILITY_GET_PROMISE(promiseCapability);
                }
            }
            //       iii. Return promiseCapability.[[Promise]]. 
            return EJS_CAPABILITY_GET_PROMISE(promiseCapability);
        }

        //    d. Let nextValue be IteratorValue(next). 
        ejsval nextValue;
        success = IteratorValue_internal(&nextValue, next);
        //    e. IfAbruptRejectPromise(nextValue, promiseCapability). 
        if (!success) {
            ejsval undef_this = _ejs_undefined;
            _ejs_invoke_closure(EJS_CAPABILITY_GET_REJECT(promiseCapability), &undef_this, 1, &nextValue, EJS_CALL_FLAGS_CALL, _ejs_undefined);
            return EJS_CAPABILITY_GET_PROMISE(promiseCapability);
        }
        //    f. Let nextPromise be Invoke(C, "resolve", (nextValue)). 
        ejsval nextPromise;
        success =  _ejs_invoke_closure_catch (&nextPromise, Get(C, _ejs_atom_resolve), &C, 1, &nextValue, EJS_CALL_FLAGS_CALL, _ejs_undefined);
        
        //    g. IfAbruptRejectPromise(nextPromise, promiseCapability).
        if (!success) {
            ejsval undef_this = _ejs_undefined;
            _ejs_invoke_closure(EJS_CAPABILITY_GET_REJECT(promiseCapability), &undef_this, 1, &nextPromise, EJS_CALL_FLAGS_CALL, _ejs_undefined);
            return EJS_CAPABILITY_GET_PROMISE(promiseCapability);
        }
 
        //    h. Let resolveElement be a new built-in function object as defined in Promise.all Resolve Element Functions. 
        ejsval resolvingElement_env = _ejs_closureenv_new(6);
        ejsval resolveElement = _ejs_function_new_anon(resolvingElement_env, resolve_element);

        //    i. Set the [[AlreadyCalled]] internal slot of resolveElement to false. 
        EJS_RESOLVEELEMENT_SET_ALREADY_CALLED(resolvingElement_env, _ejs_false);
        //    j. Set the [[Index]] internal slot of resolveElement to index. 
        EJS_RESOLVEELEMENT_SET_INDEX(resolvingElement_env, NUMBER_TO_EJSVAL(index));
        //    k. Set the [[Values]] internal slot of resolveElement to values. 
        EJS_RESOLVEELEMENT_SET_VALUES(resolvingElement_env, values);
        //    l. Set the [[Capabilities]] internal slot of resolveElement to promiseCapability. 
        EJS_RESOLVEELEMENT_SET_CAPABILITIES(resolvingElement_env, promiseCapability);
        //    m. Set the [[RemainingElements]] internal slot of resolveElement to remainingElementsCount. 
        EJS_RESOLVEELEMENT_SET_REMAINING_ELEMENTS(resolvingElement_env, NUMBER_TO_EJSVAL(remainingElementsCount));
        //    n. Set remainingElementsCount.[[value]] to remainingElementsCount.[[value]] + 1. 
        remainingElementsCount++;
        //    o. Let result be Invoke(nextPromise, "then", (resolveElement, promiseCapability.[[Reject]])). 
        ejsval thenargs[] = { resolveElement, EJS_CAPABILITY_GET_REJECT(promiseCapability) };
        ejsval result;
        success =  _ejs_invoke_closure_catch (&result, Get(nextPromise, _ejs_atom_then), &nextPromise, 2, thenargs, EJS_CALL_FLAGS_CALL, _ejs_undefined);
        //    p. IfAbruptRejectPromise(result, promiseCapability). 
        if (!success) {
            ejsval undef_this = _ejs_undefined;
            _ejs_invoke_closure(EJS_CAPABILITY_GET_REJECT(promiseCapability), &undef_this, 1, &result, EJS_CALL_FLAGS_CALL, _ejs_undefined);
            return EJS_CAPABILITY_GET_PROMISE(promiseCapability);
        }
        //    q. Set index to index + 1.
        index ++;
    }
}

// ECMA262 25.4.4.3 Promise.race ( iterable ) 
static EJS_NATIVE_FUNC(_ejs_Promise_race) {
    EJSBool success;

    ejsval iterable = _ejs_undefined;
    if (argc > 0) iterable = args[0];

    // 1. Let C be the this value. 
    ejsval C = *_this;

    // 2. Let promiseCapability be NewPromiseCapability(C). 
    // 3. ReturnIfAbrupt(promiseCapability). 
    ejsval promiseCapability = NewPromiseCapability(C);

    // 4. Let iterator be GetIterator(iterable). 
    ejsval iterator;
    success = GetIterator_internal(&iterator, iterable);
    
    // 5. IfAbruptRejectPromise(iterator, promiseCapability).
    if (!success) {
        ejsval undef_this = _ejs_undefined;
        _ejs_invoke_closure(EJS_CAPABILITY_GET_REJECT(promiseCapability), &undef_this, 1, &iterator, EJS_CALL_FLAGS_CALL, _ejs_undefined);
        return EJS_CAPABILITY_GET_PROMISE(promiseCapability);
    }

    // 6. Repeat 
    while (EJS_TRUE) {
        ejsval next;

        //    a. Let next be IteratorStep(iterator). 
        EJSBool success = IteratorStep_internal(&next, iterator);
        //    b. IfAbruptRejectPromise(next, promiseCapability). 
        if (!success) {
            ejsval undef_this = _ejs_undefined;
            _ejs_invoke_closure(EJS_CAPABILITY_GET_REJECT(promiseCapability), &undef_this, 1, &next, EJS_CALL_FLAGS_CALL, _ejs_undefined);
            return EJS_CAPABILITY_GET_PROMISE(promiseCapability);
        }

        //    c. If next is false, return promiseCapability.[[Promise]]. 
        if (EJSVAL_IS_BOOLEAN(next) && !EJSVAL_TO_BOOLEAN(next))
            return EJS_CAPABILITY_GET_PROMISE(promiseCapability);

        //    d. Let nextValue be IteratorValue(next). 
        ejsval nextValue;
        success = IteratorValue_internal(&nextValue, next);

        //    e. IfAbruptRejectPromise(nextValue, promiseCapability). 
        if (!success) {
            ejsval undef_this = _ejs_undefined;
            _ejs_invoke_closure(EJS_CAPABILITY_GET_REJECT(promiseCapability), &undef_this, 1, &nextValue, EJS_CALL_FLAGS_CALL, _ejs_undefined);
            return EJS_CAPABILITY_GET_PROMISE(promiseCapability);
        }

        //    f. Let nextPromise be Invoke(C, "resolve", (nextValue)). 
        ejsval nextPromise;
        success = _ejs_invoke_closure_catch(&nextPromise, Get(C, _ejs_atom_resolve), &C, 1, &nextValue, EJS_CALL_FLAGS_CALL, _ejs_undefined);
        //    g. IfAbruptRejectPromise(nextPromise, promiseCapability). 
        if (!success) {
            ejsval undef_this = _ejs_undefined;
            _ejs_invoke_closure(EJS_CAPABILITY_GET_REJECT(promiseCapability), &undef_this, 1, &nextPromise, EJS_CALL_FLAGS_CALL, _ejs_undefined);
            return EJS_CAPABILITY_GET_PROMISE(promiseCapability);
        }

        //    h. Let result be Invoke(nextPromise, "then", (promiseCapability.[[Resolve]], promiseCapability.[[Reject]])). 
        ejsval result;
        ejsval args[] = { EJS_CAPABILITY_GET_RESOLVE(promiseCapability), EJS_CAPABILITY_GET_REJECT(promiseCapability) };
        success = _ejs_invoke_closure_catch(&result, Get(nextPromise, _ejs_atom_then), &nextPromise, 2, args, EJS_CALL_FLAGS_CALL, _ejs_undefined);

        //    i. IfAbruptRejectPromise(result, promiseCapability). 
        if (!success) {
            ejsval undef_this = _ejs_undefined;
            _ejs_invoke_closure(EJS_CAPABILITY_GET_REJECT(promiseCapability), &undef_this, 1, &result, EJS_CALL_FLAGS_CALL, _ejs_undefined);
            return EJS_CAPABILITY_GET_PROMISE(promiseCapability);
        }
    }
}

// ECMA262 25.4.4.4 Promise.reject ( r )
static EJS_NATIVE_FUNC(_ejs_Promise_reject) {
    ejsval r = _ejs_undefined;
    if (argc > 0) r = args[0];

    // 1. Let C be the this value. 
    ejsval C = *_this;

    // 2. Let promiseCapability be NewPromiseCapability(C). 
    // 3. ReturnIfAbrupt(promiseCapability). 
    ejsval promiseCapability = NewPromiseCapability(C);

    // 4. Let rejectResult be the result of calling the [[Call]] internal method of promiseCapability.[[Reject]] with undefined as thisArgument and (r) as argumentsList. 
    // 5. ReturnIfAbrupt(rejectResult). 
    ejsval undef_this = _ejs_undefined;
    _ejs_invoke_closure(EJS_CAPABILITY_GET_REJECT(promiseCapability), &undef_this, 1, &r, EJS_CALL_FLAGS_CALL, _ejs_undefined);

    // 6. Return promiseCapability.[[Promise]]. 
    return EJS_CAPABILITY_GET_PROMISE(promiseCapability);
}

// ECMA262 25.4.4.5 Promise.resolve ( x )
static EJS_NATIVE_FUNC(_ejs_Promise_resolve) {
    ejsval x = _ejs_undefined;
    if (argc > 0) x = args[0];

    // 1. Let C be the this value. 
    ejsval C = *_this;
    // 2. If IsPromise(x) is true, 
    if (EJSVAL_IS_PROMISE(x)) {
        EJSPromise* _x = EJSVAL_TO_PROMISE(x);
        //    a. Let constructor be the value of x's [[PromiseConstructor]] internal slot. 
        //    b. If SameValue(constructor, C) is true, return x. 
        if (SameValue(_x->constructor, C))
            return x;
    }

    // 3. Let promiseCapability be NewPromiseCapability(C). 
    // 4. ReturnIfAbrupt(promiseCapability). 
    ejsval promiseCapability = NewPromiseCapability(C);

    // 5. Let resolveResult be the result of calling the [[Call]] internal method of promiseCapability.[[Resolve]] with undefined as thisArgument and (x) as argumentsList. 
    // 6. ReturnIfAbrupt(resolveResult). 
    ejsval undef_this = _ejs_undefined;
    _ejs_invoke_closure(EJS_CAPABILITY_GET_RESOLVE(promiseCapability), &undef_this, 1, &x, EJS_CALL_FLAGS_CALL, _ejs_undefined);

    // 7. Return promiseCapability.[[Promise]]. 
    return EJS_CAPABILITY_GET_PROMISE(promiseCapability);
}

void
_ejs_promise_init(ejsval global)
{
    _ejs_Promise = _ejs_function_new_without_proto (_ejs_null, _ejs_atom_Promise, _ejs_Promise_impl);
    _ejs_object_setprop (global, _ejs_atom_Promise, _ejs_Promise);

    _ejs_gc_add_root (&_ejs_Promise_prototype);
    _ejs_Promise_prototype = _ejs_object_new(_ejs_null, &_ejs_Object_specops);
    _ejs_object_setprop (_ejs_Promise,       _ejs_atom_prototype,  _ejs_Promise_prototype);
    _ejs_object_define_value_property (_ejs_Promise_prototype, _ejs_atom_constructor, _ejs_Promise,
                                       EJS_PROP_NOT_ENUMERABLE | EJS_PROP_CONFIGURABLE | EJS_PROP_WRITABLE);

#define PROTO_METHOD(x) EJS_INSTALL_ATOM_FUNCTION_FLAGS (_ejs_Promise_prototype, x, _ejs_Promise_prototype_##x, EJS_PROP_NOT_ENUMERABLE)
#define OBJ_METHOD(x) EJS_INSTALL_ATOM_FUNCTION_FLAGS (_ejs_Promise, x, _ejs_Promise_##x, EJS_PROP_NOT_ENUMERABLE)

#define PROTO_METHOD_LEN(x,l) EJS_INSTALL_ATOM_FUNCTION_LEN_FLAGS (_ejs_Promise_prototype, x, _ejs_Promise_prototype_##x, l, EJS_PROP_NOT_ENUMERABLE)
#define OBJ_METHOD_LEN(x,l) EJS_INSTALL_ATOM_FUNCTION_LEN_FLAGS (_ejs_Promise, x, _ejs_Promise_##x, l, EJS_PROP_NOT_ENUMERABLE)

    PROTO_METHOD(catch);
    PROTO_METHOD(then);

    _ejs_object_define_value_property (_ejs_Promise_prototype, _ejs_Symbol_toStringTag, _ejs_atom_Promise, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_WRITABLE | EJS_PROP_CONFIGURABLE);

    OBJ_METHOD_LEN(all, 1);
    OBJ_METHOD(race);
    OBJ_METHOD(reject);
    OBJ_METHOD(resolve);

#undef PROTO_METHOD

    EJS_INSTALL_SYMBOL_GETTER(_ejs_Promise, species, _ejs_Promise_get_species);

    _ejs_gc_add_root(&_ejs_identity_function);
    _ejs_identity_function = _ejs_function_new_anon(_ejs_undefined, identity);
    _ejs_gc_add_root(&_ejs_thrower_function);
    _ejs_thrower_function = _ejs_function_new_anon(_ejs_undefined, thrower);
}

static EJSObject*
_ejs_promise_specop_allocate()
{
    return (EJSObject*)_ejs_gc_new (EJSPromise);
}

static void
_ejs_promise_specop_scan (EJSObject* obj, EJSValueFunc scan_func)
{
    EJSPromise* promise = (EJSPromise*)obj;

    scan_func(promise->result);
    scan_func(promise->constructor);

    for (EJSPromiseReaction* reaction = promise->fulfillReactions; reaction; reaction = reaction->next) {
        scan_func(reaction->capabilities);
        scan_func(reaction->handler);
    }

    for (EJSPromiseReaction* reaction = promise->rejectReactions; reaction; reaction = reaction->next) {
        scan_func(reaction->capabilities);
        scan_func(reaction->handler);
    }

    _ejs_Object_specops.Scan (obj, scan_func);
}

EJS_DEFINE_CLASS(Promise,
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
                 _ejs_promise_specop_allocate,
                 OP_INHERIT, // [[Finalize]]
                 _ejs_promise_specop_scan
                 )
