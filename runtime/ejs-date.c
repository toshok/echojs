#include <assert.h>

#include "ejs-ops.h"
#include "ejs-value.h"
#include "ejs-date.h"

EJSObject* _ejs_date_alloc_instance()
{
  return (EJSObject*)calloc(1, sizeof (EJSDate));
}

EJSValue*
_ejs_date_new_unix (int timestamp)
{
  EJSDate* rv = (EJSDate*)calloc(1, sizeof (EJSDate));

  _ejs_init_object ((EJSObject*)rv, _ejs_date_get_prototype());

  time_t t = (time_t)timestamp;

  if (!localtime_r(&t, &rv->tm))
    NOT_IMPLEMENTED();

  return (EJSValue*)rv;
}

EJSValue* _ejs_Date;
static EJSValue*
_ejs_Date_impl (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  if (EJSVAL_IS_UNDEFINED(_this)) {
    // called as a function
    if (argc == 0) {
      return _ejs_date_new_unix(time(NULL));
    }
    else {
      NOT_IMPLEMENTED();
    }
  }
  else {
    printf ("called Date() as a constructor!\n");

    EJSDate* date = (EJSDate*) _this;

    // new Date (year, month [, date [, hours [, minutes [, seconds [, ms ] ] ] ] ] )

    if (argc <= 1) {
      time_t t = (time_t)time(NULL);

      if (!gmtime_r(&t, &date->tm))
	NOT_IMPLEMENTED();
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

static EJSValue* _ejs_Date_proto;
EJSValue*
_ejs_date_get_prototype()
{
  return _ejs_Date_proto;
}

static EJSValue*
_ejs_Date_prototype_toString (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  EJSDate *date = (EJSDate*)_this;

  // returns strings of the format 'Tue Aug 28 2012 16:45:58 GMT-0700 (PDT)'

  char date_buf[256];
  if (date->tm.tm_gmtoff == 0)
    strftime (date_buf, sizeof(date_buf), "%a %b %d %Y %T GMT", &date->tm);
  else
    strftime (date_buf, sizeof(date_buf), "%a %b %d %Y %T GMT%z (%Z)", &date->tm);

  return _ejs_string_new_utf8 (date_buf);
}

static EJSValue*
_ejs_Date_prototype_getTimezoneOffset (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  EJSDate *date = (EJSDate*)_this;

  return _ejs_number_new (date->tm.tm_gmtoff);
}

void
_ejs_date_init(EJSValue *global)
{
  _ejs_Date = _ejs_function_new_utf8 (NULL, "Date", (EJSClosureFunc)_ejs_Date_impl);
  _ejs_Date_proto = _ejs_object_new(NULL);

  _ejs_object_setprop_utf8 (_ejs_Date,       "prototype",  _ejs_Date_proto);

#define PROTO_METHOD(x) _ejs_object_setprop_utf8 (_ejs_Date_proto, #x, _ejs_function_new_utf8 (NULL, #x, (EJSClosureFunc)_ejs_Date_prototype_##x))

  PROTO_METHOD(toString);
  PROTO_METHOD(getTimezoneOffset);

  _ejs_object_setprop_utf8 (global, "Date", _ejs_Date);
}
