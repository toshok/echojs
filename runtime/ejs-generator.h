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

typedef struct {
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

    void* stack;
    size_t stack_size;

    // the caller-side stack position recorded just before each swap INTO
    // this generator (the address of a local in the resuming frame).  While
    // the generator runs, its caller's frames live ABOVE this address (the
    // stack grows down) — the GC scans [caller_stack_top, caller's stack
    // end) to cover the suspended segment (gc-plan P0).
    void* caller_stack_top;

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

extern void   _ejs_destructure_iterator_wrapper_init (ejsval global);
extern ejsval _ejs_destructure_iterator_new(ejsval iterator);

extern void _ejs_iterator_wrapper_init (ejsval global);

extern void _ejs_iterator_init_proto ();

/* these live in ejs-gc.c but it's easier on everything to have the decls here */
extern void _ejs_gc_push_generator(EJSGenerator *gen);
extern void _ejs_gc_pop_generator();

EJS_END_DECLS

#endif
