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

#define EJSVAL_IS_PROMISE_CAPABILITY(v)     (EJSVAL_IS_OBJECT(v) && (EJSVAL_TO_OBJECT(v)->ops == &_ejs_PromiseCapability_specops))
#define EJSVAL_TO_PROMISE_CAPABILITY(v)     ((EJSPromiseCapability*)EJSVAL_TO_OBJECT(v))

typedef enum {
    PROMISE_STATE_PENDING,
    PROMISE_STATE_FULFILLED,
    PROMISE_STATE_REJECTED
} PromiseState;

typedef struct {
    EJSListNode _listnode;
    ejsval capabilities;
    ejsval handler;
} EJSPromiseReaction;

typedef struct {
    /* object header */
    EJSObject obj;

    ejsval promise;
    ejsval reject;
    ejsval resolve;
} EJSPromiseCapability;

typedef struct {
    /* object header */
    EJSObject obj;

    /* promise specific data */
    PromiseState state;
    ejsval result;
    ejsval constructor;
    EJSList fulfillReactions;
    EJSList rejectReactions;
} EJSPromise;


EJS_BEGIN_DECLS

extern ejsval _ejs_Promise;
extern ejsval _ejs_Promise_prototype;
extern EJSSpecOps _ejs_Promise_specops;

void _ejs_promise_init(ejsval global);

EJS_END_DECLS

#endif /* _ejs_promise_h */
