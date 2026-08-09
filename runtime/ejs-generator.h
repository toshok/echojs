/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#ifndef _ejs_generator_h_
#define _ejs_generator_h_

#include "ejs.h"
#include "ejs-object.h"

EJS_BEGIN_DECLS

#define EJSVAL_IS_GENERATOR(v)  (EJSVAL_IS_OBJECT(v) && (EJSVAL_TO_OBJECT(v)->ops == &_ejs_Generator_specops))

// state-machine generators (docs/generator-eir-plan.md): the body is a
// compiled resume-dispatch state machine the driver re-calls as
// body(gen, mode, sent); every yield suspends by returning.  The
// generator's entire suspended state is the precisely-scanned env —
// no machine stack, no saved contexts, nothing conservative.
typedef struct _EJSGenerator {
    /* object header */
    EJSObject obj;

    EJSBool started;
    // the body ran to completion (normally, via an uncaught throw, or
    // via the return sentinel); next/throw/return answer per spec
    // without calling the body again
    EJSBool completed;
    // reentrancy fence: resuming a generator from inside its own body
    // is a TypeError
    EJSBool running;

    ejsval body;

    // the latest resume argument; the wrapper's sentinel catch reads
    // .return(v)'s value back through _ejs_generator_return_value
    ejsval sent_value;

    // eir_state: 0 = not started, k > 0 = suspended at yield #k.
    // eir_suspended is set by the compiled suspend
    // (_ejs_generator_eir_suspend) and cleared by the driver before
    // each resume — it distinguishes a yield's return from a
    // completion return.  eir_env: the body's persistent closure env.
    int32_t eir_state;
    EJSBool eir_suspended;
    ejsval eir_env;
} EJSGenerator;

extern ejsval _ejs_generator_new_eir (ejsval body);
extern ejsval _ejs_generator_eir_state (ejsval generator);
extern ejsval _ejs_generator_eir_get_env (ejsval generator);
extern void   _ejs_generator_eir_set_env (ejsval generator, ejsval env);
extern void   _ejs_generator_eir_suspend (ejsval generator, ejsval state);
extern ejsval _ejs_generator_eir_sentinel (void);

extern ejsval _ejs_generator_return_sentinel;
ejsval _ejs_generator_is_return_sentinel (ejsval exc);
ejsval _ejs_generator_return_value (ejsval generator);

extern ejsval _ejs_IteratorWrapper_prototype;
extern EJSSpecOps _ejs_IteratorWrapper_specops;

extern ejsval _ejs_Iterator_prototype;

extern ejsval _ejs_Generator_prototype;
extern EJSSpecOps _ejs_Generator_specops;

extern void _ejs_generator_init (ejsval global);
extern ejsval _ejs_mark_async_generator (ejsval fn);

extern void   _ejs_destructure_iterator_wrapper_init (ejsval global);
extern ejsval _ejs_destructure_iterator_new(ejsval iterator);

extern void _ejs_iterator_wrapper_init (ejsval global);

extern void _ejs_iterator_init_proto ();

EJS_END_DECLS

#endif
