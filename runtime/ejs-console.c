/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include <math.h>
#include <string.h>
#include <sys/time.h>

#include "ejs-console.h"
#include "ejs-gc.h"
#include "ejs-ops.h"
#include "ejs-function.h"
#include "ejs-string.h"
#include "ejs-error.h"
#include "ejs-array.h"

#if IOS
#import <Foundation/Foundation.h>
#endif

#if IOS
#define OUTPUT0(str) NSLog(@str)
#define OUTPUT(format, ...) NSLog(@format, __VA_ARGS__)
#else
#define OUTPUT0(str) fprintf (outfile, str)
#define OUTPUT(format, ...) fprintf (outfile, format, __VA_ARGS__)
#endif

static ejsval
output (FILE *outfile, uint32_t argc, ejsval *args)
{
    for (int i = 0; i < argc; i ++) {
        if (EJSVAL_IS_NUMBER(args[i])) {
            double d = EJSVAL_TO_NUMBER(args[i]);
            int di;
            if (EJSDOUBLE_IS_INT32(d, &di))
                OUTPUT ("%d", di);
            else {
                int classified = fpclassify(d);
                if (classified == FP_INFINITE) {
                    if (d < 0)
                        OUTPUT0 ("-Infinity");
                    else
                        OUTPUT0 ("Infinity");
                }
                else if (classified == FP_NAN) {
                    OUTPUT0 ("NaN");
                }
                else
                    OUTPUT (EJS_NUMBER_FORMAT, d);
            }
        }
        else if (EJSVAL_IS_ARRAY(args[i])) {
            char* strval_utf8;

            if (EJS_ARRAY_LEN(args[i]) == 0) {
                OUTPUT0 ("[]");
            }
            else {
                ejsval comma_space = _ejs_string_new_utf8(", ");
                ejsval lbracket = _ejs_string_new_utf8("[ ");
                ejsval rbracket = _ejs_string_new_utf8(" ]");

                ejsval contents = _ejs_array_join (args[i], comma_space);

                strval_utf8 = ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(_ejs_string_concatv (lbracket, contents, rbracket, _ejs_null)));

                OUTPUT ("%s", strval_utf8);
                free (strval_utf8);
            }
        }
        else if (EJSVAL_IS_ERROR(args[i])) {
            ejsval strval = ToString(args[i]);

            char* strval_utf8 = ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(strval));
            OUTPUT ("[%s]", strval_utf8);
            free (strval_utf8);
        }
        else if (EJSVAL_IS_FUNCTION(args[i])) {
            ejsval func_name = _ejs_object_getprop (args[i], _ejs_atom_name);

            if (EJSVAL_IS_NULL_OR_UNDEFINED(func_name) || EJSVAL_TO_STRLEN(func_name) == 0) {
                OUTPUT0("[Function]");
            }
            else {
                char* strval_utf8 = ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(func_name));
                OUTPUT ("[Function: %s]", strval_utf8);
                free (strval_utf8);
            }
        }
        else {
            ejsval strval = ToString(args[i]);

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

// terrible, use a linked list for the timeval slots.
typedef struct TimevalSlot {
    EJS_LIST_HEADER(struct TimevalSlot);

    int str_len;
    char* str;
    struct timeval tv;
} TimevalSlot;

static TimevalSlot* timevals;

static void
add_timeval_slot(ejsval for_string, struct timeval** tv)
{
    // no checking here, we rely on callers having previously called
    // get_timeval_slot in order to check for duplicates
    TimevalSlot* new_tvs = malloc(sizeof(TimevalSlot));
    EJS_LIST_INIT(new_tvs);

    new_tvs->str = ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(for_string));
    new_tvs->str_len = EJSVAL_TO_STRLEN(for_string);
    memset(&new_tvs->tv, 0, sizeof(struct timeval));
    EJS_LIST_PREPEND(new_tvs, timevals);
    *tv = &new_tvs->tv;
}

static EJSBool
get_timeval_slot(ejsval for_string, struct timeval** tv, char** str_out)
{
    char* str = ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(for_string));
    int forstr_len = EJSVAL_TO_STRLEN(for_string);

    for (TimevalSlot* tvs = timevals; tvs; tvs = tvs->next) {
        if (forstr_len == tvs->str_len && !strcmp(str, tvs->str)) {
            if (tv)      *tv = &tvs->tv;
            if (str_out) *str_out = tvs->str;
            free(str);
            return EJS_TRUE;
        }
    }
    free(str);
    return EJS_FALSE;
}

static void
remove_timeval_slot(ejsval for_string)
{
    char* str = ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(for_string));
    int forstr_len = EJSVAL_TO_STRLEN(for_string);

    for (TimevalSlot* tvs = timevals; tvs; tvs = tvs->next) {
        if (forstr_len == tvs->str_len && !strcmp(str, tvs->str)) {
            EJS_LIST_DETACH(tvs, timevals);
            free(str);
            free(tvs->str);
            free(tvs);
            return;
        }
    }
    
    free(str);
    _ejs_log("timer wasn't present in list.");
}

static ejsval
_ejs_console_time (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    if (argc == 0 || !EJSVAL_IS_STRING(args[0])) return _ejs_undefined;

    char* str;
    if (get_timeval_slot(args[0], NULL, &str)) {
        char buf[256];
        snprintf (buf, sizeof(buf), "the timer %s is already active", str);
        _ejs_throw_nativeerror_utf8 (EJS_ERROR, buf);
    }

    struct timeval *tv_before_slot;

    add_timeval_slot(args[0], &tv_before_slot);

    // do the gettimeofday as last as possible here so we don't include our overhead
    gettimeofday (tv_before_slot, NULL);

    return _ejs_undefined;
}

static ejsval
_ejs_console_timeEnd (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    struct timeval tvafter;

    // do the gettimeofday as early as possible here so we don't include our overhead
    gettimeofday (&tvafter, NULL);

    if (argc == 0 || !EJSVAL_IS_STRING(args[0])) return _ejs_undefined;

    struct timeval *tv_before_slot;
    char *str;
    if (!get_timeval_slot(args[0], &tv_before_slot, &str)) {
        _ejs_throw_nativeerror_utf8 (EJS_ERROR, "the timer XXX was not active");
    }

    uint64_t usec_before = tv_before_slot->tv_sec * 1000000 + tv_before_slot->tv_usec;
    uint64_t usec_after = tvafter.tv_sec * 1000000 + tvafter.tv_usec;

#if !IOS
    FILE* outfile = stdout;
#endif
    OUTPUT ("%s: %gms", str, (usec_after - usec_before) / 1000.0);
#if !IOS
    fprintf (outfile, "\n");
#endif

    remove_timeval_slot(args[0]);

    return _ejs_undefined;
}

ejsval _ejs_console EJSVAL_ALIGNMENT;

void
_ejs_console_init(ejsval global)
{
    _ejs_console = _ejs_object_new (_ejs_null, &_ejs_Object_specops);
    _ejs_object_setprop (global, _ejs_atom_console, _ejs_console);

#define OBJ_METHOD(x) EJS_INSTALL_ATOM_FUNCTION(_ejs_console, x, _ejs_console_##x)

    OBJ_METHOD(log);
    OBJ_METHOD(warn);
    OBJ_METHOD(time);
    OBJ_METHOD(timeEnd);

#undef OBJ_METHOD
}
