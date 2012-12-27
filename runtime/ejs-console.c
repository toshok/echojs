/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include "ejs.h"
#include "ejs-gc.h"
#include "ejs-ops.h"
#include "ejs-value.h"
#include "ejs-function.h"

static ejsval
output (FILE *outfile, int argc, ejsval *args)
{
    START_SHADOW_STACK_FRAME;

    if (argc > 0) {
        if (EJSVAL_IS_NUMBER(args[0])) {
            fprintf (outfile, EJS_NUMBER_FORMAT, EJSVAL_TO_NUMBER(args[0]));
        }
        else {
            ADD_STACK_ROOT(ejsval, strval, ToString(args[0]));

            fprintf (outfile, "%s\n", EJSVAL_TO_STRING(strval));
        }
    }

    END_SHADOW_STACK_FRAME;

    return _ejs_undefined;
}

static ejsval
_ejs_console_log (ejsval env, ejsval _this, int argc, ejsval *args)
{
    return output (stdout, argc, args);
}

static ejsval
_ejs_console_warn (ejsval env, ejsval _this, int argc, ejsval *args)
{
    return output (stderr, argc, args);
}


void
_ejs_console_init(ejsval global)
{
    START_SHADOW_STACK_FRAME;

    ADD_STACK_ROOT(ejsval, _ejs_console, _ejs_object_new (_ejs_null));

#define OBJ_METHOD(x) EJS_MACRO_START ADD_STACK_ROOT(ejsval, funcname, _ejs_string_new_utf8(#x)); ADD_STACK_ROOT(ejsval, tmpfunc, _ejs_function_new (_ejs_null, funcname, (EJSClosureFunc)_ejs_console_##x)); _ejs_object_setprop (_ejs_console, funcname, tmpfunc); EJS_MACRO_END

    OBJ_METHOD(log);
    OBJ_METHOD(warn);

    _ejs_object_setprop_utf8 (global, "console", _ejs_console);

#undef OBJ_METHOD

    END_SHADOW_STACK_FRAME;
}
