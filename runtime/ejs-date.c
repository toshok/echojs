/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include "ejs-ops.h"
#include "ejs-value.h"
#include "ejs-function.h"
#include "ejs-date.h"
#include "ejs-string.h"

#include <string.h>

static ejsval  _ejs_date_specop_get (ejsval obj, ejsval propertyName);
static EJSPropertyDesc* _ejs_date_specop_get_own_property (ejsval obj, ejsval propertyName);
static EJSPropertyDesc* _ejs_date_specop_get_property (ejsval obj, ejsval propertyName);
static void    _ejs_date_specop_put (ejsval obj, ejsval propertyName, ejsval val, EJSBool flag);
static EJSBool _ejs_date_specop_can_put (ejsval obj, ejsval propertyName);
static EJSBool _ejs_date_specop_has_property (ejsval obj, ejsval propertyName);
static EJSBool _ejs_date_specop_delete (ejsval obj, ejsval propertyName, EJSBool flag);
static ejsval  _ejs_date_specop_default_value (ejsval obj, const char *hint);
static EJSBool _ejs_date_specop_define_own_property (ejsval obj, ejsval propertyName, EJSPropertyDesc* propertyDescriptor, EJSBool flag);
static EJSObject* _ejs_date_specop_allocate ();
static void    _ejs_date_specop_finalize (EJSObject* obj);
static void    _ejs_date_specop_scan (EJSObject* obj, EJSValueFunc scan_func);

EJSSpecOps _ejs_date_specops = {
    "Date",
    _ejs_date_specop_get,
    _ejs_date_specop_get_own_property,
    _ejs_date_specop_get_property,
    _ejs_date_specop_put,
    _ejs_date_specop_can_put,
    _ejs_date_specop_has_property,
    _ejs_date_specop_delete,
    _ejs_date_specop_default_value,
    _ejs_date_specop_define_own_property,
    NULL, /* [[HasInstance]] */

    _ejs_date_specop_allocate,
    _ejs_date_specop_finalize,
    _ejs_date_specop_scan,
};

ejsval
_ejs_date_unix_now ()
{
    EJSDate* rv = _ejs_gc_new (EJSDate);

    _ejs_init_object ((EJSObject*)rv, _ejs_Date_proto, &_ejs_date_specops);

    gettimeofday (&rv->tv, &rv->tz);

    return OBJECT_TO_EJSVAL(rv);
}

double
_ejs_date_get_time (EJSDate *date)
{
    return (double)date->tv.tv_sec * 1000 + (double)date->tv.tv_usec / 1000;
}

ejsval _ejs_Date EJSVAL_ALIGNMENT;
ejsval _ejs_Date_proto EJSVAL_ALIGNMENT;

static ejsval
_ejs_Date_impl (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    if (EJSVAL_IS_UNDEFINED(_this)) {
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
        EJSDate* date = (EJSDate*) EJSVAL_TO_OBJECT(_this);

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
        }
      
        return _this;
    }
}

static ejsval
_ejs_Date_prototype_toString (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    EJSDate *date = (EJSDate*)EJSVAL_TO_OBJECT(_this);
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

static ejsval
_ejs_Date_prototype_getTimezoneOffset (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    EJSDate *date = (EJSDate*)EJSVAL_TO_OBJECT(_this);
    struct tm tm;

    time_t time_in_sec = date->tv.tv_sec;

    localtime_r (&time_in_sec, &tm);

    return NUMBER_TO_EJSVAL (tm.tm_gmtoff);
}

static ejsval
_ejs_Date_prototype_getTime (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    return NUMBER_TO_EJSVAL(_ejs_date_get_time ((EJSDate*)EJSVAL_TO_OBJECT(_this)));
}

void
_ejs_date_init(ejsval global)
{
    _ejs_Date = _ejs_function_new_without_proto (_ejs_null, _ejs_atom_Date, (EJSClosureFunc)_ejs_Date_impl);
    _ejs_object_setprop (global, _ejs_atom_Date, _ejs_Date);

    _ejs_gc_add_root (&_ejs_Date_proto);
    _ejs_Date_proto = _ejs_object_new(_ejs_null, &_ejs_object_specops);
    _ejs_object_setprop (_ejs_Date,       _ejs_atom_prototype,  _ejs_Date_proto);

#define PROTO_METHOD(x) EJS_INSTALL_ATOM_FUNCTION_FLAGS (_ejs_Date_proto, x, _ejs_Date_prototype_##x, EJS_PROP_NOT_ENUMERABLE)

    PROTO_METHOD(toString);
    PROTO_METHOD(getTime);
    PROTO_METHOD(getTimezoneOffset);

#undef PROTO_METHOD
}

static ejsval
_ejs_date_specop_get (ejsval obj, ejsval propertyName)
{
    return _ejs_object_specops.get (obj, propertyName);
}

static EJSPropertyDesc*
_ejs_date_specop_get_own_property (ejsval obj, ejsval propertyName)
{
    return _ejs_object_specops.get_own_property (obj, propertyName);
}

static EJSPropertyDesc*
_ejs_date_specop_get_property (ejsval obj, ejsval propertyName)
{
    return _ejs_object_specops.get_property (obj, propertyName);
}

static void
_ejs_date_specop_put (ejsval obj, ejsval propertyName, ejsval val, EJSBool flag)
{
    _ejs_object_specops.put (obj, propertyName, val, flag);
}

static EJSBool
_ejs_date_specop_can_put (ejsval obj, ejsval propertyName)
{
    return _ejs_object_specops.can_put (obj, propertyName);
}

static EJSBool
_ejs_date_specop_has_property (ejsval obj, ejsval propertyName)
{
    return _ejs_object_specops.has_property (obj, propertyName);
}

static EJSBool
_ejs_date_specop_delete (ejsval obj, ejsval propertyName, EJSBool flag)
{
    return _ejs_object_specops._delete (obj, propertyName, flag);
}

static ejsval
_ejs_date_specop_default_value (ejsval obj, const char *hint)
{
    return _ejs_object_specops.default_value (obj, hint);
}

static EJSBool
_ejs_date_specop_define_own_property (ejsval obj, ejsval propertyName, EJSPropertyDesc* propertyDescriptor, EJSBool flag)
{
    return _ejs_object_specops.define_own_property (obj, propertyName, propertyDescriptor, flag);
}

static EJSObject*
_ejs_date_specop_allocate()
{
    return (EJSObject*)_ejs_gc_new (EJSDate);
}

static void
_ejs_date_specop_finalize (EJSObject* obj)
{
    _ejs_object_specops.finalize (obj);
}

static void
_ejs_date_specop_scan (EJSObject* obj, EJSValueFunc scan_func)
{
    _ejs_object_specops.scan (obj, scan_func);
}
