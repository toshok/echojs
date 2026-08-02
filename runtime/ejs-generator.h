/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#ifndef _ejs_generator_h_
#define _ejs_generator_h_

#include "ejs.h"
#include "ejs-object.h"
#include <ucontext.h>

EJS_BEGIN_DECLS

#define EJSVAL_IS_GENERATOR(v)  (EJSVAL_IS_OBJECT(v) && (EJSVAL_TO_OBJECT(v)->ops == &_ejs_Generator_specops))

typedef struct _EJSGenerator {
    /* object header */
    EJSObject obj;

    EJSBool started;

    ejsval body;

    ejsval yielded_value;
    ejsval sent_value;

    // when true, we throw from the yield point.  when false we simply return
    EJSBool throwing;

    // when true, the resume is a .return(): the yield point throws the
    // return sentinel (sent_value holds the return value)
    EJSBool returning;

    // the body ran to completion (normally, or via the return sentinel);
    // next/throw/return on a completed generator must not resume the
    // dead context
    EJSBool completed;

    // the body ended with an uncaught throw; yielded_value holds the
    // exception, which the resume site rethrows on the CALLER's stack
    // (unwinding it on the generator stack would walk off the
    // makecontext frame)
    EJSBool threw_out;

    void* stack;
    size_t stack_size;

    // all live generators sit on a registry so a minor
    // collection can scan every suspended stack CONSERVATIVELY before
    // any evacuation — a generator discovered mid-trace would pin its
    // stack referents too late (they may already have moved)
    struct _EJSGenerator* reg_next;
    struct _EJSGenerator* reg_prev;

    // the caller-side stack position recorded just before each swap INTO
    // this generator (the address of a local in the resuming frame).  While
    // the generator runs, its caller's frames live ABOVE this address (the
    // stack grows down) — the GC scans [caller_stack_top, caller's stack
    // end) to cover the suspended segment.
    void* caller_stack_top;

    // each machine stack owns a disjoint gc-frame chain.
    // The push hook parks the caller's chain head here and installs
    // this generator's saved head (NULL on first entry); the pop hook
    // does the reverse.  While suspended, gc_frame_head is the walk
    // root for this stack's precise frames; while running it is NULL
    // (the live chain is _ejs_heap.gc_frame_head) and the caller's
    // segment is reachable via caller_gc_frame_head.
    void* gc_frame_head;
    void* caller_gc_frame_head;

    ucontext_t generator_context;
    ucontext_t caller_context;
} EJSGenerator;

extern ejsval _ejs_generator_return_sentinel;
ejsval _ejs_generator_is_return_sentinel (ejsval exc);
ejsval _ejs_generator_return_value (ejsval generator);

extern ejsval _ejs_IteratorWrapper_prototype;
extern EJSSpecOps _ejs_IteratorWrapper_specops;

extern ejsval _ejs_Iterator_prototype;

extern ejsval _ejs_Generator_prototype;
extern EJSSpecOps _ejs_Generator_specops;

extern ejsval _ejs_generator_new (ejsval generator_body);

extern void _ejs_generator_init (ejsval global);
extern ejsval _ejs_mark_async_generator (ejsval fn);

extern void   _ejs_destructure_iterator_wrapper_init (ejsval global);
extern ejsval _ejs_destructure_iterator_new(ejsval iterator);

extern void _ejs_iterator_wrapper_init (ejsval global);

extern void _ejs_iterator_init_proto ();

/* these live in ejs-gc.c but it's easier on everything to have the decls here */
extern void _ejs_gc_push_generator(EJSGenerator *gen);
extern void _ejs_gc_pop_generator();

/* the live-generator registry (ejs-generator.c) + the
   conservative half of the generator scan, shared by the specop and the
   minor collection's pre-evacuation pass */
extern EJSGenerator* _ejs_generator_registry;
extern void _ejs_generator_scan_conservative(EJSGenerator* gen);

EJS_END_DECLS

#endif
