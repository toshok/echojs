/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include "ejs-process.h"
#include "ejs-array.h"
#include "ejs-gc.h"
#include "ejs-function.h"
#include "ejs-string.h"

static ejsval
_ejs_Process_exit (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    int exit_status = 0;

    // FIXME ignore argc/args[0] for now

    exit (exit_status);
}

void
_ejs_process_init(ejsval global, uint32_t argc, char **argv)
{
    START_SHADOW_STACK_FRAME;

    ADD_STACK_ROOT(ejsval, _ejs_Process, _ejs_object_new (_ejs_null, &_ejs_object_specops));
    ADD_STACK_ROOT(ejsval, _argv, _ejs_array_new (argc));
    int i;

    for (i = 0; i < argc; i ++) {
        START_SHADOW_STACK_FRAME;
        ejsval _i = NUMBER_TO_EJSVAL(i);
        ADD_STACK_ROOT(ejsval, _argv_i, _ejs_string_new_utf8(argv[i]));
        _ejs_object_setprop (_argv, _i, _argv_i);
        END_SHADOW_STACK_FRAME;
    }

    _ejs_object_setprop_utf8 (_ejs_Process, "argv", _argv);

#define OBJ_METHOD(x) EJS_INSTALL_FUNCTION(_ejs_Process, EJS_STRINGIFY(x), _ejs_Process_##x)

    OBJ_METHOD(exit);

#undef OBJ_METHOD

    _ejs_object_setprop_utf8 (global, "process", _ejs_Process);

    END_SHADOW_STACK_FRAME;
}
