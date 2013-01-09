/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include "ejs-console.h"
#include "ejs-gc.h"
#include "ejs-ops.h"
#include "ejs-function.h"

static ejsval
output (FILE *outfile, uint32_t argc, ejsval *args)
{
    for (int i = 0; i < argc; i ++) {
        if (EJSVAL_IS_NUMBER(args[i])) {
            double d = EJSVAL_TO_NUMBER(args[i]);
            int di;
            if (EJSDOUBLE_IS_INT32(d, &di))
                fprintf (outfile, "%d", di);
            else
                fprintf (outfile, EJS_NUMBER_FORMAT, d);
        }
        else {
            START_SHADOW_STACK_FRAME;
            
            ADD_STACK_ROOT(ejsval, strval, ToString(args[i]));

            fprintf (outfile, "%s", EJSVAL_TO_FLAT_STRING(strval));

            END_SHADOW_STACK_FRAME;
        }
        if (i < argc - 1)
            fputc (' ', outfile);
    }

    fputc ('\n', outfile);

    return _ejs_undefined;
}

static ejsval
_ejs_console_log (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    return output (stdout, argc, args);
}

static ejsval
_ejs_console_warn (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    return output (stderr, argc, args);
}


void
_ejs_console_init(ejsval global)
{
    START_SHADOW_STACK_FRAME;

    ADD_STACK_ROOT(ejsval, _ejs_console, _ejs_object_new (_ejs_null, &_ejs_object_specops));

#define OBJ_METHOD(x) EJS_INSTALL_FUNCTION(_ejs_console, EJS_STRINGIFY(x), _ejs_console_##x)

    OBJ_METHOD(log);
    OBJ_METHOD(warn);

    _ejs_object_setprop_utf8 (global, "console", _ejs_console);

#undef OBJ_METHOD

    END_SHADOW_STACK_FRAME;
}
