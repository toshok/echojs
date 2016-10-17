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
#include "ejs-symbol.h"
#include "ejs-error.h"
#include "ejs-array.h"
#include "ejs-number.h"
#include "ejs-typedarrays.h"
#include "ejs-json.h"
#include "ejs-regexp.h"

#if IOS
#import <Foundation/Foundation.h>
#endif

#if IOS
#define OUTPUT(format, ...) if (force_stdout) fprintf (outfile, format, __VA_ARGS__); else NSLog(@format, __VA_ARGS__)
#else
#define OUTPUT(format, ...) fprintf (outfile, format, __VA_ARGS__)
#endif

static EJSBool force_stdout;

ejsval console_toString(ejsval arg);

ejsval
console_toString(ejsval arg) {
    if (EJSVAL_IS_STRING(arg)) {
        return arg;
    }
    else if (EJSVAL_IS_SYMBOL(arg)) {
        return EJSVAL_TO_SYMBOL(arg)->description;
    }
    else if (EJSVAL_IS_NUMBER(arg) || EJSVAL_IS_NUMBER_OBJECT(arg)) {
        return _ejs_number_to_string(arg);
    }
    else if (EJSVAL_IS_ARRAY(arg)) {
        if (EJS_ARRAY_LEN(arg) == 0) {
            return _ejs_string_new_utf8("[]");
        }
        else {
            ejsval comma_space = _ejs_string_new_utf8(", ");
            ejsval lbracket = _ejs_string_new_utf8("[ ");
            ejsval rbracket = _ejs_string_new_utf8(" ]");
             
            ejsval content_strings = _ejs_array_new(EJS_ARRAY_LEN(arg), EJS_FALSE);
            // XXX the loop below assumes arg is a dense array
            EJS_ASSERT(EJSVAL_IS_DENSE_ARRAY(arg));
            for (int i = 0; i < EJS_ARRAY_LEN(arg); i ++) {
                EJS_DENSE_ARRAY_ELEMENTS(content_strings)[i] = console_toString(EJS_DENSE_ARRAY_ELEMENTS(arg)[i]);
            }

            ejsval contents = _ejs_array_join (content_strings, comma_space);
             
            return _ejs_string_concatv (lbracket, contents, rbracket, _ejs_null);
        }
    }
    else if (EJSVAL_IS_TYPEDARRAY(arg)) {
        if (EJS_TYPED_ARRAY_LEN(arg) == 0) {
            return _ejs_string_new_utf8("[]");
        }
        else {
            ejsval lbracket = _ejs_string_new_utf8("[ ");
            ejsval rbracket = _ejs_string_new_utf8(" ]");
            ejsval contents = ToString(arg);
            return _ejs_string_concatv (lbracket, contents, rbracket, _ejs_null);
        }
    }
    else if (EJSVAL_IS_ERROR(arg)) {
        ejsval strval = ToString(arg);
        ejsval lbracket = _ejs_string_new_utf8("[");
        ejsval rbracket = _ejs_string_new_utf8("]");

        return _ejs_string_concatv(lbracket, strval, rbracket, _ejs_null);
    }
    else if (EJSVAL_IS_FUNCTION(arg)) {
        ejsval func_name = _ejs_object_getprop (arg, _ejs_atom_name);

        if (EJSVAL_IS_NULL_OR_UNDEFINED(func_name) || EJSVAL_TO_STRLEN(func_name) == 0) {
            return _ejs_string_new_utf8("[Function]");
        }
        else {
            ejsval left = _ejs_string_new_utf8("[Function: ");
            ejsval right = _ejs_string_new_utf8("]");

            return _ejs_string_concatv(left, func_name, right, _ejs_null);
        }
    }
    else if (EJSVAL_IS_STRING_OBJECT(arg)) {
        return ToString(arg);
    }
    else if (EJSVAL_IS_SYMBOL_OBJECT(arg)) {
        EJS_NOT_IMPLEMENTED();
    }
    else if (EJSVAL_IS_REGEXP(arg)) {
        return ToString(arg);
    }
    else if (EJSVAL_IS_OBJECT(arg)) {
        ejsval ret = _ejs_json_stringify(arg);
        if (EJSVAL_IS_NULL(ret))
            return ToString(arg);
        return ret;
    }
    else {
        return ToString(arg);
    }
}

static ejsval
output (FILE *outfile, uint32_t argc, ejsval *args)
{
    for (int i = 0; i < argc; i ++) {
        ejsval out_str = console_toString(args[i]);
        char* strval_utf8 = ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(out_str));
        OUTPUT ("%s", strval_utf8);
        free (strval_utf8);
#if IOS
        if (force_stdout)
#endif
            if (i < argc - 1)
                fputc (' ', outfile);
    }

#if IOS
    if (force_stdout)
#endif
        fputc ('\n', outfile);

    return _ejs_undefined;
}


static EJS_NATIVE_FUNC(_ejs_console_log) {
    return output (stdout, argc, args);
}

static EJS_NATIVE_FUNC(_ejs_console_warn) {
    return output (stderr, argc, args);
}

static EJS_NATIVE_FUNC(_ejs_console_error) {
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

static EJS_NATIVE_FUNC(_ejs_console_time) {
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

static EJS_NATIVE_FUNC(_ejs_console_timeEnd) {
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

    FILE* outfile = stdout;

    OUTPUT ("%s: %gms", str, (usec_after - usec_before) / 1000.0);

#if IOS
    if (force_stdout)
#endif
        fprintf (outfile, "\n");

    remove_timeval_slot(args[0]);

    return _ejs_undefined;
}

ejsval _ejs_console EJSVAL_ALIGNMENT;

void
_ejs_console_init(ejsval global)
{
    force_stdout = getenv("EJS_FORCE_STDOUT") != NULL;

    _ejs_console = _ejs_object_new (_ejs_null, &_ejs_Object_specops);
    _ejs_object_setprop (global, _ejs_atom_console, _ejs_console);

#define OBJ_METHOD(x) EJS_INSTALL_ATOM_FUNCTION(_ejs_console, x, _ejs_console_##x)

    OBJ_METHOD(log);
    OBJ_METHOD(warn);
    OBJ_METHOD(error);
    OBJ_METHOD(time);
    OBJ_METHOD(timeEnd);

#undef OBJ_METHOD
}
