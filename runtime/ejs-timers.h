/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#ifndef _ejs_timers_h_
#define _ejs_timers_h_

#include "ejs-object.h"
#include "ejs-timers.h"

#define EJSVAL_IS_TIMER(v)      (EJSVAL_IS_OBJECT(v) && (EJSVAL_TO_OBJECT(v)->ops == &_ejs_Timer_specops))
#define EJSVAL_TO_TIMER(v)      ((EJSTimer*)EJSVAL_TO_OBJECT(v))

typedef struct {
    /* object header */
    EJSObject obj;

    /* native handle */
    void *handle;

} EJSTimer;

EJS_BEGIN_DECLS

extern ejsval _ejs_Timer;
extern ejsval _ejs_Timer_prototype;
extern EJSSpecOps _ejs_Timer_specops;

extern ejsval _ejs_setInterval;
extern ejsval _ejs_setTimeout;
extern ejsval _ejs_clearInterval;
extern ejsval _ejs_clearTimeout;

/*
ejsval _ejs_setInterval_impl (ejsval env, ejsval _this, uint32_t argc, ejsval* args);
ejsval _ejs_setTimeout_impl (ejsval env, ejsval _this, uint32_t argc, ejsval* args);
ejsval _ejs_clearInterval_impl (ejsval env, ejsval _this, uint32_t argc, ejsval* args);
ejsval _ejs_clearTimeout_impl (ejsval env, ejsval _this, uint32_t argc, ejsval* args);
*/
void _ejs_timers_init(ejsval global);

EJS_END_DECLS

#endif /* _ejs_timers_h_ */
