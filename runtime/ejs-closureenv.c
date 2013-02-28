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
_ejs_closureenv_new (uint32_t length)
{
    size_t value_size = sizeof(EJSClosureEnv) + sizeof(ejsval) * (length - 1);
    EJSClosureEnv* env = _ejs_gc_new_closureenv(value_size);
    env->length = length;
    for (int i = 0; i < length; i ++) {
        env->slots[i] = _ejs_undefined;
    }
    return CLOSUREENV_TO_EJSVAL_IMPL(env);
}

ejsval
_ejs_closureenv_get_slot (ejsval env, uint32_t slot)
{
    return (EJSVAL_TO_CLOSUREENV_IMPL(env))->slots[slot];
}

ejsval*
_ejs_closureenv_get_slot_ref (ejsval env, uint32_t slot)
{
    return &(EJSVAL_TO_CLOSUREENV_IMPL(env))->slots[slot];
}
