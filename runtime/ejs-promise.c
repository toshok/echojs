/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include "ejs-ops.h"
#include "ejs-value.h"
#include "ejs-function.h"
#include "ejs-promise.h"
#include "ejs-error.h"
#include "ejs-string.h"
#include "ejs-proxy.h"
#include "ejs-symbol.h"

#include <string.h>

ejsval _ejs_Promise EJSVAL_ALIGNMENT;
ejsval _ejs_Promise_prototype EJSVAL_ALIGNMENT;

ejsval _ejs_identity_function EJSVAL_ALIGNMENT;
ejsval _ejs_thrower_function EJSVAL_ALIGNMENT;

// ECMA262 25.4.5.4.0 Identity Functions 
static ejsval
identity(ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval x = _ejs_undefined;
    if (argc > 0) x = args[0];
    return x;
}

// ECMA262 25.4.5.25.0 Thrower Functions 
static ejsval
thrower(ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval x = _ejs_undefined;
    if (argc > 0) x = args[0];
    _ejs_throw(x);
    EJS_NOT_REACHED();
}

static ejsval
NewPromiseCapability(ejsval C)
{
    EJS_NOT_IMPLEMENTED();
}


static ejsval
_ejs_Promise_impl (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    EJS_NOT_IMPLEMENTED();
}

// ECMA262 25.4.5.1 Promise.prototype.catch ( onRejected ) 
static ejsval
_ejs_Promise_prototype_catch (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval onRejected = _ejs_undefined;
    if (argc > 0) onRejected = args[0];

    // 1. Let promise be the this value. 
    ejsval promise = _this;

    // 2. Return Invoke(promise, "then", (undefined, onRejected)). 
    ejsval thenargs[] = { _ejs_undefined, onRejected };
    return _ejs_invoke_closure(Get(promise, _ejs_atom_then), promise, 2, thenargs);
}

// ECMA262 25.4.5.3 Promise.prototype.then ( onFulfilled , onRejected ) 
static ejsval
_ejs_Promise_prototype_then (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval onFulfilled = _ejs_undefined;
    ejsval onRejected = _ejs_undefined;
    if (argc > 0) onFulfilled = args[0];
    if (argc > 1) onRejected  = args[1];

    // 1. Let promise be the this value. 
    ejsval promise = _this;

    // 2. If IsPromise(promise) is false, throw a TypeError exception. 
    if (!EJSVAL_IS_PROMISE(promise))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "this is not a promise");
        
    EJSPromise* _promise = EJSVAL_TO_PROMISE(promise);

    // 3. If IsCallable(onFulfilled) is undefined or nullfalse, then 
    if (!EJSVAL_IS_CALLABLE(onFulfilled)) {
        //    a. Let onFulfilled be "Identity" a new Identity Function (see 25.4.5.3.1).
        onFulfilled = _ejs_identity_function; // XXX does it really need to be a newly instantiated one? realm-specific?  we don't instantiate a new one
    }

    // 4. If IsCallable(onRejected) is undefined or nullfalse, then 
    if (!EJSVAL_IS_CALLABLE(onRejected)) {
        //    a. Let onRejected be "Thrower"a new Thrower Function (see 25.4.5.3.3). 
        onFulfilled = _ejs_thrower_function; // XXX does it really need to be a newly instantiated one?  realm-specific?  we don't instantiate a new one
    }
    // 5. If IsCallable(onFulfilled) is false or if IsCallable(onRejected) is false, then throw a TypeError exception. 
    if (!EJSVAL_IS_CALLABLE(onFulfilled) || !EJSVAL_IS_CALLABLE(onRejected)) {
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "shouldn't happen.  runtime error?");
    }

    // 6. Let C be Get(promise, "constructor"). 
    // 7. ReturnIfAbrupt(C). 
    ejsval C = Get(promise, _ejs_atom_constructor);

    // 8. Let promiseCapability be NewPromiseCapability(C). 
    // 9. ReturnIfAbrupt(promiseCapability). 

    ejsval promiseCapability = NewPromiseCapability(C);
    EJSPromiseCapability *cap = EJSVAL_TO_PROMISE_CAPABILITY(promiseCapability);

    // 10. Let fulfillReaction be the PromiseReaction { [[Capabilities]]: promiseCapability, [[Handler]]: onFulfilled }.
    // 11. Let rejectReaction  be the PromiseReaction { [[Capabilities]]: promiseCapability, [[Handler]]: onRejected  }.

    // 12. If the value of promise's [[PromiseState]] internal slot is "pending", 
    if (_promise->state == PROMISE_STATE_PENDING) {
        //        a. Append fulfillReaction as the last element of the List that is the value of promise's [[PromiseFulfillReactions]] internal slot. 
        //        b. Append rejectReaction as the last element of the List that is the value of promise's [[PromiseRejectReactions]] internal slot. 
    }
    // 13. Else if the value of promise's [[PromiseState]] internal slot is "fulfilled", 
    else if (_promise->state == PROMISE_STATE_FULFILLED) {
        //     a. Let value be the value of promise's [[PromiseResult]] internal slot. 
        ejsval value = _promise->result;
        //     b. Perform EnqueueTask("PromiseTasks", PromiseReactionTask, (fulfillReaction, value)). 
    }
    // 14. Else if the value of promise's [[PromiseState]] internal slot is "rejected", 
    else if (_promise->state == PROMISE_STATE_REJECTED) {
        //     a. Let reason be the value of promise's [[PromiseResult]] internal slot. 
        ejsval reason = _promise->result;
        //     b. Perform EnqueueTask("PromiseTasks", PromiseReactionTask, (rejectReaction, reason)). 
    }
    else {
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "shouldn't happen.  runtime error?");
    }
    // 15. Return promiseCapability.[[Promise]]. 
    return cap->promise;
}

// ECMA262 25.4.4.6 Promise [ @@create ] ( )
static ejsval
_ejs_Promise_create (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    EJS_NOT_IMPLEMENTED();
    // 1. Let F be the this value
    // 2. Return AllocatePromise(F). 
}

// ECMA262 25.4.4.1 Promise.all ( iterable )
static ejsval
_ejs_Promise_all (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    EJS_NOT_IMPLEMENTED();
    // 1. Let C be the this value. 
    // 2. Let promiseCapability be NewPromiseCapability(C). 
    // 3. ReturnIfAbrupt(promiseCapability). 
    // 4. Let iterator be GetIterator(iterable). 
    // 5. IfAbruptRejectPromise(iterator, promiseCapability). 
    // 6. Let values be ArrayCreate(0). 
    // 7. Let remainingElementsCount be a new Record { [[value]]: 1 }. 
    // 8. Let index be 0. 
    // 9. Repeat 
    //    a. Let next be IteratorStep(iterator). 
    //    b. IfAbruptRejectPromise(next, promiseCapability). 
    //    c. If next is false, 
    //       i. Set remainingElementsCount.[[value]] to remainingElementsCount.[[value]] - 1. 
    //       ii. If remainingElementsCount.[[value]] is 0, 
    //           1. Let resolveResult be the result of calling the [[Call]] internal method of promiseCapability.[[Resolve]] with undefined as thisArgument and (values) as argumentsList. 
    //           2. ReturnIfAbrupt(resolveResult). 
    //       iii. Return promiseCapability.[[Promise]]. 
    //    d. Let nextValue be IteratorValue(next). 
    //    e. IfAbruptRejectPromise(nextValue, promiseCapability). 
    //    f. Let nextPromise be Invoke(C, "resolve", (nextValue)). 
    //    g. IfAbruptRejectPromise(nextPromise, promiseCapability). 
    //    h. Let resolveElement be a new built-in function object as defined in Promise.all Resolve Element Functions. 
    //    i. Set the [[AlreadyCalled]] internal slot of resolveElement to false. 
    //    j. Set the [[Index]] internal slot of resolveElement to index. 
    //    k. Set the [[Values]] internal slot of resolveElement to values. 
    //    l. Set the [[Capabilities]] internal slot of resolveElement to promiseCapability. 
    //    m. Set the [[RemainingElements]] internal slot of resolveElement to remainingElementsCount. 
    //    n. Set remainingElementsCount.[[value]] to remainingElementsCount.[[value]] + 1. 
    //    o. Let result be Invoke(nextPromise, "then", (resolveElement, promiseCapability.[[Reject]])). 
    //    p. IfAbruptRejectPromise(result, promiseCapability). 
    //    q. Set index to index + 1.
}

// ECMA262 25.4.4.3 Promise.race ( iterable ) 
static ejsval
_ejs_Promise_race (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    EJS_NOT_IMPLEMENTED();
    // 1. Let C be the this value. 
    // 2. Let promiseCapability be NewPromiseCapability(C). 
    // 3. ReturnIfAbrupt(promiseCapability). 
    // 4. Let iterator be GetIterator(iterable). 
    // 5. IfAbruptRejectPromise(iterator, promiseCapability).
    // 6. Repeat 
    //    a. Let next be IteratorStep(iterator). 
    //    b. IfAbruptRejectPromise(next, promiseCapability). 
    //    c. If next is false, return promiseCapability.[[Promise]]. 
    //    d. Let nextValue be IteratorValue(next). 
    //    e. IfAbruptRejectPromise(nextValue, promiseCapability). 
    //    f. Let nextPromise be Invoke(C, "resolve", (nextValue)). 
    //    g. IfAbruptRejectPromise(nextPromise, promiseCapability). 
    //    h. Let result be Invoke(nextPromise, "then", (promiseCapability.[[Resolve]], promiseCapability.[[Reject]])). 
    //    i. IfAbruptRejectPromise(result, promiseCapability). 
}

// ECMA262 25.4.4.4 Promise.reject ( r )
static ejsval
_ejs_Promise_reject (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval r = _ejs_undefined;
    if (argc > 0) r = args[0];

    // 1. Let C be the this value. 
    ejsval C = _this;

    // 2. Let promiseCapability be NewPromiseCapability(C). 
    // 3. ReturnIfAbrupt(promiseCapability). 
    ejsval promiseCapability = NewPromiseCapability(C);
    
    EJSPromiseCapability *cap = EJSVAL_TO_PROMISE_CAPABILITY(promiseCapability);

    // 4. Let rejectResult be the result of calling the [[Call]] internal method of promiseCapability.[[Reject]] with undefined as thisArgument and (r) as argumentsList. 
    // 5. ReturnIfAbrupt(rejectResult). 
    _ejs_invoke_closure(cap->reject, _ejs_undefined, 1, &r);

    // 6. Return promiseCapability.[[Promise]]. 
    return cap->promise;
}

// ECMA262 25.4.4.5 Promise.resolve ( x )
static ejsval
_ejs_Promise_resolve (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval x = _ejs_undefined;
    if (argc > 0) x = args[0];

    // 1. Let C be the this value. 
    ejsval C = _this;
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
    
    EJSPromiseCapability *cap = EJSVAL_TO_PROMISE_CAPABILITY(promiseCapability);

    // 5. Let resolveResult be the result of calling the [[Call]] internal method of promiseCapability.[[Resolve]] with undefined as thisArgument and (x) as argumentsList. 
    // 6. ReturnIfAbrupt(resolveResult). 
    _ejs_invoke_closure(cap->resolve, _ejs_undefined, 1, &x);

    // 7. Return promiseCapability.[[Promise]]. 
    return cap->promise;
}

void
_ejs_promise_init(ejsval global)
{
    _ejs_Promise = _ejs_function_new_without_proto (_ejs_null, _ejs_atom_Promise, (EJSClosureFunc)_ejs_Promise_impl);
    _ejs_object_setprop (global, _ejs_atom_Promise, _ejs_Promise);

    _ejs_gc_add_root (&_ejs_Promise_prototype);
    _ejs_Promise_prototype = _ejs_object_new(_ejs_null, &_ejs_Object_specops);
    _ejs_object_setprop (_ejs_Promise,       _ejs_atom_prototype,  _ejs_Promise_prototype);

#define PROTO_METHOD(x) EJS_INSTALL_ATOM_FUNCTION_FLAGS (_ejs_Promise_prototype, x, _ejs_Promise_prototype_##x, EJS_PROP_NOT_ENUMERABLE)
#define OBJ_METHOD(x) EJS_INSTALL_ATOM_FUNCTION_FLAGS (_ejs_Promise, x, _ejs_Promise_##x, EJS_PROP_NOT_ENUMERABLE)

    PROTO_METHOD(catch);
    PROTO_METHOD(then);

    _ejs_object_define_value_property (_ejs_Promise_prototype, _ejs_Symbol_toStringTag, _ejs_atom_Promise, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_WRITABLE | EJS_PROP_CONFIGURABLE);

    OBJ_METHOD(all);
    OBJ_METHOD(race);
    OBJ_METHOD(reject);
    OBJ_METHOD(resolve);

#undef PROTO_METHOD

    EJS_INSTALL_SYMBOL_FUNCTION_FLAGS (_ejs_Promise, create, _ejs_Promise_create, EJS_PROP_NOT_ENUMERABLE);

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

    EJS_LIST_FOREACH(&promise->fulfillReactions, EJSPromiseReaction, r, { scan_func(r->capabilities); scan_func(r->handler); })
    EJS_LIST_FOREACH(&promise->rejectReactions,  EJSPromiseReaction, r, { scan_func(r->capabilities); scan_func(r->handler); })

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
                 _ejs_promise_specop_allocate,
                 OP_INHERIT, // [[Finalize]]
                 _ejs_promise_specop_scan
                 )
