/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include <stdlib.h>

#include "ejs-error.h"
#include "ejs-function.h"
#include "ejs-ops.h"
#include "ejs-runloop.h"
#include "ejs-timers.h"

ejsval _ejs_Timer EJSVAL_ALIGNMENT;
ejsval _ejs_Timer_prototype EJSVAL_ALIGNMENT;

ejsval _ejs_setTimeout EJSVAL_ALIGNMENT;
ejsval _ejs_setInterval EJSVAL_ALIGNMENT;

ejsval _ejs_clearTimeout EJSVAL_ALIGNMENT;
ejsval _ejs_clearInterval EJSVAL_ALIGNMENT;

typedef struct {
    EJSTimer *timer;

    ejsval callbackfn;
    ejsval *args;
    uint32_t argc;
} TimerTaskArg;


static TimerTaskArg*
create_task_arg (ejsval callbackfn, uint32_t argc, ejsval *args)
{
    TimerTaskArg *rv = malloc(sizeof(TimerTaskArg));
    rv->callbackfn = callbackfn;
    rv->argc = argc;
    rv->args = args;

    _ejs_gc_add_root(&rv->callbackfn);
    for (int i = 0; i < rv->argc; i++)
        _ejs_gc_add_root(&rv->args[i]);

    return rv;
}

static void
destroy_task_arg (TimerTaskArg *arg)
{
    _ejs_gc_remove_root(&arg->callbackfn);
    for (int i = 0; i < arg->argc; i++)
        _ejs_gc_remove_root(&arg->args[i]);

    if (arg->argc > 0)
        free(arg->args);

    free(arg);
}

static void
call_timeout_task (void* data)
{
    TimerTaskArg *arg = (TimerTaskArg*)data;
    _ejs_invoke_closure (arg->callbackfn, _ejs_undefined, arg->argc, arg->args);
}

static void
dtor_timeout_task (void *data)
{
    destroy_task_arg((TimerTaskArg*)data);
}

static ejsval
_ejs_clearTimer (ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
    ejsval timer = _ejs_undefined;

    if (argc >= 1)
        timer = args[0];

    /* it looks passing the wrong value doesn't cause any effect/error */
    if (!EJSVAL_IS_TIMER(timer))
        return _ejs_undefined;

    EJSTimer *timerObj = EJSVAL_TO_TIMER(timer);
    if (timerObj->handle == NULL)
        return _ejs_undefined;

    _ejs_runloop_remove_task(timerObj->handle);
    timerObj->handle = NULL;

    return _ejs_undefined;
}

ejsval
_ejs_clearInterval_impl (ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
    return _ejs_clearTimer (env, _this, argc, args);
}

ejsval
_ejs_clearTimeout_impl (ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
    return _ejs_clearTimer (env, _this, argc, args);
}

static ejsval
create_timer_value(void *handle)
{
    EJSObject *timer = (EJSObject*)_ejs_gc_new (EJSTimer);
    _ejs_init_object (timer, _ejs_Timer_prototype, &_ejs_Timer_specops);

    ((EJSTimer*)timer)->handle = handle;

    return OBJECT_TO_EJSVAL(timer);
}

static ejsval
_ejs_setTimer (ejsval env, ejsval _this, uint32_t argc, ejsval* args, EJSBool repeats)
{
    ejsval callbackfn = _ejs_undefined;
    ejsval interval = _ejs_undefined;

    if (argc >= 1)
        callbackfn = args[0];

    if (argc >= 2)
        interval = args[1];

    if (!IsCallable(callbackfn))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "argument is not a function");

    uint32_t timer_argc = 0;
    if (argc > 2)
        timer_argc = argc - 2;

    ejsval *timer_args = NULL;
    if (timer_argc > 0)
        timer_args = malloc(sizeof(ejsval) * timer_argc);

    for (int i = 0; i < timer_argc; i++)
        timer_args[i] = args[i + 2];

    TimerTaskArg *taskArg = create_task_arg (callbackfn, timer_argc, timer_args);
    void *handle = _ejs_runloop_add_task_timeout (call_timeout_task, taskArg, dtor_timeout_task, ToLength(interval), repeats);

    return create_timer_value (handle);
}

ejsval
_ejs_setTimeout_impl (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    return _ejs_setTimer(env, _this, argc, args, EJS_FALSE);
}

ejsval
_ejs_setInterval_impl (ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
    return _ejs_setTimer(env, _this, argc, args, EJS_TRUE);
}

static ejsval
_ejs_Timer_impl (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    return _this;
}

void
_ejs_timers_init(ejsval global)
{
    _ejs_Timer = _ejs_function_new_without_proto (_ejs_null, _ejs_atom_empty, (EJSClosureFunc)_ejs_Timer_impl);

    _ejs_gc_add_root (&_ejs_Timer_prototype);
    _ejs_Timer_prototype = _ejs_object_new (_ejs_null, &_ejs_Object_specops);
    _ejs_object_setprop (_ejs_Timer, _ejs_atom_prototype, _ejs_Timer_prototype);
    _ejs_object_define_value_property (_ejs_Timer_prototype, _ejs_atom_constructor, _ejs_Timer,
            EJS_PROP_NOT_ENUMERABLE | EJS_PROP_CONFIGURABLE | EJS_PROP_WRITABLE);
#define GLOBAL_METHOD(x) EJS_MACRO_START                                \
    _ejs_##x = _ejs_function_new_native (_ejs_null, _ejs_atom_##x, (EJSClosureFunc)_ejs_##x##_impl); \
    _ejs_object_setprop (global, _ejs_atom_##x, _ejs_##x);         \
    EJS_MACRO_END

    GLOBAL_METHOD(clearInterval);
    GLOBAL_METHOD(clearTimeout);
    GLOBAL_METHOD(setInterval);
    GLOBAL_METHOD(setTimeout);
#undef GLOBAL_METHOD
}

EJS_DEFINE_INHERIT_ALL_CLASS(Timer)

