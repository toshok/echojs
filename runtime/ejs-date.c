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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

ejsval _ejs_Date EJSVAL_ALIGNMENT;
ejsval _ejs_Date_prototype EJSVAL_ALIGNMENT;

// ES2015 20.3.1: date/time abstract operations over the time value
// (a double: ms since the epoch, NaN = invalid).  All of this is pure
// double math so NaN propagates naturally.

#define MS_PER_SECOND 1000.0
#define MS_PER_MINUTE 60000.0
#define MS_PER_HOUR   3600000.0
#define MS_PER_DAY    86400000.0
#define MAX_TIME_VALUE 8.64e15

static double
pos_mod (double x, double y)
{
    double r = fmod(x, y);
    if (r < 0) r += y;
    return r;
}

static double
Day (double t)
{
    return floor(t / MS_PER_DAY);
}

static double
TimeWithinDay (double t)
{
    return pos_mod(t, MS_PER_DAY);
}

static double
DayFromYear (double y)
{
    return 365 * (y - 1970) + floor((y - 1969) / 4) - floor((y - 1901) / 100) + floor((y - 1601) / 400);
}

static double
TimeFromYear (double y)
{
    return DayFromYear(y) * MS_PER_DAY;
}

static EJSBool
IsLeapYear (double y)
{
    if (fmod(y, 4) != 0) return EJS_FALSE;
    if (fmod(y, 100) != 0) return EJS_TRUE;
    return fmod(y, 400) == 0;
}

static double
YearFromTime (double t)
{
    if (isnan(t)) return NAN;
    double y = floor(t / (365.2425 * MS_PER_DAY)) + 1970;
    while (TimeFromYear(y) > t)
        y -= 1;
    while (TimeFromYear(y + 1) <= t)
        y += 1;
    return y;
}

// day offset of the first of each month in a non-leap year
static const int month_starts[12] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };

static double
DayWithinYear (double t)
{
    return Day(t) - DayFromYear(YearFromTime(t));
}

static double
MonthFromTime (double t)
{
    if (isnan(t)) return NAN;
    int dwy = (int)DayWithinYear(t);
    int leap = IsLeapYear(YearFromTime(t)) ? 1 : 0;
    for (int m = 11; m >= 0; m--) {
        int start = month_starts[m] + (m >= 2 ? leap : 0);
        if (dwy >= start)
            return m;
    }
    return 0;
}

static double
DateFromTime (double t)
{
    if (isnan(t)) return NAN;
    int m = (int)MonthFromTime(t);
    int leap = IsLeapYear(YearFromTime(t)) ? 1 : 0;
    return DayWithinYear(t) - (month_starts[m] + (m >= 2 ? leap : 0)) + 1;
}

static double
WeekDay (double t)
{
    return pos_mod(Day(t) + 4, 7);
}

static double
HourFromTime (double t)
{
    return pos_mod(floor(t / MS_PER_HOUR), 24);
}

static double
MinFromTime (double t)
{
    return pos_mod(floor(t / MS_PER_MINUTE), 60);
}

static double
SecFromTime (double t)
{
    return pos_mod(floor(t / MS_PER_SECOND), 60);
}

static double
msFromTime (double t)
{
    return pos_mod(t, MS_PER_SECOND);
}

static double
MakeTime (double hour, double min, double sec, double ms)
{
    if (!isfinite(hour) || !isfinite(min) || !isfinite(sec) || !isfinite(ms))
        return NAN;
    // separate statements keep the compiler from contracting these into
    // fma, which would diverge from the spec's IEEE754 add/mul sequence
    double h = trunc(hour) * MS_PER_HOUR;
    double m = trunc(min) * MS_PER_MINUTE;
    double s = trunc(sec) * MS_PER_SECOND;
    double t = h + m;
    t = t + s;
    t = t + trunc(ms);
    return t;
}

static double
MakeDay (double year, double month, double date)
{
    if (!isfinite(year) || !isfinite(month) || !isfinite(date))
        return NAN;
    double y = trunc(year);
    double m = trunc(month);
    double dt = trunc(date);
    double ym = y + floor(m / 12);
    if (!isfinite(ym))
        return NAN;
    int mn = (int)pos_mod(m, 12);
    double day = DayFromYear(ym) + month_starts[mn] + ((mn >= 2 && IsLeapYear(ym)) ? 1 : 0);
    return day + dt - 1;
}

static double
MakeDate (double day, double time)
{
    if (!isfinite(day) || !isfinite(time))
        return NAN;
    double d = day * MS_PER_DAY;
    return d + time;
}

static double
TimeClip (double t)
{
    if (!isfinite(t) || fabs(t) > MAX_TIME_VALUE)
        return NAN;
    double c = trunc(t);
    return c == 0 ? 0 : c; // normalize -0 to +0
}

// local timezone adjustment (ms) at time value t, via the C library.
// tests run under TZ=UTC so this is 0 in practice.
static double
local_tza (double t)
{
    if (isnan(t)) return 0;
    time_t sec = (time_t)floor(t / 1000);
    struct tm tm;
    if (localtime_r (&sec, &tm) == NULL)
        return 0;
    return (double)tm.tm_gmtoff * 1000;
}

static double
LocalTime (double t)
{
    if (isnan(t)) return NAN;
    return t + local_tza(t);
}

static double
UTCTime (double t)
{
    if (isnan(t)) return NAN;
    return t - local_tza(t);
}

static double
time_now (void)
{
    struct timeval tv;
    gettimeofday (&tv, NULL);
    return (double)tv.tv_sec * 1000 + (double)(tv.tv_usec / 1000);
}

double
_ejs_date_get_time (EJSDate *date)
{
    return date->date_value;
}

// 20.3.4: thisTimeValue(value)
static double
this_time_value (ejsval value)
{
    if (!EJSVAL_IS_DATE(value))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "this is not a Date object.");
    return ((EJSDate*)EJSVAL_TO_OBJECT(value))->date_value;
}

// every Date method is a non-constructor: [[Construct]] must throw
#define REJECT_CONSTRUCT() EJS_MACRO_START                              \
    if (!EJSVAL_IS_UNDEFINED(newTarget))                                \
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "function is not a constructor"); \
    EJS_MACRO_END

static ejsval
arg_or_undefined (uint32_t argc, ejsval* args, uint32_t i)
{
    return argc > i ? args[i] : _ejs_undefined;
}

// ---------------------------------------------------------------- formatting

static const char* const weekday_names[7] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
static const char* const month_names[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                             "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

static void
format_year (char* buf, size_t buflen, double y)
{
    if (y >= 0)
        snprintf (buf, buflen, "%04d", (int)y);
    else
        snprintf (buf, buflen, "-%06d", (int)-y);
}

// "Fri Aug 01 2026"
static void
format_date_part (char* buf, size_t buflen, double tl)
{
    char ybuf[16];
    format_year (ybuf, sizeof(ybuf), YearFromTime(tl));
    snprintf (buf, buflen, "%s %s %02d %s",
              weekday_names[(int)WeekDay(tl)],
              month_names[(int)MonthFromTime(tl)],
              (int)DateFromTime(tl),
              ybuf);
}

// "12:34:56 GMT+0000 (Coordinated Universal Time)"
static void
format_time_part (char* buf, size_t buflen, double tv, double tl)
{
    double off = local_tza(tv);
    int mins = (int)(off / 60000);
    char sign = mins < 0 ? '-' : '+';
    int am = mins < 0 ? -mins : mins;

    const char* zone = "Coordinated Universal Time";
    if (off != 0) {
        static char zbuf[64];
        time_t sec = (time_t)floor(tv / 1000);
        struct tm tm;
        if (localtime_r (&sec, &tm) && tm.tm_zone) {
            snprintf (zbuf, sizeof(zbuf), "%s", tm.tm_zone);
            zone = zbuf;
        }
    }

    snprintf (buf, buflen, "%02d:%02d:%02d GMT%c%02d%02d (%s)",
              (int)HourFromTime(tl), (int)MinFromTime(tl), (int)SecFromTime(tl),
              sign, am / 60, am % 60, zone);
}

static ejsval
date_to_string (double tv)
{
    if (isnan(tv))
        return _ejs_string_new_utf8 ("Invalid Date");

    double tl = LocalTime(tv);
    char dbuf[64], tbuf[96], buf[176];
    format_date_part (dbuf, sizeof(dbuf), tl);
    format_time_part (tbuf, sizeof(tbuf), tv, tl);
    snprintf (buf, sizeof(buf), "%s %s", dbuf, tbuf);
    return _ejs_string_new_utf8 (buf);
}

// ---------------------------------------------------------------- parsing

static int
read_digits (const char** pp, int n, int* out)
{
    const char* p = *pp;
    int v = 0;
    for (int i = 0; i < n; i++) {
        if (p[i] < '0' || p[i] > '9')
            return 0;
        v = v * 10 + (p[i] - '0');
    }
    *pp = p + n;
    *out = v;
    return 1;
}

// ES2015 20.3.1.16 Date Time String Format (ISO 8601 subset)
static EJSBool
parse_iso_string (const char* s, double* out)
{
    const char* p = s;
    double year;
    int month = 1, day = 1, hh = 0, mm = 0, ss = 0, ms = 0;
    int have_time = 0, have_tz = 0;
    double tzoff = 0;
    int v;

    if (*p == '+' || *p == '-') {
        int neg = (*p == '-');
        p++;
        if (!read_digits (&p, 6, &v)) return EJS_FALSE;
        if (neg && v == 0) return EJS_FALSE; // -000000 is disallowed
        year = neg ? -v : v;
    }
    else {
        if (!read_digits (&p, 4, &v)) return EJS_FALSE;
        year = v;
    }

    if (*p == '-') {
        p++;
        if (!read_digits (&p, 2, &month)) return EJS_FALSE;
        if (month < 1 || month > 12) return EJS_FALSE;
        if (*p == '-') {
            p++;
            if (!read_digits (&p, 2, &day)) return EJS_FALSE;
            if (day < 1 || day > 31) return EJS_FALSE;
        }
    }

    if (*p == 'T' || *p == 't' || *p == ' ') {
        p++;
        have_time = 1;
        if (!read_digits (&p, 2, &hh)) return EJS_FALSE;
        if (*p != ':') return EJS_FALSE;
        p++;
        if (!read_digits (&p, 2, &mm)) return EJS_FALSE;
        if (*p == ':') {
            p++;
            if (!read_digits (&p, 2, &ss)) return EJS_FALSE;
            if (*p == '.') {
                p++;
                if (*p < '0' || *p > '9') return EJS_FALSE;
                int scale = 100;
                while (*p >= '0' && *p <= '9') {
                    ms += (*p - '0') * scale;
                    scale /= 10;
                    p++;
                }
            }
        }
        if (hh > 24 || mm > 59 || ss > 59) return EJS_FALSE;
        if (hh == 24 && (mm != 0 || ss != 0 || ms != 0)) return EJS_FALSE;

        if (*p == 'Z' || *p == 'z') {
            p++;
            have_tz = 1;
        }
        else if (*p == '+' || *p == '-') {
            int neg = (*p == '-');
            int oh, om;
            p++;
            if (!read_digits (&p, 2, &oh)) return EJS_FALSE;
            if (*p == ':') p++;
            if (!read_digits (&p, 2, &om)) return EJS_FALSE;
            if (oh > 23 || om > 59) return EJS_FALSE;
            tzoff = (oh * MS_PER_HOUR + om * MS_PER_MINUTE) * (neg ? -1 : 1);
            have_tz = 1;
        }
    }

    if (*p != 0) return EJS_FALSE;

    double t = MakeDate (MakeDay (year, month - 1, day), MakeTime (hh, mm, ss, ms));
    if (have_tz)
        t -= tzoff;
    else if (have_time)
        t = UTCTime (t); // date-time without offset is local time
    *out = TimeClip (t);
    return EJS_TRUE;
}

static int
month_from_name (const char* name)
{
    for (int i = 0; i < 12; i++)
        if (!strncmp (name, month_names[i], 3))
            return i;
    return -1;
}

// "Fri, 01 Aug 2026 12:34:56 GMT" (Date.prototype.toUTCString)
static EJSBool
parse_utc_string (const char* s, double* out)
{
    char wd[4], mon[4];
    int day, year, hh, mm, ss;
    if (sscanf (s, "%3[A-Za-z], %d %3[A-Za-z] %d %d:%d:%d", wd, &day, mon, &year, &hh, &mm, &ss) != 7)
        return EJS_FALSE;
    int m = month_from_name (mon);
    if (m < 0)
        return EJS_FALSE;
    *out = TimeClip (MakeDate (MakeDay (year, m, day), MakeTime (hh, mm, ss, 0)));
    return EJS_TRUE;
}

// "Fri Aug 01 2026 12:34:56 GMT+0000 (Coordinated Universal Time)"
// (Date.prototype.toString), or its date-only prefix (toDateString)
static EJSBool
parse_date_time_string (const char* s, double* out)
{
    char wd[4], mon[4];
    int day, year, hh, mm, ss;
    int m;

    int consumed = -1;
    if (sscanf (s, "%3s %3s %d %d %d:%d:%d %n", wd, mon, &day, &year, &hh, &mm, &ss, &consumed) == 7 && consumed > 0) {
        m = month_from_name (mon);
        if (m < 0)
            return EJS_FALSE;

        double t = MakeDate (MakeDay (year, m, day), MakeTime (hh, mm, ss, 0));

        const char* p = s + consumed;
        if (!strncmp (p, "GMT", 3)) {
            p += 3;
            if (*p == '+' || *p == '-') {
                int neg = (*p == '-');
                int oh, om;
                p++;
                if (!read_digits (&p, 2, &oh)) return EJS_FALSE;
                if (!read_digits (&p, 2, &om)) return EJS_FALSE;
                t -= (oh * MS_PER_HOUR + om * MS_PER_MINUTE) * (neg ? -1 : 1);
            }
            // anything after the offset (e.g. " (Coordinated Universal Time)") is ignored
        }
        else {
            t = UTCTime (t);
        }
        *out = TimeClip (t);
        return EJS_TRUE;
    }

    // date-only: "Fri Aug 01 2026"
    if (sscanf (s, "%3s %3s %d %d", wd, mon, &day, &year) == 4) {
        m = month_from_name (mon);
        if (m < 0)
            return EJS_FALSE;
        *out = TimeClip (UTCTime (MakeDate (MakeDay (year, m, day), 0)));
        return EJS_TRUE;
    }

    return EJS_FALSE;
}

static double
parse_date_string (ejsval str)
{
    char* utf8 = _ejs_string_to_utf8 (_ejs_string_flatten (str));

    // trim leading/trailing ascii whitespace
    char* start = utf8;
    while (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r')
        start++;
    char* end = start + strlen(start);
    while (end > start && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\n' || end[-1] == '\r'))
        *--end = 0;

    double rv = NAN;
    double parsed;
    if (parse_iso_string (start, &parsed))
        rv = parsed;
    else if (strchr (start, ',') ? parse_utc_string (start, &parsed)
                                 : parse_date_time_string (start, &parsed))
        rv = parsed;

    free (utf8);
    return rv;
}

// ---------------------------------------------------------------- constructor

static EJS_NATIVE_FUNC(_ejs_Date_impl) {
    if (EJSVAL_IS_UNDEFINED(newTarget)) {
        // 20.3.2.3: called as a function: a string representing the current time,
        // ignoring any arguments
        return date_to_string (time_now());
    }

    double tv;

    if (argc == 0) {
        tv = time_now();
    }
    else if (argc == 1) {
        ejsval value = args[0];
        if (EJSVAL_IS_DATE(value)) {
            tv = ((EJSDate*)EJSVAL_TO_OBJECT(value))->date_value;
        }
        else {
            ejsval prim = ToPrimitive (value, TO_PRIM_HINT_DEFAULT);
            if (EJSVAL_IS_STRING(prim))
                tv = parse_date_string (prim);
            else
                tv = TimeClip (ToDouble (prim));
        }
    }
    else {
        double y     = ToDouble (args[0]);
        double m     = ToDouble (args[1]);
        double dt    = argc > 2 ? ToDouble (args[2]) : 1;
        double h     = argc > 3 ? ToDouble (args[3]) : 0;
        double min   = argc > 4 ? ToDouble (args[4]) : 0;
        double s     = argc > 5 ? ToDouble (args[5]) : 0;
        double milli = argc > 6 ? ToDouble (args[6]) : 0;

        if (!isnan(y)) {
            double yi = trunc(y);
            if (yi >= 0 && yi <= 99)
                y = 1900 + yi;
        }
        double finalDate = MakeDate (MakeDay (y, m, dt), MakeTime (h, min, s, milli));
        tv = TimeClip (UTCTime (finalDate));
    }

    ejsval O = OrdinaryCreateFromConstructor (newTarget, _ejs_Date_prototype, &_ejs_Date_specops);
    *_this = O;
    ((EJSDate*)EJSVAL_TO_OBJECT(O))->date_value = TimeClip (tv);
    return *_this;
}

// ---------------------------------------------------------------- statics

static EJS_NATIVE_FUNC(_ejs_Date_now) {
    REJECT_CONSTRUCT();
    return NUMBER_TO_EJSVAL (time_now());
}

static EJS_NATIVE_FUNC(_ejs_Date_parse) {
    REJECT_CONSTRUCT();
    ejsval str = ToString (arg_or_undefined (argc, args, 0));
    return NUMBER_TO_EJSVAL (parse_date_string (str));
}

static EJS_NATIVE_FUNC(_ejs_Date_UTC) {
    REJECT_CONSTRUCT();
    double y     = ToDouble (arg_or_undefined (argc, args, 0));
    double m     = argc > 1 ? ToDouble (args[1]) : 0;
    double dt    = argc > 2 ? ToDouble (args[2]) : 1;
    double h     = argc > 3 ? ToDouble (args[3]) : 0;
    double min   = argc > 4 ? ToDouble (args[4]) : 0;
    double s     = argc > 5 ? ToDouble (args[5]) : 0;
    double milli = argc > 6 ? ToDouble (args[6]) : 0;

    if (!isnan(y)) {
        double yi = trunc(y);
        if (yi >= 0 && yi <= 99)
            y = 1900 + yi;
    }
    return NUMBER_TO_EJSVAL (TimeClip (MakeDate (MakeDay (y, m, dt), MakeTime (h, min, s, milli))));
}

// ---------------------------------------------------------------- getters

#define DATE_GETTER(name, expr)                                     \
    static EJS_NATIVE_FUNC(_ejs_Date_prototype_##name) {            \
        REJECT_CONSTRUCT();                                         \
        double tv = this_time_value (*_this);                       \
        if (isnan(tv)) return NUMBER_TO_EJSVAL(NAN);                \
        return NUMBER_TO_EJSVAL(expr);                              \
    }

DATE_GETTER(getTime,            tv)
DATE_GETTER(valueOf,            tv)
DATE_GETTER(getFullYear,        YearFromTime (LocalTime (tv)))
DATE_GETTER(getUTCFullYear,     YearFromTime (tv))
DATE_GETTER(getMonth,           MonthFromTime (LocalTime (tv)))
DATE_GETTER(getUTCMonth,        MonthFromTime (tv))
DATE_GETTER(getDate,            DateFromTime (LocalTime (tv)))
DATE_GETTER(getUTCDate,         DateFromTime (tv))
DATE_GETTER(getDay,             WeekDay (LocalTime (tv)))
DATE_GETTER(getUTCDay,          WeekDay (tv))
DATE_GETTER(getHours,           HourFromTime (LocalTime (tv)))
DATE_GETTER(getUTCHours,        HourFromTime (tv))
DATE_GETTER(getMinutes,         MinFromTime (LocalTime (tv)))
DATE_GETTER(getUTCMinutes,      MinFromTime (tv))
DATE_GETTER(getSeconds,         SecFromTime (LocalTime (tv)))
DATE_GETTER(getUTCSeconds,      SecFromTime (tv))
DATE_GETTER(getMilliseconds,    msFromTime (LocalTime (tv)))
DATE_GETTER(getUTCMilliseconds, msFromTime (tv))
DATE_GETTER(getTimezoneOffset,  (tv - LocalTime (tv)) / MS_PER_MINUTE)
DATE_GETTER(getYear,            YearFromTime (LocalTime (tv)) - 1900)

// ---------------------------------------------------------------- setters

static ejsval
store_date_value (ejsval date, double u)
{
    ((EJSDate*)EJSVAL_TO_OBJECT(date))->date_value = u;
    return NUMBER_TO_EJSVAL(u);
}

static EJS_NATIVE_FUNC(_ejs_Date_prototype_setTime) {
    REJECT_CONSTRUCT();
    this_time_value (*_this);
    double t = ToDouble (arg_or_undefined (argc, args, 0));
    return store_date_value (*_this, TimeClip (t));
}

// each setter reads [[DateValue]] first, then coerces every supplied
// argument with ToNumber in argument order, and only then bails out on an
// invalid date -- returning NaN WITHOUT overwriting [[DateValue]] (the
// coercion may have set a new time value via side effects)

static EJS_NATIVE_FUNC(_ejs_Date_prototype_setMilliseconds) {
    REJECT_CONSTRUCT();
    double tv = this_time_value (*_this);
    double t = LocalTime (tv);
    double ms = ToDouble (arg_or_undefined (argc, args, 0));
    if (isnan(tv))
        return NUMBER_TO_EJSVAL(NAN);
    double time = MakeTime (HourFromTime(t), MinFromTime(t), SecFromTime(t), ms);
    return store_date_value (*_this, TimeClip (UTCTime (MakeDate (Day(t), time))));
}

static EJS_NATIVE_FUNC(_ejs_Date_prototype_setUTCMilliseconds) {
    REJECT_CONSTRUCT();
    double t = this_time_value (*_this);
    double ms = ToDouble (arg_or_undefined (argc, args, 0));
    if (isnan(t))
        return NUMBER_TO_EJSVAL(NAN);
    double time = MakeTime (HourFromTime(t), MinFromTime(t), SecFromTime(t), ms);
    return store_date_value (*_this, TimeClip (MakeDate (Day(t), time)));
}

static EJS_NATIVE_FUNC(_ejs_Date_prototype_setSeconds) {
    REJECT_CONSTRUCT();
    double tv = this_time_value (*_this);
    double t = LocalTime (tv);
    double s = ToDouble (arg_or_undefined (argc, args, 0));
    double milli = argc > 1 ? ToDouble (args[1]) : msFromTime(t);
    if (isnan(tv))
        return NUMBER_TO_EJSVAL(NAN);
    double date = MakeDate (Day(t), MakeTime (HourFromTime(t), MinFromTime(t), s, milli));
    return store_date_value (*_this, TimeClip (UTCTime (date)));
}

static EJS_NATIVE_FUNC(_ejs_Date_prototype_setUTCSeconds) {
    REJECT_CONSTRUCT();
    double t = this_time_value (*_this);
    double s = ToDouble (arg_or_undefined (argc, args, 0));
    double milli = argc > 1 ? ToDouble (args[1]) : msFromTime(t);
    if (isnan(t))
        return NUMBER_TO_EJSVAL(NAN);
    double date = MakeDate (Day(t), MakeTime (HourFromTime(t), MinFromTime(t), s, milli));
    return store_date_value (*_this, TimeClip (date));
}

static EJS_NATIVE_FUNC(_ejs_Date_prototype_setMinutes) {
    REJECT_CONSTRUCT();
    double tv = this_time_value (*_this);
    double t = LocalTime (tv);
    double m = ToDouble (arg_or_undefined (argc, args, 0));
    double s = argc > 1 ? ToDouble (args[1]) : SecFromTime(t);
    double milli = argc > 2 ? ToDouble (args[2]) : msFromTime(t);
    if (isnan(tv))
        return NUMBER_TO_EJSVAL(NAN);
    double date = MakeDate (Day(t), MakeTime (HourFromTime(t), m, s, milli));
    return store_date_value (*_this, TimeClip (UTCTime (date)));
}

static EJS_NATIVE_FUNC(_ejs_Date_prototype_setUTCMinutes) {
    REJECT_CONSTRUCT();
    double t = this_time_value (*_this);
    double m = ToDouble (arg_or_undefined (argc, args, 0));
    double s = argc > 1 ? ToDouble (args[1]) : SecFromTime(t);
    double milli = argc > 2 ? ToDouble (args[2]) : msFromTime(t);
    if (isnan(t))
        return NUMBER_TO_EJSVAL(NAN);
    double date = MakeDate (Day(t), MakeTime (HourFromTime(t), m, s, milli));
    return store_date_value (*_this, TimeClip (date));
}

static EJS_NATIVE_FUNC(_ejs_Date_prototype_setHours) {
    REJECT_CONSTRUCT();
    double tv = this_time_value (*_this);
    double t = LocalTime (tv);
    double h = ToDouble (arg_or_undefined (argc, args, 0));
    double m = argc > 1 ? ToDouble (args[1]) : MinFromTime(t);
    double s = argc > 2 ? ToDouble (args[2]) : SecFromTime(t);
    double milli = argc > 3 ? ToDouble (args[3]) : msFromTime(t);
    if (isnan(tv))
        return NUMBER_TO_EJSVAL(NAN);
    double date = MakeDate (Day(t), MakeTime (h, m, s, milli));
    return store_date_value (*_this, TimeClip (UTCTime (date)));
}

static EJS_NATIVE_FUNC(_ejs_Date_prototype_setUTCHours) {
    REJECT_CONSTRUCT();
    double t = this_time_value (*_this);
    double h = ToDouble (arg_or_undefined (argc, args, 0));
    double m = argc > 1 ? ToDouble (args[1]) : MinFromTime(t);
    double s = argc > 2 ? ToDouble (args[2]) : SecFromTime(t);
    double milli = argc > 3 ? ToDouble (args[3]) : msFromTime(t);
    if (isnan(t))
        return NUMBER_TO_EJSVAL(NAN);
    double date = MakeDate (Day(t), MakeTime (h, m, s, milli));
    return store_date_value (*_this, TimeClip (date));
}

static EJS_NATIVE_FUNC(_ejs_Date_prototype_setDate) {
    REJECT_CONSTRUCT();
    double tv = this_time_value (*_this);
    double t = LocalTime (tv);
    double dt = ToDouble (arg_or_undefined (argc, args, 0));
    if (isnan(tv))
        return NUMBER_TO_EJSVAL(NAN);
    double newDate = MakeDate (MakeDay (YearFromTime(t), MonthFromTime(t), dt), TimeWithinDay(t));
    return store_date_value (*_this, TimeClip (UTCTime (newDate)));
}

static EJS_NATIVE_FUNC(_ejs_Date_prototype_setUTCDate) {
    REJECT_CONSTRUCT();
    double t = this_time_value (*_this);
    double dt = ToDouble (arg_or_undefined (argc, args, 0));
    if (isnan(t))
        return NUMBER_TO_EJSVAL(NAN);
    double newDate = MakeDate (MakeDay (YearFromTime(t), MonthFromTime(t), dt), TimeWithinDay(t));
    return store_date_value (*_this, TimeClip (newDate));
}

static EJS_NATIVE_FUNC(_ejs_Date_prototype_setMonth) {
    REJECT_CONSTRUCT();
    double tv = this_time_value (*_this);
    double t = LocalTime (tv);
    double m = ToDouble (arg_or_undefined (argc, args, 0));
    double dt = argc > 1 ? ToDouble (args[1]) : DateFromTime(t);
    if (isnan(tv))
        return NUMBER_TO_EJSVAL(NAN);
    double newDate = MakeDate (MakeDay (YearFromTime(t), m, dt), TimeWithinDay(t));
    return store_date_value (*_this, TimeClip (UTCTime (newDate)));
}

static EJS_NATIVE_FUNC(_ejs_Date_prototype_setUTCMonth) {
    REJECT_CONSTRUCT();
    double t = this_time_value (*_this);
    double m = ToDouble (arg_or_undefined (argc, args, 0));
    double dt = argc > 1 ? ToDouble (args[1]) : DateFromTime(t);
    if (isnan(t))
        return NUMBER_TO_EJSVAL(NAN);
    double newDate = MakeDate (MakeDay (YearFromTime(t), m, dt), TimeWithinDay(t));
    return store_date_value (*_this, TimeClip (newDate));
}

// setFullYear treats an invalid date as +0 rather than bailing out
static EJS_NATIVE_FUNC(_ejs_Date_prototype_setFullYear) {
    REJECT_CONSTRUCT();
    double tv = this_time_value (*_this);
    double t = isnan(tv) ? 0 : LocalTime (tv);
    double y = ToDouble (arg_or_undefined (argc, args, 0));
    double m = argc > 1 ? ToDouble (args[1]) : MonthFromTime(t);
    double dt = argc > 2 ? ToDouble (args[2]) : DateFromTime(t);
    double newDate = MakeDate (MakeDay (y, m, dt), TimeWithinDay(t));
    return store_date_value (*_this, TimeClip (UTCTime (newDate)));
}

static EJS_NATIVE_FUNC(_ejs_Date_prototype_setUTCFullYear) {
    REJECT_CONSTRUCT();
    double tv = this_time_value (*_this);
    double t = isnan(tv) ? 0 : tv;
    double y = ToDouble (arg_or_undefined (argc, args, 0));
    double m = argc > 1 ? ToDouble (args[1]) : MonthFromTime(t);
    double dt = argc > 2 ? ToDouble (args[2]) : DateFromTime(t);
    double newDate = MakeDate (MakeDay (y, m, dt), TimeWithinDay(t));
    return store_date_value (*_this, TimeClip (newDate));
}

// B.2.5 Date.prototype.setYear
static EJS_NATIVE_FUNC(_ejs_Date_prototype_setYear) {
    REJECT_CONSTRUCT();
    double tv = this_time_value (*_this);
    double t = isnan(tv) ? 0 : LocalTime (tv);
    double y = ToDouble (arg_or_undefined (argc, args, 0));
    if (isnan(y))
        return store_date_value (*_this, NAN);
    double yi = trunc(y);
    double yyyy = (yi >= 0 && yi <= 99) ? yi + 1900 : y;
    double d = MakeDay (yyyy, MonthFromTime(t), DateFromTime(t));
    return store_date_value (*_this, TimeClip (UTCTime (MakeDate (d, TimeWithinDay(t)))));
}

// ---------------------------------------------------------------- to-string family

static EJS_NATIVE_FUNC(_ejs_Date_prototype_toString) {
    REJECT_CONSTRUCT();
    return date_to_string (this_time_value (*_this));
}

static EJS_NATIVE_FUNC(_ejs_Date_prototype_toDateString) {
    REJECT_CONSTRUCT();
    double tv = this_time_value (*_this);
    if (isnan(tv))
        return _ejs_string_new_utf8 ("Invalid Date");
    char buf[64];
    format_date_part (buf, sizeof(buf), LocalTime(tv));
    return _ejs_string_new_utf8 (buf);
}

static EJS_NATIVE_FUNC(_ejs_Date_prototype_toTimeString) {
    REJECT_CONSTRUCT();
    double tv = this_time_value (*_this);
    if (isnan(tv))
        return _ejs_string_new_utf8 ("Invalid Date");
    char buf[96];
    format_time_part (buf, sizeof(buf), tv, LocalTime(tv));
    return _ejs_string_new_utf8 (buf);
}

static EJS_NATIVE_FUNC(_ejs_Date_prototype_toUTCString) {
    REJECT_CONSTRUCT();
    double tv = this_time_value (*_this);
    if (isnan(tv))
        return _ejs_string_new_utf8 ("Invalid Date");
    char ybuf[16], buf[64];
    format_year (ybuf, sizeof(ybuf), YearFromTime(tv));
    snprintf (buf, sizeof(buf), "%s, %02d %s %s %02d:%02d:%02d GMT",
              weekday_names[(int)WeekDay(tv)],
              (int)DateFromTime(tv),
              month_names[(int)MonthFromTime(tv)],
              ybuf,
              (int)HourFromTime(tv), (int)MinFromTime(tv), (int)SecFromTime(tv));
    return _ejs_string_new_utf8 (buf);
}

static EJS_NATIVE_FUNC(_ejs_Date_prototype_toISOString) {
    REJECT_CONSTRUCT();
    double tv = this_time_value (*_this);
    if (isnan(tv))
        _ejs_throw_nativeerror_utf8 (EJS_RANGE_ERROR, "Invalid time value");

    int y = (int)YearFromTime(tv);
    char ybuf[16];
    if (y >= 0 && y <= 9999)
        snprintf (ybuf, sizeof(ybuf), "%04d", y);
    else
        snprintf (ybuf, sizeof(ybuf), "%+07d", y);

    char buf[48];
    snprintf (buf, sizeof(buf), "%s-%02d-%02dT%02d:%02d:%02d.%03dZ",
              ybuf,
              (int)MonthFromTime(tv) + 1,
              (int)DateFromTime(tv),
              (int)HourFromTime(tv), (int)MinFromTime(tv), (int)SecFromTime(tv),
              (int)msFromTime(tv));
    return _ejs_string_new_utf8 (buf);
}

// 20.3.4.37 Date.prototype.toJSON
static EJS_NATIVE_FUNC(_ejs_Date_prototype_toJSON) {
    REJECT_CONSTRUCT();
    ejsval O = ToObject (*_this);
    ejsval tv = ToPrimitive (O, TO_PRIM_HINT_NUMBER);
    if (EJSVAL_IS_NUMBER(tv) && !isfinite(EJSVAL_TO_NUMBER(tv)))
        return _ejs_null;
    ejsval func = Get (O, _ejs_atom_toISOString);
    if (!IsCallable(func))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "toISOString is not callable");
    return _ejs_invoke_closure (func, &O, 0, NULL, _ejs_undefined);
}

// 20.3.4.45 Date.prototype[@@toPrimitive]
static EJS_NATIVE_FUNC(_ejs_Date_prototype_toPrimitive) {
    REJECT_CONSTRUCT();
    if (!EJSVAL_IS_OBJECT(*_this))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Date[Symbol.toPrimitive] called on a non-object");

    ejsval hint = arg_or_undefined (argc, args, 0);
    ToPrimitiveHint h;
    if (EJSVAL_TO_BOOLEAN(_ejs_op_strict_eq (hint, _ejs_atom_number)))
        h = TO_PRIM_HINT_NUMBER;
    else if (EJSVAL_TO_BOOLEAN(_ejs_op_strict_eq (hint, _ejs_atom_string)) ||
             EJSVAL_TO_BOOLEAN(_ejs_op_strict_eq (hint, _ejs_atom_default)))
        h = TO_PRIM_HINT_STRING;
    else
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "invalid hint");

    // OrdinaryToPrimitive: calling ToPrimitive here would re-enter @@toPrimitive
    ejsval methodNames[2];
    if (h == TO_PRIM_HINT_STRING) {
        methodNames[0] = _ejs_atom_toString;
        methodNames[1] = _ejs_atom_valueOf;
    }
    else {
        methodNames[0] = _ejs_atom_valueOf;
        methodNames[1] = _ejs_atom_toString;
    }
    for (int i = 0; i < 2; i++) {
        ejsval method = Get (*_this, methodNames[i]);
        if (IsCallable(method)) {
            ejsval result = _ejs_invoke_closure (method, _this, 0, NULL, _ejs_undefined);
            if (!EJSVAL_IS_OBJECT(result))
                return result;
        }
    }
    _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "couldn't convert Date to primitive");
}

// ---------------------------------------------------------------- init

void
_ejs_date_init(ejsval global)
{
    _ejs_Date = _ejs_function_new_without_proto (_ejs_null, _ejs_atom_Date, _ejs_Date_impl);
    _ejs_object_setprop (global, _ejs_atom_Date, _ejs_Date);

    _ejs_gc_add_root (&_ejs_Date_prototype);
    // Date.prototype is an ordinary object, not a Date: it has no
    // [[DateValue]], so the methods below throw when applied to it
    _ejs_Date_prototype = _ejs_object_new(_ejs_Object_prototype, &_ejs_Object_specops);

    _ejs_object_define_value_property (_ejs_Date, _ejs_atom_prototype, _ejs_Date_prototype, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_CONFIGURABLE | EJS_PROP_NOT_WRITABLE);
    _ejs_object_define_value_property (_ejs_Date_prototype, _ejs_atom_constructor, _ejs_Date, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_WRITABLE | EJS_PROP_CONFIGURABLE);

#define METHOD_FLAGS (EJS_PROP_NOT_ENUMERABLE | EJS_PROP_WRITABLE | EJS_PROP_CONFIGURABLE)
#define PROTO_METHOD(x) EJS_INSTALL_ATOM_FUNCTION_FLAGS (_ejs_Date_prototype, x, _ejs_Date_prototype_##x, METHOD_FLAGS)
#define PROTO_METHOD_IMPL(x, impl) EJS_INSTALL_ATOM_FUNCTION_FLAGS (_ejs_Date_prototype, x, impl, METHOD_FLAGS)
#define OBJ_METHOD(x) EJS_INSTALL_ATOM_FUNCTION_FLAGS (_ejs_Date, x, _ejs_Date_##x, METHOD_FLAGS)

    PROTO_METHOD(getTime);
    PROTO_METHOD(valueOf);
    PROTO_METHOD(getTimezoneOffset);

    PROTO_METHOD(getFullYear);
    PROTO_METHOD(getMonth);
    PROTO_METHOD(getDate);
    PROTO_METHOD(getDay);
    PROTO_METHOD(getHours);
    PROTO_METHOD(getMinutes);
    PROTO_METHOD(getSeconds);
    PROTO_METHOD(getMilliseconds);
    PROTO_METHOD(getUTCFullYear);
    PROTO_METHOD(getUTCMonth);
    PROTO_METHOD(getUTCDate);
    PROTO_METHOD(getUTCDay);
    PROTO_METHOD(getUTCHours);
    PROTO_METHOD(getUTCMinutes);
    PROTO_METHOD(getUTCSeconds);
    PROTO_METHOD(getUTCMilliseconds);

    PROTO_METHOD(setTime);
    PROTO_METHOD(setMilliseconds);
    PROTO_METHOD(setSeconds);
    PROTO_METHOD(setMinutes);
    PROTO_METHOD(setHours);
    PROTO_METHOD(setDate);
    PROTO_METHOD(setMonth);
    PROTO_METHOD(setFullYear);
    PROTO_METHOD(setUTCMilliseconds);
    PROTO_METHOD(setUTCSeconds);
    PROTO_METHOD(setUTCMinutes);
    PROTO_METHOD(setUTCHours);
    PROTO_METHOD(setUTCDate);
    PROTO_METHOD(setUTCMonth);
    PROTO_METHOD(setUTCFullYear);

    PROTO_METHOD(toString);
    PROTO_METHOD(toDateString);
    PROTO_METHOD(toTimeString);
    PROTO_METHOD(toISOString);
    PROTO_METHOD(toJSON);

    // the toLocale* variants alias the non-locale implementations
    PROTO_METHOD_IMPL(toLocaleString, _ejs_Date_prototype_toString);
    PROTO_METHOD_IMPL(toLocaleDateString, _ejs_Date_prototype_toDateString);
    PROTO_METHOD_IMPL(toLocaleTimeString, _ejs_Date_prototype_toTimeString);

    // B.2.4.3: toGMTString is the same function object as toUTCString
    ejsval toUTCString_func = _ejs_function_new_native (_ejs_null, _ejs_atom_toUTCString, _ejs_Date_prototype_toUTCString);
    _ejs_object_define_value_property (_ejs_Date_prototype, _ejs_atom_toUTCString, toUTCString_func, METHOD_FLAGS);
    _ejs_object_define_value_property (_ejs_Date_prototype, _ejs_atom_toGMTString, toUTCString_func, METHOD_FLAGS);

    // annex B
    PROTO_METHOD(getYear);
    PROTO_METHOD(setYear);

    // 20.3.4.45: @@toPrimitive is non-writable, non-enumerable, configurable
    ejsval toPrimitive_func = _ejs_function_new_native (_ejs_null, _ejs_string_new_utf8("[Symbol.toPrimitive]"), _ejs_Date_prototype_toPrimitive);
    _ejs_object_define_value_property (_ejs_Date_prototype, _ejs_Symbol_toPrimitive, toPrimitive_func, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_WRITABLE | EJS_PROP_CONFIGURABLE);

    OBJ_METHOD(now);
    OBJ_METHOD(parse);
    OBJ_METHOD(UTC);

#undef METHOD_FLAGS
#undef PROTO_METHOD
#undef PROTO_METHOD_IMPL
#undef OBJ_METHOD
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
