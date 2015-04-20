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

    ucontext_t generator_context;
    ucontext_t caller_context;
} EJSGenerator;

extern ejsval _ejs_Generator_prototype;
extern EJSSpecOps _ejs_Generator_specops;

extern ejsval _ejs_generator_new (ejsval generator_body);

extern void _ejs_generator_init (ejsval global);

EJS_END_DECLS

#endif
