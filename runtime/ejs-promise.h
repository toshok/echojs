/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#ifndef _ejs_promise_h_
#define _ejs_promise_h_

#include <time.h>
#include <sys/time.h>
#include "ejs-object.h"

#define EJSVAL_IS_PROMISE(v)     (EJSVAL_IS_OBJECT(v) && (EJSVAL_TO_OBJECT(v)->ops == &_ejs_Promise_specops))
#define EJSVAL_TO_PROMISE(v)     ((EJSPromise*)EJSVAL_TO_OBJECT(v))

typedef enum {
    PROMISE_STATE_UNINITIALIZED,
    PROMISE_STATE_PENDING,
    PROMISE_STATE_FULFILLED,
    PROMISE_STATE_REJECTED
} PromiseState;

#define EJS_CAPABILITY_PROMISE_SLOT 0
#define EJS_CAPABILITY_REJECT_SLOT  1
#define EJS_CAPABILITY_RESOLVE_SLOT 2
#define EJS_CAPABILITY_SLOT_COUNT   3

#define EJS_CAPABILITY_NEW() (_ejs_closureenv_new(EJS_CAPABILITY_SLOT_COUNT))
#define EJS_CAPABILITY_GET_PROMISE(cap) (_ejs_closureenv_get_slot(cap, EJS_CAPABILITY_PROMISE_SLOT))
#define EJS_CAPABILITY_GET_REJECT(cap)  (_ejs_closureenv_get_slot(cap, EJS_CAPABILITY_REJECT_SLOT))
#define EJS_CAPABILITY_GET_RESOLVE(cap) (_ejs_closureenv_get_slot(cap, EJS_CAPABILITY_RESOLVE_SLOT))

#define EJS_CAPABILITY_SET_PROMISE(cap,v) (*_ejs_closureenv_get_slot_ref(cap, EJS_CAPABILITY_PROMISE_SLOT) = (v))
#define EJS_CAPABILITY_SET_REJECT(cap,v)  (*_ejs_closureenv_get_slot_ref(cap, EJS_CAPABILITY_REJECT_SLOT)  = (v))
#define EJS_CAPABILITY_SET_RESOLVE(cap,v) (*_ejs_closureenv_get_slot_ref(cap, EJS_CAPABILITY_RESOLVE_SLOT) = (v))

typedef struct EJSPromiseReaction {
    EJS_LIST_HEADER(struct EJSPromiseReaction);
    ejsval capabilities;
    ejsval handler;
} EJSPromiseReaction;

typedef struct {
    EJSFunction function;
    ejsval promise;
    ejsval alreadyResolved;
} EJSPromiseResolveFunction;

typedef struct {
    /* object header */
    EJSObject obj;

    /* promise specific data */
    PromiseState state;
    ejsval result;
    ejsval constructor;
    EJSPromiseReaction* fulfillReactions;
    EJSPromiseReaction* rejectReactions;
} EJSPromise;


EJS_BEGIN_DECLS

extern ejsval _ejs_Promise;
extern ejsval _ejs_Promise_prototype;
extern EJSSpecOps _ejs_Promise_specops;

void _ejs_promise_init(ejsval global);

EJS_END_DECLS

#endif /* _ejs_promise_h */
