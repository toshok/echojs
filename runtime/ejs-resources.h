/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#ifndef _ejs_resources_h_
#define _ejs_resources_h_

#include "ejs-object.h"

// explicit-resource-management (DisposableStack/AsyncDisposableStack,
// SuppressedError) plus the weak-reference surface (WeakRef,
// FinalizationRegistry).  the collector never moves or reclaims
// referents out from under these objects, so WeakRef targets and
// FinalizationRegistry cells are held strongly — permitted, since the
// spec allows an implementation to never collect them.

// one queued dispose callback.  the list head is the most recently
// added resource, so walking head->next runs callbacks in reverse
// insertion order, as dispose() requires.
typedef struct EJSDisposeResource {
    struct EJSDisposeResource* next;
    ejsval value;   // receiver for the callback (undefined for defer())
    ejsval method;  // the callable to invoke
    // adopt() callbacks receive the value as their sole argument
    // instead of as the receiver
    EJSBool pass_value_as_arg;
} EJSDisposeResource;

typedef struct {
    /* object header */
    EJSObject obj;

    EJSBool disposed;
    EJSDisposeResource* resources;
} EJSDisposableStack;

typedef struct EJSFinalizationCell {
    struct EJSFinalizationCell* next;
    ejsval target;
    ejsval held;
    ejsval token;
} EJSFinalizationCell;

typedef struct {
    /* object header */
    EJSObject obj;

    ejsval cleanup_callback;
    EJSFinalizationCell* cells;
} EJSFinalizationRegistry;

typedef struct {
    /* object header */
    EJSObject obj;

    ejsval target;
} EJSWeakRef;

#define EJSVAL_IS_DISPOSABLESTACK(v)      (EJSVAL_IS_OBJECT(v) && (EJSVAL_TO_OBJECT(v)->ops == &_ejs_DisposableStack_specops))
#define EJSVAL_IS_ASYNCDISPOSABLESTACK(v) (EJSVAL_IS_OBJECT(v) && (EJSVAL_TO_OBJECT(v)->ops == &_ejs_AsyncDisposableStack_specops))
#define EJSVAL_IS_FINALIZATIONREGISTRY(v) (EJSVAL_IS_OBJECT(v) && (EJSVAL_TO_OBJECT(v)->ops == &_ejs_FinalizationRegistry_specops))
#define EJSVAL_IS_WEAKREF(v)              (EJSVAL_IS_OBJECT(v) && (EJSVAL_TO_OBJECT(v)->ops == &_ejs_WeakRef_specops))

EJS_BEGIN_DECLS

extern ejsval _ejs_DisposableStack;
extern ejsval _ejs_DisposableStack_prototype;
extern EJSSpecOps _ejs_DisposableStack_specops;

extern ejsval _ejs_AsyncDisposableStack;
extern ejsval _ejs_AsyncDisposableStack_prototype;
extern EJSSpecOps _ejs_AsyncDisposableStack_specops;

extern ejsval _ejs_SuppressedError;
extern ejsval _ejs_SuppressedError_prototype;

extern ejsval _ejs_FinalizationRegistry;
extern ejsval _ejs_FinalizationRegistry_prototype;
extern EJSSpecOps _ejs_FinalizationRegistry_specops;

extern ejsval _ejs_WeakRef;
extern ejsval _ejs_WeakRef_prototype;
extern EJSSpecOps _ejs_WeakRef_specops;

void _ejs_resources_init(ejsval global);

EJS_END_DECLS

#endif
