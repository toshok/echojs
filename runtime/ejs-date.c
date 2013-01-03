/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include <assert.h>

#include "ejs-ops.h"
#include "ejs-value.h"
#include "ejs-function.h"
#include "ejs-date.h"

static ejsval  _ejs_date_specop_get (ejsval obj, ejsval propertyName, EJSBool isCStr);
static EJSPropertyDesc* _ejs_date_specop_get_own_property (ejsval obj, ejsval propertyName);
static EJSPropertyDesc* _ejs_date_specop_get_property (ejsval obj, ejsval propertyName);
static void    _ejs_date_specop_put (ejsval obj, ejsval propertyName, ejsval val, EJSBool flag);
static EJSBool _ejs_date_specop_can_put (ejsval obj, ejsval propertyName);
static EJSBool _ejs_date_specop_has_property (ejsval obj, ejsval propertyName);
static EJSBool _ejs_date_specop_delete (ejsval obj, ejsval propertyName, EJSBool flag);
static ejsval  _ejs_date_specop_default_value (ejsval obj, const char *hint);
static EJSBool _ejs_date_specop_define_own_property (ejsval obj, ejsval propertyName, EJSPropertyDesc* propertyDescriptor, EJSBool flag);
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
    _ejs_date_specop_finalize,
    _ejs_date_specop_scan,
};

EJSObject* _ejs_date_alloc_instance()
{
    return (EJSObject*)_ejs_gc_new (EJSDate);
}

ejsval
_ejs_date_new_unix (int timestamp)
{
    EJSDate* rv = _ejs_gc_new (EJSDate);

    _ejs_init_object ((EJSObject*)rv, _ejs_Date_proto, &_ejs_date_specops);

    time_t t = (time_t)timestamp;

    if (!localtime_r(&t, &rv->tm))
        EJS_NOT_IMPLEMENTED();

    return OBJECT_TO_EJSVAL((EJSObject*)rv);
}

ejsval _ejs_Date;
ejsval _ejs_Date_proto;

static ejsval
_ejs_Date_impl (ejsval env, ejsval _this, int argc, ejsval *args)
{
    if (EJSVAL_IS_UNDEFINED(_this)) {
        // called as a function
        if (argc == 0) {
            return _ejs_date_new_unix(time(NULL));
        }
        else {
            EJS_NOT_IMPLEMENTED();
        }
    }
    else {
        printf ("called Date() as a constructor!\n");

        EJSDate* date = (EJSDate*) EJSVAL_TO_OBJECT(_this);

        // new Date (year, month [, date [, hours [, minutes [, seconds [, ms ] ] ] ] ] )

        if (argc <= 1) {
            time_t t = (time_t)time(NULL);

            if (!gmtime_r(&t, &date->tm))
                EJS_NOT_IMPLEMENTED();
        }
        else {
            // there are all sorts of validation steps here that are missing from ejs
            date->tm.tm_year = (int)(ToDouble(args[0]) - 1900);
            date->tm.tm_mon = (int)(ToDouble(args[1]));
            if (argc > 2) date->tm.tm_mday = (int)(ToDouble(args[2]));
            if (argc > 3) date->tm.tm_hour = (int)(ToDouble(args[3]));
            if (argc > 4) date->tm.tm_min = (int)(ToDouble(args[4]));
            if (argc > 5) date->tm.tm_sec = (int)(ToDouble(args[5]));
            // ms?
        }
      
        return _this;
    }
}

static ejsval
_ejs_Date_prototype_toString (ejsval env, ejsval _this, int argc, ejsval *args)
{
    EJSDate *date = (EJSDate*)EJSVAL_TO_OBJECT(_this);

    // returns strings of the format 'Tue Aug 28 2012 16:45:58 GMT-0700 (PDT)'

    char date_buf[256];
    if (date->tm.tm_gmtoff == 0)
        strftime (date_buf, sizeof(date_buf), "%a %b %d %Y %T GMT", &date->tm);
    else
        strftime (date_buf, sizeof(date_buf), "%a %b %d %Y %T GMT%z (%Z)", &date->tm);

    return _ejs_string_new_utf8 (date_buf);
}

static ejsval
_ejs_Date_prototype_getTimezoneOffset (ejsval env, ejsval _this, int argc, ejsval *args)
{
    EJSDate *date = (EJSDate*)EJSVAL_TO_OBJECT(_this);

    return NUMBER_TO_EJSVAL (date->tm.tm_gmtoff);
}

void
_ejs_date_init(ejsval global)
{
    START_SHADOW_STACK_FRAME;

    _ejs_gc_add_named_root (_ejs_Date_proto);
    _ejs_Date_proto = _ejs_object_new(_ejs_null);

    ADD_STACK_ROOT(ejsval, tmpobj, _ejs_function_new (_ejs_null, _ejs_atom_Date, (EJSClosureFunc)_ejs_Date_impl));
    _ejs_Date = tmpobj;

    _ejs_object_setprop (_ejs_Date,       _ejs_atom_prototype,  _ejs_Date_proto);

#define PROTO_METHOD(x) EJS_INSTALL_FUNCTION(_ejs_Date_proto, EJS_STRINGIFY(x), _ejs_Date_prototype_##x)

    PROTO_METHOD(toString);
    PROTO_METHOD(getTimezoneOffset);

#undef PROTO_METHOD

    _ejs_object_setprop (global, _ejs_atom_Date, _ejs_Date);

    END_SHADOW_STACK_FRAME;
}

static ejsval
_ejs_date_specop_get (ejsval obj, ejsval propertyName, EJSBool isCStr)
{
    return _ejs_object_specops.get (obj, propertyName, isCStr);
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
