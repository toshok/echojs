/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include "ejs-console.h"
#include "ejs-gc.h"
#include "ejs-ops.h"
#include "ejs-function.h"
#include "ejs-string.h"
#include "ejs-error.h"

#if IOS
#import <Foundation/Foundation.h>
#endif

static ejsval
output (FILE *outfile, uint32_t argc, ejsval *args)
{
#if IOS
#define OUTPUT(format, val) NSLog(@format, val)
#else
#define OUTPUT(format, val) fprintf (outfile, format, val)
#endif

    START_SHADOW_STACK_FRAME;
    for (int i = 0; i < argc; i ++) {
        if (EJSVAL_IS_NUMBER(args[i])) {
            double d = EJSVAL_TO_NUMBER(args[i]);
            int di;
            if (EJSDOUBLE_IS_INT32(d, &di))
                OUTPUT ("%d", di);
            else
                OUTPUT (EJS_NUMBER_FORMAT, d);
        }
        else if (EJSVAL_IS_ERROR(args[i])) {
            ADD_STACK_ROOT(ejsval, strval, _ejs_object_getprop(args[i], _ejs_atom_name));
            char* strval_utf8 = ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(strval));
            OUTPUT ("[%s]", strval_utf8);
            free (strval_utf8);
        }
        else if (EJSVAL_IS_FUNCTION(args[i])) {
            ADD_STACK_ROOT(ejsval, strval, ((EJSFunction*)EJSVAL_TO_OBJECT(args[i]))->name);
            char* strval_utf8 = ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(strval));
            OUTPUT ("[Function: %s]", strval_utf8);
            free (strval_utf8);
        }
        else {
            ADD_STACK_ROOT(ejsval, strval, ToString(args[i]));

            char* strval_utf8 = ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(strval));
            OUTPUT ("%s", strval_utf8);
            free (strval_utf8);
        }
#if !IOS
        if (i < argc - 1)
            fputc (' ', outfile);
#endif
    }

#if !IOS
    fputc ('\n', outfile);
#endif

    END_SHADOW_STACK_FRAME;
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
