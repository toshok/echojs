/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */
#include <assert.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <ctype.h>
#include <stdarg.h>

#include "ejs-ops.h"
#include "ejs-closureenv.h"


ejsval
_ejs_closureenv_new (ejsval length)
{
    uint32_t _length = ToUint32(length);
    size_t value_size = sizeof(EJSClosureEnv) + sizeof(ejsval) * (_length - 1);
    EJSClosureEnv* env = _ejs_gc_new_closureenv(value_size);
    env->length = _length;
    for (int i = 0; i < _length; i ++) {
        env->slots[i] = _ejs_undefined;
    }
    return CLOSUREENV_TO_EJSVAL_IMPL(env);
}

ejsval
_ejs_closureenv_get_slot (ejsval env, ejsval slot)
{
    return (EJSVAL_TO_CLOSUREENV_IMPL(env))->slots[ToUint32(slot)];
}

ejsval*
_ejs_closureenv_get_slot_ref (ejsval env, ejsval slot)
{
    return &(EJSVAL_TO_CLOSUREENV_IMPL(env))->slots[ToUint32(slot)];
}
