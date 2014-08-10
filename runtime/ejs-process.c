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
#include <string.h>
#include <sys/param.h>

extern char** environ;

static ejsval
_ejs_Process_get_env (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval env_obj = _ejs_object_new(_ejs_null, &_ejs_Object_specops);

    char** p = environ;

    while (*p) {
        char *env_entry = strdup(*p);
        char *eq = strchr(env_entry, '=');
        if (!eq) {
            free (env_entry);
            p++;
            continue;
        }

        *eq = '\0';

        ejsval k = _ejs_string_new_utf8(env_entry);
        ejsval v = _ejs_string_new_utf8(eq+1);

        _ejs_object_define_value_property (env_obj, k, v, EJS_PROP_ENUMERABLE | EJS_PROP_NOT_CONFIGURABLE | EJS_PROP_NOT_WRITABLE);
        free (env_entry);
        p++;
    }

    return env_obj;
}

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

ejsval _ejs_Process EJSVAL_ALIGNMENT;

void
_ejs_process_init(ejsval global, uint32_t argc, char **argv)
{
    _ejs_Process = _ejs_object_new (_ejs_null, &_ejs_Object_specops);
    _ejs_object_setprop (global, _ejs_atom_process, _ejs_Process);

    ejsval _argv = _ejs_array_new (argc, EJS_FALSE);
    _ejs_object_setprop (_ejs_Process, _ejs_atom_argv, _argv);

    for (int i = 0; i < argc; i ++)
        _ejs_object_setprop (_argv, NUMBER_TO_EJSVAL(i), _ejs_string_new_utf8(argv[i]));

#define OBJ_PROP(x) EJS_INSTALL_ATOM_GETTER(_ejs_Process, x, _ejs_Process_get_##x)
#define OBJ_METHOD(x) EJS_INSTALL_ATOM_FUNCTION(_ejs_Process, x, _ejs_Process_##x)

    OBJ_PROP(env);

    OBJ_METHOD(exit);

    OBJ_METHOD(chdir);
    OBJ_METHOD(cwd);

#undef OBJ_PROP
#undef OBJ_METHOD
}
