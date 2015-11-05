/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include "ejs-ops.h"
#include "ejs-value.h"
#include "ejs-function.h"
#include "ejs-date.h"
#include "ejs-error.h"
#include "ejs-string.h"
#include "ejs-proxy.h"
#include "ejs-symbol.h"

#include <string.h>

ejsval
_ejs_date_unix_now ()
{
    EJSDate* rv = _ejs_gc_new (EJSDate);

    _ejs_init_object ((EJSObject*)rv, _ejs_Date_prototype, &_ejs_Date_specops);

    gettimeofday (&rv->tv, &rv->tz);

    return OBJECT_TO_EJSVAL(rv);
}

double
_ejs_date_get_time (EJSDate *date)
{
    return (double)date->tv.tv_sec * 1000 + (double)date->tv.tv_usec / 1000;
}

ejsval _ejs_Date EJSVAL_ALIGNMENT;
ejsval _ejs_Date_prototype EJSVAL_ALIGNMENT;

static EJS_NATIVE_FUNC(_ejs_Date_impl) {
    if (callFlags == EJS_CALL_FLAGS_CALL) {
        // called as a function
        if (argc == 0) {
            // XXX we shouldn't be creating a date object here and immediately throwing it away.
            // instead just refactor and create the string directly
            return ToString(_ejs_date_unix_now());
        }
        else {
            EJS_NOT_IMPLEMENTED();
        }
    }
    else {
        EJSDate* date = (EJSDate*) EJSVAL_TO_OBJECT(*_this);

        // new Date (year, month [, date [, hours [, minutes [, seconds [, ms ] ] ] ] ] )

        if (argc < 2) {
            if (gettimeofday (&date->tv, &date->tz) < 0)
                EJS_NOT_IMPLEMENTED();
        }
        else {
            struct tm tm;
            int ms = 0;

            memset (&tm, 0, sizeof(tm));
            // there are all sorts of validation steps here that are missing from ejs
            tm.tm_year = ToInteger(args[0]) - 1900;
            tm.tm_mon = ToInteger(args[1]);
            
            tm.tm_mday = (argc > 2) ? ToInteger(args[2]) : 1;
            tm.tm_hour = (argc > 3) ? ToInteger(args[3]) : 0;
            tm.tm_hour = (argc > 4) ? ToInteger(args[4]) : 0;
            tm.tm_sec  = (argc > 5) ? ToInteger(args[5]) : 0;
   
            if (argc > 6) {
                ms = ToInteger(args[6]);
                if (ms > 1000) {
                    tm.tm_sec += ms / 1000;
                    ms = ms % 1000;
                }
            }

            time_t time_in_sec = mktime(&tm);
            date->tv.tv_sec = time_in_sec;
            date->tv.tv_usec = ms * 1000;
            date->valid = EJS_TRUE;
        }
      
        return *_this;
    }
}

static EJS_NATIVE_FUNC(_ejs_Date_prototype_toString) {
    if (!EJSVAL_IS_DATE(*_this))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "this is not a Date object.");

    EJSDate *date = (EJSDate*)EJSVAL_TO_OBJECT(*_this);

    if (!date->valid) {
        return _ejs_string_new_utf8 ("Invalid Date");
    }

    struct tm tm;

    memset (&tm, 0, sizeof(tm));

    time_t time_in_sec = date->tv.tv_sec;

    localtime_r (&time_in_sec, &tm);

    // returns strings of the format 'Tue Aug 28 2012 16:45:58 GMT-0700 (PDT)'

    char date_buf[256];
    if (tm.tm_gmtoff == 0)
        strftime (date_buf, sizeof(date_buf), "%a %b %d %Y %T GMT", &tm);
    else
        strftime (date_buf, sizeof(date_buf), "%a %b %d %Y %T GMT%z (%Z)", &tm);

    return _ejs_string_new_utf8 (date_buf);
}

static EJS_NATIVE_FUNC(_ejs_Date_prototype_getTimezoneOffset) {
    EJSDate *date = (EJSDate*)EJSVAL_TO_OBJECT(*_this);
    struct tm tm;

    time_t time_in_sec = date->tv.tv_sec;

    localtime_r (&time_in_sec, &tm);

    return NUMBER_TO_EJSVAL (tm.tm_gmtoff);
}

static EJS_NATIVE_FUNC(_ejs_Date_prototype_getTime) {
    return NUMBER_TO_EJSVAL(_ejs_date_get_time ((EJSDate*)EJSVAL_TO_OBJECT(*_this)));
}

static EJS_NATIVE_FUNC(_ejs_Date_now) {
    struct timeval tv;
    struct timezone tz;
    gettimeofday (&tv, &tz);
    return NUMBER_TO_EJSVAL(tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

void
_ejs_date_init(ejsval global)
{
    _ejs_Date = _ejs_function_new_without_proto (_ejs_null, _ejs_atom_Date, _ejs_Date_impl);
    _ejs_object_setprop (global, _ejs_atom_Date, _ejs_Date);

    _ejs_gc_add_root (&_ejs_Date_prototype);
    _ejs_Date_prototype = _ejs_object_new(_ejs_null, &_ejs_Date_specops);
    
    _ejs_object_setprop (_ejs_Date,       _ejs_atom_prototype,  _ejs_Date_prototype);

#define PROTO_METHOD(x) EJS_INSTALL_ATOM_FUNCTION_FLAGS (_ejs_Date_prototype, x, _ejs_Date_prototype_##x, EJS_PROP_NOT_ENUMERABLE)
#define OBJ_METHOD(x) EJS_INSTALL_ATOM_FUNCTION_FLAGS (_ejs_Date, x, _ejs_Date_##x, EJS_PROP_NOT_ENUMERABLE)

    PROTO_METHOD(toString);
    PROTO_METHOD(getTime);
    PROTO_METHOD(getTimezoneOffset);

    OBJ_METHOD(now);

#undef PROTO_METHOD
}

static EJSObject*
_ejs_date_specop_allocate()
{
    return (EJSObject*)_ejs_gc_new (EJSDate);
}

EJS_DEFINE_CLASS(Date,
                 OP_INHERIT, // [[GetPrototypeOf]]
                 OP_INHERIT, // [[SetPrototypeOf]]
                 OP_INHERIT, // [[IsExtensible]]
                 OP_INHERIT, // [[PreventExtensions]]
                 OP_INHERIT, // [[GetOwnProperty]]
                 OP_INHERIT, // [[DefineOwnProperty]]
                 OP_INHERIT, // [[HasProperty]]
                 OP_INHERIT, // [[Get]]
                 OP_INHERIT, // [[Set]]
                 OP_INHERIT, // [[Delete]]
                 OP_INHERIT, // [[Enumerate]]
                 OP_INHERIT, // [[OwnPropertyKeys]]
                 OP_INHERIT, // [[Call]]
                 OP_INHERIT, // [[Construct]]
                 _ejs_date_specop_allocate,
                 OP_INHERIT, // [[Finalize]]
                 OP_INHERIT  // [[Scan]]
                 )
