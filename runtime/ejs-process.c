/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include "ejs-process.h"
#include "ejs-array.h"
#include "ejs-gc.h"
#include "ejs-function.h"
#include "ejs-string.h"
#include "ejs-error.h"

#include <unistd.h>
#include <sys/param.h>

static ejsval
_ejs_Process_exit (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    int exit_status = 0;

    // FIXME ignore argc/args[0] for now

    exit (exit_status);
}

static ejsval
_ejs_Process_chdir (ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
    ejsval dir = _ejs_undefined;
    if (argc > 0)
        dir = args[0];

    if (!EJSVAL_IS_STRING(dir))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "chdir passed non-string");
        
    char *dir_utf8 = ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(dir));
    chdir(dir_utf8);
    free(dir_utf8);

    return _ejs_undefined;
}

static ejsval
_ejs_Process_cwd (ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
    char cwd[MAXPATHLEN];

    getcwd(cwd, MAXPATHLEN);

    return _ejs_string_new_utf8(cwd);
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

    _ejs_object_setprop (_ejs_Process, _ejs_atom_argv, _argv);

#define OBJ_METHOD(x) EJS_INSTALL_ATOM_FUNCTION(_ejs_Process, x, _ejs_Process_##x)

    OBJ_METHOD(exit);

    OBJ_METHOD(chdir);
    OBJ_METHOD(cwd);

#undef OBJ_METHOD

    _ejs_object_setprop (global, _ejs_atom_process, _ejs_Process);

    END_SHADOW_STACK_FRAME;
}
