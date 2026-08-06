/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <time.h>
#include <unistd.h>

#include "ejs-temporal.h"
#include "ejs-bigint.h"
#include "ejs-error.h"
#include "ejs-function.h"
#include "ejs-gc.h"
#include "ejs-number.h"
#include "ejs-object.h"
#include "ejs-ops.h"
#include "ejs-string.h"
#include "ejs-symbol.h"
#include "ejs-types.h"

ejsval _ejs_Temporal EJSVAL_ALIGNMENT;
ejsval _ejs_Temporal_Now EJSVAL_ALIGNMENT;

ejsval _ejs_TemporalInstant EJSVAL_ALIGNMENT;
ejsval _ejs_TemporalInstant_prototype EJSVAL_ALIGNMENT;
ejsval _ejs_TemporalPlainDate EJSVAL_ALIGNMENT;
ejsval _ejs_TemporalPlainDate_prototype EJSVAL_ALIGNMENT;
ejsval _ejs_TemporalPlainTime EJSVAL_ALIGNMENT;
ejsval _ejs_TemporalPlainTime_prototype EJSVAL_ALIGNMENT;
ejsval _ejs_TemporalPlainDateTime EJSVAL_ALIGNMENT;
ejsval _ejs_TemporalPlainDateTime_prototype EJSVAL_ALIGNMENT;
ejsval _ejs_TemporalPlainYearMonth EJSVAL_ALIGNMENT;
ejsval _ejs_TemporalPlainYearMonth_prototype EJSVAL_ALIGNMENT;
ejsval _ejs_TemporalPlainMonthDay EJSVAL_ALIGNMENT;
ejsval _ejs_TemporalPlainMonthDay_prototype EJSVAL_ALIGNMENT;
ejsval _ejs_TemporalZonedDateTime EJSVAL_ALIGNMENT;
ejsval _ejs_TemporalZonedDateTime_prototype EJSVAL_ALIGNMENT;
ejsval _ejs_TemporalDuration EJSVAL_ALIGNMENT;
ejsval _ejs_TemporalDuration_prototype EJSVAL_ALIGNMENT;

static ejsval _ejs_temporal_str_iso8601 EJSVAL_ALIGNMENT; // "iso8601", interned once
static ejsval _ejs_temporal_str_UTC EJSVAL_ALIGNMENT;     // "UTC", interned once

#define NS_PER_DAY (((ejs_i128)86400) * 1000000000LL)

// ---------------------------------------------------------------- helpers

static void
throw_not_implemented(const char* what)
{
    char buf[256];
    snprintf(buf, sizeof(buf), "Temporal: %s is not implemented yet", what);
    _ejs_throw_nativeerror_utf8(EJS_ERROR, buf);
}

// install a built-in method: correct .name, .length, and property
// attributes (writable, non-enumerable, configurable)
static ejsval
install_method(ejsval obj, const char* name, EJSClosureFunc fn, int len)
{
    ejsval namestr = _ejs_string_new_utf8(name);
    ejsval f = _ejs_function_new_native(_ejs_null, namestr, fn);
    _ejs_object_define_value_property(f, _ejs_atom_length, NUMBER_TO_EJSVAL(len),
                                      EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_WRITABLE | EJS_PROP_CONFIGURABLE);
    _ejs_object_define_value_property(obj, namestr, f,
                                      EJS_PROP_NOT_ENUMERABLE | EJS_PROP_WRITABLE | EJS_PROP_CONFIGURABLE);
    return f;
}

// install a built-in accessor: getter function named "get <name>",
// .length 0, accessor property non-enumerable + configurable
static void
install_getter(ejsval obj, const char* name, EJSClosureFunc fn)
{
    char getter_name[64];
    snprintf(getter_name, sizeof(getter_name), "get %s", name);
    ejsval f = _ejs_function_new_native(_ejs_null, _ejs_string_new_utf8(getter_name), fn);
    _ejs_object_define_value_property(f, _ejs_atom_length, NUMBER_TO_EJSVAL(0),
                                      EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_WRITABLE | EJS_PROP_CONFIGURABLE);
    _ejs_object_define_accessor_property(obj, _ejs_string_new_utf8(name), f, _ejs_undefined,
                                         EJS_PROP_FLAGS_GETTER_SET | EJS_PROP_NOT_ENUMERABLE | EJS_PROP_CONFIGURABLE);
}

static void
install_tostringtag(ejsval obj, const char* tag)
{
    _ejs_object_define_value_property(obj, _ejs_Symbol_toStringTag, _ejs_string_new_utf8(tag),
                                      EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_WRITABLE | EJS_PROP_CONFIGURABLE);
}

// ToIntegerWithTruncation: ToNumber, RangeError on NaN/±∞, truncate
static double
to_integer_with_truncation(ejsval v)
{
    double d = ToDouble(v);
    if (isnan(d) || !isfinite(d))
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "value must be finite");
    return trunc(d);
}

// ToIntegerIfIntegral: ToNumber, RangeError unless an integral number
static double
to_integer_if_integral(ejsval v)
{
    double d = ToDouble(v);
    if (!isfinite(d) || trunc(d) != d)
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "value must be an integer");
    return d == 0 ? 0 : d; // normalize -0
}

// utf8 copy of a JS string value; caller frees
static char*
string_to_utf8(ejsval str)
{
    return ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(str));
}

// ASCII-clean utf8 copy: NULL when the string contains an embedded NUL
// or any non-ASCII unit — every grammar these feed (ISO strings, option
// values, unit names, ids) is ASCII-only, and C-string comparison would
// otherwise silently truncate at a NUL
static char*
string_to_ascii(ejsval str)
{
    EJSPrimString* ps = _ejs_string_flatten(str);
    char* s = ucs2_to_utf8(ps->data.flat);
    if (strlen(s) != (size_t)ps->length) {
        free(s);
        return NULL;
    }
    for (const char* c = s; *c; c++) {
        if ((unsigned char)*c > 0x7f) {
            free(s);
            return NULL;
        }
    }
    return s;
}

static EJSBool
string_equals_utf8(ejsval str, const char* expected)
{
    char* s = string_to_ascii(str);
    if (!s) return EJS_FALSE;
    EJSBool eq = !strcmp(s, expected);
    free(s);
    return eq;
}

// ASCII-lowercase in place
static void
ascii_lowercase(char* s)
{
    for (; *s; s++)
        if (*s >= 'A' && *s <= 'Z') *s += 'a' - 'A';
}

// ParseTemporalCalendarString fallback: any ISO date/time-shaped string
// is a calendar reference through its [u-ca] annotation (defined after
// the parser)
static EJSBool calendar_string_resolves_to_iso(const char* s);

// CanonicalizeCalendar: only "iso8601" (and its alias "gregory"-free
// world: an ECMA-262-only build supports just iso8601), matched
// case-insensitively.  undefined => "iso8601"; non-string => TypeError;
// unknown => RangeError.
static ejsval
to_temporal_calendar_identifier(ejsval calendar_like)
{
    if (EJSVAL_IS_UNDEFINED(calendar_like))
        return _ejs_temporal_str_iso8601;
    if (EJSVAL_IS_TEMPORAL_PLAINDATE(calendar_like))
        return EJSVAL_TO_TEMPORAL_PLAINDATE(calendar_like)->calendar;
    if (EJSVAL_IS_TEMPORAL_PLAINDATETIME(calendar_like))
        return EJSVAL_TO_TEMPORAL_PLAINDATETIME(calendar_like)->calendar;
    if (EJSVAL_IS_TEMPORAL_PLAINYEARMONTH(calendar_like))
        return EJSVAL_TO_TEMPORAL_PLAINYEARMONTH(calendar_like)->calendar;
    if (EJSVAL_IS_TEMPORAL_PLAINMONTHDAY(calendar_like))
        return EJSVAL_TO_TEMPORAL_PLAINMONTHDAY(calendar_like)->calendar;
    if (EJSVAL_IS_TEMPORAL_ZONEDDATETIME(calendar_like))
        return EJSVAL_TO_TEMPORAL_ZONEDDATETIME(calendar_like)->calendar;
    if (!EJSVAL_IS_STRING(calendar_like))
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "calendar must be a string");
    char* s = string_to_ascii(calendar_like);
    EJSBool ok = EJS_FALSE;
    if (s) {
        char* lower = strdup(s);
        ascii_lowercase(lower);
        ok = !strcmp(lower, "iso8601");
        free(lower);
        if (!ok)
            ok = calendar_string_resolves_to_iso(s);
    }
    free(s);
    if (!ok)
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "unsupported calendar");
    return _ejs_temporal_str_iso8601;
}

// GetOptionsObject: undefined => treated as empty; otherwise must be an
// object.  Returns the value (callers Get from it or ignore).
static ejsval
get_options_object(ejsval options)
{
    if (EJSVAL_IS_UNDEFINED(options))
        return _ejs_undefined;
    if (!EJSVAL_IS_OBJECT(options))
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "options must be an object or undefined");
    return options;
}

// GetOption for string-valued options with a fixed value set.
// Returns the index into `values` (or `default_index` when absent).
static int
get_string_option(ejsval options, const char* name, const char* const* values, int nvalues, int default_index)
{
    if (EJSVAL_IS_UNDEFINED(options))
        return default_index;
    ejsval v = Get(options, _ejs_string_new_utf8(name));
    if (EJSVAL_IS_UNDEFINED(v))
        return default_index;
    ejsval str = ToString(v);
    char* s = string_to_ascii(str);
    if (s) {
        for (int i = 0; i < nvalues; i++) {
            if (!strcmp(s, values[i])) {
                free(s);
                return i;
            }
        }
    }
    char buf[128];
    snprintf(buf, sizeof(buf), "invalid value for option %s", name);
    free(s);
    _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, buf);
    EJS_NOT_REACHED();
    return -1;
}

// ------------------------------------------------------------- ISO math

EJSBool
_ejs_temporal_iso_is_leap_year(int32_t y)
{
    return (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0)) ? EJS_TRUE : EJS_FALSE;
}

int32_t
_ejs_temporal_iso_days_in_month(int32_t year, int32_t month)
{
    static const int32_t dim[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (month == 2 && _ejs_temporal_iso_is_leap_year(year)) return 29;
    return dim[month - 1];
}

EJSBool
_ejs_temporal_is_valid_iso_date(int32_t year, int32_t month, int32_t day)
{
    if (month < 1 || month > 12) return EJS_FALSE;
    if (day < 1 || day > _ejs_temporal_iso_days_in_month(year, month)) return EJS_FALSE;
    return EJS_TRUE;
}

EJSBool
_ejs_temporal_is_valid_time(int32_t h, int32_t mi, int32_t s, int32_t ms, int32_t us, int32_t ns)
{
    if (h < 0 || h > 23) return EJS_FALSE;
    if (mi < 0 || mi > 59) return EJS_FALSE;
    if (s < 0 || s > 59) return EJS_FALSE;
    if (ms < 0 || ms > 999) return EJS_FALSE;
    if (us < 0 || us > 999) return EJS_FALSE;
    if (ns < 0 || ns > 999) return EJS_FALSE;
    return EJS_TRUE;
}

// Howard Hinnant's days_from_civil: proleptic Gregorian date -> days
// since the 1970-01-01 epoch
int64_t
_ejs_temporal_iso_date_to_epoch_days(int32_t year, int32_t month, int32_t day)
{
    int64_t y = year;
    if (month <= 2) y -= 1;
    int64_t era = (y >= 0 ? y : y - 399) / 400;
    int64_t yoe = y - era * 400;                                        // [0, 399]
    int64_t doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1; // [0, 365]
    int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;                // [0, 146096]
    return era * 146097 + doe - 719468;
}

// civil_from_days: the inverse
void
_ejs_temporal_epoch_days_to_iso_date(int64_t z, int32_t* year, int32_t* month, int32_t* day)
{
    z += 719468;
    int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    int64_t doe = z - era * 146097;                                     // [0, 146096]
    int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365; // [0, 399]
    int64_t y = yoe + era * 400;
    int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);              // [0, 365]
    int64_t mp = (5 * doy + 2) / 153;                                   // [0, 11]
    int64_t d = doy - (153 * mp + 2) / 5 + 1;                           // [1, 31]
    int64_t m = mp + (mp < 10 ? 3 : -9);                                // [1, 12]
    if (m <= 2) y += 1;
    *year = (int32_t)y;
    *month = (int32_t)m;
    *day = (int32_t)d;
}

int32_t
_ejs_temporal_iso_day_of_week(int32_t year, int32_t month, int32_t day)
{
    int64_t days = _ejs_temporal_iso_date_to_epoch_days(year, month, day);
    // epoch day 0 = Thursday; ISO: 1 = Monday .. 7 = Sunday
    return (int32_t)(((days + 3) % 7 + 7) % 7 + 1);
}

int32_t
_ejs_temporal_iso_day_of_year(int32_t year, int32_t month, int32_t day)
{
    int32_t doy = day;
    for (int32_t m = 1; m < month; m++)
        doy += _ejs_temporal_iso_days_in_month(year, m);
    return doy;
}

static int32_t
iso_weeks_in_year(int32_t year)
{
    // 53-week years: Jan 1 is a Thursday, or a leap year where Jan 1 is
    // a Wednesday
    int32_t jan1 = _ejs_temporal_iso_day_of_week(year, 1, 1);
    if (jan1 == 4) return 53;
    if (jan1 == 3 && _ejs_temporal_iso_is_leap_year(year)) return 53;
    return 52;
}

void
_ejs_temporal_iso_week_of_year(int32_t year, int32_t month, int32_t day, int32_t* week, int32_t* week_year)
{
    int32_t doy = _ejs_temporal_iso_day_of_year(year, month, day);
    int32_t dow = _ejs_temporal_iso_day_of_week(year, month, day);
    int32_t w = (doy - dow + 10) / 7;
    if (w < 1) {
        *week = iso_weeks_in_year(year - 1);
        *week_year = year - 1;
    } else if (w > iso_weeks_in_year(year)) {
        *week = 1;
        *week_year = year + 1;
    } else {
        *week = w;
        *week_year = year;
    }
}

static ejs_i128
iso_datetime_to_epoch_ns(int32_t year, int32_t month, int32_t day,
                         int32_t h, int32_t mi, int32_t s, int32_t ms, int32_t us, int32_t ns)
{
    int64_t days = _ejs_temporal_iso_date_to_epoch_days(year, month, day);
    ejs_i128 time_ns = ((ejs_i128)h) * 3600000000000LL
        + ((ejs_i128)mi) * 60000000000LL
        + ((ejs_i128)s) * 1000000000LL
        + ((ejs_i128)ms) * 1000000LL
        + ((ejs_i128)us) * 1000LL
        + (ejs_i128)ns;
    return ((ejs_i128)days) * NS_PER_DAY + time_ns;
}

EJSBool
_ejs_temporal_iso_datetime_within_limits(int32_t year, int32_t month, int32_t day,
                                         int32_t h, int32_t mi, int32_t s,
                                         int32_t ms, int32_t us, int32_t ns)
{
    ejs_i128 v = iso_datetime_to_epoch_ns(year, month, day, h, mi, s, ms, us, ns);
    return (v > EJS_TEMPORAL_NS_MIN_INSTANT - NS_PER_DAY && v < EJS_TEMPORAL_NS_MAX_INSTANT + NS_PER_DAY)
        ? EJS_TRUE : EJS_FALSE;
}

// ISODateWithinLimits: the date at noon must be within the datetime limits
static EJSBool
iso_date_within_limits(int32_t year, int32_t month, int32_t day)
{
    return _ejs_temporal_iso_datetime_within_limits(year, month, day, 12, 0, 0, 0, 0, 0);
}

// --------------------------------------------------------- BigInt boundary

ejsval
_ejs_temporal_i128_to_bigint(ejs_i128 v)
{
    EJSBool neg = v < 0;
    unsigned __int128 mag = neg ? (unsigned __int128)(-v) : (unsigned __int128)v;
    char buf[48];
    int i = sizeof(buf);
    if (mag == 0) buf[--i] = '0';
    while (mag > 0) {
        buf[--i] = '0' + (int)(mag % 10);
        mag /= 10;
    }
    ejsval bi = _ejs_bigint_from_digits_utf8(buf + i, (int)(sizeof(buf) - i), 10);
    return neg ? _ejs_bigint_neg(bi) : bi;
}

// throws RangeError unless bi is within the valid epoch-ns range
ejs_i128
_ejs_temporal_bigint_to_epoch_ns(ejsval bi)
{
    EJSBigInt* b = (EJSBigInt*)EJSVAL_TO_BIGINT(bi);
    if (b->length > 2)
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "epoch nanoseconds out of range");
    unsigned __int128 mag = 0;
    if (b->length >= 1) mag = b->digits[0];
    if (b->length == 2) mag |= ((unsigned __int128)b->digits[1]) << 64;
    ejs_i128 v = b->sign ? -(ejs_i128)mag : (ejs_i128)mag;
    if (v < EJS_TEMPORAL_NS_MIN_INSTANT || v > EJS_TEMPORAL_NS_MAX_INSTANT)
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "epoch nanoseconds out of range");
    return v;
}

// local ToBigInt (7.1.13; the one in ejs-bigint.cpp is static there)
static ejsval
temporal_to_bigint(ejsval value)
{
    ejsval prim = ToPrimitive(value, TO_PRIM_HINT_NUMBER);
    if (EJSVAL_IS_BIGINT(prim)) return prim;
    if (EJSVAL_IS_BOOLEAN(prim)) return _ejs_bigint_new_from_int64(EJSVAL_TO_BOOLEAN(prim) ? 1 : 0);
    if (EJSVAL_IS_STRING(prim)) {
        ejsval rv = _ejs_bigint_from_string(prim);
        if (!EJSVAL_IS_BIGINT(rv))
            _ejs_throw_nativeerror_utf8(EJS_SYNTAX_ERROR, "Cannot convert string to a BigInt");
        return rv;
    }
    _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "Cannot convert value to a BigInt");
    EJS_NOT_REACHED();
    return _ejs_undefined;
}

static ejs_i128
i128_floordiv(ejs_i128 a, ejs_i128 b)
{
    ejs_i128 q = a / b;
    if ((a % b != 0) && ((a < 0) != (b < 0))) q -= 1;
    return q;
}

static ejs_i128
i128_floormod(ejs_i128 a, ejs_i128 b)
{
    ejs_i128 r = a % b;
    if (r != 0 && ((r < 0) != (b < 0))) r += b;
    return r;
}

// ----------------------------------------------------- instance creation

ejsval
_ejs_temporal_instant_new(ejs_i128 epoch_ns)
{
    EJSTemporalInstant* rv = _ejs_gc_new(EJSTemporalInstant);
    _ejs_init_object((EJSObject*)rv, _ejs_TemporalInstant_prototype, &_ejs_TemporalInstant_specops);
    rv->epoch_ns = epoch_ns;
    return OBJECT_TO_EJSVAL(rv);
}

ejsval
_ejs_temporal_plain_date_new(int32_t year, int32_t month, int32_t day, ejsval calendar)
{
    EJSTemporalPlainDate* rv = _ejs_gc_new(EJSTemporalPlainDate);
    _ejs_init_object((EJSObject*)rv, _ejs_TemporalPlainDate_prototype, &_ejs_TemporalPlainDate_specops);
    rv->year = year; rv->month = month; rv->day = day;
    rv->calendar = calendar;
    return OBJECT_TO_EJSVAL(rv);
}

ejsval
_ejs_temporal_plain_time_new(int32_t h, int32_t mi, int32_t s, int32_t ms, int32_t us, int32_t ns)
{
    EJSTemporalPlainTime* rv = _ejs_gc_new(EJSTemporalPlainTime);
    _ejs_init_object((EJSObject*)rv, _ejs_TemporalPlainTime_prototype, &_ejs_TemporalPlainTime_specops);
    rv->hour = h; rv->minute = mi; rv->second = s;
    rv->millisecond = ms; rv->microsecond = us; rv->nanosecond = ns;
    return OBJECT_TO_EJSVAL(rv);
}

ejsval
_ejs_temporal_plain_datetime_new(int32_t year, int32_t month, int32_t day,
                                 int32_t h, int32_t mi, int32_t s,
                                 int32_t ms, int32_t us, int32_t ns, ejsval calendar)
{
    EJSTemporalPlainDateTime* rv = _ejs_gc_new(EJSTemporalPlainDateTime);
    _ejs_init_object((EJSObject*)rv, _ejs_TemporalPlainDateTime_prototype, &_ejs_TemporalPlainDateTime_specops);
    rv->year = year; rv->month = month; rv->day = day;
    rv->hour = h; rv->minute = mi; rv->second = s;
    rv->millisecond = ms; rv->microsecond = us; rv->nanosecond = ns;
    rv->calendar = calendar;
    return OBJECT_TO_EJSVAL(rv);
}

ejsval
_ejs_temporal_plain_yearmonth_new(int32_t year, int32_t month, int32_t ref_day, ejsval calendar)
{
    EJSTemporalPlainYearMonth* rv = _ejs_gc_new(EJSTemporalPlainYearMonth);
    _ejs_init_object((EJSObject*)rv, _ejs_TemporalPlainYearMonth_prototype, &_ejs_TemporalPlainYearMonth_specops);
    rv->year = year; rv->month = month; rv->ref_day = ref_day;
    rv->calendar = calendar;
    return OBJECT_TO_EJSVAL(rv);
}

ejsval
_ejs_temporal_plain_monthday_new(int32_t month, int32_t day, int32_t ref_year, ejsval calendar)
{
    EJSTemporalPlainMonthDay* rv = _ejs_gc_new(EJSTemporalPlainMonthDay);
    _ejs_init_object((EJSObject*)rv, _ejs_TemporalPlainMonthDay_prototype, &_ejs_TemporalPlainMonthDay_specops);
    rv->month = month; rv->day = day; rv->ref_year = ref_year;
    rv->calendar = calendar;
    return OBJECT_TO_EJSVAL(rv);
}

ejsval
_ejs_temporal_zoneddatetime_new(ejs_i128 epoch_ns, ejsval time_zone, ejsval calendar)
{
    EJSTemporalZonedDateTime* rv = _ejs_gc_new(EJSTemporalZonedDateTime);
    _ejs_init_object((EJSObject*)rv, _ejs_TemporalZonedDateTime_prototype, &_ejs_TemporalZonedDateTime_specops);
    rv->epoch_ns = epoch_ns;
    rv->time_zone = time_zone;
    rv->calendar = calendar;
    return OBJECT_TO_EJSVAL(rv);
}

// IsValidDuration
static EJSBool
is_valid_duration(double years, double months, double weeks, double days,
                  double hours, double minutes, double seconds,
                  double ms, double us, double ns)
{
    double fields[10] = { years, months, weeks, days, hours, minutes, seconds, ms, us, ns };
    int sign = 0;
    for (int i = 0; i < 10; i++) {
        double v = fields[i];
        if (!isfinite(v)) return EJS_FALSE;
        if (v < 0) {
            if (sign > 0) return EJS_FALSE;
            sign = -1;
        } else if (v > 0) {
            if (sign < 0) return EJS_FALSE;
            sign = 1;
        }
    }
    if (fabs(years) >= 4294967296.0 /* 2^32 */) return EJS_FALSE;
    if (fabs(months) >= 4294967296.0) return EJS_FALSE;
    if (fabs(weeks) >= 4294967296.0) return EJS_FALSE;
    // normalized total must be under 2^53 seconds.  A lone field can
    // exceed int64 while still being valid (ns up to ~9e24), so cast
    // through i128 — but only after bounding out magnitudes that could
    // overflow even i128 arithmetic.
    if (fabs(days) >= 1e30 || fabs(hours) >= 1e30 || fabs(minutes) >= 1e30
        || fabs(seconds) >= 1e30 || fabs(ms) >= 1e30 || fabs(us) >= 1e30 || fabs(ns) >= 1e30)
        return EJS_FALSE;
    ejs_i128 total_ns = ((ejs_i128)fabs(days)) * 86400000000000LL
        + ((ejs_i128)fabs(hours)) * 3600000000000LL
        + ((ejs_i128)fabs(minutes)) * 60000000000LL
        + ((ejs_i128)fabs(seconds)) * 1000000000LL
        + ((ejs_i128)fabs(ms)) * 1000000LL
        + ((ejs_i128)fabs(us)) * 1000LL
        + (ejs_i128)fabs(ns);
    if (total_ns >= ((ejs_i128)9007199254740992LL) * 1000000000LL /* 2^53 s */) return EJS_FALSE;
    return EJS_TRUE;
}

ejsval
_ejs_temporal_duration_new(double years, double months, double weeks, double days,
                           double hours, double minutes, double seconds,
                           double ms, double us, double ns)
{
    EJSTemporalDuration* rv = _ejs_gc_new(EJSTemporalDuration);
    _ejs_init_object((EJSObject*)rv, _ejs_TemporalDuration_prototype, &_ejs_TemporalDuration_specops);
    // fields are mathematical values: normalize -0 (negation and
    // sign-multiplication of zero fields must not observably produce it)
#define NZ(v) ((v) == 0 ? 0 : (v))
    rv->years = NZ(years); rv->months = NZ(months); rv->weeks = NZ(weeks); rv->days = NZ(days);
    rv->hours = NZ(hours); rv->minutes = NZ(minutes); rv->seconds = NZ(seconds);
    rv->milliseconds = NZ(ms); rv->microseconds = NZ(us); rv->nanoseconds = NZ(ns);
#undef NZ
    return OBJECT_TO_EJSVAL(rv);
}

static int
duration_sign(EJSTemporalDuration* d)
{
    double fields[10] = { d->years, d->months, d->weeks, d->days, d->hours,
                          d->minutes, d->seconds, d->milliseconds, d->microseconds, d->nanoseconds };
    for (int i = 0; i < 10; i++) {
        if (fields[i] < 0) return -1;
        if (fields[i] > 0) return 1;
    }
    return 0;
}

// ------------------------------------------------------------- formatting

// ISO year: 4 digits for 0..9999, otherwise signed 6 digits
static void
format_iso_year(char* buf, size_t bufsize, int32_t year)
{
    if (year >= 0 && year <= 9999)
        snprintf(buf, bufsize, "%04d", year);
    else
        snprintf(buf, bufsize, "%c%06d", year < 0 ? '-' : '+', abs(year));
}

static void
format_iso_date(char* buf, size_t bufsize, int32_t year, int32_t month, int32_t day)
{
    char ybuf[16];
    format_iso_year(ybuf, sizeof(ybuf), year);
    snprintf(buf, bufsize, "%s-%02d-%02d", ybuf, month, day);
}

// time-of-day with "auto" sub-second precision (trim trailing zeros)
static void
format_iso_time_auto(char* buf, size_t bufsize, int32_t h, int32_t mi, int32_t s,
                     int32_t ms, int32_t us, int32_t ns)
{
    int64_t frac = (int64_t)ms * 1000000 + (int64_t)us * 1000 + ns;
    if (frac == 0) {
        snprintf(buf, bufsize, "%02d:%02d:%02d", h, mi, s);
        return;
    }
    char fbuf[10];
    snprintf(fbuf, sizeof(fbuf), "%09lld", (long long)frac);
    int len = 9;
    while (len > 1 && fbuf[len - 1] == '0') len--;
    fbuf[len] = 0;
    snprintf(buf, bufsize, "%02d:%02d:%02d.%s", h, mi, s, fbuf);
}

// epoch ns -> ISO datetime fields at UTC (+offset_ns applied)
static void
epoch_ns_to_iso_fields(ejs_i128 epoch_ns, int64_t offset_ns,
                       int32_t* year, int32_t* month, int32_t* day,
                       int32_t* h, int32_t* mi, int32_t* s, int32_t* ms, int32_t* us, int32_t* ns)
{
    ejs_i128 local = epoch_ns + offset_ns;
    ejs_i128 days = i128_floordiv(local, NS_PER_DAY);
    ejs_i128 time_ns = i128_floormod(local, NS_PER_DAY);
    _ejs_temporal_epoch_days_to_iso_date((int64_t)days, year, month, day);
    *h = (int32_t)(time_ns / 3600000000000LL); time_ns %= 3600000000000LL;
    *mi = (int32_t)(time_ns / 60000000000LL); time_ns %= 60000000000LL;
    *s = (int32_t)(time_ns / 1000000000LL); time_ns %= 1000000000LL;
    *ms = (int32_t)(time_ns / 1000000LL); time_ns %= 1000000LL;
    *us = (int32_t)(time_ns / 1000LL);
    *ns = (int32_t)(time_ns % 1000LL);
}

// ---------------------------------------------------------------- specops

static EJSObject* _ejs_temporal_instant_specop_allocate(void) { return (EJSObject*)_ejs_gc_new(EJSTemporalInstant); }
static EJSObject* _ejs_temporal_plaindate_specop_allocate(void) { return (EJSObject*)_ejs_gc_new(EJSTemporalPlainDate); }
static EJSObject* _ejs_temporal_plaintime_specop_allocate(void) { return (EJSObject*)_ejs_gc_new(EJSTemporalPlainTime); }
static EJSObject* _ejs_temporal_plaindatetime_specop_allocate(void) { return (EJSObject*)_ejs_gc_new(EJSTemporalPlainDateTime); }
static EJSObject* _ejs_temporal_plainyearmonth_specop_allocate(void) { return (EJSObject*)_ejs_gc_new(EJSTemporalPlainYearMonth); }
static EJSObject* _ejs_temporal_plainmonthday_specop_allocate(void) { return (EJSObject*)_ejs_gc_new(EJSTemporalPlainMonthDay); }
static EJSObject* _ejs_temporal_zoneddatetime_specop_allocate(void) { return (EJSObject*)_ejs_gc_new(EJSTemporalZonedDateTime); }
static EJSObject* _ejs_temporal_duration_specop_allocate(void) { return (EJSObject*)_ejs_gc_new(EJSTemporalDuration); }

static void
_ejs_temporal_plaindate_specop_scan(EJSObject* obj, EJSValueFunc scan_func)
{
    scan_func(&((EJSTemporalPlainDate*)obj)->calendar);
    _ejs_Object_specops.Scan(obj, scan_func);
}

static void
_ejs_temporal_plaindatetime_specop_scan(EJSObject* obj, EJSValueFunc scan_func)
{
    scan_func(&((EJSTemporalPlainDateTime*)obj)->calendar);
    _ejs_Object_specops.Scan(obj, scan_func);
}

static void
_ejs_temporal_plainyearmonth_specop_scan(EJSObject* obj, EJSValueFunc scan_func)
{
    scan_func(&((EJSTemporalPlainYearMonth*)obj)->calendar);
    _ejs_Object_specops.Scan(obj, scan_func);
}

static void
_ejs_temporal_plainmonthday_specop_scan(EJSObject* obj, EJSValueFunc scan_func)
{
    scan_func(&((EJSTemporalPlainMonthDay*)obj)->calendar);
    _ejs_Object_specops.Scan(obj, scan_func);
}

static void
_ejs_temporal_zoneddatetime_specop_scan(EJSObject* obj, EJSValueFunc scan_func)
{
    scan_func(&((EJSTemporalZonedDateTime*)obj)->calendar);
    scan_func(&((EJSTemporalZonedDateTime*)obj)->time_zone);
    _ejs_Object_specops.Scan(obj, scan_func);
}

EJS_DEFINE_CLASS(TemporalInstant,
                 OP_INHERIT, OP_INHERIT, OP_INHERIT, OP_INHERIT, OP_INHERIT, OP_INHERIT,
                 OP_INHERIT, OP_INHERIT, OP_INHERIT, OP_INHERIT, OP_INHERIT, OP_INHERIT,
                 OP_INHERIT, OP_INHERIT,
                 _ejs_temporal_instant_specop_allocate,
                 OP_INHERIT,
                 OP_INHERIT)

EJS_DEFINE_CLASS(TemporalPlainDate,
                 OP_INHERIT, OP_INHERIT, OP_INHERIT, OP_INHERIT, OP_INHERIT, OP_INHERIT,
                 OP_INHERIT, OP_INHERIT, OP_INHERIT, OP_INHERIT, OP_INHERIT, OP_INHERIT,
                 OP_INHERIT, OP_INHERIT,
                 _ejs_temporal_plaindate_specop_allocate,
                 OP_INHERIT,
                 _ejs_temporal_plaindate_specop_scan)

EJS_DEFINE_CLASS(TemporalPlainTime,
                 OP_INHERIT, OP_INHERIT, OP_INHERIT, OP_INHERIT, OP_INHERIT, OP_INHERIT,
                 OP_INHERIT, OP_INHERIT, OP_INHERIT, OP_INHERIT, OP_INHERIT, OP_INHERIT,
                 OP_INHERIT, OP_INHERIT,
                 _ejs_temporal_plaintime_specop_allocate,
                 OP_INHERIT,
                 OP_INHERIT)

EJS_DEFINE_CLASS(TemporalPlainDateTime,
                 OP_INHERIT, OP_INHERIT, OP_INHERIT, OP_INHERIT, OP_INHERIT, OP_INHERIT,
                 OP_INHERIT, OP_INHERIT, OP_INHERIT, OP_INHERIT, OP_INHERIT, OP_INHERIT,
                 OP_INHERIT, OP_INHERIT,
                 _ejs_temporal_plaindatetime_specop_allocate,
                 OP_INHERIT,
                 _ejs_temporal_plaindatetime_specop_scan)

EJS_DEFINE_CLASS(TemporalPlainYearMonth,
                 OP_INHERIT, OP_INHERIT, OP_INHERIT, OP_INHERIT, OP_INHERIT, OP_INHERIT,
                 OP_INHERIT, OP_INHERIT, OP_INHERIT, OP_INHERIT, OP_INHERIT, OP_INHERIT,
                 OP_INHERIT, OP_INHERIT,
                 _ejs_temporal_plainyearmonth_specop_allocate,
                 OP_INHERIT,
                 _ejs_temporal_plainyearmonth_specop_scan)

EJS_DEFINE_CLASS(TemporalPlainMonthDay,
                 OP_INHERIT, OP_INHERIT, OP_INHERIT, OP_INHERIT, OP_INHERIT, OP_INHERIT,
                 OP_INHERIT, OP_INHERIT, OP_INHERIT, OP_INHERIT, OP_INHERIT, OP_INHERIT,
                 OP_INHERIT, OP_INHERIT,
                 _ejs_temporal_plainmonthday_specop_allocate,
                 OP_INHERIT,
                 _ejs_temporal_plainmonthday_specop_scan)

EJS_DEFINE_CLASS(TemporalZonedDateTime,
                 OP_INHERIT, OP_INHERIT, OP_INHERIT, OP_INHERIT, OP_INHERIT, OP_INHERIT,
                 OP_INHERIT, OP_INHERIT, OP_INHERIT, OP_INHERIT, OP_INHERIT, OP_INHERIT,
                 OP_INHERIT, OP_INHERIT,
                 _ejs_temporal_zoneddatetime_specop_allocate,
                 OP_INHERIT,
                 _ejs_temporal_zoneddatetime_specop_scan)

EJS_DEFINE_CLASS(TemporalDuration,
                 OP_INHERIT, OP_INHERIT, OP_INHERIT, OP_INHERIT, OP_INHERIT, OP_INHERIT,
                 OP_INHERIT, OP_INHERIT, OP_INHERIT, OP_INHERIT, OP_INHERIT, OP_INHERIT,
                 OP_INHERIT, OP_INHERIT,
                 _ejs_temporal_duration_specop_allocate,
                 OP_INHERIT,
                 OP_INHERIT)

// ------------------------------------------------------ ISO string parser

static EJSBool parse_offset_tz_id(const char* s, int64_t* offset_ns, char* canonical, size_t canonical_size);

// parsed RFC 9557 / ISO 8601 string
typedef struct {
    EJSBool has_date;
    int32_t year, month, day;
    EJSBool has_time;
    int32_t hour, minute, second, millisecond, microsecond, nanosecond;
    EJSBool has_offset;      // numeric UTC offset present
    EJSBool offset_z;        // the offset was Z/z
    EJSBool offset_subminute; // the offset spelled seconds (or finer)
    int64_t offset_ns;
    EJSBool has_tz;          // [tz] annotation
    char tz[128];
    EJSBool has_calendar;    // [u-ca=...] annotation
    EJSBool calendar_critical;
    char calendar[64];
} ParsedISO;

static int
parse_fixed_digits(const char** pp, int n, int32_t* out)
{
    const char* p = *pp;
    int32_t v = 0;
    for (int i = 0; i < n; i++) {
        if (p[i] < '0' || p[i] > '9') return 0;
        v = v * 10 + (p[i] - '0');
    }
    *pp = p + n;
    *out = v;
    return 1;
}

// [.,]digits{1,9} -> ns
static int
parse_fraction_ns(const char** pp, int32_t* out_ns)
{
    const char* p = *pp;
    if (*p != '.' && *p != ',') return 0;
    p++;
    int32_t v = 0;
    int n = 0;
    while (*p >= '0' && *p <= '9' && n < 9) {
        v = v * 10 + (*p - '0');
        p++; n++;
    }
    if (n == 0) return 0;
    if (*p >= '0' && *p <= '9') return 0; // >9 digits
    while (n < 9) { v *= 10; n++; }
    *pp = p;
    *out_ns = v;
    return 1;
}

// Time: HH[[:]MM[[:]SS[.frac]]] — separators all-or-nothing
static int
parse_time_spec(const char** pp, ParsedISO* r)
{
    const char* p = *pp;
    int32_t hh, mm = 0, ss = 0, frac = 0;
    if (!parse_fixed_digits(&p, 2, &hh)) return 0;
    if (hh > 23) return 0;
    int ext = (*p == ':');
    if (ext) p++;
    if (*p >= '0' && *p <= '9') {
        if (!parse_fixed_digits(&p, 2, &mm)) return 0;
        if (mm > 59) return 0;
        int has_sep = (*p == ':');
        if (has_sep != ext) {
            if (has_sep) return 0; // mixed separators
        }
        if (has_sep) p++;
        if ((has_sep || !ext) && *p >= '0' && *p <= '9') {
            if (!parse_fixed_digits(&p, 2, &ss)) return 0;
            if (ss > 60) return 0;
            if (ss == 60) ss = 59; // leap second clamps
            parse_fraction_ns(&p, &frac);
        } else if (has_sep) {
            return 0; // trailing separator
        }
    } else if (ext) {
        return 0;
    }
    r->has_time = EJS_TRUE;
    r->hour = hh; r->minute = mm; r->second = ss;
    r->millisecond = frac / 1000000;
    r->microsecond = (frac / 1000) % 1000;
    r->nanosecond = frac % 1000;
    *pp = p;
    return 1;
}

// UTC offset: Z | ±HH[[:]MM[[:]SS[.frac]]]
static int
parse_utc_offset(const char** pp, ParsedISO* r, EJSBool allow_subminute)
{
    const char* p = *pp;
    if (*p == 'Z' || *p == 'z') {
        r->has_offset = EJS_TRUE;
        r->offset_z = EJS_TRUE;
        r->offset_ns = 0;
        *pp = p + 1;
        return 1;
    }
    if (*p != '+' && *p != '-') return 0;
    int sign = (*p == '-') ? -1 : 1;
    p++;
    int32_t hh, mm = 0, ss = 0, frac = 0;
    if (!parse_fixed_digits(&p, 2, &hh) || hh > 23) return 0;
    int ext = (*p == ':');
    if (ext) p++;
    if (*p >= '0' && *p <= '9') {
        if (!parse_fixed_digits(&p, 2, &mm) || mm > 59) return 0;
        if (allow_subminute) {
            int has_sep = (*p == ':');
            if (has_sep == ext && *((has_sep) ? p + 1 : p) >= '0') {
                const char* q = has_sep ? p + 1 : p;
                if (*q >= '0' && *q <= '9') {
                    int32_t s2;
                    const char* q2 = q;
                    if (parse_fixed_digits(&q2, 2, &s2) && s2 <= 59) {
                        ss = s2;
                        p = q2;
                        parse_fraction_ns(&p, &frac);
                        r->offset_subminute = EJS_TRUE; // spelled, even if zero
                    }
                }
            }
        }
    } else if (ext) {
        return 0;
    }
    r->has_offset = EJS_TRUE;
    r->offset_z = EJS_FALSE;
    if (ss != 0 || frac != 0) r->offset_subminute = EJS_TRUE;
    r->offset_ns = (int64_t)sign * (((int64_t)hh * 3600 + (int64_t)mm * 60 + ss) * 1000000000LL + frac);
    *pp = p;
    return 1;
}

static int
is_tzid_char(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')
        || c == '.' || c == '_' || c == '-' || c == '/' || c == '+';
}

// bracketed annotations: [tz], [!tz], [u-ca=x], [!key=value]
static int
parse_annotations(const char** pp, ParsedISO* r)
{
    const char* p = *pp;
    EJSBool seen_calendar = EJS_FALSE;
    EJSBool first = EJS_TRUE;
    while (*p == '[') {
        p++;
        EJSBool critical = EJS_FALSE;
        if (*p == '!') { critical = EJS_TRUE; p++; }
        const char* start = p;
        // annotation key: [a-z_][a-z0-9_-]* followed by '='; otherwise a
        // time zone annotation (only valid as the first bracket)
        const char* q = p;
        while (*q && *q != ']' && *q != '=') q++;
        if (*q == '=') {
            size_t klen = (size_t)(q - start);
            // key grammar: [a-z_][a-z0-9_-]*
            if (klen == 0) return 0;
            if (!((start[0] >= 'a' && start[0] <= 'z') || start[0] == '_')) return 0;
            for (size_t ki = 1; ki < klen; ki++) {
                char kc = start[ki];
                if (!((kc >= 'a' && kc <= 'z') || (kc >= '0' && kc <= '9') || kc == '_' || kc == '-'))
                    return 0;
            }
            const char* vstart = q + 1;
            const char* v = vstart;
            while (*v && *v != ']') v++;
            if (*v != ']') return 0;
            size_t vlen = (size_t)(v - vstart);
            if (vlen == 0) return 0;
            // value grammar: 1*8alphanum groups joined by '-'
            {
                size_t run = 0;
                for (size_t vi = 0; vi < vlen; vi++) {
                    char vc = vstart[vi];
                    if (vc == '-') {
                        if (run == 0) return 0;
                        run = 0;
                    } else if ((vc >= 'a' && vc <= 'z') || (vc >= 'A' && vc <= 'Z') || (vc >= '0' && vc <= '9')) {
                        run++;
                    } else {
                        return 0;
                    }
                }
                if (run == 0) return 0;
            }
            if (klen == 4 && !strncmp(start, "u-ca", 4)) {
                if (!seen_calendar) {
                    seen_calendar = EJS_TRUE;
                    r->calendar_critical = critical;
                    r->has_calendar = EJS_TRUE;
                    if (vlen >= sizeof(r->calendar)) return 0;
                    memcpy(r->calendar, vstart, vlen);
                    r->calendar[vlen] = 0;
                } else {
                    // any critical flag among duplicates is a syntax error
                    if (critical || r->calendar_critical) return 0;
                }
            } else if (critical) {
                return 0; // unknown critical annotation
            }
            p = v + 1;
        } else if (*q == ']') {
            // time zone annotation: must be the first bracket, only once
            if (!first || r->has_tz) return 0;
            size_t len = (size_t)(q - start);
            if (len == 0 || len >= sizeof(r->tz)) return 0;
            for (const char* c = start; c < q; c++)
                if (!is_tzid_char(*c) && *c != ':') return 0;
            memcpy(r->tz, start, len);
            r->tz[len] = 0;
            if (r->tz[0] == '+' || r->tz[0] == '-') {
                // offset-shaped: the annotation grammar is minute precision
                int64_t aoff;
                if (!parse_offset_tz_id(r->tz, &aoff, NULL, 0)) return 0;
            }
            r->has_tz = EJS_TRUE;
            p = q + 1;
        } else {
            return 0;
        }
        first = EJS_FALSE;
    }
    *pp = p;
    return 1;
}

// Date: ±YYYYYY-MM-DD | ±YYYYYYMMDD | YYYY-MM-DD | YYYYMMDD
static int
parse_date_spec(const char** pp, ParsedISO* r)
{
    const char* p = *pp;
    int32_t year;
    if (*p == '+' || *p == '-') {
        int sign = (*p == '-') ? -1 : 1;
        p++;
        int32_t y6;
        if (!parse_fixed_digits(&p, 6, &y6)) return 0;
        if (sign < 0 && y6 == 0) return 0; // -000000 is invalid
        year = sign * y6;
    } else {
        if (!parse_fixed_digits(&p, 4, &year)) return 0;
    }
    int ext = (*p == '-');
    if (ext) p++;
    int32_t month, day;
    if (!parse_fixed_digits(&p, 2, &month)) return 0;
    if (ext) {
        if (*p != '-') return 0;
        p++;
    }
    if (!parse_fixed_digits(&p, 2, &day)) return 0;
    if (month < 1 || month > 12) return 0;
    if (day < 1 || day > _ejs_temporal_iso_days_in_month(year, month)) return 0;
    r->has_date = EJS_TRUE;
    r->year = year; r->month = month; r->day = day;
    *pp = p;
    return 1;
}

// AnnotatedDateTime: Date [ (T|t|space) Time [offset] ] annotations*
static int
parse_annotated_datetime(const char* s, ParsedISO* r)
{
    memset(r, 0, sizeof(*r));
    const char* p = s;
    if (!parse_date_spec(&p, r)) return 0;
    if (*p == 'T' || *p == 't' || *p == ' ') {
        p++;
        if (!parse_time_spec(&p, r)) return 0;
        if (*p == 'Z' || *p == 'z' || *p == '+' || *p == '-') {
            if (!parse_utc_offset(&p, r, EJS_TRUE)) return 0;
        }
    }
    if (!parse_annotations(&p, r)) return 0;
    return *p == 0;
}

// AnnotatedTime: [T|t] Time [offset] annotations*
static int
parse_annotated_time(const char* s, ParsedISO* r)
{
    memset(r, 0, sizeof(*r));
    const char* p = s;
    if (*p == 'T' || *p == 't') p++;
    if (!parse_time_spec(&p, r)) return 0;
    if (*p == 'Z' || *p == 'z' || *p == '+' || *p == '-') {
        if (!parse_utc_offset(&p, r, EJS_TRUE)) return 0;
    }
    if (!parse_annotations(&p, r)) return 0;
    return *p == 0;
}

// YYYY-MM | YYYYMM | ±YYYYYY-MM (year-month, no day)
static int
parse_yearmonth_only(const char* s, ParsedISO* r)
{
    memset(r, 0, sizeof(*r));
    const char* p = s;
    int32_t year;
    if (*p == '+' || *p == '-') {
        int sign = (*p == '-') ? -1 : 1;
        p++;
        int32_t y6;
        if (!parse_fixed_digits(&p, 6, &y6)) return 0;
        if (sign < 0 && y6 == 0) return 0;
        year = sign * y6;
    } else {
        if (!parse_fixed_digits(&p, 4, &year)) return 0;
    }
    if (*p == '-') p++;
    int32_t month;
    if (!parse_fixed_digits(&p, 2, &month)) return 0;
    if (month < 1 || month > 12) return 0;
    if (!parse_annotations(&p, r)) return 0;
    if (*p != 0) return 0;
    r->has_date = EJS_TRUE;
    r->year = year; r->month = month; r->day = 1;
    return 1;
}

// MM-DD | --MM-DD | MMDD | --MMDD (month-day)
static int
parse_monthday_only(const char* s, ParsedISO* r)
{
    memset(r, 0, sizeof(*r));
    const char* p = s;
    if (p[0] == '-' && p[1] == '-') p += 2;
    int32_t month, day;
    if (!parse_fixed_digits(&p, 2, &month)) return 0;
    if (*p == '-') p++;
    if (!parse_fixed_digits(&p, 2, &day)) return 0;
    if (month < 1 || month > 12) return 0;
    if (day < 1 || day > _ejs_temporal_iso_days_in_month(1972, month)) return 0;
    if (!parse_annotations(&p, r)) return 0;
    if (*p != 0) return 0;
    r->has_date = EJS_TRUE;
    r->year = 1972; r->month = month; r->day = day;
    return 1;
}

// validate a parsed [u-ca=...] annotation; returns the calendar value
static ejsval
parsed_calendar(ParsedISO* r)
{
    if (!r->has_calendar)
        return _ejs_temporal_str_iso8601;
    char lower[64];
    snprintf(lower, sizeof(lower), "%s", r->calendar);
    ascii_lowercase(lower);
    if (strcmp(lower, "iso8601"))
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "unsupported calendar");
    return _ejs_temporal_str_iso8601;
}

static EJSBool
calendar_string_resolves_to_iso(const char* s)
{
    ParsedISO r;
    if (parse_annotated_datetime(s, &r) || parse_annotated_time(s, &r)
        || parse_yearmonth_only(s, &r) || parse_monthday_only(s, &r)) {
        if (!r.has_calendar)
            return EJS_TRUE;
        char lower[64];
        snprintf(lower, sizeof(lower), "%s", r.calendar);
        ascii_lowercase(lower);
        return !strcmp(lower, "iso8601") ? EJS_TRUE : EJS_FALSE;
    }
    return EJS_FALSE;
}

static void
format_offset_ns(char* buf, size_t bufsize, int64_t offset_ns)
{
    int64_t abs_ns = offset_ns < 0 ? -offset_ns : offset_ns;
    int64_t total_min = abs_ns / 60000000000LL;
    int hh = (int)(total_min / 60);
    int mm = (int)(total_min % 60);
    snprintf(buf, bufsize, "%c%02d:%02d", offset_ns < 0 ? '-' : '+', hh, mm);
}


// ---------------------------------------------------- TZif named zones

// IANA zone data parsed from /usr/share/zoneinfo TZif v2+ files: the
// 64-bit transition block plus the POSIX-TZ footer that governs times
// after the last explicit transition (RFC 8536).
typedef struct {
    int mon, week, day;   // M<mon>.<week>.<day>; week 5 = last
    int64_t time;         // seconds, local; POSIX extension allows <0 and >24h
} TZRule;

typedef struct {
    int64_t* transitions; // UTC seconds, ascending
    int32_t* offsets;     // seconds east of UTC in effect after each transition
    int count;
    int32_t first_offset; // before the first transition
    EJSBool has_rules;    // footer DST rules present
    int32_t std_off, dst_off;
    TZRule dst_start, dst_end;
    EJSBool footer_std_only; // footer with a fixed offset only
    int32_t footer_std;
} TZData;

typedef struct TZCacheEntry {
    char* lower;          // lookup key (ascii-lowercased id)
    char* canonical;      // IANA-cased id
    TZData* data;
    struct TZCacheEntry* next;
} TZCacheEntry;

static TZCacheEntry* tz_cache;

static uint32_t rd_be32(const unsigned char* p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }
static int64_t rd_be64(const unsigned char* p) { return ((int64_t)(int8_t)p[0] << 56) | ((int64_t)p[1] << 48) | ((int64_t)p[2] << 40) | ((int64_t)p[3] << 32) | ((int64_t)p[4] << 24) | ((int64_t)p[5] << 16) | ((int64_t)p[6] << 8) | p[7]; }

// POSIX TZ offset: [+-]hh[:mm[:ss]] -> seconds (POSIX sign: west
// positive, so utoff = -value)
static int
parse_posix_offset(const char** pp, int64_t* out)
{
    const char* p = *pp;
    int sign = 1;
    if (*p == '+') p++;
    else if (*p == '-') { sign = -1; p++; }
    if (*p < '0' || *p > '9') return 0;
    int64_t h = 0, m = 0, s = 0;
    while (*p >= '0' && *p <= '9') h = h * 10 + (*p++ - '0');
    if (*p == ':') {
        p++;
        while (*p >= '0' && *p <= '9') m = m * 10 + (*p++ - '0');
        if (*p == ':') {
            p++;
            while (*p >= '0' && *p <= '9') s = s * 10 + (*p++ - '0');
        }
    }
    *out = sign * (h * 3600 + m * 60 + s);
    *pp = p;
    return 1;
}

static int
parse_posix_rule(const char** pp, TZRule* rule)
{
    const char* p = *pp;
    if (*p != 'M') return 0; // Jn / n forms don't occur in tzdata footers
    p++;
    int mon = 0, week = 0, day = 0;
    while (*p >= '0' && *p <= '9') mon = mon * 10 + (*p++ - '0');
    if (*p++ != '.') return 0;
    while (*p >= '0' && *p <= '9') week = week * 10 + (*p++ - '0');
    if (*p++ != '.') return 0;
    while (*p >= '0' && *p <= '9') day = day * 10 + (*p++ - '0');
    rule->mon = mon; rule->week = week; rule->day = day;
    rule->time = 2 * 3600;
    if (*p == '/') {
        p++;
        int sign = 1;
        if (*p == '-') { sign = -1; p++; }
        int64_t h = 0, m = 0, s = 0;
        while (*p >= '0' && *p <= '9') h = h * 10 + (*p++ - '0');
        if (*p == ':') {
            p++;
            while (*p >= '0' && *p <= '9') m = m * 10 + (*p++ - '0');
            if (*p == ':') {
                p++;
                while (*p >= '0' && *p <= '9') s = s * 10 + (*p++ - '0');
            }
        }
        rule->time = sign * (h * 3600 + m * 60 + s);
    }
    *pp = p;
    return 1;
}

// skip a POSIX TZ name: alpha run or <...>
static int
skip_posix_name(const char** pp)
{
    const char* p = *pp;
    if (*p == '<') {
        while (*p && *p != '>') p++;
        if (*p != '>') return 0;
        p++;
    } else {
        const char* start = p;
        while ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z')) p++;
        if (p - start < 3) return 0;
    }
    *pp = p;
    return 1;
}

static void
parse_tz_footer(const char* s, TZData* tz)
{
    const char* p = s;
    int64_t std;
    if (!skip_posix_name(&p)) return;
    if (!parse_posix_offset(&p, &std)) return;
    tz->footer_std = (int32_t)-std;
    tz->footer_std_only = EJS_TRUE;
    if (!*p || *p == ',') {
        // no DST name: fixed offset footer
        return;
    }
    if (!skip_posix_name(&p)) return;
    int64_t dst = std - 3600;
    if (*p && *p != ',') {
        if (!parse_posix_offset(&p, &dst)) return;
    }
    if (*p != ',') return;
    p++;
    TZRule start, end;
    if (!parse_posix_rule(&p, &start)) return;
    if (*p != ',') return;
    p++;
    if (!parse_posix_rule(&p, &end)) return;
    tz->std_off = (int32_t)-std;
    tz->dst_off = (int32_t)-dst;
    tz->dst_start = start;
    tz->dst_end = end;
    tz->has_rules = EJS_TRUE;
    tz->footer_std_only = EJS_FALSE;
}

static TZData*
parse_tzif(const unsigned char* buf, size_t len)
{
    if (len < 44 || memcmp(buf, "TZif", 4) || buf[4] < '2')
        return NULL;
    // v1 block sizes
    uint32_t isutcnt = rd_be32(buf + 20), isstdcnt = rd_be32(buf + 24), leapcnt = rd_be32(buf + 28);
    uint32_t timecnt = rd_be32(buf + 32), typecnt = rd_be32(buf + 36), charcnt = rd_be32(buf + 40);
    size_t v1 = 44 + (size_t)timecnt * 5 + (size_t)typecnt * 6 + charcnt + (size_t)leapcnt * 8 + isstdcnt + isutcnt;
    if (len < v1 + 44) return NULL;
    const unsigned char* h2 = buf + v1;
    if (memcmp(h2, "TZif", 4)) return NULL;
    isutcnt = rd_be32(h2 + 20); isstdcnt = rd_be32(h2 + 24); leapcnt = rd_be32(h2 + 28);
    timecnt = rd_be32(h2 + 32); typecnt = rd_be32(h2 + 36); charcnt = rd_be32(h2 + 40);
    const unsigned char* p = h2 + 44;
    size_t need = (size_t)timecnt * 9 + (size_t)typecnt * 6 + charcnt + (size_t)leapcnt * 12 + isstdcnt + isutcnt;
    if ((size_t)(p - buf) + need > len) return NULL;
    TZData* tz = (TZData*)calloc(1, sizeof(TZData));
    tz->count = (int)timecnt;
    tz->transitions = (int64_t*)malloc(sizeof(int64_t) * (timecnt ? timecnt : 1));
    tz->offsets = (int32_t*)malloc(sizeof(int32_t) * (timecnt ? timecnt : 1));
    const unsigned char* trans = p;
    const unsigned char* idx = trans + (size_t)timecnt * 8;
    const unsigned char* types = idx + timecnt;
    for (uint32_t i = 0; i < timecnt; i++) {
        tz->transitions[i] = rd_be64(trans + (size_t)i * 8);
        uint8_t ti = idx[i];
        if (ti >= typecnt) ti = 0;
        tz->offsets[i] = (int32_t)rd_be32(types + (size_t)ti * 6);
    }
    // first offset: first non-dst type, else type 0
    tz->first_offset = typecnt ? (int32_t)rd_be32(types) : 0;
    for (uint32_t t = 0; t < typecnt; t++) {
        if (types[(size_t)t * 6 + 4] == 0) {
            tz->first_offset = (int32_t)rd_be32(types + (size_t)t * 6);
            break;
        }
    }
    // footer
    const unsigned char* rest = types + (size_t)typecnt * 6 + charcnt + (size_t)leapcnt * 12 + isstdcnt + isutcnt;
    if (rest < buf + len && *rest == '\n') {
        const unsigned char* nl = memchr(rest + 1, '\n', (size_t)(buf + len - rest - 1));
        if (nl && nl > rest + 1) {
            char footer[256];
            size_t flen = (size_t)(nl - rest - 1);
            if (flen < sizeof(footer)) {
                memcpy(footer, rest + 1, flen);
                footer[flen] = 0;
                parse_tz_footer(footer, tz);
            }
        }
    }
    return tz;
}

// M<mon>.<week>.<day> rule for a year -> UTC seconds, given the offset
// in effect before the transition
static int64_t
rule_epoch_seconds(int year, const TZRule* rule, int32_t offset_before)
{
    // last (or nth) <day>-weekday of/in month
    int32_t dim = _ejs_temporal_iso_days_in_month(year, rule->mon);
    // weekday of the 1st (0=Sunday per POSIX)
    int64_t first_days = _ejs_temporal_iso_date_to_epoch_days(year, rule->mon, 1);
    int wd_first = (int)(((first_days + 4) % 7 + 7) % 7); // epoch day 0 = Thursday
    int day1 = 1 + ((rule->day - wd_first) % 7 + 7) % 7;  // first <day> weekday
    int dom = day1 + (rule->week - 1) * 7;
    while (dom > dim) dom -= 7;
    int64_t days = _ejs_temporal_iso_date_to_epoch_days(year, rule->mon, dom);
    return days * 86400 + rule->time - offset_before;
}

static int32_t
tz_footer_offset_at(const TZData* tz, int64_t t)
{
    if (!tz->has_rules)
        return tz->footer_std_only ? tz->footer_std : (tz->count ? tz->offsets[tz->count - 1] : tz->first_offset);
    // year of t under std offset (close enough for rule years)
    int32_t y, m, d;
    _ejs_temporal_epoch_days_to_iso_date((t + tz->std_off) / 86400 - (((t + tz->std_off) % 86400) < 0 ? 1 : 0), &y, &m, &d);
    for (int dy = -1; dy <= 1; dy++) {
        // evaluate the DST window of each candidate year; windows never
        // overlap year boundaries in both directions simultaneously
        (void)dy;
    }
    int64_t start = rule_epoch_seconds(y, &tz->dst_start, tz->std_off);
    int64_t end = rule_epoch_seconds(y, &tz->dst_end, tz->dst_off);
    EJSBool dst;
    if (start <= end)
        dst = (t >= start && t < end);
    else
        dst = (t >= start || t < end);
    // boundary years: t may fall in the previous year's window
    if (!dst && start > end) {
        // southern hemisphere handled by the wrap above
    }
    return dst ? tz->dst_off : tz->std_off;
}

static int32_t
tz_data_offset_at(const TZData* tz, int64_t t)
{
    if (tz->count == 0 || t < tz->transitions[0]) {
        if (tz->count == 0 && (tz->has_rules || tz->footer_std_only))
            return tz_footer_offset_at(tz, t);
        return tz->first_offset;
    }
    if (t >= tz->transitions[tz->count - 1]) {
        if (tz->has_rules || tz->footer_std_only)
            return tz_footer_offset_at(tz, t);
        return tz->offsets[tz->count - 1];
    }
    int lo = 0, hi = tz->count - 1;
    while (lo + 1 < hi) {
        int mid = (lo + hi) / 2;
        if (tz->transitions[mid] <= t) lo = mid;
        else hi = mid;
    }
    return tz->offsets[lo];
}

// case-insensitive canonical lookup under /usr/share/zoneinfo.
// Returns the cache entry or NULL.
static TZCacheEntry*
tz_lookup(const char* name)
{
    char lower[128];
    if (strlen(name) >= sizeof(lower)) return NULL;
    snprintf(lower, sizeof(lower), "%s", name);
    ascii_lowercase(lower);
    for (TZCacheEntry* e = tz_cache; e; e = e->next)
        if (!strcmp(e->lower, lower))
            return e->data ? e : NULL;
    // canonicalize component-by-component against the directory tree
    char canonical[192] = "";
    char path[256];
    snprintf(path, sizeof(path), "/usr/share/zoneinfo");
    char work[128];
    snprintf(work, sizeof(work), "%s", name);
    char* save = NULL;
    EJSBool ok = EJS_TRUE;
    for (char* comp = strtok_r(work, "/", &save); comp; comp = strtok_r(NULL, "/", &save)) {
        if (!strcmp(comp, ".") || !strcmp(comp, "..")) { ok = EJS_FALSE; break; }
        DIR* dir = opendir(path);
        if (!dir) { ok = EJS_FALSE; break; }
        struct dirent* ent;
        EJSBool found = EJS_FALSE;
        while ((ent = readdir(dir))) {
            if (!strcasecmp(ent->d_name, comp)) {
                strlcat(path, "/", sizeof(path));
                strlcat(path, ent->d_name, sizeof(path));
                if (canonical[0]) strlcat(canonical, "/", sizeof(canonical));
                strlcat(canonical, ent->d_name, sizeof(canonical));
                found = EJS_TRUE;
                break;
            }
        }
        closedir(dir);
        if (!found) { ok = EJS_FALSE; break; }
    }
    TZData* data = NULL;
    if (ok && canonical[0]) {
        FILE* f = fopen(path, "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            long len = ftell(f);
            fseek(f, 0, SEEK_SET);
            if (len > 0 && len < 4 * 1024 * 1024) {
                unsigned char* buf = (unsigned char*)malloc((size_t)len);
                if (fread(buf, 1, (size_t)len, f) == (size_t)len)
                    data = parse_tzif(buf, (size_t)len);
                free(buf);
            }
            fclose(f);
        }
    }
    TZCacheEntry* e = (TZCacheEntry*)calloc(1, sizeof(TZCacheEntry));
    e->lower = strdup(lower);
    e->canonical = data ? strdup(canonical) : NULL;
    e->data = data;
    e->next = tz_cache;
    tz_cache = e;
    return data ? e : NULL;
}

// named-zone offset in ns at an instant
static EJSBool
tz_named_offset_ns(const char* name, ejs_i128 epoch_ns, int64_t* out_ns)
{
    TZCacheEntry* e = tz_lookup(name);
    if (!e) return EJS_FALSE;
    int64_t secs = (int64_t)i128_floordiv(epoch_ns, (ejs_i128)1000000000LL);
    *out_ns = (int64_t)tz_data_offset_at(e->data, secs) * 1000000000LL;
    return EJS_TRUE;
}

// next/previous offset-change transition; returns EJS_FALSE for none
static EJSBool
tz_named_transition(const char* name, ejs_i128 epoch_ns, int direction /*1 next, -1 prev*/, ejs_i128* out_ns)
{
    TZCacheEntry* e = tz_lookup(name);
    if (!e) return EJS_FALSE;
    TZData* tz = e->data;
    int64_t t = (int64_t)i128_floordiv(epoch_ns, (ejs_i128)1000000000LL);
    if (direction > 0) {
        // ns sub-second: next transition strictly after epoch_ns
        if (epoch_ns > (ejs_i128)t * 1000000000LL) t += 1;
        for (int i = 0; i < tz->count; i++) {
            if (tz->transitions[i] > t || ((ejs_i128)tz->transitions[i] * 1000000000LL > epoch_ns)) {
                if ((ejs_i128)tz->transitions[i] * 1000000000LL <= epoch_ns) continue;
                int32_t before = i ? tz->offsets[i - 1] : tz->first_offset;
                if (tz->offsets[i] != before) {
                    *out_ns = (ejs_i128)tz->transitions[i] * 1000000000LL;
                    return EJS_TRUE;
                }
            }
        }
        if (tz->has_rules) {
            int32_t y, m, d;
            _ejs_temporal_epoch_days_to_iso_date(t / 86400 - ((t % 86400) < 0 ? 1 : 0), &y, &m, &d);
            for (int yy = y - 1; yy <= y + 2; yy++) {
                int64_t cands[2] = {
                    rule_epoch_seconds(yy, &tz->dst_start, tz->std_off),
                    rule_epoch_seconds(yy, &tz->dst_end, tz->dst_off)
                };
                // ascending pair
                if (cands[0] > cands[1]) { int64_t tmp = cands[0]; cands[0] = cands[1]; cands[1] = tmp; }
                for (int k = 0; k < 2; k++) {
                    if ((ejs_i128)cands[k] * 1000000000LL > epoch_ns
                        && (tz->count == 0 || cands[k] > tz->transitions[tz->count - 1])) {
                        *out_ns = (ejs_i128)cands[k] * 1000000000LL;
                        return EJS_TRUE;
                    }
                }
            }
        }
        return EJS_FALSE;
    }
    // previous: largest transition strictly before epoch_ns
    ejs_i128 best = 0;
    EJSBool have = EJS_FALSE;
    for (int i = 0; i < tz->count; i++) {
        ejs_i128 tns = (ejs_i128)tz->transitions[i] * 1000000000LL;
        if (tns < epoch_ns) {
            int32_t before = i ? tz->offsets[i - 1] : tz->first_offset;
            if (tz->offsets[i] != before) { best = tns; have = EJS_TRUE; }
        }
    }
    if (tz->has_rules) {
        int32_t y, m, d;
        _ejs_temporal_epoch_days_to_iso_date(t / 86400 - ((t % 86400) < 0 ? 1 : 0), &y, &m, &d);
        for (int yy = y - 2; yy <= y + 1; yy++) {
            int64_t cands[2] = {
                rule_epoch_seconds(yy, &tz->dst_start, tz->std_off),
                rule_epoch_seconds(yy, &tz->dst_end, tz->dst_off)
            };
            for (int k = 0; k < 2; k++) {
                ejs_i128 tns = (ejs_i128)cands[k] * 1000000000LL;
                if (tns < epoch_ns && (tz->count == 0 || cands[k] > tz->transitions[tz->count - 1])
                    && (!have || tns > best)) {
                    best = tns;
                    have = EJS_TRUE;
                }
            }
        }
    }
    if (have) { *out_ns = best; return EJS_TRUE; }
    return EJS_FALSE;
}

// ------------------------------------------------------------ time zones

// offset time zone identifier: ±HH[:MM] / ±HHMM (minute precision).
// returns EJS_TRUE and the offset in ns, plus the canonical ±HH:MM form.
static EJSBool
parse_offset_tz_id(const char* s, int64_t* offset_ns, char* canonical, size_t canonical_size)
{
    if (s[0] != '+' && s[0] != '-') return EJS_FALSE;
    int sign = s[0] == '-' ? -1 : 1;
    const char* p = s + 1;
    if (p[0] < '0' || p[0] > '9' || p[1] < '0' || p[1] > '9') return EJS_FALSE;
    int hh = (p[0] - '0') * 10 + (p[1] - '0');
    p += 2;
    int mm = 0;
    if (*p == ':') p++;
    if (*p) {
        if (p[0] < '0' || p[0] > '9' || p[1] < '0' || p[1] > '9' || p[2]) return EJS_FALSE;
        mm = (p[0] - '0') * 10 + (p[1] - '0');
    }
    if (hh > 23 || mm > 59) return EJS_FALSE;
    *offset_ns = (int64_t)sign * ((int64_t)hh * 3600 + (int64_t)mm * 60) * 1000000000LL;
    if (canonical)
        snprintf(canonical, canonical_size, "%c%02d:%02d", sign < 0 ? '-' : '+', hh, mm);
    return EJS_TRUE;
}

// system zone: $TZ if set, else the /etc/localtime symlink's zoneinfo
// suffix, else UTC
static void
system_time_zone_id(char* buf, size_t bufsize)
{
    const char* tz = getenv("TZ");
    if (tz && tz[0]) {
        snprintf(buf, bufsize, "%s", tz);
        return;
    }
    char link[512];
    ssize_t n = readlink("/etc/localtime", link, sizeof(link) - 1);
    if (n > 0) {
        link[n] = 0;
        const char* z = strstr(link, "zoneinfo/");
        if (z) {
            snprintf(buf, bufsize, "%s", z + strlen("zoneinfo/"));
            return;
        }
    }
    snprintf(buf, bufsize, "UTC");
}

// ToTemporalTimeZoneIdentifier for string ids we support natively:
// "UTC" (any case) and offset ids.  Everything else RangeErrors until
// TZif-backed named zones land.
static ejsval
to_time_zone_identifier(ejsval tz_like)
{
    if (EJSVAL_IS_TEMPORAL_ZONEDDATETIME(tz_like))
        return EJSVAL_TO_TEMPORAL_ZONEDDATETIME(tz_like)->time_zone;
    if (!EJSVAL_IS_STRING(tz_like))
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "time zone must be a string");
    char* s = string_to_ascii(tz_like);
    if (!s)
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "unsupported or invalid time zone");
    int64_t off;
    char canonical[16];
    if (parse_offset_tz_id(s, &off, canonical, sizeof(canonical))) {
        free(s);
        return _ejs_string_new_utf8(canonical);
    }
    char* lower = strdup(s);
    ascii_lowercase(lower);
    EJSBool is_utc = !strcmp(lower, "utc");
    free(lower);
    if (is_utc) {
        free(s);
        return _ejs_temporal_str_UTC;
    }
    TZCacheEntry* e = tz_lookup(s);
    if (e) {
        free(s);
        return _ejs_string_new_utf8(e->canonical);
    }
    // a full ISO datetime string names a zone through its [tz]
    // annotation, its Z designator, or its numeric offset
    ParsedISO r;
    if (parse_annotated_datetime(s, &r)) {
        free(s);
        if (r.has_tz) {
            int64_t aoff;
            char acanon[16];
            if (parse_offset_tz_id(r.tz, &aoff, acanon, sizeof(acanon)))
                return _ejs_string_new_utf8(acanon);
            char lower2[128];
            snprintf(lower2, sizeof(lower2), "%s", r.tz);
            ascii_lowercase(lower2);
            if (!strcmp(lower2, "utc"))
                return _ejs_temporal_str_UTC;
            TZCacheEntry* e2 = tz_lookup(r.tz);
            if (e2)
                return _ejs_string_new_utf8(e2->canonical);
            _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "unsupported or invalid time zone");
        }
        if (r.has_offset && r.offset_z)
            return _ejs_temporal_str_UTC;
        if (r.has_offset) {
            if (r.offset_subminute || r.offset_ns % 60000000000LL != 0)
                _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "offset time zones are minute precision");
            char obuf[16];
            format_offset_ns(obuf, sizeof(obuf), r.offset_ns);
            return _ejs_string_new_utf8(obuf);
        }
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "bare datetime string is not a time zone");
    }
    free(s);
    _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "unsupported or invalid time zone");
    EJS_NOT_REACHED();
    return _ejs_undefined;
}

// offset of a supported zone id at an instant.  UTC and offset ids
// don't depend on the instant; the system zone goes through libc.
static int64_t
tz_offset_ns(ejsval tz_id, ejs_i128 epoch_ns)
{
    if (EJSVAL_IS_STRING(tz_id)) {
        char* s = string_to_utf8(tz_id);
        int64_t off;
        if (parse_offset_tz_id(s, &off, NULL, 0)) {
            free(s);
            return off;
        }
        if (!strcmp(s, "UTC")) {
            free(s);
            return 0;
        }
        int64_t named_off;
        if (tz_named_offset_ns(s, epoch_ns, &named_off)) {
            free(s);
            return named_off;
        }
        // the system zone may be a POSIX $TZ string with no tzfile;
        // libc still understands it
        char sysbuf[256];
        system_time_zone_id(sysbuf, sizeof(sysbuf));
        if (!strcmp(s, sysbuf)) {
            free(s);
            time_t secs = (time_t)i128_floordiv(epoch_ns, (ejs_i128)1000000000LL);
            struct tm tm;
            if (localtime_r(&secs, &tm))
                return (int64_t)tm.tm_gmtoff * 1000000000LL;
            return 0;
        }
        free(s);
    }
    _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "unsupported or invalid time zone");
    return 0;
}

// CheckISODaysRange: wall-clock -> epoch conversions require the local
// date itself within +-1e8 epoch days (stricter than the datetime
// within-limits slack)
static void
check_iso_days_range(ejs_i128 local_ns)
{
    ejs_i128 days = i128_floordiv(local_ns, NS_PER_DAY);
    if (days > 100000000LL || days < -100000000LL)
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "date out of range");
}

// GetEpochNanosecondsFor: local wall-clock -> epoch, honoring DST gaps
// and ambiguity.  disambig: 0 compatible, 1 earlier, 2 later, 3 reject.
static ejs_i128
zone_epoch_from_local(ejsval tz_id, ejs_i128 local_ns, int disambig)
{
    check_iso_days_range(local_ns);
    int64_t off_before = tz_offset_ns(tz_id, local_ns - NS_PER_DAY);
    int64_t off_after = tz_offset_ns(tz_id, local_ns + NS_PER_DAY);
    ejs_i128 cands[2];
    int ncands = 0;
    int64_t offs[2] = { off_before, off_after };
    int noffs = (off_before == off_after) ? 1 : 2;
    for (int i = 0; i < noffs; i++) {
        ejs_i128 cand = local_ns - offs[i];
        if (tz_offset_ns(tz_id, cand) == offs[i]) {
            if (ncands == 0 || cands[ncands - 1] != cand)
                cands[ncands++] = cand;
        }
    }
    if (ncands == 1) {
        if (cands[0] < EJS_TEMPORAL_NS_MIN_INSTANT || cands[0] > EJS_TEMPORAL_NS_MAX_INSTANT)
            _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "instant out of range");
        return cands[0];
    }
    if (ncands == 2) {
        if (cands[0] > cands[1]) { ejs_i128 t = cands[0]; cands[0] = cands[1]; cands[1] = t; }
        if (disambig == 3)
            _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "ambiguous wall-clock time");
        return disambig == 2 ? cands[1] : cands[0];
    }
    // gap
    if (disambig == 3)
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "wall-clock time falls in a gap");
    ejs_i128 gap_result = (disambig == 1) ? local_ns - off_after : local_ns - off_before;
    if (gap_result < EJS_TEMPORAL_NS_MIN_INSTANT || gap_result > EJS_TEMPORAL_NS_MAX_INSTANT)
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "instant out of range");
    return gap_result;
}

// ------------------------------------------------------- constructors

static EJS_NATIVE_FUNC(_ejs_TemporalInstant_impl) {
    if (EJSVAL_IS_UNDEFINED(newTarget))
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "Temporal.Instant must be called with new");
    ejsval bi = temporal_to_bigint(argc > 0 ? args[0] : _ejs_undefined);
    ejs_i128 ns = _ejs_temporal_bigint_to_epoch_ns(bi);
    ejsval O = OrdinaryCreateFromConstructor(newTarget, _ejs_TemporalInstant_prototype, &_ejs_TemporalInstant_specops);
    ((EJSTemporalInstant*)EJSVAL_TO_OBJECT(O))->epoch_ns = ns;
    *_this = O;
    return O;
}

// constructor-position calendar argument: string only
static ejsval
canonicalize_constructor_calendar(ejsval cal)
{
    if (EJSVAL_IS_UNDEFINED(cal))
        return _ejs_temporal_str_iso8601;
    if (!EJSVAL_IS_STRING(cal))
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "calendar must be a string");
    char* s = string_to_ascii(cal);
    EJSBool ok = EJS_FALSE;
    if (s) {
        ascii_lowercase(s);
        ok = !strcmp(s, "iso8601");
    }
    free(s);
    if (!ok)
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "unsupported calendar");
    return _ejs_temporal_str_iso8601;
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainDate_impl) {
    if (EJSVAL_IS_UNDEFINED(newTarget))
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "Temporal.PlainDate must be called with new");
    double y = to_integer_with_truncation(argc > 0 ? args[0] : _ejs_undefined);
    double m = to_integer_with_truncation(argc > 1 ? args[1] : _ejs_undefined);
    double d = to_integer_with_truncation(argc > 2 ? args[2] : _ejs_undefined);
    ejsval calendar = canonicalize_constructor_calendar(argc > 3 ? args[3] : _ejs_undefined);
    if (y < -1000000 || y > 1000000) // far outside limits; keep int32 safe
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "date out of range");
    if (!_ejs_temporal_is_valid_iso_date((int32_t)y, (int32_t)m, (int32_t)d))
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "invalid ISO date");
    if (!iso_date_within_limits((int32_t)y, (int32_t)m, (int32_t)d))
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "date out of range");
    ejsval O = OrdinaryCreateFromConstructor(newTarget, _ejs_TemporalPlainDate_prototype, &_ejs_TemporalPlainDate_specops);
    EJSTemporalPlainDate* pd = (EJSTemporalPlainDate*)EJSVAL_TO_OBJECT(O);
    pd->year = (int32_t)y; pd->month = (int32_t)m; pd->day = (int32_t)d;
    pd->calendar = calendar;
    *_this = O;
    return O;
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainTime_impl) {
    if (EJSVAL_IS_UNDEFINED(newTarget))
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "Temporal.PlainTime must be called with new");
    double parts[6];
    for (int i = 0; i < 6; i++) {
        ejsval v = argc > (uint32_t)i ? args[i] : _ejs_undefined;
        parts[i] = EJSVAL_IS_UNDEFINED(v) ? 0 : to_integer_with_truncation(v);
    }
    if (!_ejs_temporal_is_valid_time((int32_t)parts[0], (int32_t)parts[1], (int32_t)parts[2],
                                     (int32_t)parts[3], (int32_t)parts[4], (int32_t)parts[5]))
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "invalid time");
    ejsval O = OrdinaryCreateFromConstructor(newTarget, _ejs_TemporalPlainTime_prototype, &_ejs_TemporalPlainTime_specops);
    EJSTemporalPlainTime* pt = (EJSTemporalPlainTime*)EJSVAL_TO_OBJECT(O);
    pt->hour = (int32_t)parts[0]; pt->minute = (int32_t)parts[1]; pt->second = (int32_t)parts[2];
    pt->millisecond = (int32_t)parts[3]; pt->microsecond = (int32_t)parts[4]; pt->nanosecond = (int32_t)parts[5];
    *_this = O;
    return O;
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainDateTime_impl) {
    if (EJSVAL_IS_UNDEFINED(newTarget))
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "Temporal.PlainDateTime must be called with new");
    double y = to_integer_with_truncation(argc > 0 ? args[0] : _ejs_undefined);
    double mo = to_integer_with_truncation(argc > 1 ? args[1] : _ejs_undefined);
    double d = to_integer_with_truncation(argc > 2 ? args[2] : _ejs_undefined);
    double t[6];
    for (int i = 0; i < 6; i++) {
        ejsval v = argc > (uint32_t)(3 + i) ? args[3 + i] : _ejs_undefined;
        t[i] = EJSVAL_IS_UNDEFINED(v) ? 0 : to_integer_with_truncation(v);
    }
    ejsval calendar = canonicalize_constructor_calendar(argc > 9 ? args[9] : _ejs_undefined);
    if (y < -1000000 || y > 1000000)
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "date out of range");
    if (!_ejs_temporal_is_valid_iso_date((int32_t)y, (int32_t)mo, (int32_t)d))
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "invalid ISO date");
    if (!_ejs_temporal_is_valid_time((int32_t)t[0], (int32_t)t[1], (int32_t)t[2], (int32_t)t[3], (int32_t)t[4], (int32_t)t[5]))
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "invalid time");
    if (!_ejs_temporal_iso_datetime_within_limits((int32_t)y, (int32_t)mo, (int32_t)d,
                                                  (int32_t)t[0], (int32_t)t[1], (int32_t)t[2],
                                                  (int32_t)t[3], (int32_t)t[4], (int32_t)t[5]))
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "datetime out of range");
    ejsval O = OrdinaryCreateFromConstructor(newTarget, _ejs_TemporalPlainDateTime_prototype, &_ejs_TemporalPlainDateTime_specops);
    EJSTemporalPlainDateTime* pdt = (EJSTemporalPlainDateTime*)EJSVAL_TO_OBJECT(O);
    pdt->year = (int32_t)y; pdt->month = (int32_t)mo; pdt->day = (int32_t)d;
    pdt->hour = (int32_t)t[0]; pdt->minute = (int32_t)t[1]; pdt->second = (int32_t)t[2];
    pdt->millisecond = (int32_t)t[3]; pdt->microsecond = (int32_t)t[4]; pdt->nanosecond = (int32_t)t[5];
    pdt->calendar = calendar;
    *_this = O;
    return O;
}

static EJSBool
iso_yearmonth_within_limits(int32_t year, int32_t month)
{
    if (year < -271821 || year > 275760) return EJS_FALSE;
    if (year == -271821 && month < 4) return EJS_FALSE;
    if (year == 275760 && month > 9) return EJS_FALSE;
    return EJS_TRUE;
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainYearMonth_impl) {
    if (EJSVAL_IS_UNDEFINED(newTarget))
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "Temporal.PlainYearMonth must be called with new");
    double y = to_integer_with_truncation(argc > 0 ? args[0] : _ejs_undefined);
    double m = to_integer_with_truncation(argc > 1 ? args[1] : _ejs_undefined);
    ejsval calendar = canonicalize_constructor_calendar(argc > 2 ? args[2] : _ejs_undefined);
    ejsval ref = argc > 3 ? args[3] : _ejs_undefined;
    double rd = EJSVAL_IS_UNDEFINED(ref) ? 1 : to_integer_with_truncation(ref);
    if (y < -1000000 || y > 1000000)
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "year-month out of range");
    if (!_ejs_temporal_is_valid_iso_date((int32_t)y, (int32_t)m, (int32_t)rd))
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "invalid ISO year-month");
    if (!iso_yearmonth_within_limits((int32_t)y, (int32_t)m))
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "year-month out of range");
    ejsval O = OrdinaryCreateFromConstructor(newTarget, _ejs_TemporalPlainYearMonth_prototype, &_ejs_TemporalPlainYearMonth_specops);
    EJSTemporalPlainYearMonth* ym = (EJSTemporalPlainYearMonth*)EJSVAL_TO_OBJECT(O);
    ym->year = (int32_t)y; ym->month = (int32_t)m; ym->ref_day = (int32_t)rd;
    ym->calendar = calendar;
    *_this = O;
    return O;
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainMonthDay_impl) {
    if (EJSVAL_IS_UNDEFINED(newTarget))
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "Temporal.PlainMonthDay must be called with new");
    double m = to_integer_with_truncation(argc > 0 ? args[0] : _ejs_undefined);
    double d = to_integer_with_truncation(argc > 1 ? args[1] : _ejs_undefined);
    ejsval calendar = canonicalize_constructor_calendar(argc > 2 ? args[2] : _ejs_undefined);
    ejsval ref = argc > 3 ? args[3] : _ejs_undefined;
    double ry = EJSVAL_IS_UNDEFINED(ref) ? 1972 : to_integer_with_truncation(ref);
    if (ry < -1000000 || ry > 1000000)
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "month-day out of range");
    if (!_ejs_temporal_is_valid_iso_date((int32_t)ry, (int32_t)m, (int32_t)d))
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "invalid ISO month-day");
    if (!iso_date_within_limits((int32_t)ry, (int32_t)m, (int32_t)d))
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "month-day out of range");
    ejsval O = OrdinaryCreateFromConstructor(newTarget, _ejs_TemporalPlainMonthDay_prototype, &_ejs_TemporalPlainMonthDay_specops);
    EJSTemporalPlainMonthDay* md = (EJSTemporalPlainMonthDay*)EJSVAL_TO_OBJECT(O);
    md->month = (int32_t)m; md->day = (int32_t)d; md->ref_year = (int32_t)ry;
    md->calendar = calendar;
    *_this = O;
    return O;
}

static EJS_NATIVE_FUNC(_ejs_TemporalZonedDateTime_impl) {
    if (EJSVAL_IS_UNDEFINED(newTarget))
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "Temporal.ZonedDateTime must be called with new");
    ejsval bi = temporal_to_bigint(argc > 0 ? args[0] : _ejs_undefined);
    ejs_i128 ns = _ejs_temporal_bigint_to_epoch_ns(bi);
    ejsval tz_like = argc > 1 ? args[1] : _ejs_undefined;
    if (!EJSVAL_IS_STRING(tz_like))
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "time zone must be a string");
    // constructor position: a pure identifier only (offset or IANA
    // name); ISO datetime strings are not accepted here
    ejsval tz;
    {
        char* s = string_to_ascii(tz_like);
        if (!s)
            _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "unsupported or invalid time zone");
        int64_t off;
        char canonical[16];
        if (parse_offset_tz_id(s, &off, canonical, sizeof(canonical))) {
            free(s);
            tz = _ejs_string_new_utf8(canonical);
        } else {
            char* lower = strdup(s);
            ascii_lowercase(lower);
            EJSBool is_utc = !strcmp(lower, "utc");
            free(lower);
            if (is_utc) {
                free(s);
                tz = _ejs_temporal_str_UTC;
            } else {
                TZCacheEntry* e = tz_lookup(s);
                free(s);
                if (!e)
                    _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "unsupported or invalid time zone");
                tz = _ejs_string_new_utf8(e->canonical);
            }
        }
    }
    ejsval calendar = canonicalize_constructor_calendar(argc > 2 ? args[2] : _ejs_undefined);
    ejsval O = OrdinaryCreateFromConstructor(newTarget, _ejs_TemporalZonedDateTime_prototype, &_ejs_TemporalZonedDateTime_specops);
    EJSTemporalZonedDateTime* zdt = (EJSTemporalZonedDateTime*)EJSVAL_TO_OBJECT(O);
    zdt->epoch_ns = ns;
    zdt->time_zone = tz;
    zdt->calendar = calendar;
    *_this = O;
    return O;
}

static EJS_NATIVE_FUNC(_ejs_TemporalDuration_impl) {
    if (EJSVAL_IS_UNDEFINED(newTarget))
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "Temporal.Duration must be called with new");
    double f[10];
    for (int i = 0; i < 10; i++) {
        ejsval v = argc > (uint32_t)i ? args[i] : _ejs_undefined;
        f[i] = EJSVAL_IS_UNDEFINED(v) ? 0 : to_integer_if_integral(v);
    }
    if (!is_valid_duration(f[0], f[1], f[2], f[3], f[4], f[5], f[6], f[7], f[8], f[9]))
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "invalid duration");
    ejsval O = OrdinaryCreateFromConstructor(newTarget, _ejs_TemporalDuration_prototype, &_ejs_TemporalDuration_specops);
    EJSTemporalDuration* dur = (EJSTemporalDuration*)EJSVAL_TO_OBJECT(O);
    for (int i = 0; i < 10; i++)
        if (f[i] == 0) f[i] = 0; // normalize -0
    dur->years = f[0]; dur->months = f[1]; dur->weeks = f[2]; dur->days = f[3];
    dur->hours = f[4]; dur->minutes = f[5]; dur->seconds = f[6];
    dur->milliseconds = f[7]; dur->microseconds = f[8]; dur->nanoseconds = f[9];
    *_this = O;
    return O;
}

// ---------------------------------------------------------------- getters

#define BRAND_CHECK(pred, what)                                         \
    EJS_MACRO_START                                                     \
    if (!pred(*_this))                                                  \
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, what " called on incompatible receiver"); \
    EJS_MACRO_END

// Instant

static EJS_NATIVE_FUNC(_ejs_TemporalInstant_get_epochNanoseconds) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_INSTANT, "Temporal.Instant.prototype.epochNanoseconds");
    return _ejs_temporal_i128_to_bigint(EJSVAL_TO_TEMPORAL_INSTANT(*_this)->epoch_ns);
}

static EJS_NATIVE_FUNC(_ejs_TemporalInstant_get_epochMilliseconds) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_INSTANT, "Temporal.Instant.prototype.epochMilliseconds");
    ejs_i128 ns = EJSVAL_TO_TEMPORAL_INSTANT(*_this)->epoch_ns;
    return NUMBER_TO_EJSVAL((double)(int64_t)i128_floordiv(ns, (ejs_i128)1000000LL));
}

// PlainDate

#define PLAINDATE_GETTER_INT(name, expr)                                \
    static EJS_NATIVE_FUNC(_ejs_TemporalPlainDate_get_##name) {         \
        BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINDATE, "Temporal.PlainDate.prototype." #name); \
        EJSTemporalPlainDate* pd = EJSVAL_TO_TEMPORAL_PLAINDATE(*_this); \
        (void)pd;                                                       \
        return NUMBER_TO_EJSVAL((double)(expr));                        \
    }

PLAINDATE_GETTER_INT(year, pd->year)
PLAINDATE_GETTER_INT(month, pd->month)
PLAINDATE_GETTER_INT(day, pd->day)
PLAINDATE_GETTER_INT(dayOfWeek, _ejs_temporal_iso_day_of_week(pd->year, pd->month, pd->day))
PLAINDATE_GETTER_INT(dayOfYear, _ejs_temporal_iso_day_of_year(pd->year, pd->month, pd->day))
PLAINDATE_GETTER_INT(daysInWeek, 7)
PLAINDATE_GETTER_INT(daysInMonth, _ejs_temporal_iso_days_in_month(pd->year, pd->month))
PLAINDATE_GETTER_INT(daysInYear, _ejs_temporal_iso_is_leap_year(pd->year) ? 366 : 365)
PLAINDATE_GETTER_INT(monthsInYear, 12)

static EJS_NATIVE_FUNC(_ejs_TemporalPlainDate_get_calendarId) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINDATE, "Temporal.PlainDate.prototype.calendarId");
    return EJSVAL_TO_TEMPORAL_PLAINDATE(*_this)->calendar;
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainDate_get_era) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINDATE, "Temporal.PlainDate.prototype.era");
    return _ejs_undefined; // iso8601 has no eras
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainDate_get_eraYear) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINDATE, "Temporal.PlainDate.prototype.eraYear");
    return _ejs_undefined;
}

static ejsval
month_code_string(int32_t month)
{
    char buf[8];
    snprintf(buf, sizeof(buf), "M%02d", month);
    return _ejs_string_new_utf8(buf);
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainDate_get_monthCode) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINDATE, "Temporal.PlainDate.prototype.monthCode");
    return month_code_string(EJSVAL_TO_TEMPORAL_PLAINDATE(*_this)->month);
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainDate_get_weekOfYear) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINDATE, "Temporal.PlainDate.prototype.weekOfYear");
    EJSTemporalPlainDate* pd = EJSVAL_TO_TEMPORAL_PLAINDATE(*_this);
    int32_t week, week_year;
    _ejs_temporal_iso_week_of_year(pd->year, pd->month, pd->day, &week, &week_year);
    return NUMBER_TO_EJSVAL((double)week);
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainDate_get_yearOfWeek) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINDATE, "Temporal.PlainDate.prototype.yearOfWeek");
    EJSTemporalPlainDate* pd = EJSVAL_TO_TEMPORAL_PLAINDATE(*_this);
    int32_t week, week_year;
    _ejs_temporal_iso_week_of_year(pd->year, pd->month, pd->day, &week, &week_year);
    return NUMBER_TO_EJSVAL((double)week_year);
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainDate_get_inLeapYear) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINDATE, "Temporal.PlainDate.prototype.inLeapYear");
    return BOOLEAN_TO_EJSVAL(_ejs_temporal_iso_is_leap_year(EJSVAL_TO_TEMPORAL_PLAINDATE(*_this)->year));
}

// PlainTime

#define PLAINTIME_GETTER(name, field)                                   \
    static EJS_NATIVE_FUNC(_ejs_TemporalPlainTime_get_##name) {         \
        BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINTIME, "Temporal.PlainTime.prototype." #name); \
        return NUMBER_TO_EJSVAL((double)EJSVAL_TO_TEMPORAL_PLAINTIME(*_this)->field); \
    }

PLAINTIME_GETTER(hour, hour)
PLAINTIME_GETTER(minute, minute)
PLAINTIME_GETTER(second, second)
PLAINTIME_GETTER(millisecond, millisecond)
PLAINTIME_GETTER(microsecond, microsecond)
PLAINTIME_GETTER(nanosecond, nanosecond)

// PlainDateTime

#define PLAINDATETIME_GETTER_INT(name, expr)                            \
    static EJS_NATIVE_FUNC(_ejs_TemporalPlainDateTime_get_##name) {     \
        BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINDATETIME, "Temporal.PlainDateTime.prototype." #name); \
        EJSTemporalPlainDateTime* pdt = EJSVAL_TO_TEMPORAL_PLAINDATETIME(*_this); \
        (void)pdt;                                                      \
        return NUMBER_TO_EJSVAL((double)(expr));                        \
    }

PLAINDATETIME_GETTER_INT(year, pdt->year)
PLAINDATETIME_GETTER_INT(month, pdt->month)
PLAINDATETIME_GETTER_INT(day, pdt->day)
PLAINDATETIME_GETTER_INT(hour, pdt->hour)
PLAINDATETIME_GETTER_INT(minute, pdt->minute)
PLAINDATETIME_GETTER_INT(second, pdt->second)
PLAINDATETIME_GETTER_INT(millisecond, pdt->millisecond)
PLAINDATETIME_GETTER_INT(microsecond, pdt->microsecond)
PLAINDATETIME_GETTER_INT(nanosecond, pdt->nanosecond)
PLAINDATETIME_GETTER_INT(dayOfWeek, _ejs_temporal_iso_day_of_week(pdt->year, pdt->month, pdt->day))
PLAINDATETIME_GETTER_INT(dayOfYear, _ejs_temporal_iso_day_of_year(pdt->year, pdt->month, pdt->day))
PLAINDATETIME_GETTER_INT(daysInWeek, 7)
PLAINDATETIME_GETTER_INT(daysInMonth, _ejs_temporal_iso_days_in_month(pdt->year, pdt->month))
PLAINDATETIME_GETTER_INT(daysInYear, _ejs_temporal_iso_is_leap_year(pdt->year) ? 366 : 365)
PLAINDATETIME_GETTER_INT(monthsInYear, 12)

static EJS_NATIVE_FUNC(_ejs_TemporalPlainDateTime_get_calendarId) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINDATETIME, "Temporal.PlainDateTime.prototype.calendarId");
    return EJSVAL_TO_TEMPORAL_PLAINDATETIME(*_this)->calendar;
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainDateTime_get_era) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINDATETIME, "Temporal.PlainDateTime.prototype.era");
    return _ejs_undefined;
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainDateTime_get_eraYear) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINDATETIME, "Temporal.PlainDateTime.prototype.eraYear");
    return _ejs_undefined;
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainDateTime_get_monthCode) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINDATETIME, "Temporal.PlainDateTime.prototype.monthCode");
    return month_code_string(EJSVAL_TO_TEMPORAL_PLAINDATETIME(*_this)->month);
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainDateTime_get_weekOfYear) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINDATETIME, "Temporal.PlainDateTime.prototype.weekOfYear");
    EJSTemporalPlainDateTime* pdt = EJSVAL_TO_TEMPORAL_PLAINDATETIME(*_this);
    int32_t week, week_year;
    _ejs_temporal_iso_week_of_year(pdt->year, pdt->month, pdt->day, &week, &week_year);
    return NUMBER_TO_EJSVAL((double)week);
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainDateTime_get_yearOfWeek) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINDATETIME, "Temporal.PlainDateTime.prototype.yearOfWeek");
    EJSTemporalPlainDateTime* pdt = EJSVAL_TO_TEMPORAL_PLAINDATETIME(*_this);
    int32_t week, week_year;
    _ejs_temporal_iso_week_of_year(pdt->year, pdt->month, pdt->day, &week, &week_year);
    return NUMBER_TO_EJSVAL((double)week_year);
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainDateTime_get_inLeapYear) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINDATETIME, "Temporal.PlainDateTime.prototype.inLeapYear");
    return BOOLEAN_TO_EJSVAL(_ejs_temporal_iso_is_leap_year(EJSVAL_TO_TEMPORAL_PLAINDATETIME(*_this)->year));
}

// PlainYearMonth

static EJS_NATIVE_FUNC(_ejs_TemporalPlainYearMonth_get_calendarId) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINYEARMONTH, "Temporal.PlainYearMonth.prototype.calendarId");
    return EJSVAL_TO_TEMPORAL_PLAINYEARMONTH(*_this)->calendar;
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainYearMonth_get_era) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINYEARMONTH, "Temporal.PlainYearMonth.prototype.era");
    return _ejs_undefined;
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainYearMonth_get_eraYear) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINYEARMONTH, "Temporal.PlainYearMonth.prototype.eraYear");
    return _ejs_undefined;
}

#define YEARMONTH_GETTER_INT(name, expr)                                \
    static EJS_NATIVE_FUNC(_ejs_TemporalPlainYearMonth_get_##name) {    \
        BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINYEARMONTH, "Temporal.PlainYearMonth.prototype." #name); \
        EJSTemporalPlainYearMonth* ym = EJSVAL_TO_TEMPORAL_PLAINYEARMONTH(*_this); \
        (void)ym;                                                       \
        return NUMBER_TO_EJSVAL((double)(expr));                        \
    }

YEARMONTH_GETTER_INT(year, ym->year)
YEARMONTH_GETTER_INT(month, ym->month)
YEARMONTH_GETTER_INT(daysInMonth, _ejs_temporal_iso_days_in_month(ym->year, ym->month))
YEARMONTH_GETTER_INT(daysInYear, _ejs_temporal_iso_is_leap_year(ym->year) ? 366 : 365)
YEARMONTH_GETTER_INT(monthsInYear, 12)

static EJS_NATIVE_FUNC(_ejs_TemporalPlainYearMonth_get_monthCode) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINYEARMONTH, "Temporal.PlainYearMonth.prototype.monthCode");
    return month_code_string(EJSVAL_TO_TEMPORAL_PLAINYEARMONTH(*_this)->month);
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainYearMonth_get_inLeapYear) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINYEARMONTH, "Temporal.PlainYearMonth.prototype.inLeapYear");
    return BOOLEAN_TO_EJSVAL(_ejs_temporal_iso_is_leap_year(EJSVAL_TO_TEMPORAL_PLAINYEARMONTH(*_this)->year));
}

// PlainMonthDay

static EJS_NATIVE_FUNC(_ejs_TemporalPlainMonthDay_get_calendarId) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINMONTHDAY, "Temporal.PlainMonthDay.prototype.calendarId");
    return EJSVAL_TO_TEMPORAL_PLAINMONTHDAY(*_this)->calendar;
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainMonthDay_get_monthCode) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINMONTHDAY, "Temporal.PlainMonthDay.prototype.monthCode");
    return month_code_string(EJSVAL_TO_TEMPORAL_PLAINMONTHDAY(*_this)->month);
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainMonthDay_get_day) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINMONTHDAY, "Temporal.PlainMonthDay.prototype.day");
    return NUMBER_TO_EJSVAL((double)EJSVAL_TO_TEMPORAL_PLAINMONTHDAY(*_this)->day);
}

// ZonedDateTime

typedef struct {
    int32_t year, month, day, hour, minute, second, millisecond, microsecond, nanosecond;
    int64_t offset_ns;
} ZDTFields;

static void
zdt_fields(EJSTemporalZonedDateTime* zdt, ZDTFields* f)
{
    f->offset_ns = tz_offset_ns(zdt->time_zone, zdt->epoch_ns);
    epoch_ns_to_iso_fields(zdt->epoch_ns, f->offset_ns,
                           &f->year, &f->month, &f->day,
                           &f->hour, &f->minute, &f->second,
                           &f->millisecond, &f->microsecond, &f->nanosecond);
}

#define ZDT_GETTER(name, expr)                                          \
    static EJS_NATIVE_FUNC(_ejs_TemporalZonedDateTime_get_##name) {     \
        BRAND_CHECK(EJSVAL_IS_TEMPORAL_ZONEDDATETIME, "Temporal.ZonedDateTime.prototype." #name); \
        EJSTemporalZonedDateTime* zdt = EJSVAL_TO_TEMPORAL_ZONEDDATETIME(*_this); \
        ZDTFields f;                                                    \
        zdt_fields(zdt, &f);                                            \
        (void)f;                                                        \
        return (expr);                                                  \
    }

ZDT_GETTER(year, NUMBER_TO_EJSVAL((double)f.year))
ZDT_GETTER(month, NUMBER_TO_EJSVAL((double)f.month))
ZDT_GETTER(day, NUMBER_TO_EJSVAL((double)f.day))
ZDT_GETTER(hour, NUMBER_TO_EJSVAL((double)f.hour))
ZDT_GETTER(minute, NUMBER_TO_EJSVAL((double)f.minute))
ZDT_GETTER(second, NUMBER_TO_EJSVAL((double)f.second))
ZDT_GETTER(millisecond, NUMBER_TO_EJSVAL((double)f.millisecond))
ZDT_GETTER(microsecond, NUMBER_TO_EJSVAL((double)f.microsecond))
ZDT_GETTER(nanosecond, NUMBER_TO_EJSVAL((double)f.nanosecond))
ZDT_GETTER(monthCode, month_code_string(f.month))
ZDT_GETTER(dayOfWeek, NUMBER_TO_EJSVAL((double)_ejs_temporal_iso_day_of_week(f.year, f.month, f.day)))
ZDT_GETTER(dayOfYear, NUMBER_TO_EJSVAL((double)_ejs_temporal_iso_day_of_year(f.year, f.month, f.day)))
ZDT_GETTER(daysInWeek, NUMBER_TO_EJSVAL(7.0))
ZDT_GETTER(daysInMonth, NUMBER_TO_EJSVAL((double)_ejs_temporal_iso_days_in_month(f.year, f.month)))
ZDT_GETTER(daysInYear, NUMBER_TO_EJSVAL(_ejs_temporal_iso_is_leap_year(f.year) ? 366.0 : 365.0))
ZDT_GETTER(monthsInYear, NUMBER_TO_EJSVAL(12.0))
ZDT_GETTER(inLeapYear, BOOLEAN_TO_EJSVAL(_ejs_temporal_iso_is_leap_year(f.year)))
ZDT_GETTER(offsetNanoseconds, NUMBER_TO_EJSVAL((double)f.offset_ns))

static EJS_NATIVE_FUNC(_ejs_TemporalZonedDateTime_get_hoursInDay) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_ZONEDDATETIME, "Temporal.ZonedDateTime.prototype.hoursInDay");
    EJSTemporalZonedDateTime* zdt = EJSVAL_TO_TEMPORAL_ZONEDDATETIME(*_this);
    ZDTFields f;
    zdt_fields(zdt, &f);
    ejs_i128 start = zone_epoch_from_local(zdt->time_zone,
        iso_datetime_to_epoch_ns(f.year, f.month, f.day, 0, 0, 0, 0, 0, 0), 0);
    int32_t ny, nm, nd;
    _ejs_temporal_epoch_days_to_iso_date(
        _ejs_temporal_iso_date_to_epoch_days(f.year, f.month, f.day) + 1, &ny, &nm, &nd);
    ejs_i128 next = zone_epoch_from_local(zdt->time_zone,
        iso_datetime_to_epoch_ns(ny, nm, nd, 0, 0, 0, 0, 0, 0), 0);
    return NUMBER_TO_EJSVAL((double)(int64_t)(next - start) / 3600000000000.0);
}

static EJS_NATIVE_FUNC(_ejs_TemporalZonedDateTime_get_weekOfYear) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_ZONEDDATETIME, "Temporal.ZonedDateTime.prototype.weekOfYear");
    EJSTemporalZonedDateTime* zdt = EJSVAL_TO_TEMPORAL_ZONEDDATETIME(*_this);
    ZDTFields f;
    zdt_fields(zdt, &f);
    int32_t week, week_year;
    _ejs_temporal_iso_week_of_year(f.year, f.month, f.day, &week, &week_year);
    return NUMBER_TO_EJSVAL((double)week);
}

static EJS_NATIVE_FUNC(_ejs_TemporalZonedDateTime_get_yearOfWeek) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_ZONEDDATETIME, "Temporal.ZonedDateTime.prototype.yearOfWeek");
    EJSTemporalZonedDateTime* zdt = EJSVAL_TO_TEMPORAL_ZONEDDATETIME(*_this);
    ZDTFields f;
    zdt_fields(zdt, &f);
    int32_t week, week_year;
    _ejs_temporal_iso_week_of_year(f.year, f.month, f.day, &week, &week_year);
    return NUMBER_TO_EJSVAL((double)week_year);
}

static EJS_NATIVE_FUNC(_ejs_TemporalZonedDateTime_get_calendarId) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_ZONEDDATETIME, "Temporal.ZonedDateTime.prototype.calendarId");
    return EJSVAL_TO_TEMPORAL_ZONEDDATETIME(*_this)->calendar;
}

static EJS_NATIVE_FUNC(_ejs_TemporalZonedDateTime_get_timeZoneId) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_ZONEDDATETIME, "Temporal.ZonedDateTime.prototype.timeZoneId");
    return EJSVAL_TO_TEMPORAL_ZONEDDATETIME(*_this)->time_zone;
}

static EJS_NATIVE_FUNC(_ejs_TemporalZonedDateTime_get_era) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_ZONEDDATETIME, "Temporal.ZonedDateTime.prototype.era");
    return _ejs_undefined;
}

static EJS_NATIVE_FUNC(_ejs_TemporalZonedDateTime_get_eraYear) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_ZONEDDATETIME, "Temporal.ZonedDateTime.prototype.eraYear");
    return _ejs_undefined;
}

static EJS_NATIVE_FUNC(_ejs_TemporalZonedDateTime_get_epochNanoseconds) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_ZONEDDATETIME, "Temporal.ZonedDateTime.prototype.epochNanoseconds");
    return _ejs_temporal_i128_to_bigint(EJSVAL_TO_TEMPORAL_ZONEDDATETIME(*_this)->epoch_ns);
}

static EJS_NATIVE_FUNC(_ejs_TemporalZonedDateTime_get_epochMilliseconds) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_ZONEDDATETIME, "Temporal.ZonedDateTime.prototype.epochMilliseconds");
    ejs_i128 ns = EJSVAL_TO_TEMPORAL_ZONEDDATETIME(*_this)->epoch_ns;
    return NUMBER_TO_EJSVAL((double)(int64_t)i128_floordiv(ns, (ejs_i128)1000000LL));
}

static EJS_NATIVE_FUNC(_ejs_TemporalZonedDateTime_get_offset) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_ZONEDDATETIME, "Temporal.ZonedDateTime.prototype.offset");
    EJSTemporalZonedDateTime* zdt = EJSVAL_TO_TEMPORAL_ZONEDDATETIME(*_this);
    char buf[16];
    format_offset_ns(buf, sizeof(buf), tz_offset_ns(zdt->time_zone, zdt->epoch_ns));
    return _ejs_string_new_utf8(buf);
}

// Duration

#define DURATION_GETTER(name, field)                                    \
    static EJS_NATIVE_FUNC(_ejs_TemporalDuration_get_##name) {          \
        BRAND_CHECK(EJSVAL_IS_TEMPORAL_DURATION, "Temporal.Duration.prototype." #name); \
        return NUMBER_TO_EJSVAL(EJSVAL_TO_TEMPORAL_DURATION(*_this)->field); \
    }

DURATION_GETTER(years, years)
DURATION_GETTER(months, months)
DURATION_GETTER(weeks, weeks)
DURATION_GETTER(days, days)
DURATION_GETTER(hours, hours)
DURATION_GETTER(minutes, minutes)
DURATION_GETTER(seconds, seconds)
DURATION_GETTER(milliseconds, milliseconds)
DURATION_GETTER(microseconds, microseconds)
DURATION_GETTER(nanoseconds, nanoseconds)

static EJS_NATIVE_FUNC(_ejs_TemporalDuration_get_sign) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_DURATION, "Temporal.Duration.prototype.sign");
    return NUMBER_TO_EJSVAL((double)duration_sign(EJSVAL_TO_TEMPORAL_DURATION(*_this)));
}

static EJS_NATIVE_FUNC(_ejs_TemporalDuration_get_blank) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_DURATION, "Temporal.Duration.prototype.blank");
    return BOOLEAN_TO_EJSVAL(duration_sign(EJSVAL_TO_TEMPORAL_DURATION(*_this)) == 0);
}

// ---------------------------------------------------- duration parsing

// signed i128 accumulate helper for fraction cascade
typedef struct {
    double years, months, weeks, days, hours, minutes, seconds, ms, us, ns;
} DurationFields;

static int
parse_duration_number(const char** pp, double* out, int32_t* frac_ns, EJSBool* has_frac)
{
    const char* p = *pp;
    if (*p < '0' || *p > '9') return 0;
    double v = 0;
    while (*p >= '0' && *p <= '9') {
        v = v * 10 + (*p - '0');
        p++;
    }
    *has_frac = EJS_FALSE;
    *frac_ns = 0;
    if (*p == '.' || *p == ',') {
        if (!parse_fraction_ns(&p, frac_ns)) return 0;
        *has_frac = EJS_TRUE;
    }
    *pp = p;
    *out = v;
    return 1;
}

// ISO 8601 duration: [±]P[nY][nM][nW][nD][T[nH][nM][nS]] with a
// fraction allowed only on the smallest present time unit
static int
parse_duration_string(const char* s, DurationFields* f)
{
    memset(f, 0, sizeof(*f));
    const char* p = s;
    int sign = 1;
    if (*p == '+') p++;
    else if (*p == '-') { sign = -1; p++; }
    if (*p != 'P' && *p != 'p') return 0;
    p++;
    EJSBool any = EJS_FALSE;
    double v; int32_t frac; EJSBool has_frac;
    // date part: Y, M, W, D — no fractions allowed
    const char date_units[4] = { 'Y', 'M', 'W', 'D' };
    double* date_dest[4] = { &f->years, &f->months, &f->weeks, &f->days };
    int di = 0;
    while (*p && *p != 'T' && *p != 't') {
        if (!parse_duration_number(&p, &v, &frac, &has_frac)) return 0;
        if (has_frac) return 0;
        char u = (char)((*p >= 'a') ? *p - 32 : *p);
        int matched = 0;
        for (; di < 4; di++) {
            if (date_units[di] == u) {
                *date_dest[di] = v;
                di++;
                matched = 1;
                break;
            }
        }
        if (!matched) return 0;
        p++;
        any = EJS_TRUE;
    }
    if (*p == 'T' || *p == 't') {
        p++;
        if (!*p) return 0;
        // time part: H, M, S — fraction ends the string
        EJSBool done_frac = EJS_FALSE;
        const char time_units[3] = { 'H', 'M', 'S' };
        double* time_dest[3] = { &f->hours, &f->minutes, &f->seconds };
        int ti = 0;
        while (*p) {
            if (done_frac) return 0;
            if (!parse_duration_number(&p, &v, &frac, &has_frac)) return 0;
            char u = (char)((*p >= 'a') ? *p - 32 : *p);
            int matched = 0;
            for (; ti < 3; ti++) {
                if (time_units[ti] == u) {
                    *time_dest[ti] = v;
                    if (has_frac) {
                        // distribute the fraction into smaller units
                        int64_t total_ns;
                        if (u == 'H') total_ns = (int64_t)frac * 3600 / 1;
                        else if (u == 'M') total_ns = (int64_t)frac * 60;
                        else total_ns = frac;
                        if (u == 'H') {
                            f->minutes = (double)(total_ns / 60000000000LL);
                            total_ns %= 60000000000LL;
                        }
                        if (u == 'H' || u == 'M') {
                            f->seconds = (double)(total_ns / 1000000000LL);
                            total_ns %= 1000000000LL;
                        }
                        f->ms = (double)(total_ns / 1000000LL);
                        f->us = (double)((total_ns / 1000LL) % 1000LL);
                        f->ns = (double)(total_ns % 1000LL);
                        done_frac = EJS_TRUE;
                    }
                    ti++;
                    matched = 1;
                    break;
                }
            }
            if (!matched) return 0;
            p++;
            any = EJS_TRUE;
        }
    }
    if (!any) return 0;
    if (sign < 0) {
        f->years = -f->years; f->months = -f->months; f->weeks = -f->weeks; f->days = -f->days;
        f->hours = -f->hours; f->minutes = -f->minutes; f->seconds = -f->seconds;
        f->ms = -f->ms; f->us = -f->us; f->ns = -f->ns;
    }
    return 1;
}

// ---------------------------------------------------------- rounding

typedef enum {
    ROUND_CEIL, ROUND_FLOOR, ROUND_EXPAND, ROUND_TRUNC,
    ROUND_HALF_CEIL, ROUND_HALF_FLOOR, ROUND_HALF_EXPAND, ROUND_HALF_TRUNC, ROUND_HALF_EVEN
} RoundingMode;

static const char* const rounding_mode_names[] = {
    "ceil", "floor", "expand", "trunc",
    "halfCeil", "halfFloor", "halfExpand", "halfTrunc", "halfEven"
};

// round v to a multiple of increment under the given mode
static ejs_i128
round_i128_to_increment(ejs_i128 v, ejs_i128 increment, RoundingMode mode)
{
    ejs_i128 q = v / increment;
    ejs_i128 r = v % increment;
    if (r == 0) return v;
    EJSBool positive = v > 0;
    ejs_i128 lower = positive ? q : q - 1;         // floor
    ejs_i128 upper = lower + 1;                     // ceil
    ejs_i128 chosen;
    switch (mode) {
    case ROUND_CEIL: chosen = upper; break;
    case ROUND_FLOOR: chosen = lower; break;
    case ROUND_EXPAND: chosen = positive ? upper : lower; break;
    case ROUND_TRUNC: chosen = positive ? lower : upper; break;
    default: {
        ejs_i128 abs_r = r < 0 ? -r : r;
        ejs_i128 twice = abs_r * 2;
        if (twice < increment) chosen = positive ? lower : upper;
        else if (twice > increment) chosen = positive ? upper : lower;
        else switch (mode) {
        case ROUND_HALF_CEIL: chosen = upper; break;
        case ROUND_HALF_FLOOR: chosen = lower; break;
        case ROUND_HALF_EXPAND: chosen = positive ? upper : lower; break;
        case ROUND_HALF_TRUNC: chosen = positive ? lower : upper; break;
        default: chosen = (lower % 2 == 0) ? lower : upper; break; // halfEven
        }
        break;
    }
    }
    return chosen * increment;
}

// RoundNumberToIncrementAsIfPositive: negative values round in the
// same lexical direction as positive ones (trunc behaves as floor)
static ejs_i128
round_i128_as_if_positive(ejs_i128 v, ejs_i128 increment, RoundingMode mode)
{
    RoundingMode m = mode;
    switch (mode) {
    case ROUND_TRUNC: m = ROUND_FLOOR; break;
    case ROUND_EXPAND: m = ROUND_CEIL; break;
    case ROUND_HALF_TRUNC: m = ROUND_HALF_FLOOR; break;
    case ROUND_HALF_EXPAND: m = ROUND_HALF_CEIL; break;
    default: break;
    }
    return round_i128_to_increment(v, increment, m);
}

static RoundingMode
get_rounding_mode_option(ejsval options, RoundingMode dflt)
{
    int idx = get_string_option(options, "roundingMode", rounding_mode_names, 9, -1);
    return idx < 0 ? dflt : (RoundingMode)idx;
}

// RegulateISODate + AddISODate
static void
regulate_iso_date(int32_t* y, int32_t* m, int32_t* d, EJSBool constrain)
{
    if (*m < 1 || *m > 12 || *d < 1 || *d > _ejs_temporal_iso_days_in_month(*y, *m > 12 ? 12 : (*m < 1 ? 1 : *m))) {
        if (!constrain)
            _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "date out of range");
        if (*m < 1) *m = 1;
        if (*m > 12) *m = 12;
        int32_t dim = _ejs_temporal_iso_days_in_month(*y, *m);
        if (*d < 1) *d = 1;
        if (*d > dim) *d = dim;
    }
}

// -------------------------------------------------- calendar field bags

static double
to_positive_integer_with_truncation(ejsval v)
{
    double d = to_integer_with_truncation(v);
    if (d <= 0)
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "value must be a positive integer");
    return d;
}

// ToMonthCode: syntax-only validation (M01..M99, optional L suffix;
// M00 only with L).  Calendar-specific rejection happens later so that
// options are read first, as the spec sequences it.
static int32_t
to_month_code(ejsval v, EJSBool* leap)
{
    ejsval prim = EJSVAL_IS_OBJECT(v) ? ToPrimitive(v, TO_PRIM_HINT_STRING) : v;
    if (!EJSVAL_IS_STRING(prim))
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "monthCode must be a string");
    char* s = string_to_ascii(prim);
    if (!s)
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "invalid monthCode");
    int32_t month = -1;
    size_t len = strlen(s);
    *leap = EJS_FALSE;
    if ((len == 3 || len == 4) && s[0] == 'M'
        && s[1] >= '0' && s[1] <= '9' && s[2] >= '0' && s[2] <= '9'
        && (len == 3 || s[3] == 'L')) {
        month = (s[1] - '0') * 10 + (s[2] - '0');
        *leap = (len == 4) ? EJS_TRUE : EJS_FALSE;
        if (month == 0 && !*leap) month = -1; // M00 requires the L suffix
    }
    free(s);
    if (month < 0)
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "invalid monthCode");
    return month;
}

typedef struct FieldsBag {
    EJSBool has_year, has_month, has_month_code, has_day;
    EJSBool month_code_leap;
    EJSBool has_hour, has_minute, has_second, has_ms, has_us, has_ns;
    EJSBool has_offset, has_time_zone;
    double year, month, day;
    int32_t month_code;
    double hour, minute, second, ms, us, ns;
    ejsval offset;      // string
    ejsval time_zone;   // unvalidated value
} FieldsBag;

#define BAG_DATE   (1 << 0)
#define BAG_TIME   (1 << 1)
#define BAG_OFFSET (1 << 2)
#define BAG_TZ     (1 << 3)
#define BAG_YM     (1 << 4)  // year+month(+code) only
#define BAG_MD     (1 << 5)  // month(+code)+day+year

// PrepareCalendarFields: reads the selected fields in alphabetical
// order with per-field conversions
static void
read_fields_bag(ejsval item, int which, FieldsBag* bag)
{
    memset(bag, 0, sizeof(*bag));
    bag->offset = _ejs_undefined;
    bag->time_zone = _ejs_undefined;
    EJSBool want_day = (which & (BAG_DATE | BAG_MD)) != 0;
    EJSBool want_ym = (which & (BAG_DATE | BAG_YM | BAG_MD)) != 0;
    ejsval v;
    if (want_day) {
        v = Get(item, _ejs_string_new_utf8("day"));
        if (!EJSVAL_IS_UNDEFINED(v)) { bag->day = to_positive_integer_with_truncation(v); bag->has_day = EJS_TRUE; }
    }
    if (which & BAG_TIME) {
        v = Get(item, _ejs_string_new_utf8("hour"));
        if (!EJSVAL_IS_UNDEFINED(v)) { bag->hour = to_integer_with_truncation(v); bag->has_hour = EJS_TRUE; }
        v = Get(item, _ejs_string_new_utf8("microsecond"));
        if (!EJSVAL_IS_UNDEFINED(v)) { bag->us = to_integer_with_truncation(v); bag->has_us = EJS_TRUE; }
        v = Get(item, _ejs_string_new_utf8("millisecond"));
        if (!EJSVAL_IS_UNDEFINED(v)) { bag->ms = to_integer_with_truncation(v); bag->has_ms = EJS_TRUE; }
        v = Get(item, _ejs_string_new_utf8("minute"));
        if (!EJSVAL_IS_UNDEFINED(v)) { bag->minute = to_integer_with_truncation(v); bag->has_minute = EJS_TRUE; }
    }
    if (want_ym) {
        v = Get(item, _ejs_string_new_utf8("month"));
        if (!EJSVAL_IS_UNDEFINED(v)) { bag->month = to_positive_integer_with_truncation(v); bag->has_month = EJS_TRUE; }
        v = Get(item, _ejs_string_new_utf8("monthCode"));
        if (!EJSVAL_IS_UNDEFINED(v)) {
            bag->month_code = to_month_code(v, &bag->month_code_leap);
            bag->has_month_code = EJS_TRUE;
        }
    }
    if (which & BAG_TIME) {
        v = Get(item, _ejs_string_new_utf8("nanosecond"));
        if (!EJSVAL_IS_UNDEFINED(v)) { bag->ns = to_integer_with_truncation(v); bag->has_ns = EJS_TRUE; }
    }
    if (which & BAG_OFFSET) {
        v = Get(item, _ejs_string_new_utf8("offset"));
        if (!EJSVAL_IS_UNDEFINED(v)) {
            ejsval prim = EJSVAL_IS_OBJECT(v) ? ToPrimitive(v, TO_PRIM_HINT_STRING) : v;
            if (!EJSVAL_IS_STRING(prim))
                _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "offset must be a string");
            bag->offset = prim;
            bag->has_offset = EJS_TRUE;
        }
    }
    if (which & BAG_TIME) {
        v = Get(item, _ejs_string_new_utf8("second"));
        if (!EJSVAL_IS_UNDEFINED(v)) { bag->second = to_integer_with_truncation(v); bag->has_second = EJS_TRUE; }
    }
    if (which & BAG_TZ) {
        v = Get(item, _ejs_string_new_utf8("timeZone"));
        if (!EJSVAL_IS_UNDEFINED(v)) { bag->time_zone = v; bag->has_time_zone = EJS_TRUE; }
    }
    if (which & (BAG_DATE | BAG_YM | BAG_MD)) {
        v = Get(item, _ejs_string_new_utf8("year"));
        if (!EJSVAL_IS_UNDEFINED(v)) { bag->year = to_integer_with_truncation(v); bag->has_year = EJS_TRUE; }
    }
}

// GetTemporalCalendarIdentifierWithISODefault
static ejsval
calendar_from_item(ejsval item)
{
    if (EJSVAL_IS_TEMPORAL_PLAINDATE(item)) return EJSVAL_TO_TEMPORAL_PLAINDATE(item)->calendar;
    if (EJSVAL_IS_TEMPORAL_PLAINDATETIME(item)) return EJSVAL_TO_TEMPORAL_PLAINDATETIME(item)->calendar;
    if (EJSVAL_IS_TEMPORAL_PLAINYEARMONTH(item)) return EJSVAL_TO_TEMPORAL_PLAINYEARMONTH(item)->calendar;
    if (EJSVAL_IS_TEMPORAL_PLAINMONTHDAY(item)) return EJSVAL_TO_TEMPORAL_PLAINMONTHDAY(item)->calendar;
    if (EJSVAL_IS_TEMPORAL_ZONEDDATETIME(item)) return EJSVAL_TO_TEMPORAL_ZONEDDATETIME(item)->calendar;
    ejsval cal = Get(item, _ejs_string_new_utf8("calendar"));
    if (EJSVAL_IS_UNDEFINED(cal))
        return _ejs_temporal_str_iso8601;
    return to_temporal_calendar_identifier(cal);
}

// ResolveISOMonth: month/monthCode agreement
static int32_t
resolve_iso_month(FieldsBag* bag)
{
    if (bag->has_month_code) {
        // calendar validation: iso8601 has M01..M12, no leap months
        if (bag->month_code_leap || bag->month_code < 1 || bag->month_code > 12)
            _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "monthCode not valid for iso8601");
        if (bag->has_month && (int32_t)bag->month != bag->month_code)
            _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "month and monthCode conflict");
        return bag->month_code;
    }
    if (!bag->has_month)
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "month or monthCode is required");
    if (bag->month > 1000000)
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "month out of range");
    return (int32_t)bag->month;
}

// CalendarDateFromFields (iso)
static void
iso_date_from_bag(FieldsBag* bag, EJSBool constrain, int32_t* y, int32_t* m, int32_t* d)
{
    if (!bag->has_year)
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "year is required");
    if (!bag->has_day)
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "day is required");
    int32_t month = resolve_iso_month(bag);
    if (fabs(bag->year) > 1000000 || bag->day > 1e9)
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "date out of range");
    int32_t iy = (int32_t)bag->year, im = month, id = (int32_t)bag->day;
    regulate_iso_date(&iy, &im, &id, constrain);
    if (!iso_date_within_limits(iy, im, id))
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "date out of range");
    *y = iy; *m = im; *d = id;
}

// RegulateTime over a bag (absent fields are 0)
static void
regulate_time_from_bag(FieldsBag* bag, EJSBool constrain,
                       int32_t* h, int32_t* mi, int32_t* s, int32_t* ms, int32_t* us, int32_t* ns)
{
    double vals[6] = { bag->hour, bag->minute, bag->second, bag->ms, bag->us, bag->ns };
    double maxes[6] = { 23, 59, 59, 999, 999, 999 };
    int32_t out[6];
    for (int i = 0; i < 6; i++) {
        double v = vals[i];
        if (constrain) {
            if (v < 0) v = 0;
            if (v > maxes[i]) v = maxes[i];
        } else if (v < 0 || v > maxes[i]) {
            _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "time out of range");
        }
        out[i] = (int32_t)v;
    }
    *h = out[0]; *mi = out[1]; *s = out[2]; *ms = out[3]; *us = out[4]; *ns = out[5];
}

// IsPartialTemporalObject check for with(): plain object, no brand, no
// calendar/timeZone properties
static void
check_partial_temporal_object(ejsval item)
{
    if (!EJSVAL_IS_OBJECT(item)
        || EJSVAL_IS_TEMPORAL_PLAINDATE(item) || EJSVAL_IS_TEMPORAL_PLAINTIME(item)
        || EJSVAL_IS_TEMPORAL_PLAINDATETIME(item) || EJSVAL_IS_TEMPORAL_PLAINYEARMONTH(item)
        || EJSVAL_IS_TEMPORAL_PLAINMONTHDAY(item) || EJSVAL_IS_TEMPORAL_ZONEDDATETIME(item)
        || EJSVAL_IS_TEMPORAL_INSTANT(item) || EJSVAL_IS_TEMPORAL_DURATION(item))
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "argument must be a plain object of fields");
    if (!EJSVAL_IS_UNDEFINED(Get(item, _ejs_string_new_utf8("calendar"))))
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "argument must not have a calendar property");
    if (!EJSVAL_IS_UNDEFINED(Get(item, _ejs_string_new_utf8("timeZone"))))
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "argument must not have a timeZone property");
}

// require at least one field present
static void
require_any_field(FieldsBag* bag)
{
    if (!bag->has_year && !bag->has_month && !bag->has_month_code && !bag->has_day
        && !bag->has_hour && !bag->has_minute && !bag->has_second
        && !bag->has_ms && !bag->has_us && !bag->has_ns
        && !bag->has_offset && !bag->has_time_zone)
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "at least one field is required");
}

// with() merge helpers: overlay partial onto current fields.  month /
// monthCode pair: any month key in the partial drops both originals.
static void
merge_date_fields(FieldsBag* partial, int32_t year, int32_t month, int32_t day)
{
    if (!partial->has_year) { partial->year = year; partial->has_year = EJS_TRUE; }
    if (!partial->has_month && !partial->has_month_code) {
        partial->month = month;
        partial->has_month = EJS_TRUE;
    }
    if (!partial->has_day) { partial->day = day; partial->has_day = EJS_TRUE; }
}

// -------------------------------------------------------- conversions

// ToTemporalDuration
static void
to_temporal_duration_fields(ejsval item, DurationFields* f)
{
    memset(f, 0, sizeof(*f));
    if (EJSVAL_IS_TEMPORAL_DURATION(item)) {
        EJSTemporalDuration* d = EJSVAL_TO_TEMPORAL_DURATION(item);
        f->years = d->years; f->months = d->months; f->weeks = d->weeks; f->days = d->days;
        f->hours = d->hours; f->minutes = d->minutes; f->seconds = d->seconds;
        f->ms = d->milliseconds; f->us = d->microseconds; f->ns = d->nanoseconds;
        return;
    }
    if (EJSVAL_IS_OBJECT(item)) {
        // ToTemporalPartialDurationRecord: alphabetical reads
        static const char* const names[10] = {
            "days", "hours", "microseconds", "milliseconds", "minutes",
            "months", "nanoseconds", "seconds", "weeks", "years"
        };
        double* dest[10] = { &f->days, &f->hours, &f->us, &f->ms, &f->minutes,
                             &f->months, &f->ns, &f->seconds, &f->weeks, &f->years };
        EJSBool any = EJS_FALSE;
        for (int i = 0; i < 10; i++) {
            ejsval v = Get(item, _ejs_string_new_utf8(names[i]));
            if (!EJSVAL_IS_UNDEFINED(v)) {
                *dest[i] = to_integer_if_integral(v);
                any = EJS_TRUE;
            }
        }
        if (!any)
            _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "invalid duration-like object");
    } else if (EJSVAL_IS_STRING(item)) {
        char* s = string_to_ascii(item);
        int ok = s != NULL && parse_duration_string(s, f);
        free(s);
        if (!ok)
            _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "invalid duration string");
    } else {
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "invalid duration");
    }
    if (!is_valid_duration(f->years, f->months, f->weeks, f->days, f->hours,
                           f->minutes, f->seconds, f->ms, f->us, f->ns))
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "invalid duration");
}

// GetTemporalOverflowOption: EJS_TRUE = constrain (default), EJS_FALSE = reject
static EJSBool
get_overflow_option(ejsval options)
{
    static const char* const values[] = { "constrain", "reject" };
    return get_string_option(options, "overflow", values, 2, 0) == 0 ? EJS_TRUE : EJS_FALSE;
}

// ToTemporalDate; options are read (for side effects and overflow
// validation) per spec ordering
static void
to_temporal_date_fields(ejsval item, ejsval options, int32_t* y, int32_t* m, int32_t* d, ejsval* calendar)
{
    if (EJSVAL_IS_TEMPORAL_PLAINDATE(item)) {
        EJSTemporalPlainDate* pd = EJSVAL_TO_TEMPORAL_PLAINDATE(item);
        get_overflow_option(get_options_object(options));
        *y = pd->year; *m = pd->month; *d = pd->day; *calendar = pd->calendar;
        return;
    }
    if (EJSVAL_IS_TEMPORAL_PLAINDATETIME(item)) {
        EJSTemporalPlainDateTime* pdt = EJSVAL_TO_TEMPORAL_PLAINDATETIME(item);
        get_overflow_option(get_options_object(options));
        *y = pdt->year; *m = pdt->month; *d = pdt->day; *calendar = pdt->calendar;
        return;
    }
    if (EJSVAL_IS_TEMPORAL_ZONEDDATETIME(item)) {
        EJSTemporalZonedDateTime* zdt = EJSVAL_TO_TEMPORAL_ZONEDDATETIME(item);
        ZDTFields f;
        zdt_fields(zdt, &f);
        get_overflow_option(get_options_object(options));
        *y = f.year; *m = f.month; *d = f.day; *calendar = zdt->calendar;
        return;
    }
    if (EJSVAL_IS_OBJECT(item)) {
        ejsval cal = calendar_from_item(item);
        FieldsBag bag;
        read_fields_bag(item, BAG_DATE, &bag);
        EJSBool constrain = get_overflow_option(get_options_object(options));
        iso_date_from_bag(&bag, constrain, y, m, d);
        *calendar = cal;
        return;
    }
    if (!EJSVAL_IS_STRING(item))
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "cannot convert value to Temporal.PlainDate");
    char* s = string_to_ascii(item);
    ParsedISO r;
    int ok = s != NULL && parse_annotated_datetime(s, &r);
    free(s);
    if (!ok || r.offset_z)
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "invalid ISO date string");
    ejsval cal = parsed_calendar(&r);
    get_overflow_option(get_options_object(options));
    if (!iso_date_within_limits(r.year, r.month, r.day))
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "date out of range");
    *y = r.year; *m = r.month; *d = r.day; *calendar = cal;
}

// ToTemporalTime
static void
to_temporal_time_fields(ejsval item, ejsval options, int32_t* h, int32_t* mi, int32_t* s,
                        int32_t* ms, int32_t* us, int32_t* ns)
{
    if (EJSVAL_IS_TEMPORAL_PLAINTIME(item)) {
        EJSTemporalPlainTime* pt = EJSVAL_TO_TEMPORAL_PLAINTIME(item);
        get_overflow_option(get_options_object(options));
        *h = pt->hour; *mi = pt->minute; *s = pt->second;
        *ms = pt->millisecond; *us = pt->microsecond; *ns = pt->nanosecond;
        return;
    }
    if (EJSVAL_IS_TEMPORAL_PLAINDATETIME(item)) {
        EJSTemporalPlainDateTime* pdt = EJSVAL_TO_TEMPORAL_PLAINDATETIME(item);
        get_overflow_option(get_options_object(options));
        *h = pdt->hour; *mi = pdt->minute; *s = pdt->second;
        *ms = pdt->millisecond; *us = pdt->microsecond; *ns = pdt->nanosecond;
        return;
    }
    if (EJSVAL_IS_TEMPORAL_ZONEDDATETIME(item)) {
        EJSTemporalZonedDateTime* zdt = EJSVAL_TO_TEMPORAL_ZONEDDATETIME(item);
        ZDTFields f;
        zdt_fields(zdt, &f);
        get_overflow_option(get_options_object(options));
        *h = f.hour; *mi = f.minute; *s = f.second;
        *ms = f.millisecond; *us = f.microsecond; *ns = f.nanosecond;
        return;
    }
    if (EJSVAL_IS_OBJECT(item)) {
        FieldsBag bag;
        read_fields_bag(item, BAG_TIME, &bag);
        if (!bag.has_hour && !bag.has_minute && !bag.has_second
            && !bag.has_ms && !bag.has_us && !bag.has_ns)
            _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "invalid time-like object");
        EJSBool constrain = get_overflow_option(get_options_object(options));
        regulate_time_from_bag(&bag, constrain, h, mi, s, ms, us, ns);
        return;
    }
    if (!EJSVAL_IS_STRING(item))
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "cannot convert value to Temporal.PlainTime");
    char* str = string_to_ascii(item);
    ParsedISO r;
    int ok = str != NULL && parse_annotated_time(str, &r);
    if (ok && str[0] != 'T' && str[0] != 't') {
        // bare time strings that also parse as month-day or year-month
        // are ambiguous and rejected
        ParsedISO amb;
        if (parse_monthday_only(str, &amb) || parse_yearmonth_only(str, &amb))
            ok = 0;
    }
    if (!ok && str) {
        // a datetime string with a time part also parses as a time
        ok = parse_annotated_datetime(str, &r) && r.has_time;
    }
    free(str);
    if (!ok || r.offset_z)
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "invalid ISO time string");
    // PlainTime carries no calendar: annotation values are ignored
    get_overflow_option(get_options_object(options));
    *h = r.hour; *mi = r.minute; *s = r.second;
    *ms = r.millisecond; *us = r.microsecond; *ns = r.nanosecond;
}

// ToTemporalInstant -> epoch ns
static ejs_i128
to_temporal_instant_ns(ejsval item)
{
    if (EJSVAL_IS_TEMPORAL_INSTANT(item))
        return EJSVAL_TO_TEMPORAL_INSTANT(item)->epoch_ns;
    if (EJSVAL_IS_TEMPORAL_ZONEDDATETIME(item))
        return EJSVAL_TO_TEMPORAL_ZONEDDATETIME(item)->epoch_ns;
    if (EJSVAL_IS_OBJECT(item)) {
        item = ToPrimitive(item, TO_PRIM_HINT_STRING);
    }
    if (!EJSVAL_IS_STRING(item))
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "cannot convert value to Temporal.Instant");
    char* s = string_to_ascii(item);
    ParsedISO r;
    int ok = s != NULL && parse_annotated_datetime(s, &r);
    free(s);
    if (!ok || !r.has_time || !r.has_offset)
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "invalid ISO instant string");
    ejs_i128 ns = iso_datetime_to_epoch_ns(r.year, r.month, r.day, r.hour, r.minute, r.second,
                                           r.millisecond, r.microsecond, r.nanosecond);
    ns -= r.offset_ns;
    if (ns < EJS_TEMPORAL_NS_MIN_INSTANT || ns > EJS_TEMPORAL_NS_MAX_INSTANT)
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "instant out of range");
    return ns;
}

// ---------------------------------------------------- calendar arithmetic

static void
add_iso_date_impl(int32_t y, int32_t m, int32_t d,
                  double years, double months, double weeks, double days,
                  EJSBool constrain, EJSBool check_limits,
                  int32_t* ry, int32_t* rm, int32_t* rd);

static void
add_iso_date(int32_t y, int32_t m, int32_t d,
             double years, double months, double weeks, double days,
             EJSBool constrain,
             int32_t* ry, int32_t* rm, int32_t* rd)
{
    add_iso_date_impl(y, m, d, years, months, weeks, days, constrain, EJS_TRUE, ry, rm, rd);
}

// bound probing during rounding is mathematical: no range enforcement
static void
add_iso_date_unchecked(int32_t y, int32_t m, int32_t d,
                       double years, double months, double weeks, double days,
                       int32_t* ry, int32_t* rm, int32_t* rd)
{
    add_iso_date_impl(y, m, d, years, months, weeks, days, EJS_TRUE, EJS_FALSE, ry, rm, rd);
}

static void
add_iso_date_impl(int32_t y, int32_t m, int32_t d,
                  double years, double months, double weeks, double days,
                  EJSBool constrain, EJSBool check_limits,
                  int32_t* ry, int32_t* rm, int32_t* rd)
{
    double yy = (double)y + years;
    double mm = (double)m + months;
    // balance months into years
    double carry = floor((mm - 1) / 12);
    yy += carry;
    mm -= carry * 12;
    if (yy < -1000000 || yy > 1000000)
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "date out of range");
    int32_t iy = (int32_t)yy, im = (int32_t)mm, id = d;
    regulate_iso_date(&iy, &im, &id, constrain);
    double total_days = weeks * 7 + days;
    if (fabs(total_days) > 1e14)
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "date out of range");
    int64_t epoch_days = _ejs_temporal_iso_date_to_epoch_days(iy, im, id) + (int64_t)total_days;
    int32_t fy, fm, fd;
    _ejs_temporal_epoch_days_to_iso_date(epoch_days, &fy, &fm, &fd);
    if (check_limits && !iso_date_within_limits(fy, fm, fd))
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "date out of range");
    *ry = fy; *rm = fm; *rd = fd;
}

// duration time part as i128 ns (exact: each field ≤ 2^53 in
// magnitude, scaled sums fit well inside 128 bits)
static ejs_i128
duration_time_ns(DurationFields* f)
{
    return ((ejs_i128)f->hours) * 3600000000000LL
        + ((ejs_i128)f->minutes) * 60000000000LL
        + ((ejs_i128)f->seconds) * 1000000000LL
        + ((ejs_i128)f->ms) * 1000000LL
        + ((ejs_i128)f->us) * 1000LL
        + (ejs_i128)f->ns;
}

// ------------------------------------------------- seconds precision

typedef struct {
    int digits;          // 0..9 fixed; -1 = auto; -2 = minute
    ejs_i128 increment;  // ns
    RoundingMode mode;
} SecPrecision;

static const char* const time_unit_names[] = {
    "minute", "minutes", "second", "seconds", "millisecond", "milliseconds",
    "microsecond", "microseconds", "nanosecond", "nanoseconds"
};

// reads fractionalSecondDigits, roundingMode, smallestUnit (spec order)
static void
get_sec_precision(ejsval options, SecPrecision* p)
{
    p->digits = -1;
    p->increment = 1;
    p->mode = ROUND_TRUNC;
    if (EJSVAL_IS_UNDEFINED(options))
        return;
    // GetTemporalFractionalSecondDigitsOption
    ejsval fsd = Get(options, _ejs_string_new_utf8("fractionalSecondDigits"));
    if (!EJSVAL_IS_UNDEFINED(fsd)) {
        if (EJSVAL_IS_NUMBER(fsd)) {
            double d = EJSVAL_TO_NUMBER(fsd);
            if (isnan(d) || !isfinite(d) || floor(d) < 0 || floor(d) > 9)
                _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "fractionalSecondDigits must be 'auto' or 0 through 9");
            p->digits = (int)floor(d);
        } else {
            ejsval str = ToString(fsd);
            if (!string_equals_utf8(str, "auto"))
                _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "fractionalSecondDigits must be 'auto' or 0 through 9");
            p->digits = -1;
        }
    }
    p->mode = get_rounding_mode_option(options, ROUND_TRUNC);
    int su = get_string_option(options, "smallestUnit", time_unit_names, 10, -1);
    if (su >= 0) {
        switch (su / 2) {
        case 0: p->digits = -2; break; // minute
        case 1: p->digits = 0; break;
        case 2: p->digits = 3; break;
        case 3: p->digits = 6; break;
        case 4: p->digits = 9; break;
        }
    }
    if (p->digits == -1) p->increment = 1;
    else if (p->digits == -2) p->increment = 60000000000LL;
    else if (p->digits == 0) p->increment = 1000000000LL;
    else {
        p->increment = 1;
        for (int i = 0; i < 9 - p->digits; i++) p->increment *= 10;
    }
}

// format HH:MM[:SS[.frac]] under a SecPrecision (time already rounded)
static void
format_time_precision(char* buf, size_t bufsize, int32_t h, int32_t mi, int32_t s,
                      int32_t ms, int32_t us, int32_t ns, const SecPrecision* p)
{
    if (p->digits == -2) {
        snprintf(buf, bufsize, "%02d:%02d", h, mi);
        return;
    }
    if (p->digits == -1) {
        format_iso_time_auto(buf, bufsize, h, mi, s, ms, us, ns);
        return;
    }
    if (p->digits == 0) {
        snprintf(buf, bufsize, "%02d:%02d:%02d", h, mi, s);
        return;
    }
    int64_t frac = (int64_t)ms * 1000000 + (int64_t)us * 1000 + ns;
    char fbuf[10];
    snprintf(fbuf, sizeof(fbuf), "%09lld", (long long)frac);
    fbuf[p->digits] = 0;
    snprintf(buf, bufsize, "%02d:%02d:%02d.%s", h, mi, s, fbuf);
}

// calendarName option: 0 auto, 1 always, 2 never, 3 critical
static int
get_calendar_name_option(ejsval options)
{
    static const char* const values[] = { "auto", "always", "never", "critical" };
    return get_string_option(options, "calendarName", values, 4, 0);
}

static void
append_calendar_annotation(char* buf, size_t bufsize, int show)
{
    if (show == 1)
        strlcat(buf, "[u-ca=iso8601]", bufsize);
    else if (show == 3)
        strlcat(buf, "[!u-ca=iso8601]", bufsize);
}

// ------------------------------------------------------- shared valueOf

static EJS_NATIVE_FUNC(_ejs_Temporal_valueOf_impl) {
    _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR,
        "Temporal objects cannot be converted to primitives; use toString() or compare()");
    return _ejs_undefined;
}

#define STUB_METHOD(cls, name)                                          \
    static EJS_NATIVE_FUNC(_ejs_##cls##_##name##_stub) {                \
        throw_not_implemented(#cls ".prototype." #name);                \
        return _ejs_undefined;                                          \
    }

// ------------------------------------------------------------ PlainDate

static EJS_NATIVE_FUNC(_ejs_TemporalPlainDate_from) {
    ejsval item = argc > 0 ? args[0] : _ejs_undefined;
    ejsval options = argc > 1 ? args[1] : _ejs_undefined;
    int32_t y, m, d;
    ejsval calendar;
    to_temporal_date_fields(item, options, &y, &m, &d, &calendar);
    return _ejs_temporal_plain_date_new(y, m, d, calendar);
}

static int
compare_iso_date(int32_t y1, int32_t m1, int32_t d1, int32_t y2, int32_t m2, int32_t d2)
{
    if (y1 != y2) return y1 < y2 ? -1 : 1;
    if (m1 != m2) return m1 < m2 ? -1 : 1;
    if (d1 != d2) return d1 < d2 ? -1 : 1;
    return 0;
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainDate_compare) {
    int32_t y1, m1, d1, y2, m2, d2;
    ejsval cal;
    to_temporal_date_fields(argc > 0 ? args[0] : _ejs_undefined, _ejs_undefined, &y1, &m1, &d1, &cal);
    to_temporal_date_fields(argc > 1 ? args[1] : _ejs_undefined, _ejs_undefined, &y2, &m2, &d2, &cal);
    return NUMBER_TO_EJSVAL((double)compare_iso_date(y1, m1, d1, y2, m2, d2));
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainDate_prototype_equals) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINDATE, "Temporal.PlainDate.prototype.equals");
    EJSTemporalPlainDate* pd = EJSVAL_TO_TEMPORAL_PLAINDATE(*_this);
    int32_t y, m, d;
    ejsval cal;
    to_temporal_date_fields(argc > 0 ? args[0] : _ejs_undefined, _ejs_undefined, &y, &m, &d, &cal);
    return BOOLEAN_TO_EJSVAL(pd->year == y && pd->month == m && pd->day == d
                             && SameValue(pd->calendar, cal));
}

static void
plaindate_to_string_buf(EJSTemporalPlainDate* pd, int show_cal, char* buf, size_t bufsize)
{
    format_iso_date(buf, bufsize, pd->year, pd->month, pd->day);
    append_calendar_annotation(buf, bufsize, show_cal);
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainDate_prototype_toString) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINDATE, "Temporal.PlainDate.prototype.toString");
    ejsval options = get_options_object(argc > 0 ? args[0] : _ejs_undefined);
    int show = get_calendar_name_option(options);
    char buf[64];
    plaindate_to_string_buf(EJSVAL_TO_TEMPORAL_PLAINDATE(*_this), show, buf, sizeof(buf));
    return _ejs_string_new_utf8(buf);
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainDate_prototype_toJSON) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINDATE, "Temporal.PlainDate.prototype.toJSON");
    char buf[64];
    plaindate_to_string_buf(EJSVAL_TO_TEMPORAL_PLAINDATE(*_this), 0, buf, sizeof(buf));
    return _ejs_string_new_utf8(buf);
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainDate_prototype_toLocaleString) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINDATE, "Temporal.PlainDate.prototype.toLocaleString");
    char buf[64];
    plaindate_to_string_buf(EJSVAL_TO_TEMPORAL_PLAINDATE(*_this), 0, buf, sizeof(buf));
    return _ejs_string_new_utf8(buf);
}

static ejsval
plaindate_add_impl(ejsval date, ejsval duration_like, ejsval options, int negate)
{
    EJSTemporalPlainDate* pd = EJSVAL_TO_TEMPORAL_PLAINDATE(date);
    DurationFields f;
    to_temporal_duration_fields(duration_like, &f);
    if (negate) {
        f.years = -f.years; f.months = -f.months; f.weeks = -f.weeks; f.days = -f.days;
        f.hours = -f.hours; f.minutes = -f.minutes; f.seconds = -f.seconds;
        f.ms = -f.ms; f.us = -f.us; f.ns = -f.ns;
    }
    EJSBool constrain = get_overflow_option(get_options_object(options));
    // time part collapses into days (truncated)
    ejs_i128 time_ns = duration_time_ns(&f);
    double extra_days = (double)(int64_t)(time_ns / NS_PER_DAY);
    int32_t ry, rm, rd;
    add_iso_date(pd->year, pd->month, pd->day, f.years, f.months, f.weeks, f.days + extra_days,
                 constrain, &ry, &rm, &rd);
    return _ejs_temporal_plain_date_new(ry, rm, rd, pd->calendar);
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainDate_prototype_add) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINDATE, "Temporal.PlainDate.prototype.add");
    return plaindate_add_impl(*_this, argc > 0 ? args[0] : _ejs_undefined,
                              argc > 1 ? args[1] : _ejs_undefined, 0);
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainDate_prototype_subtract) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINDATE, "Temporal.PlainDate.prototype.subtract");
    return plaindate_add_impl(*_this, argc > 0 ? args[0] : _ejs_undefined,
                              argc > 1 ? args[1] : _ejs_undefined, 1);
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainDate_prototype_withCalendar) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINDATE, "Temporal.PlainDate.prototype.withCalendar");
    EJSTemporalPlainDate* pd = EJSVAL_TO_TEMPORAL_PLAINDATE(*_this);
    ejsval cal = to_temporal_calendar_identifier(argc > 0 ? args[0] : _ejs_undefined);
    if (argc == 0 || EJSVAL_IS_UNDEFINED(args[0]))
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "calendar is required");
    return _ejs_temporal_plain_date_new(pd->year, pd->month, pd->day, cal);
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainDate_prototype_toPlainDateTime) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINDATE, "Temporal.PlainDate.prototype.toPlainDateTime");
    EJSTemporalPlainDate* pd = EJSVAL_TO_TEMPORAL_PLAINDATE(*_this);
    ejsval t = argc > 0 ? args[0] : _ejs_undefined;
    int32_t h = 0, mi = 0, s = 0, ms = 0, us = 0, ns = 0;
    if (!EJSVAL_IS_UNDEFINED(t))
        to_temporal_time_fields(t, _ejs_undefined, &h, &mi, &s, &ms, &us, &ns);
    if (!_ejs_temporal_iso_datetime_within_limits(pd->year, pd->month, pd->day, h, mi, s, ms, us, ns))
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "datetime out of range");
    return _ejs_temporal_plain_datetime_new(pd->year, pd->month, pd->day, h, mi, s, ms, us, ns, pd->calendar);
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainDate_prototype_toPlainYearMonth) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINDATE, "Temporal.PlainDate.prototype.toPlainYearMonth");
    EJSTemporalPlainDate* pd = EJSVAL_TO_TEMPORAL_PLAINDATE(*_this);
    return _ejs_temporal_plain_yearmonth_new(pd->year, pd->month, 1, pd->calendar);
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainDate_prototype_toPlainMonthDay) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINDATE, "Temporal.PlainDate.prototype.toPlainMonthDay");
    EJSTemporalPlainDate* pd = EJSVAL_TO_TEMPORAL_PLAINDATE(*_this);
    return _ejs_temporal_plain_monthday_new(pd->month, pd->day, 1972, pd->calendar);
}


// ------------------------------------------------------------ PlainTime

static EJS_NATIVE_FUNC(_ejs_TemporalPlainTime_from) {
    ejsval item = argc > 0 ? args[0] : _ejs_undefined;
    ejsval options = argc > 1 ? args[1] : _ejs_undefined;
    int32_t h, mi, s, ms, us, ns;
    to_temporal_time_fields(item, options, &h, &mi, &s, &ms, &us, &ns);
    return _ejs_temporal_plain_time_new(h, mi, s, ms, us, ns);
}

static ejs_i128
time_fields_to_ns(int32_t h, int32_t mi, int32_t s, int32_t ms, int32_t us, int32_t ns)
{
    return ((ejs_i128)h) * 3600000000000LL + ((ejs_i128)mi) * 60000000000LL
        + ((ejs_i128)s) * 1000000000LL + ((ejs_i128)ms) * 1000000LL
        + ((ejs_i128)us) * 1000LL + (ejs_i128)ns;
}

static void
ns_to_time_fields(ejs_i128 t, int32_t* h, int32_t* mi, int32_t* s, int32_t* ms, int32_t* us, int32_t* ns)
{
    *h = (int32_t)(t / 3600000000000LL); t %= 3600000000000LL;
    *mi = (int32_t)(t / 60000000000LL); t %= 60000000000LL;
    *s = (int32_t)(t / 1000000000LL); t %= 1000000000LL;
    *ms = (int32_t)(t / 1000000LL); t %= 1000000LL;
    *us = (int32_t)(t / 1000LL);
    *ns = (int32_t)(t % 1000LL);
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainTime_compare) {
    int32_t h1, mi1, s1, ms1, us1, ns1, h2, mi2, s2, ms2, us2, ns2;
    to_temporal_time_fields(argc > 0 ? args[0] : _ejs_undefined, _ejs_undefined, &h1, &mi1, &s1, &ms1, &us1, &ns1);
    to_temporal_time_fields(argc > 1 ? args[1] : _ejs_undefined, _ejs_undefined, &h2, &mi2, &s2, &ms2, &us2, &ns2);
    ejs_i128 a = time_fields_to_ns(h1, mi1, s1, ms1, us1, ns1);
    ejs_i128 b = time_fields_to_ns(h2, mi2, s2, ms2, us2, ns2);
    return NUMBER_TO_EJSVAL(a < b ? -1.0 : (a > b ? 1.0 : 0.0));
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainTime_prototype_equals) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINTIME, "Temporal.PlainTime.prototype.equals");
    EJSTemporalPlainTime* pt = EJSVAL_TO_TEMPORAL_PLAINTIME(*_this);
    int32_t h, mi, s, ms, us, ns;
    to_temporal_time_fields(argc > 0 ? args[0] : _ejs_undefined, _ejs_undefined, &h, &mi, &s, &ms, &us, &ns);
    return BOOLEAN_TO_EJSVAL(pt->hour == h && pt->minute == mi && pt->second == s
                             && pt->millisecond == ms && pt->microsecond == us && pt->nanosecond == ns);
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainTime_prototype_toString) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINTIME, "Temporal.PlainTime.prototype.toString");
    EJSTemporalPlainTime* pt = EJSVAL_TO_TEMPORAL_PLAINTIME(*_this);
    ejsval options = get_options_object(argc > 0 ? args[0] : _ejs_undefined);
    SecPrecision p;
    get_sec_precision(options, &p);
    ejs_i128 t = time_fields_to_ns(pt->hour, pt->minute, pt->second, pt->millisecond, pt->microsecond, pt->nanosecond);
    t = round_i128_to_increment(t, p.increment, p.mode);
    t = i128_floormod(t, NS_PER_DAY);
    int32_t h, mi, s, ms, us, ns;
    ns_to_time_fields(t, &h, &mi, &s, &ms, &us, &ns);
    char buf[64];
    format_time_precision(buf, sizeof(buf), h, mi, s, ms, us, ns, &p);
    return _ejs_string_new_utf8(buf);
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainTime_prototype_toJSON) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINTIME, "Temporal.PlainTime.prototype.toJSON");
    EJSTemporalPlainTime* pt = EJSVAL_TO_TEMPORAL_PLAINTIME(*_this);
    char buf[64];
    format_iso_time_auto(buf, sizeof(buf), pt->hour, pt->minute, pt->second,
                         pt->millisecond, pt->microsecond, pt->nanosecond);
    return _ejs_string_new_utf8(buf);
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainTime_prototype_toLocaleString) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINTIME, "Temporal.PlainTime.prototype.toLocaleString");
    EJSTemporalPlainTime* pt = EJSVAL_TO_TEMPORAL_PLAINTIME(*_this);
    char buf[64];
    format_iso_time_auto(buf, sizeof(buf), pt->hour, pt->minute, pt->second,
                         pt->millisecond, pt->microsecond, pt->nanosecond);
    return _ejs_string_new_utf8(buf);
}

static ejsval
plaintime_add_impl(ejsval time, ejsval duration_like, int negate)
{
    EJSTemporalPlainTime* pt = EJSVAL_TO_TEMPORAL_PLAINTIME(time);
    DurationFields f;
    to_temporal_duration_fields(duration_like, &f);
    ejs_i128 delta = duration_time_ns(&f);
    // days and larger are ignored mod-24h?  no: time-only arithmetic
    // wraps, and date units contribute nothing to the time of day
    if (negate) delta = -delta;
    ejs_i128 t = time_fields_to_ns(pt->hour, pt->minute, pt->second, pt->millisecond, pt->microsecond, pt->nanosecond);
    t = i128_floormod(t + i128_floormod(delta, NS_PER_DAY) , NS_PER_DAY);
    int32_t h, mi, s, ms, us, ns;
    ns_to_time_fields(t, &h, &mi, &s, &ms, &us, &ns);
    return _ejs_temporal_plain_time_new(h, mi, s, ms, us, ns);
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainTime_prototype_add) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINTIME, "Temporal.PlainTime.prototype.add");
    return plaintime_add_impl(*_this, argc > 0 ? args[0] : _ejs_undefined, 0);
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainTime_prototype_subtract) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINTIME, "Temporal.PlainTime.prototype.subtract");
    return plaintime_add_impl(*_this, argc > 0 ? args[0] : _ejs_undefined, 1);
}


// -------------------------------------------------------- PlainDateTime

// ToTemporalDateTime
static void
to_temporal_datetime_fields(ejsval item, ejsval options,
                            int32_t* y, int32_t* m, int32_t* d,
                            int32_t* h, int32_t* mi, int32_t* s,
                            int32_t* ms, int32_t* us, int32_t* ns, ejsval* calendar)
{
    if (EJSVAL_IS_TEMPORAL_PLAINDATETIME(item)) {
        EJSTemporalPlainDateTime* pdt = EJSVAL_TO_TEMPORAL_PLAINDATETIME(item);
        get_overflow_option(get_options_object(options));
        *y = pdt->year; *m = pdt->month; *d = pdt->day;
        *h = pdt->hour; *mi = pdt->minute; *s = pdt->second;
        *ms = pdt->millisecond; *us = pdt->microsecond; *ns = pdt->nanosecond;
        *calendar = pdt->calendar;
        return;
    }
    if (EJSVAL_IS_TEMPORAL_PLAINDATE(item)) {
        EJSTemporalPlainDate* pd = EJSVAL_TO_TEMPORAL_PLAINDATE(item);
        get_overflow_option(get_options_object(options));
        *y = pd->year; *m = pd->month; *d = pd->day;
        *h = *mi = *s = *ms = *us = *ns = 0;
        *calendar = pd->calendar;
        return;
    }
    if (EJSVAL_IS_TEMPORAL_ZONEDDATETIME(item)) {
        EJSTemporalZonedDateTime* zdt = EJSVAL_TO_TEMPORAL_ZONEDDATETIME(item);
        ZDTFields f;
        zdt_fields(zdt, &f);
        get_overflow_option(get_options_object(options));
        *y = f.year; *m = f.month; *d = f.day;
        *h = f.hour; *mi = f.minute; *s = f.second;
        *ms = f.millisecond; *us = f.microsecond; *ns = f.nanosecond;
        *calendar = zdt->calendar;
        return;
    }
    if (EJSVAL_IS_OBJECT(item)) {
        ejsval cal = calendar_from_item(item);
        FieldsBag bag;
        read_fields_bag(item, BAG_DATE | BAG_TIME, &bag);
        EJSBool constrain = get_overflow_option(get_options_object(options));
        iso_date_from_bag(&bag, constrain, y, m, d);
        regulate_time_from_bag(&bag, constrain, h, mi, s, ms, us, ns);
        if (!_ejs_temporal_iso_datetime_within_limits(*y, *m, *d, *h, *mi, *s, *ms, *us, *ns))
            _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "datetime out of range");
        *calendar = cal;
        return;
    }
    if (!EJSVAL_IS_STRING(item))
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "cannot convert value to Temporal.PlainDateTime");
    char* str = string_to_ascii(item);
    ParsedISO r;
    int ok = str != NULL && parse_annotated_datetime(str, &r);
    free(str);
    if (!ok || r.offset_z)
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "invalid ISO datetime string");
    ejsval cal = parsed_calendar(&r);
    get_overflow_option(get_options_object(options));
    if (!_ejs_temporal_iso_datetime_within_limits(r.year, r.month, r.day, r.hour, r.minute, r.second,
                                                  r.millisecond, r.microsecond, r.nanosecond))
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "datetime out of range");
    *y = r.year; *m = r.month; *d = r.day;
    *h = r.hour; *mi = r.minute; *s = r.second;
    *ms = r.millisecond; *us = r.microsecond; *ns = r.nanosecond;
    *calendar = cal;
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainDateTime_from) {
    int32_t y, m, d, h, mi, s, ms, us, ns;
    ejsval cal;
    to_temporal_datetime_fields(argc > 0 ? args[0] : _ejs_undefined,
                                argc > 1 ? args[1] : _ejs_undefined,
                                &y, &m, &d, &h, &mi, &s, &ms, &us, &ns, &cal);
    return _ejs_temporal_plain_datetime_new(y, m, d, h, mi, s, ms, us, ns, cal);
}

static int
compare_iso_datetime(int32_t y1, int32_t mo1, int32_t d1, ejs_i128 t1,
                     int32_t y2, int32_t mo2, int32_t d2, ejs_i128 t2)
{
    int c = compare_iso_date(y1, mo1, d1, y2, mo2, d2);
    if (c != 0) return c;
    if (t1 != t2) return t1 < t2 ? -1 : 1;
    return 0;
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainDateTime_compare) {
    int32_t y1, m1, d1, h1, mi1, s1, ms1, us1, ns1;
    int32_t y2, m2, d2, h2, mi2, s2, ms2, us2, ns2;
    ejsval cal;
    to_temporal_datetime_fields(argc > 0 ? args[0] : _ejs_undefined, _ejs_undefined,
                                &y1, &m1, &d1, &h1, &mi1, &s1, &ms1, &us1, &ns1, &cal);
    to_temporal_datetime_fields(argc > 1 ? args[1] : _ejs_undefined, _ejs_undefined,
                                &y2, &m2, &d2, &h2, &mi2, &s2, &ms2, &us2, &ns2, &cal);
    return NUMBER_TO_EJSVAL((double)compare_iso_datetime(
        y1, m1, d1, time_fields_to_ns(h1, mi1, s1, ms1, us1, ns1),
        y2, m2, d2, time_fields_to_ns(h2, mi2, s2, ms2, us2, ns2)));
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainDateTime_prototype_equals) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINDATETIME, "Temporal.PlainDateTime.prototype.equals");
    EJSTemporalPlainDateTime* pdt = EJSVAL_TO_TEMPORAL_PLAINDATETIME(*_this);
    int32_t y, m, d, h, mi, s, ms, us, ns;
    ejsval cal;
    to_temporal_datetime_fields(argc > 0 ? args[0] : _ejs_undefined, _ejs_undefined,
                                &y, &m, &d, &h, &mi, &s, &ms, &us, &ns, &cal);
    return BOOLEAN_TO_EJSVAL(pdt->year == y && pdt->month == m && pdt->day == d
                             && pdt->hour == h && pdt->minute == mi && pdt->second == s
                             && pdt->millisecond == ms && pdt->microsecond == us && pdt->nanosecond == ns
                             && SameValue(pdt->calendar, cal));
}

static void
plaindatetime_to_string_buf(int32_t y, int32_t mo, int32_t d,
                            int32_t h, int32_t mi, int32_t s, int32_t ms, int32_t us, int32_t ns,
                            const SecPrecision* p, int show_cal, char* buf, size_t bufsize)
{
    char dbuf[32], tbuf[32];
    format_iso_date(dbuf, sizeof(dbuf), y, mo, d);
    format_time_precision(tbuf, sizeof(tbuf), h, mi, s, ms, us, ns, p);
    snprintf(buf, bufsize, "%s"
             "T%s", dbuf, tbuf);
    append_calendar_annotation(buf, bufsize, show_cal);
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainDateTime_prototype_toString) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINDATETIME, "Temporal.PlainDateTime.prototype.toString");
    EJSTemporalPlainDateTime* pdt = EJSVAL_TO_TEMPORAL_PLAINDATETIME(*_this);
    ejsval options = get_options_object(argc > 0 ? args[0] : _ejs_undefined);
    int show_cal = get_calendar_name_option(options);
    SecPrecision p;
    get_sec_precision(options, &p);
    // round the full datetime so carries propagate into the date
    int64_t days = _ejs_temporal_iso_date_to_epoch_days(pdt->year, pdt->month, pdt->day);
    ejs_i128 total = ((ejs_i128)days) * NS_PER_DAY
        + time_fields_to_ns(pdt->hour, pdt->minute, pdt->second, pdt->millisecond, pdt->microsecond, pdt->nanosecond);
    total = round_i128_as_if_positive(total, p.increment, p.mode);
    int32_t y, mo, d, h, mi, s, ms, us, ns;
    epoch_ns_to_iso_fields(total, 0, &y, &mo, &d, &h, &mi, &s, &ms, &us, &ns);
    if (!_ejs_temporal_iso_datetime_within_limits(y, mo, d, h, mi, s, ms, us, ns))
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "datetime out of range");
    char buf[96];
    plaindatetime_to_string_buf(y, mo, d, h, mi, s, ms, us, ns, &p, show_cal, buf, sizeof(buf));
    return _ejs_string_new_utf8(buf);
}

static ejsval
plaindatetime_default_string(EJSTemporalPlainDateTime* pdt)
{
    SecPrecision p = { .digits = -1, .increment = 1, .mode = ROUND_TRUNC };
    char buf[96];
    plaindatetime_to_string_buf(pdt->year, pdt->month, pdt->day, pdt->hour, pdt->minute, pdt->second,
                                pdt->millisecond, pdt->microsecond, pdt->nanosecond, &p, 0, buf, sizeof(buf));
    return _ejs_string_new_utf8(buf);
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainDateTime_prototype_toJSON) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINDATETIME, "Temporal.PlainDateTime.prototype.toJSON");
    return plaindatetime_default_string(EJSVAL_TO_TEMPORAL_PLAINDATETIME(*_this));
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainDateTime_prototype_toLocaleString) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINDATETIME, "Temporal.PlainDateTime.prototype.toLocaleString");
    return plaindatetime_default_string(EJSVAL_TO_TEMPORAL_PLAINDATETIME(*_this));
}

static ejsval
plaindatetime_add_impl(ejsval datetime, ejsval duration_like, ejsval options, int negate)
{
    EJSTemporalPlainDateTime* pdt = EJSVAL_TO_TEMPORAL_PLAINDATETIME(datetime);
    DurationFields f;
    to_temporal_duration_fields(duration_like, &f);
    if (negate) {
        f.years = -f.years; f.months = -f.months; f.weeks = -f.weeks; f.days = -f.days;
        f.hours = -f.hours; f.minutes = -f.minutes; f.seconds = -f.seconds;
        f.ms = -f.ms; f.us = -f.us; f.ns = -f.ns;
    }
    EJSBool constrain = get_overflow_option(get_options_object(options));
    // add time first, carrying whole days into the date addition
    ejs_i128 t = time_fields_to_ns(pdt->hour, pdt->minute, pdt->second,
                                   pdt->millisecond, pdt->microsecond, pdt->nanosecond)
        + duration_time_ns(&f);
    ejs_i128 day_carry = i128_floordiv(t, NS_PER_DAY);
    ejs_i128 t_norm = i128_floormod(t, NS_PER_DAY);
    int32_t ry, rm, rd;
    add_iso_date(pdt->year, pdt->month, pdt->day, f.years, f.months, f.weeks,
                 f.days + (double)(int64_t)day_carry, constrain, &ry, &rm, &rd);
    int32_t h, mi, s, ms, us, ns;
    ns_to_time_fields(t_norm, &h, &mi, &s, &ms, &us, &ns);
    if (!_ejs_temporal_iso_datetime_within_limits(ry, rm, rd, h, mi, s, ms, us, ns))
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "datetime out of range");
    return _ejs_temporal_plain_datetime_new(ry, rm, rd, h, mi, s, ms, us, ns, pdt->calendar);
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainDateTime_prototype_add) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINDATETIME, "Temporal.PlainDateTime.prototype.add");
    return plaindatetime_add_impl(*_this, argc > 0 ? args[0] : _ejs_undefined,
                                  argc > 1 ? args[1] : _ejs_undefined, 0);
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainDateTime_prototype_subtract) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINDATETIME, "Temporal.PlainDateTime.prototype.subtract");
    return plaindatetime_add_impl(*_this, argc > 0 ? args[0] : _ejs_undefined,
                                  argc > 1 ? args[1] : _ejs_undefined, 1);
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainDateTime_prototype_withPlainTime) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINDATETIME, "Temporal.PlainDateTime.prototype.withPlainTime");
    EJSTemporalPlainDateTime* pdt = EJSVAL_TO_TEMPORAL_PLAINDATETIME(*_this);
    ejsval t = argc > 0 ? args[0] : _ejs_undefined;
    int32_t h = 0, mi = 0, s = 0, ms = 0, us = 0, ns = 0;
    if (!EJSVAL_IS_UNDEFINED(t))
        to_temporal_time_fields(t, _ejs_undefined, &h, &mi, &s, &ms, &us, &ns);
    if (!_ejs_temporal_iso_datetime_within_limits(pdt->year, pdt->month, pdt->day, h, mi, s, ms, us, ns))
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "datetime out of range");
    return _ejs_temporal_plain_datetime_new(pdt->year, pdt->month, pdt->day, h, mi, s, ms, us, ns, pdt->calendar);
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainDateTime_prototype_withCalendar) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINDATETIME, "Temporal.PlainDateTime.prototype.withCalendar");
    EJSTemporalPlainDateTime* pdt = EJSVAL_TO_TEMPORAL_PLAINDATETIME(*_this);
    if (argc == 0 || EJSVAL_IS_UNDEFINED(args[0]))
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "calendar is required");
    ejsval cal = to_temporal_calendar_identifier(args[0]);
    return _ejs_temporal_plain_datetime_new(pdt->year, pdt->month, pdt->day, pdt->hour, pdt->minute,
                                            pdt->second, pdt->millisecond, pdt->microsecond, pdt->nanosecond, cal);
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainDateTime_prototype_toPlainDate) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINDATETIME, "Temporal.PlainDateTime.prototype.toPlainDate");
    EJSTemporalPlainDateTime* pdt = EJSVAL_TO_TEMPORAL_PLAINDATETIME(*_this);
    return _ejs_temporal_plain_date_new(pdt->year, pdt->month, pdt->day, pdt->calendar);
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainDateTime_prototype_toPlainTime) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINDATETIME, "Temporal.PlainDateTime.prototype.toPlainTime");
    EJSTemporalPlainDateTime* pdt = EJSVAL_TO_TEMPORAL_PLAINDATETIME(*_this);
    return _ejs_temporal_plain_time_new(pdt->hour, pdt->minute, pdt->second,
                                        pdt->millisecond, pdt->microsecond, pdt->nanosecond);
}


// ------------------------------------------------------- PlainYearMonth

static void
to_temporal_yearmonth_fields(ejsval item, ejsval options, int32_t* y, int32_t* m, int32_t* rd, ejsval* calendar)
{
    if (EJSVAL_IS_TEMPORAL_PLAINYEARMONTH(item)) {
        EJSTemporalPlainYearMonth* ym = EJSVAL_TO_TEMPORAL_PLAINYEARMONTH(item);
        get_overflow_option(get_options_object(options));
        *y = ym->year; *m = ym->month; *rd = ym->ref_day; *calendar = ym->calendar;
        return;
    }
    if (EJSVAL_IS_OBJECT(item)) {
        ejsval cal = calendar_from_item(item);
        FieldsBag bag;
        read_fields_bag(item, BAG_YM, &bag);
        if (!bag.has_year)
            _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "year is required");
        EJSBool constrain = get_overflow_option(get_options_object(options));
        int32_t month = resolve_iso_month(&bag);
        if (fabs(bag.year) > 1000000)
            _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "year-month out of range");
        int32_t iy = (int32_t)bag.year, im = month, id = 1;
        regulate_iso_date(&iy, &im, &id, constrain);
        if (!iso_yearmonth_within_limits(iy, im))
            _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "year-month out of range");
        *y = iy; *m = im; *rd = 1; *calendar = cal;
        return;
    }
    if (!EJSVAL_IS_STRING(item))
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "cannot convert value to Temporal.PlainYearMonth");
    char* s = string_to_ascii(item);
    ParsedISO r;
    int ok = s != NULL && parse_yearmonth_only(s, &r);
    if (!ok && s)
        ok = parse_annotated_datetime(s, &r) && !r.offset_z;
    free(s);
    if (!ok)
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "invalid ISO year-month string");
    ejsval cal = parsed_calendar(&r);
    get_overflow_option(get_options_object(options));
    if (!iso_yearmonth_within_limits(r.year, r.month))
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "year-month out of range");
    // string-sourced year-months normalize to reference day 1
    *y = r.year; *m = r.month; *rd = 1; *calendar = cal;
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainYearMonth_from) {
    int32_t y, m, rd;
    ejsval cal;
    to_temporal_yearmonth_fields(argc > 0 ? args[0] : _ejs_undefined,
                                 argc > 1 ? args[1] : _ejs_undefined, &y, &m, &rd, &cal);
    return _ejs_temporal_plain_yearmonth_new(y, m, rd, cal);
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainYearMonth_compare) {
    int32_t y1, m1, rd1, y2, m2, rd2;
    ejsval cal;
    to_temporal_yearmonth_fields(argc > 0 ? args[0] : _ejs_undefined, _ejs_undefined, &y1, &m1, &rd1, &cal);
    to_temporal_yearmonth_fields(argc > 1 ? args[1] : _ejs_undefined, _ejs_undefined, &y2, &m2, &rd2, &cal);
    return NUMBER_TO_EJSVAL((double)compare_iso_date(y1, m1, rd1, y2, m2, rd2));
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainYearMonth_prototype_equals) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINYEARMONTH, "Temporal.PlainYearMonth.prototype.equals");
    EJSTemporalPlainYearMonth* ym = EJSVAL_TO_TEMPORAL_PLAINYEARMONTH(*_this);
    int32_t y, m, rd;
    ejsval cal;
    to_temporal_yearmonth_fields(argc > 0 ? args[0] : _ejs_undefined, _ejs_undefined, &y, &m, &rd, &cal);
    return BOOLEAN_TO_EJSVAL(ym->year == y && ym->month == m && ym->ref_day == rd
                             && SameValue(ym->calendar, cal));
}

static void
yearmonth_to_string_buf(EJSTemporalPlainYearMonth* ym, int show_cal, char* buf, size_t bufsize)
{
    char ybuf[16];
    format_iso_year(ybuf, sizeof(ybuf), ym->year);
    if (show_cal == 1 || show_cal == 3)
        snprintf(buf, bufsize, "%s-%02d-%02d", ybuf, ym->month, ym->ref_day);
    else
        snprintf(buf, bufsize, "%s-%02d", ybuf, ym->month);
    append_calendar_annotation(buf, bufsize, show_cal);
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainYearMonth_prototype_toString) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINYEARMONTH, "Temporal.PlainYearMonth.prototype.toString");
    ejsval options = get_options_object(argc > 0 ? args[0] : _ejs_undefined);
    int show = get_calendar_name_option(options);
    char buf[64];
    yearmonth_to_string_buf(EJSVAL_TO_TEMPORAL_PLAINYEARMONTH(*_this), show, buf, sizeof(buf));
    return _ejs_string_new_utf8(buf);
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainYearMonth_prototype_toJSON) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINYEARMONTH, "Temporal.PlainYearMonth.prototype.toJSON");
    char buf[64];
    yearmonth_to_string_buf(EJSVAL_TO_TEMPORAL_PLAINYEARMONTH(*_this), 0, buf, sizeof(buf));
    return _ejs_string_new_utf8(buf);
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainYearMonth_prototype_toLocaleString) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINYEARMONTH, "Temporal.PlainYearMonth.prototype.toLocaleString");
    char buf[64];
    yearmonth_to_string_buf(EJSVAL_TO_TEMPORAL_PLAINYEARMONTH(*_this), 0, buf, sizeof(buf));
    return _ejs_string_new_utf8(buf);
}


// -------------------------------------------------------- PlainMonthDay

static void
to_temporal_monthday_fields(ejsval item, ejsval options, int32_t* m, int32_t* d, int32_t* ry, ejsval* calendar)
{
    if (EJSVAL_IS_TEMPORAL_PLAINMONTHDAY(item)) {
        EJSTemporalPlainMonthDay* md = EJSVAL_TO_TEMPORAL_PLAINMONTHDAY(item);
        get_overflow_option(get_options_object(options));
        *m = md->month; *d = md->day; *ry = md->ref_year; *calendar = md->calendar;
        return;
    }
    if (EJSVAL_IS_OBJECT(item)) {
        ejsval cal = calendar_from_item(item);
        FieldsBag bag;
        read_fields_bag(item, BAG_MD, &bag);
        if (!bag.has_day)
            _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "day is required");
        EJSBool constrain = get_overflow_option(get_options_object(options));
        int32_t month = resolve_iso_month(&bag);
        // a provided year governs clamping (even with monthCode);
        // without one, clamp by the leap-capable reference year
        int32_t iy = bag.has_year ? (int32_t)bag.year : 1972;
        int32_t im = month, id = (int32_t)bag.day;
        if (bag.day > 1e9)
            _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "day out of range");
        regulate_iso_date(&iy, &im, &id, constrain);
        *m = im; *d = id; *ry = 1972; *calendar = cal;
        return;
    }
    if (!EJSVAL_IS_STRING(item))
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "cannot convert value to Temporal.PlainMonthDay");
    char* s = string_to_ascii(item);
    ParsedISO r;
    int ok = s != NULL && parse_monthday_only(s, &r);
    EJSBool md_form = ok;
    if (!ok && s)
        ok = parse_annotated_datetime(s, &r) && !r.offset_z;
    free(s);
    if (!ok)
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "invalid ISO month-day string");
    ejsval cal = parsed_calendar(&r);
    get_overflow_option(get_options_object(options));
    *m = r.month; *d = r.day;
    *ry = md_form ? 1972 : r.year;
    // iso8601 reference year is always 1972
    *ry = 1972;
    *calendar = cal;
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainMonthDay_from) {
    int32_t m, d, ry;
    ejsval cal;
    to_temporal_monthday_fields(argc > 0 ? args[0] : _ejs_undefined,
                                argc > 1 ? args[1] : _ejs_undefined, &m, &d, &ry, &cal);
    return _ejs_temporal_plain_monthday_new(m, d, ry, cal);
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainMonthDay_prototype_equals) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINMONTHDAY, "Temporal.PlainMonthDay.prototype.equals");
    EJSTemporalPlainMonthDay* md = EJSVAL_TO_TEMPORAL_PLAINMONTHDAY(*_this);
    int32_t m, d, ry;
    ejsval cal;
    to_temporal_monthday_fields(argc > 0 ? args[0] : _ejs_undefined, _ejs_undefined, &m, &d, &ry, &cal);
    return BOOLEAN_TO_EJSVAL(md->month == m && md->day == d && md->ref_year == ry
                             && SameValue(md->calendar, cal));
}

static void
monthday_to_string_buf(EJSTemporalPlainMonthDay* md, int show_cal, char* buf, size_t bufsize)
{
    if (show_cal == 1 || show_cal == 3) {
        char ybuf[16];
        format_iso_year(ybuf, sizeof(ybuf), md->ref_year);
        snprintf(buf, bufsize, "%s-%02d-%02d", ybuf, md->month, md->day);
    } else {
        snprintf(buf, bufsize, "%02d-%02d", md->month, md->day);
    }
    append_calendar_annotation(buf, bufsize, show_cal);
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainMonthDay_prototype_toString) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINMONTHDAY, "Temporal.PlainMonthDay.prototype.toString");
    ejsval options = get_options_object(argc > 0 ? args[0] : _ejs_undefined);
    int show = get_calendar_name_option(options);
    char buf[64];
    monthday_to_string_buf(EJSVAL_TO_TEMPORAL_PLAINMONTHDAY(*_this), show, buf, sizeof(buf));
    return _ejs_string_new_utf8(buf);
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainMonthDay_prototype_toJSON) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINMONTHDAY, "Temporal.PlainMonthDay.prototype.toJSON");
    char buf[64];
    monthday_to_string_buf(EJSVAL_TO_TEMPORAL_PLAINMONTHDAY(*_this), 0, buf, sizeof(buf));
    return _ejs_string_new_utf8(buf);
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainMonthDay_prototype_toLocaleString) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINMONTHDAY, "Temporal.PlainMonthDay.prototype.toLocaleString");
    char buf[64];
    monthday_to_string_buf(EJSVAL_TO_TEMPORAL_PLAINMONTHDAY(*_this), 0, buf, sizeof(buf));
    return _ejs_string_new_utf8(buf);
}


// --------------------------------------------------------------- Instant

static EJS_NATIVE_FUNC(_ejs_TemporalInstant_from) {
    return _ejs_temporal_instant_new(to_temporal_instant_ns(argc > 0 ? args[0] : _ejs_undefined));
}

static EJS_NATIVE_FUNC(_ejs_TemporalInstant_fromEpochMilliseconds) {
    double ms = ToDouble(argc > 0 ? args[0] : _ejs_undefined);
    if (!isfinite(ms) || trunc(ms) != ms)
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "epoch milliseconds must be an integer");
    ejs_i128 ns = ((ejs_i128)(int64_t)ms) * 1000000LL;
    if (ns < EJS_TEMPORAL_NS_MIN_INSTANT || ns > EJS_TEMPORAL_NS_MAX_INSTANT)
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "epoch milliseconds out of range");
    return _ejs_temporal_instant_new(ns);
}

static EJS_NATIVE_FUNC(_ejs_TemporalInstant_fromEpochNanoseconds) {
    ejsval bi = temporal_to_bigint(argc > 0 ? args[0] : _ejs_undefined);
    return _ejs_temporal_instant_new(_ejs_temporal_bigint_to_epoch_ns(bi));
}

static EJS_NATIVE_FUNC(_ejs_TemporalInstant_compare) {
    ejs_i128 a = to_temporal_instant_ns(argc > 0 ? args[0] : _ejs_undefined);
    ejs_i128 b = to_temporal_instant_ns(argc > 1 ? args[1] : _ejs_undefined);
    return NUMBER_TO_EJSVAL(a < b ? -1.0 : (a > b ? 1.0 : 0.0));
}

static EJS_NATIVE_FUNC(_ejs_TemporalInstant_prototype_equals) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_INSTANT, "Temporal.Instant.prototype.equals");
    ejs_i128 other = to_temporal_instant_ns(argc > 0 ? args[0] : _ejs_undefined);
    return BOOLEAN_TO_EJSVAL(EJSVAL_TO_TEMPORAL_INSTANT(*_this)->epoch_ns == other);
}

static ejsval
instant_add_impl(ejsval instant, ejsval duration_like, int negate)
{
    EJSTemporalInstant* in = EJSVAL_TO_TEMPORAL_INSTANT(instant);
    DurationFields f;
    to_temporal_duration_fields(duration_like, &f);
    if (f.years != 0 || f.months != 0 || f.weeks != 0 || f.days != 0)
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "Instant arithmetic cannot use date units");
    ejs_i128 delta = duration_time_ns(&f);
    if (negate) delta = -delta;
    ejs_i128 ns = in->epoch_ns + delta;
    if (ns < EJS_TEMPORAL_NS_MIN_INSTANT || ns > EJS_TEMPORAL_NS_MAX_INSTANT)
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "instant out of range");
    return _ejs_temporal_instant_new(ns);
}

static EJS_NATIVE_FUNC(_ejs_TemporalInstant_prototype_add) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_INSTANT, "Temporal.Instant.prototype.add");
    return instant_add_impl(*_this, argc > 0 ? args[0] : _ejs_undefined, 0);
}

static EJS_NATIVE_FUNC(_ejs_TemporalInstant_prototype_subtract) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_INSTANT, "Temporal.Instant.prototype.subtract");
    return instant_add_impl(*_this, argc > 0 ? args[0] : _ejs_undefined, 1);
}

// Instant toString: optional timeZone, fractional second precision.
// option read order: fractionalSecondDigits, roundingMode, smallestUnit,
// timeZone
static EJS_NATIVE_FUNC(_ejs_TemporalInstant_prototype_toString) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_INSTANT, "Temporal.Instant.prototype.toString");
    EJSTemporalInstant* in = EJSVAL_TO_TEMPORAL_INSTANT(*_this);
    ejsval options = get_options_object(argc > 0 ? args[0] : _ejs_undefined);
    SecPrecision p;
    get_sec_precision(options, &p);
    if (p.digits == -2)
        p.increment = 60000000000LL;
    int64_t offset_ns = 0;
    EJSBool show_z = EJS_TRUE;
    if (!EJSVAL_IS_UNDEFINED(options)) {
        ejsval tz = Get(options, _ejs_string_new_utf8("timeZone"));
        if (!EJSVAL_IS_UNDEFINED(tz)) {
            ejsval tzid = to_time_zone_identifier(tz);
            offset_ns = tz_offset_ns(tzid, in->epoch_ns);
            show_z = EJS_FALSE;
        }
    }
    ejs_i128 rounded = round_i128_as_if_positive(in->epoch_ns, p.increment, p.mode);
    if (rounded < EJS_TEMPORAL_NS_MIN_INSTANT || rounded > EJS_TEMPORAL_NS_MAX_INSTANT)
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "instant out of range");
    int32_t y, mo, d, h, mi, s, ms, us, ns;
    epoch_ns_to_iso_fields(rounded, offset_ns, &y, &mo, &d, &h, &mi, &s, &ms, &us, &ns);
    char dbuf[32], tbuf[32], buf[96];
    format_iso_date(dbuf, sizeof(dbuf), y, mo, d);
    format_time_precision(tbuf, sizeof(tbuf), h, mi, s, ms, us, ns, &p);
    if (show_z) {
        snprintf(buf, sizeof(buf), "%sT%sZ", dbuf, tbuf);
    } else {
        char obuf[16];
        format_offset_ns(obuf, sizeof(obuf), offset_ns);
        snprintf(buf, sizeof(buf), "%sT%s%s", dbuf, tbuf, obuf);
    }
    return _ejs_string_new_utf8(buf);
}

static ejsval
instant_default_string(EJSTemporalInstant* in)
{
    int32_t y, mo, d, h, mi, s, ms, us, ns;
    epoch_ns_to_iso_fields(in->epoch_ns, 0, &y, &mo, &d, &h, &mi, &s, &ms, &us, &ns);
    char dbuf[32], tbuf[32], buf[96];
    format_iso_date(dbuf, sizeof(dbuf), y, mo, d);
    format_iso_time_auto(tbuf, sizeof(tbuf), h, mi, s, ms, us, ns);
    snprintf(buf, sizeof(buf), "%sT%sZ", dbuf, tbuf);
    return _ejs_string_new_utf8(buf);
}

static EJS_NATIVE_FUNC(_ejs_TemporalInstant_prototype_toJSON) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_INSTANT, "Temporal.Instant.prototype.toJSON");
    return instant_default_string(EJSVAL_TO_TEMPORAL_INSTANT(*_this));
}

static EJS_NATIVE_FUNC(_ejs_TemporalInstant_prototype_toLocaleString) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_INSTANT, "Temporal.Instant.prototype.toLocaleString");
    return instant_default_string(EJSVAL_TO_TEMPORAL_INSTANT(*_this));
}

static EJS_NATIVE_FUNC(_ejs_TemporalInstant_prototype_toZonedDateTimeISO) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_INSTANT, "Temporal.Instant.prototype.toZonedDateTimeISO");
    EJSTemporalInstant* in = EJSVAL_TO_TEMPORAL_INSTANT(*_this);
    ejsval tzid = to_time_zone_identifier(argc > 0 ? args[0] : _ejs_undefined);
    return _ejs_temporal_zoneddatetime_new(in->epoch_ns, tzid, _ejs_temporal_str_iso8601);
}


// ------------------------------------------- ZonedDateTime conversions

// parse a full-precision offset string (±HH[:MM[:SS[.fff...]]])
static int64_t
parse_full_offset_string(ejsval str)
{
    char* s = string_to_ascii(str);
    ParsedISO r;
    memset(&r, 0, sizeof(r));
    const char* p = s;
    int ok = s != NULL && parse_utc_offset(&p, &r, EJS_TRUE) && *p == 0 && !r.offset_z;
    free(s);
    if (!ok)
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "invalid offset string");
    return r.offset_ns;
}

// ToTemporalZonedDateTime.  offset_default: 3=reject (from), 0=prefer
// (with/relativeTo paths use their own merged flow).
static void
to_temporal_zdt(ejsval item, ejsval options, ejs_i128* epoch, ejsval* tz, ejsval* cal)
{
    if (EJSVAL_IS_TEMPORAL_ZONEDDATETIME(item)) {
        EJSTemporalZonedDateTime* zdt = EJSVAL_TO_TEMPORAL_ZONEDDATETIME(item);
        ejsval o = get_options_object(options);
        static const char* const disambig[] = { "compatible", "earlier", "later", "reject" };
        get_string_option(o, "disambiguation", disambig, 4, 0);
        static const char* const offsetopt[] = { "prefer", "use", "ignore", "reject" };
        get_string_option(o, "offset", offsetopt, 4, 3);
        get_overflow_option(o);
        *epoch = zdt->epoch_ns;
        *tz = zdt->time_zone;
        *cal = zdt->calendar;
        return;
    }
    if (EJSVAL_IS_OBJECT(item)) {
        ejsval calendar = calendar_from_item(item);
        FieldsBag bag;
        read_fields_bag(item, BAG_DATE | BAG_TIME | BAG_OFFSET | BAG_TZ, &bag);
        if (!bag.has_time_zone)
            _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "timeZone is required");
        ejsval tzid = to_time_zone_identifier(bag.time_zone);
        ejsval o = get_options_object(options);
        static const char* const disambig[] = { "compatible", "earlier", "later", "reject" };
        int disambiguation = get_string_option(o, "disambiguation", disambig, 4, 0);
        static const char* const offsetopt[] = { "prefer", "use", "ignore", "reject" };
        int offset_behavior = get_string_option(o, "offset", offsetopt, 4, 3);
        EJSBool constrain = get_overflow_option(o);
        int32_t y, m, d, h, mi, s, ms, us, ns;
        iso_date_from_bag(&bag, constrain, &y, &m, &d);
        regulate_time_from_bag(&bag, constrain, &h, &mi, &s, &ms, &us, &ns);
        ejs_i128 local = iso_datetime_to_epoch_ns(y, m, d, h, mi, s, ms, us, ns);
        ejs_i128 result;
        int64_t bag_off = 0;
        if (bag.has_offset)
            bag_off = parse_full_offset_string(bag.offset);
        if (bag.has_offset && offset_behavior == 1 /* use */) {
            result = local - bag_off;
        } else if (bag.has_offset && offset_behavior != 2 /* prefer/reject */) {
            check_iso_days_range(local);
            result = local - bag_off;
            if (offset_behavior == 3) { // reject: must match the zone
                int64_t zone_off = tz_offset_ns(tzid, result);
                if (zone_off != bag_off)
                    _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "offset does not match time zone");
            }
        } else {
            result = zone_epoch_from_local(tzid, local, disambiguation);
        }
        if (result < EJS_TEMPORAL_NS_MIN_INSTANT || result > EJS_TEMPORAL_NS_MAX_INSTANT)
            _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "instant out of range");
        *epoch = result;
        *tz = tzid;
        *cal = calendar;
        return;
    }
    if (!EJSVAL_IS_STRING(item))
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "cannot convert value to Temporal.ZonedDateTime");
    char* str = string_to_ascii(item);
    ParsedISO r;
    int ok = str != NULL && parse_annotated_datetime(str, &r);
    free(str);
    if (!ok || !r.has_tz)
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "invalid ZonedDateTime string (time zone annotation required)");
    ejsval calendar = parsed_calendar(&r);
    if (!_ejs_temporal_iso_datetime_within_limits(r.year, r.month, r.day, r.hour, r.minute, r.second,
                                                  r.millisecond, r.microsecond, r.nanosecond))
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "datetime out of range");
    ejsval tzid = to_time_zone_identifier(_ejs_string_new_utf8(r.tz));
    ejsval o = get_options_object(options);
    static const char* const disambig[] = { "compatible", "earlier", "later", "reject" };
    int disambiguation = get_string_option(o, "disambiguation", disambig, 4, 0);
    static const char* const offsetopt[] = { "prefer", "use", "ignore", "reject" };
    int offset_behavior = get_string_option(o, "offset", offsetopt, 4, 3);
    get_overflow_option(o);
    ejs_i128 dt_ns = iso_datetime_to_epoch_ns(r.year, r.month, r.day, r.hour, r.minute, r.second,
                                              r.millisecond, r.microsecond, r.nanosecond);
    ejs_i128 result;
    if (r.has_offset && r.offset_z) {
        result = dt_ns;
    } else if (r.has_offset && offset_behavior == 1) {
        // offset "use": the parsed offset is exact; no wall-clock
        // computation happens, so out-of-range local dates are fine
        result = dt_ns - r.offset_ns;
    } else if (r.has_offset && offset_behavior == 2) {
        // offset "ignore": an offset time zone's offset is constant,
        // so this also avoids wall-clock validation
        int64_t direct_off;
        char* tzs = string_to_utf8(tzid);
        EJSBool is_offset_zone = parse_offset_tz_id(tzs, &direct_off, NULL, 0);
        EJSBool is_utc = !strcmp(tzs, "UTC");
        free(tzs);
        if (is_offset_zone || is_utc)
            result = dt_ns - (is_utc ? 0 : direct_off);
        else
            result = zone_epoch_from_local(tzid, dt_ns, disambiguation);
    } else if (r.has_offset) {
        check_iso_days_range(dt_ns);
        result = dt_ns - r.offset_ns;
        if (offset_behavior == 3) {
            int64_t zone_off = tz_offset_ns(tzid, result);
            if (zone_off != r.offset_ns)
                _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "offset does not match time zone");
        }
    } else {
        result = zone_epoch_from_local(tzid, dt_ns, disambiguation);
    }
    if (result < EJS_TEMPORAL_NS_MIN_INSTANT || result > EJS_TEMPORAL_NS_MAX_INSTANT)
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "instant out of range");
    *epoch = result;
    *tz = tzid;
    *cal = calendar;
}

static EJS_NATIVE_FUNC(_ejs_TemporalZonedDateTime_prototype_with) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_ZONEDDATETIME, "Temporal.ZonedDateTime.prototype.with");
    EJSTemporalZonedDateTime* zdt = EJSVAL_TO_TEMPORAL_ZONEDDATETIME(*_this);
    ejsval item = argc > 0 ? args[0] : _ejs_undefined;
    check_partial_temporal_object(item);
    FieldsBag bag;
    read_fields_bag(item, BAG_DATE | BAG_TIME | BAG_OFFSET, &bag);
    require_any_field(&bag);
    ZDTFields f;
    zdt_fields(zdt, &f);
    merge_date_fields(&bag, f.year, f.month, f.day);
    if (!bag.has_hour) bag.hour = f.hour;
    if (!bag.has_minute) bag.minute = f.minute;
    if (!bag.has_second) bag.second = f.second;
    if (!bag.has_ms) bag.ms = f.millisecond;
    if (!bag.has_us) bag.us = f.microsecond;
    if (!bag.has_ns) bag.ns = f.nanosecond;
    int64_t offset_ns = f.offset_ns;
    if (bag.has_offset)
        offset_ns = parse_full_offset_string(bag.offset);
    ejsval o = get_options_object(argc > 1 ? args[1] : _ejs_undefined);
    static const char* const disambig[] = { "compatible", "earlier", "later", "reject" };
    get_string_option(o, "disambiguation", disambig, 4, 0);
    static const char* const offsetopt[] = { "prefer", "use", "ignore", "reject" };
    int offset_behavior = get_string_option(o, "offset", offsetopt, 4, 0); // default prefer
    EJSBool constrain = get_overflow_option(o);
    int32_t y, m, d, h, mi, s, ms, us, ns;
    iso_date_from_bag(&bag, constrain, &y, &m, &d);
    regulate_time_from_bag(&bag, constrain, &h, &mi, &s, &ms, &us, &ns);
    ejs_i128 local = iso_datetime_to_epoch_ns(y, m, d, h, mi, s, ms, us, ns);
    check_iso_days_range(local);
    ejs_i128 epoch;
    switch (offset_behavior) {
    case 1: // use
        epoch = local - offset_ns;
        break;
    case 2: // ignore
        epoch = zone_epoch_from_local(zdt->time_zone, local, 0);
        break;
    case 3: { // reject
        ejs_i128 cand = local - offset_ns;
        if (tz_offset_ns(zdt->time_zone, cand) != offset_ns)
            _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "offset does not match time zone");
        epoch = cand;
        break;
    }
    default: { // prefer: keep the offset when it is valid for the zone
        ejs_i128 cand = local - offset_ns;
        if (bag.has_offset && tz_offset_ns(zdt->time_zone, cand) == offset_ns)
            epoch = cand;
        else
            epoch = zone_epoch_from_local(zdt->time_zone, local, 0);
        break;
    }
    }
    if (epoch < EJS_TEMPORAL_NS_MIN_INSTANT || epoch > EJS_TEMPORAL_NS_MAX_INSTANT)
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "instant out of range");
    return _ejs_temporal_zoneddatetime_new(epoch, zdt->time_zone, zdt->calendar);
}

// --------------------------------------------------------- ZonedDateTime

static EJS_NATIVE_FUNC(_ejs_TemporalZonedDateTime_from) {
    ejs_i128 epoch;
    ejsval tz, cal;
    to_temporal_zdt(argc > 0 ? args[0] : _ejs_undefined,
                    argc > 1 ? args[1] : _ejs_undefined, &epoch, &tz, &cal);
    return _ejs_temporal_zoneddatetime_new(epoch, tz, cal);
}

static EJS_NATIVE_FUNC(_ejs_TemporalZonedDateTime_compare) {
    ejs_i128 a, b;
    ejsval tz, cal;
    to_temporal_zdt(argc > 0 ? args[0] : _ejs_undefined, _ejs_undefined, &a, &tz, &cal);
    to_temporal_zdt(argc > 1 ? args[1] : _ejs_undefined, _ejs_undefined, &b, &tz, &cal);
    return NUMBER_TO_EJSVAL(a < b ? -1.0 : (a > b ? 1.0 : 0.0));
}

static EJS_NATIVE_FUNC(_ejs_TemporalZonedDateTime_prototype_equals) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_ZONEDDATETIME, "Temporal.ZonedDateTime.prototype.equals");
    EJSTemporalZonedDateTime* zdt = EJSVAL_TO_TEMPORAL_ZONEDDATETIME(*_this);
    ejs_i128 o_epoch;
    ejsval o_tz, o_cal;
    to_temporal_zdt(argc > 0 ? args[0] : _ejs_undefined, _ejs_undefined, &o_epoch, &o_tz, &o_cal);
    return BOOLEAN_TO_EJSVAL(zdt->epoch_ns == o_epoch
                             && SameValue(zdt->time_zone, o_tz)
                             && SameValue(zdt->calendar, o_cal));
}

static ejsval
zdt_to_string_impl(EJSTemporalZonedDateTime* zdt, const SecPrecision* p, int show_cal,
                   int show_offset /*1 auto/0 never*/, int show_tz /*0 auto,1 never,2 critical*/)
{
    ejs_i128 rounded = round_i128_as_if_positive(zdt->epoch_ns, p->increment, p->mode);
    int64_t offset_ns = tz_offset_ns(zdt->time_zone, rounded);
    int32_t y, mo, d, h, mi, s, ms, us, ns;
    epoch_ns_to_iso_fields(rounded, offset_ns, &y, &mo, &d, &h, &mi, &s, &ms, &us, &ns);
    char dbuf[32], tbuf[32], buf[192];
    format_iso_date(dbuf, sizeof(dbuf), y, mo, d);
    format_time_precision(tbuf, sizeof(tbuf), h, mi, s, ms, us, ns, p);
    snprintf(buf, sizeof(buf), "%sT%s", dbuf, tbuf);
    if (show_offset) {
        char obuf[16];
        format_offset_ns(obuf, sizeof(obuf), offset_ns);
        strlcat(buf, obuf, sizeof(buf));
    }
    if (show_tz != 1) {
        char* tz = string_to_utf8(zdt->time_zone);
        strlcat(buf, show_tz == 2 ? "[!" : "[", sizeof(buf));
        strlcat(buf, tz, sizeof(buf));
        strlcat(buf, "]", sizeof(buf));
        free(tz);
    }
    append_calendar_annotation(buf, sizeof(buf), show_cal);
    return _ejs_string_new_utf8(buf);
}

// option read order: calendarName, fractionalSecondDigits, offset,
// roundingMode, smallestUnit, timeZoneName
static EJS_NATIVE_FUNC(_ejs_TemporalZonedDateTime_prototype_toString) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_ZONEDDATETIME, "Temporal.ZonedDateTime.prototype.toString");
    EJSTemporalZonedDateTime* zdt = EJSVAL_TO_TEMPORAL_ZONEDDATETIME(*_this);
    ejsval options = get_options_object(argc > 0 ? args[0] : _ejs_undefined);
    int show_cal = get_calendar_name_option(options);
    SecPrecision p;
    p.digits = -1;
    p.increment = 1;
    p.mode = ROUND_TRUNC;
    if (!EJSVAL_IS_UNDEFINED(options)) {
        ejsval fsd = Get(options, _ejs_string_new_utf8("fractionalSecondDigits"));
        if (!EJSVAL_IS_UNDEFINED(fsd)) {
            if (EJSVAL_IS_NUMBER(fsd)) {
                double d = EJSVAL_TO_NUMBER(fsd);
                if (isnan(d) || !isfinite(d) || floor(d) < 0 || floor(d) > 9)
                    _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "fractionalSecondDigits must be 'auto' or 0 through 9");
                p.digits = (int)floor(d);
            } else {
                ejsval str = ToString(fsd);
                if (!string_equals_utf8(str, "auto"))
                    _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "fractionalSecondDigits must be 'auto' or 0 through 9");
            }
        }
    }
    static const char* const offsetvals[] = { "auto", "never" };
    int show_offset = get_string_option(options, "offset", offsetvals, 2, 0) == 0;
    p.mode = get_rounding_mode_option(options, ROUND_TRUNC);
    int su = get_string_option(options, "smallestUnit", time_unit_names, 10, -1);
    if (su >= 0) {
        switch (su / 2) {
        case 0: p.digits = -2; break;
        case 1: p.digits = 0; break;
        case 2: p.digits = 3; break;
        case 3: p.digits = 6; break;
        case 4: p.digits = 9; break;
        }
    }
    if (p.digits == -1) p.increment = 1;
    else if (p.digits == -2) p.increment = 60000000000LL;
    else if (p.digits == 0) p.increment = 1000000000LL;
    else {
        p.increment = 1;
        for (int i = 0; i < 9 - p.digits; i++) p.increment *= 10;
    }
    static const char* const tzvals[] = { "auto", "never", "critical" };
    int show_tz = get_string_option(options, "timeZoneName", tzvals, 3, 0);
    return zdt_to_string_impl(zdt, &p, show_cal, show_offset, show_tz);
}

static ejsval
zdt_default_string(EJSTemporalZonedDateTime* zdt)
{
    SecPrecision p = { .digits = -1, .increment = 1, .mode = ROUND_TRUNC };
    return zdt_to_string_impl(zdt, &p, 0, 1, 0);
}

static EJS_NATIVE_FUNC(_ejs_TemporalZonedDateTime_prototype_toJSON) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_ZONEDDATETIME, "Temporal.ZonedDateTime.prototype.toJSON");
    return zdt_default_string(EJSVAL_TO_TEMPORAL_ZONEDDATETIME(*_this));
}

static EJS_NATIVE_FUNC(_ejs_TemporalZonedDateTime_prototype_toLocaleString) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_ZONEDDATETIME, "Temporal.ZonedDateTime.prototype.toLocaleString");
    return zdt_default_string(EJSVAL_TO_TEMPORAL_ZONEDDATETIME(*_this));
}

static EJS_NATIVE_FUNC(_ejs_TemporalZonedDateTime_prototype_toInstant) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_ZONEDDATETIME, "Temporal.ZonedDateTime.prototype.toInstant");
    return _ejs_temporal_instant_new(EJSVAL_TO_TEMPORAL_ZONEDDATETIME(*_this)->epoch_ns);
}

static EJS_NATIVE_FUNC(_ejs_TemporalZonedDateTime_prototype_toPlainDate) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_ZONEDDATETIME, "Temporal.ZonedDateTime.prototype.toPlainDate");
    EJSTemporalZonedDateTime* zdt = EJSVAL_TO_TEMPORAL_ZONEDDATETIME(*_this);
    ZDTFields f;
    zdt_fields(zdt, &f);
    return _ejs_temporal_plain_date_new(f.year, f.month, f.day, zdt->calendar);
}

static EJS_NATIVE_FUNC(_ejs_TemporalZonedDateTime_prototype_toPlainTime) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_ZONEDDATETIME, "Temporal.ZonedDateTime.prototype.toPlainTime");
    EJSTemporalZonedDateTime* zdt = EJSVAL_TO_TEMPORAL_ZONEDDATETIME(*_this);
    ZDTFields f;
    zdt_fields(zdt, &f);
    return _ejs_temporal_plain_time_new(f.hour, f.minute, f.second, f.millisecond, f.microsecond, f.nanosecond);
}

static EJS_NATIVE_FUNC(_ejs_TemporalZonedDateTime_prototype_toPlainDateTime) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_ZONEDDATETIME, "Temporal.ZonedDateTime.prototype.toPlainDateTime");
    EJSTemporalZonedDateTime* zdt = EJSVAL_TO_TEMPORAL_ZONEDDATETIME(*_this);
    ZDTFields f;
    zdt_fields(zdt, &f);
    return _ejs_temporal_plain_datetime_new(f.year, f.month, f.day, f.hour, f.minute, f.second,
                                            f.millisecond, f.microsecond, f.nanosecond, zdt->calendar);
}

static EJS_NATIVE_FUNC(_ejs_TemporalZonedDateTime_prototype_withTimeZone) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_ZONEDDATETIME, "Temporal.ZonedDateTime.prototype.withTimeZone");
    EJSTemporalZonedDateTime* zdt = EJSVAL_TO_TEMPORAL_ZONEDDATETIME(*_this);
    ejsval tzid = to_time_zone_identifier(argc > 0 ? args[0] : _ejs_undefined);
    return _ejs_temporal_zoneddatetime_new(zdt->epoch_ns, tzid, zdt->calendar);
}

static EJS_NATIVE_FUNC(_ejs_TemporalZonedDateTime_prototype_withCalendar) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_ZONEDDATETIME, "Temporal.ZonedDateTime.prototype.withCalendar");
    EJSTemporalZonedDateTime* zdt = EJSVAL_TO_TEMPORAL_ZONEDDATETIME(*_this);
    if (argc == 0 || EJSVAL_IS_UNDEFINED(args[0]))
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "calendar is required");
    ejsval cal = to_temporal_calendar_identifier(args[0]);
    return _ejs_temporal_zoneddatetime_new(zdt->epoch_ns, zdt->time_zone, cal);
}

static EJS_NATIVE_FUNC(_ejs_TemporalZonedDateTime_prototype_startOfDay) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_ZONEDDATETIME, "Temporal.ZonedDateTime.prototype.startOfDay");
    EJSTemporalZonedDateTime* zdt = EJSVAL_TO_TEMPORAL_ZONEDDATETIME(*_this);
    ZDTFields f;
    zdt_fields(zdt, &f);
    ejs_i128 midnight = zone_epoch_from_local(zdt->time_zone,
        iso_datetime_to_epoch_ns(f.year, f.month, f.day, 0, 0, 0, 0, 0, 0), 0);
    return _ejs_temporal_zoneddatetime_new(midnight, zdt->time_zone, zdt->calendar);
}

static EJS_NATIVE_FUNC(_ejs_TemporalZonedDateTime_prototype_getTimeZoneTransition) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_ZONEDDATETIME, "Temporal.ZonedDateTime.prototype.getTimeZoneTransition");
    ejsval dir = argc > 0 ? args[0] : _ejs_undefined;
    if (EJSVAL_IS_UNDEFINED(dir))
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "direction is required");
    int direction;
    static const char* const dirs[] = { "next", "previous" };
    if (EJSVAL_IS_STRING(dir)) {
        if (string_equals_utf8(dir, "next")) direction = 1;
        else if (string_equals_utf8(dir, "previous")) direction = -1;
        else _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "direction must be 'next' or 'previous'");
    } else if (EJSVAL_IS_OBJECT(dir)) {
        int idx = get_string_option(dir, "direction", dirs, 2, -1);
        if (idx < 0)
            _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "direction is required");
        direction = idx == 1 ? -1 : 1;
    } else {
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "invalid direction");
        EJS_NOT_REACHED();
        return _ejs_undefined;
    }
    EJSTemporalZonedDateTime* zdt = EJSVAL_TO_TEMPORAL_ZONEDDATETIME(*_this);
    if (EJSVAL_IS_STRING(zdt->time_zone)) {
        char* tzs = string_to_utf8(zdt->time_zone);
        EJSBool named = strcmp(tzs, "UTC") != 0 && tzs[0] != '+' && tzs[0] != '-';
        ejs_i128 result;
        EJSBool have = named && tz_named_transition(tzs, zdt->epoch_ns, direction, &result);
        free(tzs);
        if (have && result >= EJS_TEMPORAL_NS_MIN_INSTANT && result <= EJS_TEMPORAL_NS_MAX_INSTANT)
            return _ejs_temporal_zoneddatetime_new(result, zdt->time_zone, zdt->calendar);
    }
    return _ejs_null;
}

static ejsval
zdt_add_impl(ejsval zdtval, ejsval duration_like, ejsval options, int negate)
{
    EJSTemporalZonedDateTime* zdt = EJSVAL_TO_TEMPORAL_ZONEDDATETIME(zdtval);
    DurationFields f;
    to_temporal_duration_fields(duration_like, &f);
    if (negate) {
        f.years = -f.years; f.months = -f.months; f.weeks = -f.weeks; f.days = -f.days;
        f.hours = -f.hours; f.minutes = -f.minutes; f.seconds = -f.seconds;
        f.ms = -f.ms; f.us = -f.us; f.ns = -f.ns;
    }
    EJSBool constrain = get_overflow_option(get_options_object(options));
    // date part in local time, time part in exact time
    ZDTFields fields;
    zdt_fields(zdt, &fields);
    int32_t ry, rm, rd;
    add_iso_date(fields.year, fields.month, fields.day, f.years, f.months, f.weeks, f.days,
                 constrain, &ry, &rm, &rd);
    ejs_i128 local_dt = iso_datetime_to_epoch_ns(ry, rm, rd, fields.hour, fields.minute, fields.second,
                                                 fields.millisecond, fields.microsecond, fields.nanosecond);
    ejs_i128 epoch = zone_epoch_from_local(zdt->time_zone, local_dt, 0) + duration_time_ns(&f);
    if (epoch < EJS_TEMPORAL_NS_MIN_INSTANT || epoch > EJS_TEMPORAL_NS_MAX_INSTANT)
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "instant out of range");
    return _ejs_temporal_zoneddatetime_new(epoch, zdt->time_zone, zdt->calendar);
}

static EJS_NATIVE_FUNC(_ejs_TemporalZonedDateTime_prototype_add) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_ZONEDDATETIME, "Temporal.ZonedDateTime.prototype.add");
    return zdt_add_impl(*_this, argc > 0 ? args[0] : _ejs_undefined, argc > 1 ? args[1] : _ejs_undefined, 0);
}

static EJS_NATIVE_FUNC(_ejs_TemporalZonedDateTime_prototype_subtract) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_ZONEDDATETIME, "Temporal.ZonedDateTime.prototype.subtract");
    return zdt_add_impl(*_this, argc > 0 ? args[0] : _ejs_undefined, argc > 1 ? args[1] : _ejs_undefined, 1);
}


typedef enum {
    TUNIT_AUTO = -1,
    TUNIT_YEAR = 0, TUNIT_MONTH, TUNIT_WEEK, TUNIT_DAY,
    TUNIT_HOUR, TUNIT_MINUTE, TUNIT_SECOND, TUNIT_MILLI, TUNIT_MICRO, TUNIT_NANO
} TUnit;

// ns per unit, TUNIT_DAY..TUNIT_NANO
static const int64_t unit_ns_table[] = {
    86400000000000LL, 3600000000000LL, 60000000000LL, 1000000000LL, 1000000LL, 1000LL, 1LL
};

static int64_t
unit_ns(TUnit u)
{
    return unit_ns_table[u - TUNIT_DAY];
}

static ejsval balance_time_duration(ejs_i128 total_ns, TUnit largest, int negate_result);

// ---------------------------------------------------- relativeTo

typedef struct {
    int kind;            // 0 none, 1 plain date, 2 zoned
    int32_t year, month, day;
    ejs_i128 local_time_ns; // zoned: wall-clock time of day
    ejsval calendar;
    ejsval time_zone;    // zoned only
    ejs_i128 epoch_ns;   // zoned only
} RelativeTo;

// GetTemporalRelativeToOption
static void
get_relative_to_option(ejsval options, RelativeTo* rel)
{
    memset(rel, 0, sizeof(*rel));
    rel->calendar = _ejs_undefined;
    rel->time_zone = _ejs_undefined;
    if (EJSVAL_IS_UNDEFINED(options) || !EJSVAL_IS_OBJECT(options))
        return;
    ejsval v = Get(options, _ejs_string_new_utf8("relativeTo"));
    if (EJSVAL_IS_UNDEFINED(v))
        return;
    if (EJSVAL_IS_TEMPORAL_ZONEDDATETIME(v) || EJSVAL_IS_TEMPORAL_PLAINDATE(v)
        || EJSVAL_IS_TEMPORAL_PLAINDATETIME(v)) {
        if (EJSVAL_IS_TEMPORAL_ZONEDDATETIME(v)) {
            EJSTemporalZonedDateTime* zdt = EJSVAL_TO_TEMPORAL_ZONEDDATETIME(v);
            rel->kind = 2;
            rel->epoch_ns = zdt->epoch_ns;
            rel->time_zone = zdt->time_zone;
            rel->calendar = zdt->calendar;
            int64_t off = tz_offset_ns(zdt->time_zone, zdt->epoch_ns);
            int32_t h, mi, sec, ms, us, ns;
            epoch_ns_to_iso_fields(zdt->epoch_ns, off, &rel->year, &rel->month, &rel->day,
                                   &h, &mi, &sec, &ms, &us, &ns);
            rel->local_time_ns = time_fields_to_ns(h, mi, sec, ms, us, ns);
        } else {
            to_temporal_date_fields(v, _ejs_undefined, &rel->year, &rel->month, &rel->day, &rel->calendar);
            rel->kind = 1;
        }
        return;
    }
    if (EJSVAL_IS_OBJECT(v)) {
        // full field read (date + time + offset + timeZone) so every
        // field is validated even when only the date part is kept
        ejsval cal = calendar_from_item(v);
        FieldsBag bag;
        read_fields_bag(v, BAG_DATE | BAG_TIME | BAG_OFFSET | BAG_TZ, &bag);
        int32_t y, m, d, h, mi, sec, ms, us, ns;
        iso_date_from_bag(&bag, EJS_TRUE, &y, &m, &d);
        regulate_time_from_bag(&bag, EJS_TRUE, &h, &mi, &sec, &ms, &us, &ns);
        if (bag.has_time_zone) {
            ejsval tzid = to_time_zone_identifier(bag.time_zone);
            ejs_i128 local = iso_datetime_to_epoch_ns(y, m, d, h, mi, sec, ms, us, ns);
            check_iso_days_range(local);
            ejs_i128 epoch;
            if (bag.has_offset) {
                int64_t off = parse_full_offset_string(bag.offset);
                epoch = local - off;
                if (tz_offset_ns(tzid, epoch) != off)
                    _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "offset does not match time zone");
            } else {
                epoch = zone_epoch_from_local(tzid, local, 0);
            }
            if (epoch < EJS_TEMPORAL_NS_MIN_INSTANT || epoch > EJS_TEMPORAL_NS_MAX_INSTANT)
                _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "instant out of range");
            rel->kind = 2;
            rel->epoch_ns = epoch;
            rel->time_zone = tzid;
            rel->calendar = cal;
            int64_t off2 = tz_offset_ns(tzid, epoch);
            int32_t th, tmi, ts, tms, tus, tns;
            epoch_ns_to_iso_fields(epoch, off2, &rel->year, &rel->month, &rel->day,
                                   &th, &tmi, &ts, &tms, &tus, &tns);
            rel->local_time_ns = time_fields_to_ns(th, tmi, ts, tms, tus, tns);
        } else {
            rel->kind = 1;
            rel->year = y; rel->month = m; rel->day = d;
            rel->calendar = cal;
        }
        return;
    }
    if (EJSVAL_IS_STRING(v)) {
        char* s = string_to_ascii(v);
        ParsedISO r;
        EJSBool zoned = s && parse_annotated_datetime(s, &r) && r.has_tz;
        free(s);
        if (zoned) {
            ejs_i128 epoch;
            ejsval tz, cal;
            to_temporal_zdt(v, _ejs_undefined, &epoch, &tz, &cal);
            rel->kind = 2;
            rel->epoch_ns = epoch;
            rel->time_zone = tz;
            rel->calendar = cal;
            int64_t off = tz_offset_ns(tz, epoch);
            int32_t h, mi, sec, ms, us, ns;
            epoch_ns_to_iso_fields(epoch, off, &rel->year, &rel->month, &rel->day, &h, &mi, &sec, &ms, &us, &ns);
            rel->local_time_ns = time_fields_to_ns(h, mi, sec, ms, us, ns);
            return;
        }
    }
    to_temporal_date_fields(v, _ejs_undefined, &rel->year, &rel->month, &rel->day, &rel->calendar);
    rel->kind = 1;
}

// apply a duration to a relativeTo point; returns the target date and
// time-of-day, plus exact total ns from the start point
static void
relative_apply_duration(const RelativeTo* rel, const DurationFields* f,
                        int32_t* ty, int32_t* tm, int32_t* td, ejs_i128* t_time,
                        ejs_i128* total_ns)
{
    if (rel->kind == 2) {
        // zoned: date part in local time, then exact time
        int64_t off = tz_offset_ns(rel->time_zone, rel->epoch_ns);
        int32_t h, mi, sec, ms, us, ns;
        int32_t y0, m0, d0;
        epoch_ns_to_iso_fields(rel->epoch_ns, off, &y0, &m0, &d0, &h, &mi, &sec, &ms, &us, &ns);
        int32_t ry, rm, rd;
        add_iso_date(y0, m0, d0, f->years, f->months, f->weeks, f->days, EJS_TRUE, &ry, &rm, &rd);
        ejs_i128 local = iso_datetime_to_epoch_ns(ry, rm, rd, h, mi, sec, ms, us, ns);
        ejs_i128 epoch = zone_epoch_from_local(rel->time_zone, local, 0) + duration_time_ns((DurationFields*)f);
        *total_ns = epoch - rel->epoch_ns;
        int64_t off2 = tz_offset_ns(rel->time_zone, epoch);
        int32_t th, tmi, ts, tms, tus, tns;
        epoch_ns_to_iso_fields(epoch, off2, ty, tm, td, &th, &tmi, &ts, &tms, &tus, &tns);
        *t_time = time_fields_to_ns(th, tmi, ts, tms, tus, tns);
        return;
    }
    int32_t ry, rm, rd;
    add_iso_date(rel->year, rel->month, rel->day, f->years, f->months, f->weeks, f->days,
                 EJS_TRUE, &ry, &rm, &rd);
    ejs_i128 t = duration_time_ns((DurationFields*)f);
    ejs_i128 day_carry = i128_floordiv(t, NS_PER_DAY);
    ejs_i128 rem = i128_floormod(t, NS_PER_DAY);
    int64_t days = _ejs_temporal_iso_date_to_epoch_days(ry, rm, rd) + (int64_t)day_carry;
    _ejs_temporal_epoch_days_to_iso_date(days, ty, tm, td);
    *t_time = rem;
    *total_ns = ((ejs_i128)(days - _ejs_temporal_iso_date_to_epoch_days(rel->year, rel->month, rel->day))) * NS_PER_DAY + rem;
    // maxTimeDuration: 2^53 seconds
    ejs_i128 max_time = ((ejs_i128)9007199254740992LL) * 1000000000LL;
    if (*total_ns >= max_time || *total_ns <= -max_time)
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "duration out of range");
    if (!iso_date_within_limits(*ty, *tm, *td))
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "date out of range");
}

// exact ns of "start + n units of `unit`" from start, for fractional
// totals (calendar units)
static ejs_i128
relative_units_ns(const RelativeTo* rel, TUnit unit, double n)
{
    double y = 0, mo = 0, w = 0, d = 0;
    switch (unit) {
    case TUNIT_YEAR: y = n; break;
    case TUNIT_MONTH: mo = n; break;
    case TUNIT_WEEK: w = n; break;
    default: d = n; break;
    }
    int32_t ry, rm, rd;
    add_iso_date(rel->year, rel->month, rel->day, y, mo, w, d, EJS_TRUE, &ry, &rm, &rd);
    if (rel->kind == 2) {
        int64_t off = tz_offset_ns(rel->time_zone, rel->epoch_ns);
        int32_t h, mi, sec, ms, us, ns;
        int32_t y0, m0, d0;
        epoch_ns_to_iso_fields(rel->epoch_ns, off, &y0, &m0, &d0, &h, &mi, &sec, &ms, &us, &ns);
        ejs_i128 local = iso_datetime_to_epoch_ns(ry, rm, rd, h, mi, sec, ms, us, ns);
        return zone_epoch_from_local(rel->time_zone, local, 0) - rel->epoch_ns;
    }
    return ((ejs_i128)(_ejs_temporal_iso_date_to_epoch_days(ry, rm, rd)
                       - _ejs_temporal_iso_date_to_epoch_days(rel->year, rel->month, rel->day))) * NS_PER_DAY;
}


// -------------------------------------------------------------- Duration

static EJS_NATIVE_FUNC(_ejs_TemporalDuration_from) {
    DurationFields f;
    to_temporal_duration_fields(argc > 0 ? args[0] : _ejs_undefined, &f);
    return _ejs_temporal_duration_new(f.years, f.months, f.weeks, f.days, f.hours,
                                      f.minutes, f.seconds, f.ms, f.us, f.ns);
}

static EJS_NATIVE_FUNC(_ejs_TemporalDuration_compare) {
    DurationFields a, b;
    to_temporal_duration_fields(argc > 0 ? args[0] : _ejs_undefined, &a);
    to_temporal_duration_fields(argc > 1 ? args[1] : _ejs_undefined, &b);
    ejsval options = get_options_object(argc > 2 ? args[2] : _ejs_undefined);
    RelativeTo rel;
    get_relative_to_option(options, &rel);
    // exact field equality short-circuits without needing relativeTo
    if (a.years == b.years && a.months == b.months && a.weeks == b.weeks && a.days == b.days
        && a.hours == b.hours && a.minutes == b.minutes && a.seconds == b.seconds
        && a.ms == b.ms && a.us == b.us && a.ns == b.ns)
        return NUMBER_TO_EJSVAL(0.0);
    if (rel.kind == 0) {
        if (a.years != 0 || b.years != 0 || a.months != 0 || b.months != 0 || a.weeks != 0 || b.weeks != 0)
            _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "comparing durations with calendar units requires relativeTo");
        ejs_i128 an = duration_time_ns(&a) + ((ejs_i128)a.days) * NS_PER_DAY;
        ejs_i128 bn = duration_time_ns(&b) + ((ejs_i128)b.days) * NS_PER_DAY;
        return NUMBER_TO_EJSVAL(an < bn ? -1.0 : (an > bn ? 1.0 : 0.0));
    }
    int32_t ty, tm, td;
    ejs_i128 tt, an, bn;
    relative_apply_duration(&rel, &a, &ty, &tm, &td, &tt, &an);
    relative_apply_duration(&rel, &b, &ty, &tm, &td, &tt, &bn);
    return NUMBER_TO_EJSVAL(an < bn ? -1.0 : (an > bn ? 1.0 : 0.0));
}

static EJS_NATIVE_FUNC(_ejs_TemporalDuration_prototype_with) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_DURATION, "Temporal.Duration.prototype.with");
    EJSTemporalDuration* d = EJSVAL_TO_TEMPORAL_DURATION(*_this);
    ejsval item = argc > 0 ? args[0] : _ejs_undefined;
    if (!EJSVAL_IS_OBJECT(item))
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "argument must be an object");
    DurationFields f = { d->years, d->months, d->weeks, d->days, d->hours,
                         d->minutes, d->seconds, d->milliseconds, d->microseconds, d->nanoseconds };
    static const char* const names[10] = {
        "days", "hours", "microseconds", "milliseconds", "minutes",
        "months", "nanoseconds", "seconds", "weeks", "years"
    };
    double* dest[10] = { &f.days, &f.hours, &f.us, &f.ms, &f.minutes,
                         &f.months, &f.ns, &f.seconds, &f.weeks, &f.years };
    EJSBool any = EJS_FALSE;
    for (int i = 0; i < 10; i++) {
        ejsval v = Get(item, _ejs_string_new_utf8(names[i]));
        if (!EJSVAL_IS_UNDEFINED(v)) {
            *dest[i] = to_integer_if_integral(v);
            any = EJS_TRUE;
        }
    }
    if (!any)
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "invalid duration-like object");
    if (!is_valid_duration(f.years, f.months, f.weeks, f.days, f.hours, f.minutes, f.seconds, f.ms, f.us, f.ns))
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "invalid duration");
    return _ejs_temporal_duration_new(f.years, f.months, f.weeks, f.days, f.hours,
                                      f.minutes, f.seconds, f.ms, f.us, f.ns);
}

static EJS_NATIVE_FUNC(_ejs_TemporalDuration_prototype_negated) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_DURATION, "Temporal.Duration.prototype.negated");
    EJSTemporalDuration* d = EJSVAL_TO_TEMPORAL_DURATION(*_this);
    return _ejs_temporal_duration_new(-d->years, -d->months, -d->weeks, -d->days, -d->hours,
                                      -d->minutes, -d->seconds, -d->milliseconds, -d->microseconds, -d->nanoseconds);
}

static EJS_NATIVE_FUNC(_ejs_TemporalDuration_prototype_abs) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_DURATION, "Temporal.Duration.prototype.abs");
    EJSTemporalDuration* d = EJSVAL_TO_TEMPORAL_DURATION(*_this);
    return _ejs_temporal_duration_new(fabs(d->years), fabs(d->months), fabs(d->weeks), fabs(d->days),
                                      fabs(d->hours), fabs(d->minutes), fabs(d->seconds),
                                      fabs(d->milliseconds), fabs(d->microseconds), fabs(d->nanoseconds));
}

static ejsval
duration_add_impl(ejsval durval, ejsval other_like, int negate)
{
    EJSTemporalDuration* d = EJSVAL_TO_TEMPORAL_DURATION(durval);
    DurationFields o;
    to_temporal_duration_fields(other_like, &o);
    if (d->years != 0 || d->months != 0 || d->weeks != 0
        || o.years != 0 || o.months != 0 || o.weeks != 0)
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "duration arithmetic with calendar units requires relativeTo");
    int sign = negate ? -1 : 1;
    // combine as exact ns (days are 24h here), then rebalance to the
    // larger of the two inputs' largest units
    ejs_i128 total = duration_time_ns(&o) * sign
        + ((ejs_i128)o.days) * NS_PER_DAY * sign
        + duration_time_ns((DurationFields*)&(DurationFields){
              0, 0, 0, 0, d->hours, d->minutes, d->seconds, d->milliseconds, d->microseconds, d->nanoseconds })
        + ((ejs_i128)d->days) * NS_PER_DAY;
    // DefaultTemporalLargestUnit of each operand (fields y/m/w are zero)
    int largest = 9; // ns
    double f1[7] = { d->days, d->hours, d->minutes, d->seconds, d->milliseconds, d->microseconds, d->nanoseconds };
    double f2[7] = { o.days, o.hours, o.minutes, o.seconds, o.ms, o.us, o.ns };
    for (int i = 0; i < 7; i++) {
        if (f1[i] != 0 || f2[i] != 0) {
            largest = 3 + i; // TUNIT_DAY .. TUNIT_NANO
            break;
        }
    }
    ejs_i128 max_time = ((ejs_i128)9007199254740992LL) * 1000000000LL;
    if (total >= max_time || total <= -max_time)
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "duration out of range");
    return balance_time_duration(total, (TUnit)largest, 0);
}

static EJS_NATIVE_FUNC(_ejs_TemporalDuration_prototype_add) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_DURATION, "Temporal.Duration.prototype.add");
    return duration_add_impl(*_this, argc > 0 ? args[0] : _ejs_undefined, 0);
}

static EJS_NATIVE_FUNC(_ejs_TemporalDuration_prototype_subtract) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_DURATION, "Temporal.Duration.prototype.subtract");
    return duration_add_impl(*_this, argc > 0 ? args[0] : _ejs_undefined, 1);
}

// TemporalDurationToString
static ejsval
duration_to_string(EJSTemporalDuration* d, int digits /* -1 auto */, RoundingMode mode, ejs_i128 increment)
{
    char buf[512] = "";
    int sign = duration_sign(d);
    // seconds + subseconds as one i128 ns quantity for precision handling
    ejs_i128 sec_ns = ((ejs_i128)d->seconds) * 1000000000LL
        + ((ejs_i128)d->milliseconds) * 1000000LL
        + ((ejs_i128)d->microseconds) * 1000LL
        + (ejs_i128)d->nanoseconds;
    double hours_f = d->hours, minutes_f = d->minutes, days_f = d->days;
    if (digits >= 0 || increment > 1) {
        // rounding rebalances the time part only up to the duration's
        // own largest time unit: seconds-only durations keep overlarge
        // seconds ("PT60S" stays), while carries from smaller fields
        // bubble as far as an existing hours/days field
        ejs_i128 orig_total = ((ejs_i128)d->days) * NS_PER_DAY
            + ((ejs_i128)d->hours) * 3600000000000LL
            + ((ejs_i128)d->minutes) * 60000000000LL + sec_ns;
        ejs_i128 time_total = round_i128_to_increment(orig_total, increment, mode);
        int cap = 6; // TUNIT index: default seconds
        if (d->days != 0) cap = 3;
        else if (d->hours != 0) cap = 4;
        else if (d->minutes != 0) cap = 5;
        ejs_i128 t = time_total < 0 ? -time_total : time_total;
        double dsign = time_total < 0 ? -1 : 1;
        days_f = 0; hours_f = 0; minutes_f = 0;
        if (cap <= 3) { days_f = dsign * (double)(t / NS_PER_DAY); t %= NS_PER_DAY; }
        if (cap <= 4) { hours_f = dsign * (double)(t / 3600000000000LL); t %= 3600000000000LL; }
        if (cap <= 5) { minutes_f = dsign * (double)(t / 60000000000LL); t %= 60000000000LL; }
        sec_ns = (time_total < 0 ? -t : t);
        if (!is_valid_duration(d->years, d->months, d->weeks, days_f, hours_f, minutes_f,
                               dsign * (double)((t < 0 ? -t : t) / 1000000000LL), 0, 0,
                               dsign * (double)((t < 0 ? -t : t) % 1000000000LL)))
            _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "rounded duration out of range");
    }
    if (sign < 0)
        strlcat(buf, "-", sizeof(buf));
    strlcat(buf, "P", sizeof(buf));
    char num[64];
    if (d->years != 0) { snprintf(num, sizeof(num), "%.0fY", fabs(d->years)); strlcat(buf, num, sizeof(buf)); }
    if (d->months != 0) { snprintf(num, sizeof(num), "%.0fM", fabs(d->months)); strlcat(buf, num, sizeof(buf)); }
    if (d->weeks != 0) { snprintf(num, sizeof(num), "%.0fW", fabs(d->weeks)); strlcat(buf, num, sizeof(buf)); }
    if (days_f != 0) { snprintf(num, sizeof(num), "%.0fD", fabs(days_f)); strlcat(buf, num, sizeof(buf)); }
    ejs_i128 abs_sec_ns = sec_ns < 0 ? -sec_ns : sec_ns;
    EJSBool have_time = hours_f != 0 || minutes_f != 0 || abs_sec_ns != 0;
    EJSBool show_seconds = abs_sec_ns != 0 || digits >= 0
        || (d->years == 0 && d->months == 0 && d->weeks == 0 && days_f == 0
            && hours_f == 0 && minutes_f == 0);
    if (have_time || show_seconds)
        strlcat(buf, "T", sizeof(buf));
    if (hours_f != 0) { snprintf(num, sizeof(num), "%.0fH", fabs(hours_f)); strlcat(buf, num, sizeof(buf)); }
    if (minutes_f != 0) { snprintf(num, sizeof(num), "%.0fM", fabs(minutes_f)); strlcat(buf, num, sizeof(buf)); }
    if (show_seconds) {
        ejs_i128 whole = abs_sec_ns / 1000000000LL;
        int64_t frac = (int64_t)(abs_sec_ns % 1000000000LL);
        // whole seconds can exceed 2^53 only in invalid durations
        snprintf(num, sizeof(num), "%lld", (long long)whole);
        strlcat(buf, num, sizeof(buf));
        if (digits == -1) {
            if (frac != 0) {
                char fbuf[16];
                snprintf(fbuf, sizeof(fbuf), "%09lld", (long long)frac);
                int len = 9;
                while (len > 1 && fbuf[len - 1] == '0') len--;
                fbuf[len] = 0;
                strlcat(buf, ".", sizeof(buf));
                strlcat(buf, fbuf, sizeof(buf));
            }
        } else if (digits > 0) {
            char fbuf[16];
            snprintf(fbuf, sizeof(fbuf), "%09lld", (long long)frac);
            fbuf[digits] = 0;
            strlcat(buf, ".", sizeof(buf));
            strlcat(buf, fbuf, sizeof(buf));
        }
        strlcat(buf, "S", sizeof(buf));
    }
    return _ejs_string_new_utf8(buf);
}

static EJS_NATIVE_FUNC(_ejs_TemporalDuration_prototype_toString) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_DURATION, "Temporal.Duration.prototype.toString");
    EJSTemporalDuration* d = EJSVAL_TO_TEMPORAL_DURATION(*_this);
    ejsval options = get_options_object(argc > 0 ? args[0] : _ejs_undefined);
    SecPrecision p;
    get_sec_precision(options, &p);
    if (p.digits == -2)
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "smallestUnit must be 'second' or smaller");
    return duration_to_string(d, p.digits, p.mode, p.increment);
}

static EJS_NATIVE_FUNC(_ejs_TemporalDuration_prototype_toJSON) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_DURATION, "Temporal.Duration.prototype.toJSON");
    return duration_to_string(EJSVAL_TO_TEMPORAL_DURATION(*_this), -1, ROUND_TRUNC, 1);
}

static EJS_NATIVE_FUNC(_ejs_TemporalDuration_prototype_toLocaleString) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_DURATION, "Temporal.Duration.prototype.toLocaleString");
    return duration_to_string(EJSVAL_TO_TEMPORAL_DURATION(*_this), -1, ROUND_TRUNC, 1);
}


// ------------------------------------------------------ with() methods

static EJS_NATIVE_FUNC(_ejs_TemporalPlainDate_prototype_with) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINDATE, "Temporal.PlainDate.prototype.with");
    EJSTemporalPlainDate* pd = EJSVAL_TO_TEMPORAL_PLAINDATE(*_this);
    ejsval item = argc > 0 ? args[0] : _ejs_undefined;
    check_partial_temporal_object(item);
    FieldsBag bag;
    read_fields_bag(item, BAG_DATE, &bag);
    require_any_field(&bag);
    merge_date_fields(&bag, pd->year, pd->month, pd->day);
    EJSBool constrain = get_overflow_option(get_options_object(argc > 1 ? args[1] : _ejs_undefined));
    int32_t y, m, d;
    iso_date_from_bag(&bag, constrain, &y, &m, &d);
    return _ejs_temporal_plain_date_new(y, m, d, pd->calendar);
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainTime_prototype_with) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINTIME, "Temporal.PlainTime.prototype.with");
    EJSTemporalPlainTime* pt = EJSVAL_TO_TEMPORAL_PLAINTIME(*_this);
    ejsval item = argc > 0 ? args[0] : _ejs_undefined;
    check_partial_temporal_object(item);
    FieldsBag bag;
    read_fields_bag(item, BAG_TIME, &bag);
    require_any_field(&bag);
    if (!bag.has_hour) bag.hour = pt->hour;
    if (!bag.has_minute) bag.minute = pt->minute;
    if (!bag.has_second) bag.second = pt->second;
    if (!bag.has_ms) bag.ms = pt->millisecond;
    if (!bag.has_us) bag.us = pt->microsecond;
    if (!bag.has_ns) bag.ns = pt->nanosecond;
    EJSBool constrain = get_overflow_option(get_options_object(argc > 1 ? args[1] : _ejs_undefined));
    int32_t h, mi, s, ms, us, ns;
    regulate_time_from_bag(&bag, constrain, &h, &mi, &s, &ms, &us, &ns);
    return _ejs_temporal_plain_time_new(h, mi, s, ms, us, ns);
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainDateTime_prototype_with) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINDATETIME, "Temporal.PlainDateTime.prototype.with");
    EJSTemporalPlainDateTime* pdt = EJSVAL_TO_TEMPORAL_PLAINDATETIME(*_this);
    ejsval item = argc > 0 ? args[0] : _ejs_undefined;
    check_partial_temporal_object(item);
    FieldsBag bag;
    read_fields_bag(item, BAG_DATE | BAG_TIME, &bag);
    require_any_field(&bag);
    merge_date_fields(&bag, pdt->year, pdt->month, pdt->day);
    if (!bag.has_hour) bag.hour = pdt->hour;
    if (!bag.has_minute) bag.minute = pdt->minute;
    if (!bag.has_second) bag.second = pdt->second;
    if (!bag.has_ms) bag.ms = pdt->millisecond;
    if (!bag.has_us) bag.us = pdt->microsecond;
    if (!bag.has_ns) bag.ns = pdt->nanosecond;
    EJSBool constrain = get_overflow_option(get_options_object(argc > 1 ? args[1] : _ejs_undefined));
    int32_t y, m, d, h, mi, s, ms, us, ns;
    iso_date_from_bag(&bag, constrain, &y, &m, &d);
    regulate_time_from_bag(&bag, constrain, &h, &mi, &s, &ms, &us, &ns);
    if (!_ejs_temporal_iso_datetime_within_limits(y, m, d, h, mi, s, ms, us, ns))
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "datetime out of range");
    return _ejs_temporal_plain_datetime_new(y, m, d, h, mi, s, ms, us, ns, pdt->calendar);
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainYearMonth_prototype_with) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINYEARMONTH, "Temporal.PlainYearMonth.prototype.with");
    EJSTemporalPlainYearMonth* ym = EJSVAL_TO_TEMPORAL_PLAINYEARMONTH(*_this);
    ejsval item = argc > 0 ? args[0] : _ejs_undefined;
    check_partial_temporal_object(item);
    FieldsBag bag;
    read_fields_bag(item, BAG_YM, &bag);
    if (!bag.has_year && !bag.has_month && !bag.has_month_code)
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "at least one field is required");
    if (!bag.has_year) { bag.year = ym->year; bag.has_year = EJS_TRUE; }
    if (!bag.has_month && !bag.has_month_code) { bag.month = ym->month; bag.has_month = EJS_TRUE; }
    EJSBool constrain = get_overflow_option(get_options_object(argc > 1 ? args[1] : _ejs_undefined));
    int32_t month = resolve_iso_month(&bag);
    int32_t y = (int32_t)bag.year, m = month, d = 1;
    regulate_iso_date(&y, &m, &d, constrain);
    if (!iso_yearmonth_within_limits(y, m))
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "year-month out of range");
    return _ejs_temporal_plain_yearmonth_new(y, m, 1, ym->calendar);
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainMonthDay_prototype_with) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINMONTHDAY, "Temporal.PlainMonthDay.prototype.with");
    EJSTemporalPlainMonthDay* md = EJSVAL_TO_TEMPORAL_PLAINMONTHDAY(*_this);
    ejsval item = argc > 0 ? args[0] : _ejs_undefined;
    check_partial_temporal_object(item);
    FieldsBag bag;
    read_fields_bag(item, BAG_MD, &bag);
    if (!bag.has_year && !bag.has_month && !bag.has_month_code && !bag.has_day)
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "at least one field is required");
    if (!bag.has_day) { bag.day = md->day; bag.has_day = EJS_TRUE; }
    if (!bag.has_month && !bag.has_month_code) { bag.month = md->month; bag.has_month = EJS_TRUE; }
    if (!bag.has_year) { bag.year = 1972; bag.has_year = EJS_TRUE; }
    EJSBool constrain = get_overflow_option(get_options_object(argc > 1 ? args[1] : _ejs_undefined));
    int32_t month = resolve_iso_month(&bag);
    int32_t y = (int32_t)bag.year, m = month, d = (int32_t)bag.day;
    regulate_iso_date(&y, &m, &d, constrain);
    return _ejs_temporal_plain_monthday_new(m, d, 1972, md->calendar);
}

// ------------------------------------------------ units and differences

// GetTemporalUnitValuedOption: singular and plural spellings + "auto".
// Returns TUNIT_AUTO when absent (callers apply their fallback).
static TUnit
get_unit_option(ejsval options, const char* name, EJSBool allow_auto)
{
    static const char* const singular[] = {
        "year", "month", "week", "day", "hour", "minute", "second",
        "millisecond", "microsecond", "nanosecond"
    };
    static const char* const plural[] = {
        "years", "months", "weeks", "days", "hours", "minutes", "seconds",
        "milliseconds", "microseconds", "nanoseconds"
    };
    if (EJSVAL_IS_UNDEFINED(options))
        return TUNIT_AUTO;
    ejsval v = Get(options, _ejs_string_new_utf8(name));
    if (EJSVAL_IS_UNDEFINED(v))
        return TUNIT_AUTO;
    ejsval str = ToString(v);
    char* s = string_to_ascii(str);
    TUnit result = TUNIT_AUTO;
    EJSBool found = EJS_FALSE;
    if (s) {
        if (allow_auto && !strcmp(s, "auto")) {
            found = EJS_TRUE;
        } else {
            for (int i = 0; i < 10; i++) {
                if (!strcmp(s, singular[i]) || !strcmp(s, plural[i])) {
                    result = (TUnit)i;
                    found = EJS_TRUE;
                    break;
                }
            }
        }
    }
    if (!found) {
        char buf[128];
        snprintf(buf, sizeof(buf), "invalid value for option %s", name);
        free(s);
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, buf);
    }
    free(s);
    return result;
}

static double
get_rounding_increment_option(ejsval options)
{
    if (EJSVAL_IS_UNDEFINED(options))
        return 1;
    ejsval v = Get(options, _ejs_string_new_utf8("roundingIncrement"));
    if (EJSVAL_IS_UNDEFINED(v))
        return 1;
    double d = to_integer_with_truncation(v);
    if (d < 1 || d > 1000000000.0)
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "roundingIncrement out of range");
    return d;
}

// ValidateTemporalRoundingIncrement
static void
validate_increment(double increment, int64_t dividend, EJSBool inclusive)
{
    int64_t max = inclusive ? dividend : dividend - 1;
    if (increment > (double)max || fmod((double)dividend, increment) != 0)
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "invalid roundingIncrement");
}

typedef struct {
    TUnit largest, smallest;
    double increment;
    RoundingMode mode;
    EJSBool check_bounds; // zoned contexts: nudge bounds must be
                          // representable dates
} DiffSettings;

// GetDifferenceSettings.  unit_min/unit_max bound the allowed group
// (inclusive, TUNIT ordering: YEAR is "largest").  Read order:
// largestUnit, roundingIncrement, roundingMode, smallestUnit.
static void
get_difference_settings(ejsval options, EJSBool is_since,
                        TUnit unit_min, TUnit unit_max,
                        TUnit fallback_smallest, TUnit largest_default,
                        DiffSettings* out)
{
    out->check_bounds = EJS_FALSE;
    TUnit largest = get_unit_option(options, "largestUnit", EJS_TRUE);
    double increment = get_rounding_increment_option(options);
    RoundingMode mode = get_rounding_mode_option(options, ROUND_TRUNC);
    TUnit smallest = get_unit_option(options, "smallestUnit", EJS_FALSE);
    if (smallest == TUNIT_AUTO) smallest = fallback_smallest;
    if (smallest < unit_min || smallest > unit_max)
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "smallestUnit is not allowed here");
    TUnit effective_largest_default = largest_default < smallest ? largest_default : smallest;
    if (largest == TUNIT_AUTO) largest = effective_largest_default;
    if (largest < unit_min || largest > unit_max)
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "largestUnit is not allowed here");
    if (largest > smallest)
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "largestUnit cannot be smaller than smallestUnit");
    if (smallest >= TUNIT_HOUR) {
        static const int64_t max_increments[] = { 24, 60, 60, 1000, 1000, 1000 };
        validate_increment(increment, max_increments[smallest - TUNIT_HOUR], EJS_FALSE);
    }
    if (is_since) {
        // NegateRoundingMode
        switch (mode) {
        case ROUND_CEIL: mode = ROUND_FLOOR; break;
        case ROUND_FLOOR: mode = ROUND_CEIL; break;
        case ROUND_HALF_CEIL: mode = ROUND_HALF_FLOOR; break;
        case ROUND_HALF_FLOOR: mode = ROUND_HALF_CEIL; break;
        default: break;
        }
    }
    out->largest = largest;
    out->smallest = smallest;
    out->increment = increment;
    out->mode = mode;
}

// balance a time quantity (ns) into a Duration object with fields no
// larger than largest (must be TUNIT_DAY or smaller)
static ejsval
balance_time_duration(ejs_i128 total_ns, TUnit largest, int negate_result)
{
    if (negate_result) total_ns = -total_ns;
    EJSBool neg = total_ns < 0;
    ejs_i128 t = neg ? -total_ns : total_ns;
    double f[7] = { 0, 0, 0, 0, 0, 0, 0 }; // day..ns
    for (int u = TUNIT_DAY; u <= TUNIT_NANO; u++) {
        if (u < largest) continue;
        if (u < TUNIT_NANO) {
            int64_t per = unit_ns((TUnit)u);
            f[u - TUNIT_DAY] = (double)(t / per);
            t %= per;
        } else {
            f[u - TUNIT_DAY] = (double)t;
        }
    }
    double s = neg ? -1 : 1;
    return _ejs_temporal_duration_new(0, 0, 0, s * f[0], s * f[1], s * f[2], s * f[3],
                                      s * f[4], s * f[5], s * f[6]);
}

// Instant.until/since
static ejsval
instant_diff_impl(ejsval instant, ejsval other_like, ejsval options, EJSBool is_since)
{
    EJSTemporalInstant* in = EJSVAL_TO_TEMPORAL_INSTANT(instant);
    ejs_i128 other = to_temporal_instant_ns(other_like);
    DiffSettings s;
    get_difference_settings(get_options_object(options), is_since,
                            TUNIT_HOUR, TUNIT_NANO, TUNIT_NANO, TUNIT_SECOND, &s);
    ejs_i128 diff = other - in->epoch_ns;
    diff = round_i128_to_increment(diff, (ejs_i128)(int64_t)s.increment * unit_ns(s.smallest), s.mode);
    return balance_time_duration(diff, s.largest, is_since);
}

static EJS_NATIVE_FUNC(_ejs_TemporalInstant_prototype_until) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_INSTANT, "Temporal.Instant.prototype.until");
    return instant_diff_impl(*_this, argc > 0 ? args[0] : _ejs_undefined,
                             argc > 1 ? args[1] : _ejs_undefined, EJS_FALSE);
}

static EJS_NATIVE_FUNC(_ejs_TemporalInstant_prototype_since) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_INSTANT, "Temporal.Instant.prototype.since");
    return instant_diff_impl(*_this, argc > 0 ? args[0] : _ejs_undefined,
                             argc > 1 ? args[1] : _ejs_undefined, EJS_TRUE);
}

// round argument handling shared by Instant/PlainTime/PlainDateTime/ZDT:
// a bare unit string or an options object with required smallestUnit.
// Read order in the object form: roundingIncrement, roundingMode,
// smallestUnit.  The day unit's maximum increment (1) is inclusive
// regardless of the flag.
static void
get_round_settings(ejsval round_to, TUnit unit_min, TUnit unit_max,
                   const int64_t* max_increments /* per unit from unit_min */,
                   EJSBool inclusive,
                   TUnit* out_unit, double* out_increment, RoundingMode* out_mode)
{
    if (EJSVAL_IS_UNDEFINED(round_to))
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "roundTo is required");
    double increment = 1;
    RoundingMode mode = ROUND_HALF_EXPAND;
    TUnit unit;
    if (EJSVAL_IS_STRING(round_to)) {
        // paramString: treat as { smallestUnit: round_to }
        char* s = string_to_ascii(round_to);
        if (!s)
            _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "invalid smallestUnit");
        static const char* const singular[] = {
            "year", "month", "week", "day", "hour", "minute", "second",
            "millisecond", "microsecond", "nanosecond"
        };
        static const char* const plural[] = {
            "years", "months", "weeks", "days", "hours", "minutes", "seconds",
            "milliseconds", "microseconds", "nanoseconds"
        };
        int found = -1;
        for (int i = 0; i < 10; i++)
            if (!strcmp(s, singular[i]) || !strcmp(s, plural[i])) { found = i; break; }
        free(s);
        if (found < 0)
            _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "invalid smallestUnit");
        unit = (TUnit)found;
    } else if (EJSVAL_IS_OBJECT(round_to)) {
        increment = get_rounding_increment_option(round_to);
        mode = get_rounding_mode_option(round_to, ROUND_HALF_EXPAND);
        unit = get_unit_option(round_to, "smallestUnit", EJS_FALSE);
        if (unit == TUNIT_AUTO)
            _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "smallestUnit is required");
    } else {
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "roundTo must be a string or object");
        EJS_NOT_REACHED();
        return;
    }
    if (unit < unit_min || unit > unit_max)
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "smallestUnit is not allowed here");
    validate_increment(increment, max_increments[unit - unit_min],
                       unit == TUNIT_DAY ? EJS_TRUE : inclusive);
    *out_unit = unit;
    *out_increment = increment;
    *out_mode = mode;
}

static EJS_NATIVE_FUNC(_ejs_TemporalInstant_prototype_round) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_INSTANT, "Temporal.Instant.prototype.round");
    EJSTemporalInstant* in = EJSVAL_TO_TEMPORAL_INSTANT(*_this);
    static const int64_t max_increments[] = {
        24, 1440, 86400, 86400000LL, 86400000000LL, 86400000000000LL
    };
    TUnit unit;
    double increment;
    RoundingMode mode;
    get_round_settings(argc > 0 ? args[0] : _ejs_undefined, TUNIT_HOUR, TUNIT_NANO,
                       max_increments, EJS_TRUE, &unit, &increment, &mode);
    ejs_i128 rounded = round_i128_as_if_positive(in->epoch_ns,
                                                 (ejs_i128)(int64_t)increment * unit_ns(unit), mode);
    if (rounded < EJS_TEMPORAL_NS_MIN_INSTANT || rounded > EJS_TEMPORAL_NS_MAX_INSTANT)
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "instant out of range");
    return _ejs_temporal_instant_new(rounded);
}

// PlainTime.until/since
static ejsval
plaintime_diff_impl(ejsval time, ejsval other_like, ejsval options, EJSBool is_since)
{
    EJSTemporalPlainTime* pt = EJSVAL_TO_TEMPORAL_PLAINTIME(time);
    int32_t h, mi, s2, ms, us, ns;
    to_temporal_time_fields(other_like, _ejs_undefined, &h, &mi, &s2, &ms, &us, &ns);
    DiffSettings s;
    get_difference_settings(get_options_object(options), is_since,
                            TUNIT_HOUR, TUNIT_NANO, TUNIT_NANO, TUNIT_HOUR, &s);
    ejs_i128 diff = time_fields_to_ns(h, mi, s2, ms, us, ns)
        - time_fields_to_ns(pt->hour, pt->minute, pt->second, pt->millisecond, pt->microsecond, pt->nanosecond);
    diff = round_i128_to_increment(diff, (ejs_i128)(int64_t)s.increment * unit_ns(s.smallest), s.mode);
    return balance_time_duration(diff, s.largest, is_since);
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainTime_prototype_until) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINTIME, "Temporal.PlainTime.prototype.until");
    return plaintime_diff_impl(*_this, argc > 0 ? args[0] : _ejs_undefined,
                               argc > 1 ? args[1] : _ejs_undefined, EJS_FALSE);
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainTime_prototype_since) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINTIME, "Temporal.PlainTime.prototype.since");
    return plaintime_diff_impl(*_this, argc > 0 ? args[0] : _ejs_undefined,
                               argc > 1 ? args[1] : _ejs_undefined, EJS_TRUE);
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainTime_prototype_round) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINTIME, "Temporal.PlainTime.prototype.round");
    EJSTemporalPlainTime* pt = EJSVAL_TO_TEMPORAL_PLAINTIME(*_this);
    static const int64_t max_increments[] = { 24, 60, 60, 1000, 1000, 1000 };
    TUnit unit;
    double increment;
    RoundingMode mode;
    get_round_settings(argc > 0 ? args[0] : _ejs_undefined, TUNIT_HOUR, TUNIT_NANO,
                       max_increments, EJS_FALSE, &unit, &increment, &mode);
    ejs_i128 t = time_fields_to_ns(pt->hour, pt->minute, pt->second, pt->millisecond, pt->microsecond, pt->nanosecond);
    t = round_i128_to_increment(t, (ejs_i128)(int64_t)increment * unit_ns(unit), mode);
    t = i128_floormod(t, NS_PER_DAY);
    int32_t h, mi, s, ms, us, ns;
    ns_to_time_fields(t, &h, &mi, &s, &ms, &us, &ns);
    return _ejs_temporal_plain_time_new(h, mi, s, ms, us, ns);
}

// DifferenceISODate
static void
difference_iso_date(int32_t y1, int32_t m1, int32_t d1, int32_t y2, int32_t m2, int32_t d2,
                    TUnit largest, double* years, double* months, double* weeks, double* days)
{
    *years = *months = *weeks = *days = 0;
    int sign = -compare_iso_date(y1, m1, d1, y2, m2, d2); // +1 when date2 is later
    if (sign == 0) return;
    if (largest == TUNIT_YEAR || largest == TUNIT_MONTH) {
        // widest whole-year candidate not surpassing date2
        double candidate_years = (double)(y2 - y1);
        if (candidate_years != 0) candidate_years -= sign;
        for (;;) {
            int32_t ry, rm, rd;
            add_iso_date_unchecked(y1, m1, d1, candidate_years + sign, 0, 0, 0, &ry, &rm, &rd);
            if (compare_iso_date(ry, rm, rd, y2, m2, d2) * sign > 0) break;
            candidate_years += sign;
        }
        double candidate_months = 0;
        for (;;) {
            int32_t ry, rm, rd;
            add_iso_date_unchecked(y1, m1, d1, candidate_years, candidate_months + sign, 0, 0, &ry, &rm, &rd);
            if (compare_iso_date(ry, rm, rd, y2, m2, d2) * sign > 0) break;
            candidate_months += sign;
            if (fabs(candidate_months) > 13) {
                // months within a year window: bounded, but guard anyway
                break;
            }
        }
        if (largest == TUNIT_MONTH) {
            candidate_months += candidate_years * 12;
            candidate_years = 0;
        }
        *years = candidate_years;
        *months = candidate_months;
        int32_t iy, im, id;
        add_iso_date_unchecked(y1, m1, d1, candidate_years, candidate_months, 0, 0, &iy, &im, &id);
        *days = (double)(_ejs_temporal_iso_date_to_epoch_days(y2, m2, d2)
                         - _ejs_temporal_iso_date_to_epoch_days(iy, im, id));
    } else {
        *days = (double)(_ejs_temporal_iso_date_to_epoch_days(y2, m2, d2)
                         - _ejs_temporal_iso_date_to_epoch_days(y1, m1, d1));
    }
    if (largest == TUNIT_WEEK) {
        *weeks = trunc(*days / 7);
        *days = fmod(*days, 7);
    }
}

// NudgeToCalendarUnit (date-only): round the final calendar unit using
// day-count progress between the bounding whole-unit dates.
static EJSBool round_date_duration_check_bounds = EJS_FALSE;

static void
round_date_duration(int32_t y1, int32_t m1, int32_t d1, int32_t y2, int32_t m2, int32_t d2,
                    ejs_i128 time_remainder_ns, // extra sub-day time toward date2 (signed)
                    TUnit smallest, double increment, RoundingMode mode, TUnit largest,
                    double* years, double* months, double* weeks, double* days,
                    ejs_i128* leftover_time_ns)
{
    difference_iso_date(y1, m1, d1, y2, m2, d2, largest, years, months, weeks, days);
    *leftover_time_ns = time_remainder_ns;
    if (smallest == TUNIT_DAY && increment == 1 && time_remainder_ns == 0)
        return;
    if (smallest >= TUNIT_HOUR)
        return; // caller rounds the time part
    // whole units in the rounded field, everything smaller folds into
    // the fractional progress.  difference_iso_date only splits out
    // weeks when they are the largest unit, so extract them here when
    // rounding to weeks under a larger largestUnit.
    if (smallest == TUNIT_WEEK && largest != TUNIT_WEEK) {
        double w = trunc(*days / 7);
        *weeks += w;
        *days -= w * 7;
    }
    double whole;
    switch (smallest) {
    case TUNIT_YEAR: whole = *years; break;
    case TUNIT_MONTH: whole = *months; break;
    case TUNIT_WEEK: whole = *weeks; break;
    default: whole = *days; break;
    }
    int sign = -compare_iso_date(y1, m1, d1, y2, m2, d2);
    if (sign == 0 && time_remainder_ns != 0)
        sign = time_remainder_ns > 0 ? 1 : -1;
    if (sign == 0) sign = 1;
    // bounds are increment-aligned multiples of the rounded unit
    double aligned = trunc(whole / increment) * increment;
    double y_lo = 0, mo_lo = 0, w_lo = 0, d_lo = 0;
    double y_hi = 0, mo_hi = 0, w_hi = 0, d_hi = 0;
    switch (smallest) {
    case TUNIT_YEAR: y_lo = aligned; y_hi = aligned + sign * increment; break;
    case TUNIT_MONTH: mo_lo = aligned; mo_hi = aligned + sign * increment;
        y_lo = y_hi = *years; break;
    case TUNIT_WEEK: w_lo = aligned; w_hi = aligned + sign * increment;
        y_lo = y_hi = *years; mo_lo = mo_hi = *months; break;
    default: d_lo = aligned; d_hi = aligned + sign * increment;
        y_lo = y_hi = *years; mo_lo = mo_hi = *months; w_lo = w_hi = *weeks; break;
    }
    whole = aligned;
    if (smallest != TUNIT_DAY) {
        // lower bound uses only the whole units of the rounded field
        // (drop smaller date fields into the fraction)
    }
    int32_t ly, lm, ld, hy, hm, hd;
    if (smallest == TUNIT_DAY) {
        add_iso_date_unchecked(y1, m1, d1, y_lo, mo_lo, w_lo, d_lo, &ly, &lm, &ld);
        add_iso_date_unchecked(y1, m1, d1, y_hi, mo_hi, w_hi, d_hi, &hy, &hm, &hd);
    } else {
        add_iso_date_unchecked(y1, m1, d1, y_lo, mo_lo, w_lo, 0, &ly, &lm, &ld);
        add_iso_date_unchecked(y1, m1, d1, y_hi, mo_hi, w_hi, 0, &hy, &hm, &hd);
    }
    ejs_i128 lo_ns = ((ejs_i128)_ejs_temporal_iso_date_to_epoch_days(ly, lm, ld)) * NS_PER_DAY;
    ejs_i128 hi_ns = ((ejs_i128)_ejs_temporal_iso_date_to_epoch_days(hy, hm, hd)) * NS_PER_DAY;
    ejs_i128 target = ((ejs_i128)_ejs_temporal_iso_date_to_epoch_days(y2, m2, d2)) * NS_PER_DAY
        + time_remainder_ns;
    ejs_i128 denom = hi_ns - lo_ns;
    ejs_i128 numer = target - lo_ns;
    // round whole + numer/denom (in increments) under mode
    double total_in_increments = (whole / increment) + (double)(int64_t)(numer / (denom == 0 ? 1 : denom));
    // exact rounding via i128 comparison instead of double fractions
    ejs_i128 chosen;
    if (denom == 0) {
        chosen = 0;
    } else {
        // value = lo + numer, snap to 0 or denom under mode
        ejs_i128 r = numer;
        EJSBool positive = sign > 0;
        ejs_i128 lower = 0, upper = denom;
        ejs_i128 twice = (r < 0 ? -r : r) * 2;
        ejs_i128 dabs = denom < 0 ? -denom : denom;
        EJSBool pick_upper;
        switch (mode) {
        case ROUND_CEIL: pick_upper = positive ? (r != 0) : EJS_FALSE; break;
        case ROUND_FLOOR: pick_upper = positive ? EJS_FALSE : (r != 0); break;
        case ROUND_EXPAND: pick_upper = (r != 0); break;
        case ROUND_TRUNC: pick_upper = EJS_FALSE; break;
        case ROUND_HALF_CEIL: pick_upper = positive ? (twice >= dabs) : (twice > dabs); break;
        case ROUND_HALF_FLOOR: pick_upper = positive ? (twice > dabs) : (twice >= dabs); break;
        case ROUND_HALF_EXPAND: pick_upper = (twice >= dabs); break;
        case ROUND_HALF_TRUNC: pick_upper = (twice > dabs); break;
        default: { // halfEven
            if (twice < dabs) pick_upper = EJS_FALSE;
            else if (twice > dabs) pick_upper = EJS_TRUE;
            else {
                long long incs = (long long)(whole / increment);
                pick_upper = (incs % 2 != 0);
            }
            break;
        }
        }
        (void)lower; (void)upper; (void)total_in_increments;
        chosen = pick_upper ? 1 : 0;
    }
    if (chosen) {
        // the bumped end point is exactly a whole-unit boundary:
        // re-derive the whole duration from it so a bumped field
        // rebalances into larger units (12 months -> 1 year)
        difference_iso_date(y1, m1, d1, hy, hm, hd, largest, years, months, weeks, days);
        if (smallest == TUNIT_WEEK && largest != TUNIT_WEEK) {
            double w = trunc(*days / 7);
            *weeks += w;
            *days -= w * 7;
        }
        switch (smallest) {
        case TUNIT_YEAR: *months = 0; *weeks = 0; *days = 0; break;
        case TUNIT_MONTH: *weeks = 0; *days = 0; break;
        case TUNIT_WEEK: *days = 0; break;
        default: break;
        }
    } else {
        switch (smallest) {
        case TUNIT_YEAR: *years = whole; *months = 0; *weeks = 0; *days = 0; break;
        case TUNIT_MONTH: *months = whole; *weeks = 0; *days = 0; break;
        case TUNIT_WEEK: *weeks = whole; *days = 0; break;
        default: *days = whole; break;
        }
    }
    *leftover_time_ns = 0;
}

// PlainDate.until/since
static ejsval
plaindate_diff_impl(ejsval date, ejsval other_like, ejsval options, EJSBool is_since)
{
    EJSTemporalPlainDate* pd = EJSVAL_TO_TEMPORAL_PLAINDATE(date);
    int32_t y2, m2, d2;
    ejsval cal;
    to_temporal_date_fields(other_like, _ejs_undefined, &y2, &m2, &d2, &cal);
    if (!SameValue(pd->calendar, cal))
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "cannot difference dates with different calendars");
    DiffSettings s;
    get_difference_settings(get_options_object(options), is_since,
                            TUNIT_YEAR, TUNIT_DAY, TUNIT_DAY, TUNIT_DAY, &s);
    double years, months, weeks, days;
    if (s.smallest == TUNIT_DAY) {
        difference_iso_date(pd->year, pd->month, pd->day, y2, m2, d2, s.largest,
                            &years, &months, &weeks, &days);
        if (s.increment != 1) {
            ejs_i128 dns = (ejs_i128)(int64_t)days * NS_PER_DAY;
            dns = round_i128_to_increment(dns, (ejs_i128)(int64_t)s.increment * NS_PER_DAY, s.mode);
            days = (double)(int64_t)(dns / NS_PER_DAY);
        }
    } else {
        ejs_i128 leftover;
        round_date_duration(pd->year, pd->month, pd->day, y2, m2, d2, 0,
                            s.smallest, s.increment, s.mode, s.largest,
                            &years, &months, &weeks, &days, &leftover);
    }
    double sgn = is_since ? -1 : 1;
    return _ejs_temporal_duration_new(sgn * years, sgn * months, sgn * weeks, sgn * days, 0, 0, 0, 0, 0, 0);
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainDate_prototype_until) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINDATE, "Temporal.PlainDate.prototype.until");
    return plaindate_diff_impl(*_this, argc > 0 ? args[0] : _ejs_undefined,
                               argc > 1 ? args[1] : _ejs_undefined, EJS_FALSE);
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainDate_prototype_since) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINDATE, "Temporal.PlainDate.prototype.since");
    return plaindate_diff_impl(*_this, argc > 0 ? args[0] : _ejs_undefined,
                               argc > 1 ? args[1] : _ejs_undefined, EJS_TRUE);
}

// core datetime difference over raw ISO fields; t1/t2 are
// times-of-day in ns.  Fills a signed DurationFields.
static void
datetime_diff_to_duration(int32_t y1, int32_t m1, int32_t d1, ejs_i128 t1,
                          int32_t y2, int32_t m2, int32_t d2, ejs_i128 t2,
                          const DiffSettings* s, DurationFields* out)
{
    memset(out, 0, sizeof(*out));
    if (s->largest >= TUNIT_HOUR) {
        ejs_i128 total = (((ejs_i128)_ejs_temporal_iso_date_to_epoch_days(y2, m2, d2)
                           - _ejs_temporal_iso_date_to_epoch_days(y1, m1, d1)) * NS_PER_DAY)
            + (t2 - t1);
        total = round_i128_to_increment(total, (ejs_i128)(int64_t)s->increment * unit_ns(s->smallest), s->mode);
        EJSBool neg = total < 0;
        ejs_i128 t = neg ? -total : total;
        double f[7] = { 0, 0, 0, 0, 0, 0, 0 };
        for (int u = TUNIT_DAY; u <= TUNIT_NANO; u++) {
            if (u < s->largest) continue;
            if (u < TUNIT_NANO) {
                int64_t per = unit_ns((TUnit)u);
                f[u - TUNIT_DAY] = (double)(t / per);
                t %= per;
            } else {
                f[u - TUNIT_DAY] = (double)t;
            }
        }
        double sg = neg ? -1 : 1;
        out->days = sg * f[0]; out->hours = sg * f[1]; out->minutes = sg * f[2];
        out->seconds = sg * f[3]; out->ms = sg * f[4]; out->us = sg * f[5]; out->ns = sg * f[6];
        return;
    }
    // date+time difference: borrow a day if the time part opposes the sign
    int32_t ay2 = y2, am2 = m2, ad2 = d2;
    ejs_i128 time_diff = t2 - t1;
    int date_sign = -compare_iso_date(y1, m1, d1, y2, m2, d2);
    if (date_sign != 0 && time_diff != 0 && ((time_diff > 0) ? 1 : -1) != date_sign) {
        int64_t days = _ejs_temporal_iso_date_to_epoch_days(y2, m2, d2) - date_sign;
        _ejs_temporal_epoch_days_to_iso_date(days, &ay2, &am2, &ad2);
        time_diff += (ejs_i128)date_sign * NS_PER_DAY;
    }
    if (s->smallest >= TUNIT_HOUR) {
        double years, months, weeks, days;
        difference_iso_date(y1, m1, d1, ay2, am2, ad2, s->largest,
                            &years, &months, &weeks, &days);
        ejs_i128 rounded = round_i128_to_increment(time_diff,
                                                   (ejs_i128)(int64_t)s->increment * unit_ns(s->smallest), s->mode);
        // rounding can carry into a full day; the extra day may bubble
        // through months and years, so re-derive the date duration from
        // the shifted end point
        if (rounded == NS_PER_DAY || rounded == -NS_PER_DAY) {
            int64_t shifted = _ejs_temporal_iso_date_to_epoch_days(ay2, am2, ad2)
                + (rounded > 0 ? 1 : -1);
            int32_t by, bm, bd;
            _ejs_temporal_epoch_days_to_iso_date(shifted, &by, &bm, &bd);
            difference_iso_date(y1, m1, d1, by, bm, bd, s->largest,
                                &years, &months, &weeks, &days);
            rounded = 0;
        }
        EJSBool neg = rounded < 0;
        ejs_i128 t = neg ? -rounded : rounded;
        double f[6];
        f[0] = (double)(t / 3600000000000LL); t %= 3600000000000LL;
        f[1] = (double)(t / 60000000000LL); t %= 60000000000LL;
        f[2] = (double)(t / 1000000000LL); t %= 1000000000LL;
        f[3] = (double)(t / 1000000LL); t %= 1000000LL;
        f[4] = (double)(t / 1000LL);
        f[5] = (double)(t % 1000LL);
        double tsgn = neg ? -1 : 1;
        out->years = years; out->months = months; out->weeks = weeks; out->days = days;
        out->hours = tsgn * f[0]; out->minutes = tsgn * f[1]; out->seconds = tsgn * f[2];
        out->ms = tsgn * f[3]; out->us = tsgn * f[4]; out->ns = tsgn * f[5];
        return;
    }
    double years, months, weeks, days;
    if (s->smallest == TUNIT_DAY) {
        difference_iso_date(y1, m1, d1, ay2, am2, ad2, s->largest,
                            &years, &months, &weeks, &days);
        ejs_i128 dns = (ejs_i128)(int64_t)days * NS_PER_DAY + time_diff;
        dns = round_i128_to_increment(dns, (ejs_i128)(int64_t)s->increment * NS_PER_DAY, s->mode);
        double rdays = (double)(int64_t)(dns / NS_PER_DAY);
        if (rdays != days && s->largest < TUNIT_DAY) {
            // a bumped day count can overflow the month/week: re-derive
            int32_t by, bm, bd;
            add_iso_date_unchecked(y1, m1, d1, years, months, weeks, rdays, &by, &bm, &bd);
            difference_iso_date(y1, m1, d1, by, bm, bd, s->largest, &years, &months, &weeks, &days);
        } else {
            days = rdays;
        }
    } else {
        ejs_i128 leftover;
        round_date_duration_check_bounds = s->check_bounds;
        round_date_duration(y1, m1, d1, ay2, am2, ad2, time_diff,
                            s->smallest, s->increment, s->mode, s->largest,
                            &years, &months, &weeks, &days, &leftover);
        round_date_duration_check_bounds = EJS_FALSE;
    }
    out->years = years; out->months = months; out->weeks = weeks; out->days = days;
}

// PlainDateTime.until/since
static ejsval
plaindatetime_diff_impl(ejsval datetime, ejsval other_like, ejsval options, EJSBool is_since)
{
    EJSTemporalPlainDateTime* pdt = EJSVAL_TO_TEMPORAL_PLAINDATETIME(datetime);
    int32_t y2, m2, d2, h2, mi2, s2, ms2, us2, ns2;
    ejsval cal;
    to_temporal_datetime_fields(other_like, _ejs_undefined, &y2, &m2, &d2, &h2, &mi2, &s2, &ms2, &us2, &ns2, &cal);
    if (!SameValue(pdt->calendar, cal))
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "cannot difference datetimes with different calendars");
    DiffSettings s;
    get_difference_settings(get_options_object(options), is_since,
                            TUNIT_YEAR, TUNIT_NANO, TUNIT_NANO, TUNIT_DAY, &s);
    DurationFields f;
    datetime_diff_to_duration(pdt->year, pdt->month, pdt->day,
                              time_fields_to_ns(pdt->hour, pdt->minute, pdt->second,
                                                pdt->millisecond, pdt->microsecond, pdt->nanosecond),
                              y2, m2, d2, time_fields_to_ns(h2, mi2, s2, ms2, us2, ns2),
                              &s, &f);
    double sgn = is_since ? -1 : 1;
    return _ejs_temporal_duration_new(sgn * f.years, sgn * f.months, sgn * f.weeks, sgn * f.days,
                                      sgn * f.hours, sgn * f.minutes, sgn * f.seconds,
                                      sgn * f.ms, sgn * f.us, sgn * f.ns);
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainDateTime_prototype_until) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINDATETIME, "Temporal.PlainDateTime.prototype.until");
    return plaindatetime_diff_impl(*_this, argc > 0 ? args[0] : _ejs_undefined,
                                   argc > 1 ? args[1] : _ejs_undefined, EJS_FALSE);
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainDateTime_prototype_since) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINDATETIME, "Temporal.PlainDateTime.prototype.since");
    return plaindatetime_diff_impl(*_this, argc > 0 ? args[0] : _ejs_undefined,
                                   argc > 1 ? args[1] : _ejs_undefined, EJS_TRUE);
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainDateTime_prototype_round) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINDATETIME, "Temporal.PlainDateTime.prototype.round");
    EJSTemporalPlainDateTime* pdt = EJSVAL_TO_TEMPORAL_PLAINDATETIME(*_this);
    static const int64_t max_increments[] = { 1, 24, 60, 60, 1000, 1000, 1000 };
    TUnit unit;
    double increment;
    RoundingMode mode;
    get_round_settings(argc > 0 ? args[0] : _ejs_undefined, TUNIT_DAY, TUNIT_NANO,
                       max_increments, EJS_FALSE, &unit, &increment, &mode);
    int64_t days = _ejs_temporal_iso_date_to_epoch_days(pdt->year, pdt->month, pdt->day);
    ejs_i128 total = ((ejs_i128)days) * NS_PER_DAY
        + time_fields_to_ns(pdt->hour, pdt->minute, pdt->second, pdt->millisecond, pdt->microsecond, pdt->nanosecond);
    total = round_i128_as_if_positive(total, (ejs_i128)(int64_t)increment * unit_ns(unit), mode);
    int32_t y, mo, d, h, mi, s, ms, us, ns;
    epoch_ns_to_iso_fields(total, 0, &y, &mo, &d, &h, &mi, &s, &ms, &us, &ns);
    if (!_ejs_temporal_iso_datetime_within_limits(y, mo, d, h, mi, s, ms, us, ns))
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "datetime out of range");
    return _ejs_temporal_plain_datetime_new(y, mo, d, h, mi, s, ms, us, ns, pdt->calendar);
}

// PlainYearMonth add/subtract/until/since/toPlainDate
static ejsval
yearmonth_add_impl(ejsval ymval, ejsval duration_like, ejsval options, int negate)
{
    EJSTemporalPlainYearMonth* ym = EJSVAL_TO_TEMPORAL_PLAINYEARMONTH(ymval);
    DurationFields f;
    to_temporal_duration_fields(duration_like, &f);
    if (negate) {
        f.years = -f.years; f.months = -f.months; f.weeks = -f.weeks; f.days = -f.days;
        f.hours = -f.hours; f.minutes = -f.minutes; f.seconds = -f.seconds;
        f.ms = -f.ms; f.us = -f.us; f.ns = -f.ns;
    }
    get_overflow_option(get_options_object(options));
    // units below month cannot apply to a year-month
    if (f.weeks != 0 || f.days != 0 || f.hours != 0 || f.minutes != 0 || f.seconds != 0
        || f.ms != 0 || f.us != 0 || f.ns != 0)
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "duration has units smaller than months");
    if (!iso_date_within_limits(ym->year, ym->month, 1))
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "year-month out of range");
    int sign = (f.years < 0 || f.months < 0) ? -1 : 1;
    // start from day 1 (positive) or the last day of the month
    // (negative); the date math constrains regardless of the option
    int32_t start_day = sign < 0 ? _ejs_temporal_iso_days_in_month(ym->year, ym->month) : 1;
    int32_t ry, rm, rd;
    add_iso_date(ym->year, ym->month, start_day, f.years, f.months, 0, 0,
                 EJS_TRUE, &ry, &rm, &rd);
    if (!iso_yearmonth_within_limits(ry, rm))
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "year-month out of range");
    return _ejs_temporal_plain_yearmonth_new(ry, rm, 1, ym->calendar);
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainYearMonth_prototype_add) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINYEARMONTH, "Temporal.PlainYearMonth.prototype.add");
    return yearmonth_add_impl(*_this, argc > 0 ? args[0] : _ejs_undefined,
                              argc > 1 ? args[1] : _ejs_undefined, 0);
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainYearMonth_prototype_subtract) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINYEARMONTH, "Temporal.PlainYearMonth.prototype.subtract");
    return yearmonth_add_impl(*_this, argc > 0 ? args[0] : _ejs_undefined,
                              argc > 1 ? args[1] : _ejs_undefined, 1);
}

static ejsval
yearmonth_diff_impl(ejsval ymval, ejsval other_like, ejsval options, EJSBool is_since)
{
    EJSTemporalPlainYearMonth* ym = EJSVAL_TO_TEMPORAL_PLAINYEARMONTH(ymval);
    int32_t y2, m2, rd2;
    ejsval cal;
    to_temporal_yearmonth_fields(other_like, _ejs_undefined, &y2, &m2, &rd2, &cal);
    if (!SameValue(ym->calendar, cal))
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "cannot difference year-months with different calendars");
    DiffSettings s;
    get_difference_settings(get_options_object(options), is_since,
                            TUNIT_YEAR, TUNIT_MONTH, TUNIT_MONTH, TUNIT_YEAR, &s);
    if (ym->year == y2 && ym->month == m2)
        return _ejs_temporal_duration_new(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    // the difference is taken between the first days of the two months,
    // which must both be representable dates
    if (!iso_date_within_limits(ym->year, ym->month, 1) || !iso_date_within_limits(y2, m2, 1))
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "year-month out of range for difference");
    double years, months, weeks, days;
    ejs_i128 leftover;
    round_date_duration(ym->year, ym->month, 1, y2, m2, 1, 0,
                        s.smallest, s.increment, s.mode, s.largest,
                        &years, &months, &weeks, &days, &leftover);
    double sgn = is_since ? -1 : 1;
    return _ejs_temporal_duration_new(sgn * years, sgn * months, 0, 0, 0, 0, 0, 0, 0, 0);
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainYearMonth_prototype_until) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINYEARMONTH, "Temporal.PlainYearMonth.prototype.until");
    return yearmonth_diff_impl(*_this, argc > 0 ? args[0] : _ejs_undefined,
                               argc > 1 ? args[1] : _ejs_undefined, EJS_FALSE);
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainYearMonth_prototype_since) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINYEARMONTH, "Temporal.PlainYearMonth.prototype.since");
    return yearmonth_diff_impl(*_this, argc > 0 ? args[0] : _ejs_undefined,
                               argc > 1 ? args[1] : _ejs_undefined, EJS_TRUE);
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainYearMonth_prototype_toPlainDate) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINYEARMONTH, "Temporal.PlainYearMonth.prototype.toPlainDate");
    EJSTemporalPlainYearMonth* ym = EJSVAL_TO_TEMPORAL_PLAINYEARMONTH(*_this);
    ejsval item = argc > 0 ? args[0] : _ejs_undefined;
    if (!EJSVAL_IS_OBJECT(item))
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "argument must be an object");
    // only the complementary field is read
    ejsval dv = Get(item, _ejs_string_new_utf8("day"));
    if (EJSVAL_IS_UNDEFINED(dv))
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "day is required");
    double dd = to_positive_integer_with_truncation(dv);
    if (dd > 1e9)
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "day out of range");
    int32_t y = ym->year, m = ym->month, d = (int32_t)dd;
    regulate_iso_date(&y, &m, &d, EJS_TRUE);
    if (!iso_date_within_limits(y, m, d))
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "date out of range");
    return _ejs_temporal_plain_date_new(y, m, d, ym->calendar);
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainMonthDay_prototype_toPlainDate) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINMONTHDAY, "Temporal.PlainMonthDay.prototype.toPlainDate");
    EJSTemporalPlainMonthDay* md = EJSVAL_TO_TEMPORAL_PLAINMONTHDAY(*_this);
    ejsval item = argc > 0 ? args[0] : _ejs_undefined;
    if (!EJSVAL_IS_OBJECT(item))
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "argument must be an object");
    // only the complementary field is read
    ejsval yv = Get(item, _ejs_string_new_utf8("year"));
    if (EJSVAL_IS_UNDEFINED(yv))
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "year is required");
    double yy = to_integer_with_truncation(yv);
    if (fabs(yy) > 1000000)
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "date out of range");
    int32_t y = (int32_t)yy, m = md->month, d = md->day;
    regulate_iso_date(&y, &m, &d, EJS_TRUE);
    if (!iso_date_within_limits(y, m, d))
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "date out of range");
    return _ejs_temporal_plain_date_new(y, m, d, md->calendar);
}

// Duration round/total

static EJS_NATIVE_FUNC(_ejs_TemporalDuration_prototype_round) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_DURATION, "Temporal.Duration.prototype.round");
    EJSTemporalDuration* d = EJSVAL_TO_TEMPORAL_DURATION(*_this);
    ejsval round_to = argc > 0 ? args[0] : _ejs_undefined;
    if (EJSVAL_IS_UNDEFINED(round_to))
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "roundTo is required");
    TUnit largest = TUNIT_AUTO, smallest = TUNIT_AUTO;
    double increment = 1;
    RoundingMode mode = ROUND_HALF_EXPAND;
    EJSBool largest_auto_explicit = EJS_FALSE;
    RelativeTo rel;
    memset(&rel, 0, sizeof(rel));
    rel.calendar = _ejs_undefined;
    rel.time_zone = _ejs_undefined;
    if (EJSVAL_IS_STRING(round_to)) {
        char* s = string_to_ascii(round_to);
        if (!s)
            _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "invalid smallestUnit");
        static const char* const singular[] = {
            "year", "month", "week", "day", "hour", "minute", "second",
            "millisecond", "microsecond", "nanosecond"
        };
        static const char* const plural[] = {
            "years", "months", "weeks", "days", "hours", "minutes", "seconds",
            "milliseconds", "microseconds", "nanoseconds"
        };
        int found = -1;
        for (int i = 0; i < 10; i++)
            if (!strcmp(s, singular[i]) || !strcmp(s, plural[i])) { found = i; break; }
        free(s);
        if (found < 0)
            _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "invalid smallestUnit");
        smallest = (TUnit)found;
    } else if (EJSVAL_IS_OBJECT(round_to)) {
        // alphabetical: largestUnit, relativeTo, roundingIncrement,
        // roundingMode, smallestUnit
        ejsval lv = Get(round_to, _ejs_string_new_utf8("largestUnit"));
        if (!EJSVAL_IS_UNDEFINED(lv)) {
            ejsval str = ToString(lv);
            if (string_equals_utf8(str, "auto")) largest_auto_explicit = EJS_TRUE;
            else {
                char* s = string_to_ascii(str);
                if (!s)
                    _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "invalid largestUnit");
                static const char* const singular[] = {
                    "year", "month", "week", "day", "hour", "minute", "second",
                    "millisecond", "microsecond", "nanosecond"
                };
                static const char* const plural[] = {
                    "years", "months", "weeks", "days", "hours", "minutes", "seconds",
                    "milliseconds", "microseconds", "nanoseconds"
                };
                int found = -1;
                for (int i = 0; i < 10; i++)
                    if (!strcmp(s, singular[i]) || !strcmp(s, plural[i])) { found = i; break; }
                free(s);
                if (found < 0)
                    _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "invalid largestUnit");
                largest = (TUnit)found;
            }
        }
        get_relative_to_option(round_to, &rel);
        increment = get_rounding_increment_option(round_to);
        mode = get_rounding_mode_option(round_to, ROUND_HALF_EXPAND);
        smallest = get_unit_option(round_to, "smallestUnit", EJS_FALSE);
    } else {
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "roundTo must be a string or object");
    }
    if (smallest == TUNIT_AUTO && largest == TUNIT_AUTO && !largest_auto_explicit)
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "at least one of smallestUnit or largestUnit is required");
    // default largest: the duration's own largest nonzero unit
    TUnit default_largest = TUNIT_NANO;
    {
        double fields[10] = { d->years, d->months, d->weeks, d->days, d->hours,
                              d->minutes, d->seconds, d->milliseconds, d->microseconds, d->nanoseconds };
        for (int i = 0; i < 10; i++)
            if (fields[i] != 0) { default_largest = (TUnit)i; break; }
    }
    if (smallest == TUNIT_AUTO) smallest = TUNIT_NANO;
    if (largest == TUNIT_AUTO)
        largest = default_largest < smallest ? default_largest : smallest;
    if (largest > smallest)
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "largestUnit cannot be smaller than smallestUnit");
    if (smallest >= TUNIT_HOUR) {
        static const int64_t max_increments[] = { 24, 60, 60, 1000, 1000, 1000 };
        validate_increment(increment, max_increments[smallest - TUNIT_HOUR], EJS_FALSE);
    }
    // rounding to a multiple of a calendar or day unit cannot also
    // balance into a larger unit
    if (smallest <= TUNIT_DAY && increment != 1 && largest != smallest)
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR,
            "cannot round to an increment of a calendar unit while balancing to a larger unit");
    if (rel.kind == 0) {
        if (d->years != 0 || d->months != 0 || d->weeks != 0
            || largest <= TUNIT_WEEK || smallest <= TUNIT_WEEK)
            _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "a starting point is required for calendar units");
        ejs_i128 total = duration_time_ns((DurationFields*)&(DurationFields){
            0, 0, 0, 0, d->hours, d->minutes, d->seconds, d->milliseconds, d->microseconds, d->nanoseconds })
            + ((ejs_i128)d->days) * NS_PER_DAY;
        total = round_i128_to_increment(total, (ejs_i128)(int64_t)increment * unit_ns(smallest), mode);
        return balance_time_duration(total, largest, 0);
    }
    DurationFields df = { d->years, d->months, d->weeks, d->days, d->hours,
                          d->minutes, d->seconds, d->milliseconds, d->microseconds, d->nanoseconds };
    int32_t ty, tm, td;
    ejs_i128 t_time, total_ns;
    relative_apply_duration(&rel, &df, &ty, &tm, &td, &t_time, &total_ns);
    if (rel.kind == 2 && largest >= TUNIT_HOUR) {
        // zoned starting point with a time-scale result: day lengths
        // vary, so round the exact epoch span
        ejs_i128 rounded = round_i128_to_increment(total_ns,
                                                   (ejs_i128)(int64_t)increment * unit_ns(smallest), mode);
        return balance_time_duration(rounded, largest, 0);
    }
    DiffSettings s = { .largest = largest, .smallest = smallest, .increment = increment, .mode = mode,
                       .check_bounds = rel.kind == 2 ? EJS_TRUE : EJS_FALSE };
    DurationFields out;
    datetime_diff_to_duration(rel.year, rel.month, rel.day,
                              rel.kind == 2 ? rel.local_time_ns : 0,
                              ty, tm, td, t_time, &s, &out);
    if (!is_valid_duration(out.years, out.months, out.weeks, out.days, out.hours,
                           out.minutes, out.seconds, out.ms, out.us, out.ns))
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "invalid duration");
    return _ejs_temporal_duration_new(out.years, out.months, out.weeks, out.days, out.hours,
                                      out.minutes, out.seconds, out.ms, out.us, out.ns);
}

static EJS_NATIVE_FUNC(_ejs_TemporalDuration_prototype_total) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_DURATION, "Temporal.Duration.prototype.total");
    EJSTemporalDuration* d = EJSVAL_TO_TEMPORAL_DURATION(*_this);
    ejsval total_of = argc > 0 ? args[0] : _ejs_undefined;
    if (EJSVAL_IS_UNDEFINED(total_of))
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "totalOf is required");
    RelativeTo trel;
    memset(&trel, 0, sizeof(trel));
    trel.calendar = _ejs_undefined;
    trel.time_zone = _ejs_undefined;
    TUnit unit;
    if (EJSVAL_IS_STRING(total_of)) {
        char* s = string_to_ascii(total_of);
        if (!s)
            _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "invalid unit");
        static const char* const singular[] = {
            "year", "month", "week", "day", "hour", "minute", "second",
            "millisecond", "microsecond", "nanosecond"
        };
        static const char* const plural[] = {
            "years", "months", "weeks", "days", "hours", "minutes", "seconds",
            "milliseconds", "microseconds", "nanoseconds"
        };
        int found = -1;
        for (int i = 0; i < 10; i++)
            if (!strcmp(s, singular[i]) || !strcmp(s, plural[i])) { found = i; break; }
        free(s);
        if (found < 0)
            _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "invalid unit");
        unit = (TUnit)found;
    } else if (EJSVAL_IS_OBJECT(total_of)) {
        get_relative_to_option(total_of, &trel);
        unit = get_unit_option(total_of, "unit", EJS_FALSE);
        if (unit == TUNIT_AUTO)
            _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "unit is required");
    } else {
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "totalOf must be a string or object");
        EJS_NOT_REACHED();
        return _ejs_undefined;
    }
    if (trel.kind == 0) {
        if (d->years != 0 || d->months != 0 || d->weeks != 0 || unit <= TUNIT_WEEK)
            _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "a starting point is required for calendar units");
        ejs_i128 total = duration_time_ns((DurationFields*)&(DurationFields){
            0, 0, 0, 0, d->hours, d->minutes, d->seconds, d->milliseconds, d->microseconds, d->nanoseconds })
            + ((ejs_i128)d->days) * NS_PER_DAY;
        int64_t per = unit_ns(unit);
        ejs_i128 q = total / per;
        ejs_i128 r = total % per;
        return NUMBER_TO_EJSVAL((double)(int64_t)q + (double)(int64_t)r / (double)per);
    }
    DurationFields df = { d->years, d->months, d->weeks, d->days, d->hours,
                          d->minutes, d->seconds, d->milliseconds, d->microseconds, d->nanoseconds };
    int32_t ty, tm, td;
    ejs_i128 t_time, total_ns;
    relative_apply_duration(&trel, &df, &ty, &tm, &td, &t_time, &total_ns);
    if (unit >= TUNIT_HOUR || (unit == TUNIT_DAY && trel.kind == 1)) {
        int64_t per = unit_ns(unit);
        ejs_i128 q = total_ns / per;
        ejs_i128 r = total_ns % per;
        return NUMBER_TO_EJSVAL((double)(int64_t)q + (double)(int64_t)r / (double)per);
    }
    // calendar unit (or zoned days): whole count + exact fraction
    DiffSettings s = { .largest = unit, .smallest = unit, .increment = 1, .mode = ROUND_TRUNC,
                       .check_bounds = trel.kind == 2 ? EJS_TRUE : EJS_FALSE };
    DurationFields out;
    datetime_diff_to_duration(trel.year, trel.month, trel.day,
                              trel.kind == 2 ? trel.local_time_ns : 0,
                              ty, tm, td, t_time, &s, &out);
    double whole;
    switch (unit) {
    case TUNIT_YEAR: whole = out.years; break;
    case TUNIT_MONTH: whole = out.months; break;
    case TUNIT_WEEK: whole = out.weeks; break;
    default: whole = out.days; break;
    }
    int sign = total_ns > 0 ? 1 : (total_ns < 0 ? -1 : 0);
    if (sign == 0)
        return NUMBER_TO_EJSVAL(0.0);
    ejs_i128 r1 = relative_units_ns(&trel, unit, whole);
    ejs_i128 r2 = relative_units_ns(&trel, unit, whole + sign);
    double frac = (r2 == r1) ? 0
        : (double)(int64_t)(total_ns - r1) / (double)(int64_t)(r2 - r1);
    return NUMBER_TO_EJSVAL(whole + sign * frac);
}

// ZonedDateTime until/since/round/withPlainTime; PlainDate/PlainDateTime
// toZonedDateTime
static ejsval
zdt_diff_impl(ejsval zdtval, ejsval other_like, ejsval options, EJSBool is_since)
{
    EJSTemporalZonedDateTime* zdt = EJSVAL_TO_TEMPORAL_ZONEDDATETIME(zdtval);
    ejs_i128 o_epoch;
    ejsval o_tz, o_cal;
    to_temporal_zdt(other_like, _ejs_undefined, &o_epoch, &o_tz, &o_cal);
    EJSTemporalZonedDateTime other_rec;
    other_rec.epoch_ns = o_epoch;
    other_rec.time_zone = o_tz;
    other_rec.calendar = o_cal;
    EJSTemporalZonedDateTime* other = &other_rec;
    if (!SameValue(zdt->calendar, other->calendar))
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "cannot difference with different calendars");
    DiffSettings s;
    get_difference_settings(get_options_object(options), is_since,
                            TUNIT_YEAR, TUNIT_NANO, TUNIT_NANO, TUNIT_HOUR, &s);
    if (s.largest >= TUNIT_HOUR) {
        ejs_i128 diff = other->epoch_ns - zdt->epoch_ns;
        diff = round_i128_to_increment(diff, (ejs_i128)(int64_t)s.increment * unit_ns(s.smallest), s.mode);
        return balance_time_duration(diff, s.largest, is_since);
    }
    // calendar units: local-date difference (requires matching zones)
    if (!SameValue(zdt->time_zone, other->time_zone))
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR,
                                    "cannot difference in calendar units across time zones");
    ZDTFields f1, f2;
    zdt_fields(zdt, &f1);
    zdt_fields(other, &f2);
    s.check_bounds = EJS_TRUE;
    DurationFields out;
    datetime_diff_to_duration(f1.year, f1.month, f1.day,
                              time_fields_to_ns(f1.hour, f1.minute, f1.second,
                                                f1.millisecond, f1.microsecond, f1.nanosecond),
                              f2.year, f2.month, f2.day,
                              time_fields_to_ns(f2.hour, f2.minute, f2.second,
                                                f2.millisecond, f2.microsecond, f2.nanosecond),
                              &s, &out);
    double sgn = is_since ? -1 : 1;
    return _ejs_temporal_duration_new(sgn * out.years, sgn * out.months, sgn * out.weeks, sgn * out.days,
                                      sgn * out.hours, sgn * out.minutes, sgn * out.seconds,
                                      sgn * out.ms, sgn * out.us, sgn * out.ns);
}

static EJS_NATIVE_FUNC(_ejs_TemporalZonedDateTime_prototype_until) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_ZONEDDATETIME, "Temporal.ZonedDateTime.prototype.until");
    return zdt_diff_impl(*_this, argc > 0 ? args[0] : _ejs_undefined,
                         argc > 1 ? args[1] : _ejs_undefined, EJS_FALSE);
}

static EJS_NATIVE_FUNC(_ejs_TemporalZonedDateTime_prototype_since) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_ZONEDDATETIME, "Temporal.ZonedDateTime.prototype.since");
    return zdt_diff_impl(*_this, argc > 0 ? args[0] : _ejs_undefined,
                         argc > 1 ? args[1] : _ejs_undefined, EJS_TRUE);
}

static EJS_NATIVE_FUNC(_ejs_TemporalZonedDateTime_prototype_round) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_ZONEDDATETIME, "Temporal.ZonedDateTime.prototype.round");
    EJSTemporalZonedDateTime* zdt = EJSVAL_TO_TEMPORAL_ZONEDDATETIME(*_this);
    static const int64_t max_increments[] = { 1, 24, 60, 60, 1000, 1000, 1000 };
    TUnit unit;
    double increment;
    RoundingMode mode;
    get_round_settings(argc > 0 ? args[0] : _ejs_undefined, TUNIT_DAY, TUNIT_NANO,
                       max_increments, EJS_FALSE, &unit, &increment, &mode);
    ZDTFields f;
    zdt_fields(zdt, &f);
    if (unit == TUNIT_DAY) {
        // day rounding measures against the real day boundaries in the
        // zone (start of this day and start of the next)
        ejs_i128 start = zone_epoch_from_local(zdt->time_zone,
            iso_datetime_to_epoch_ns(f.year, f.month, f.day, 0, 0, 0, 0, 0, 0), 0);
        int32_t ny, nm, nd;
        _ejs_temporal_epoch_days_to_iso_date(
            _ejs_temporal_iso_date_to_epoch_days(f.year, f.month, f.day) + 1, &ny, &nm, &nd);
        ejs_i128 end = zone_epoch_from_local(zdt->time_zone,
            iso_datetime_to_epoch_ns(ny, nm, nd, 0, 0, 0, 0, 0, 0), 0);
        if (end < EJS_TEMPORAL_NS_MIN_INSTANT || end > EJS_TEMPORAL_NS_MAX_INSTANT)
            _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "instant out of range");
        ejs_i128 len = end - start;
        ejs_i128 r = zdt->epoch_ns - start;
        // round r/len to 0 or 1 under as-if-positive semantics
        RoundingMode m2 = mode;
        switch (mode) {
        case ROUND_TRUNC: m2 = ROUND_FLOOR; break;
        case ROUND_EXPAND: m2 = ROUND_CEIL; break;
        case ROUND_HALF_TRUNC: m2 = ROUND_HALF_FLOOR; break;
        case ROUND_HALF_EXPAND: m2 = ROUND_HALF_CEIL; break;
        default: break;
        }
        EJSBool pick_end;
        ejs_i128 twice = r * 2;
        switch (m2) {
        case ROUND_CEIL: pick_end = r != 0; break;
        case ROUND_FLOOR: pick_end = EJS_FALSE; break;
        case ROUND_HALF_CEIL: pick_end = twice >= len; break;
        case ROUND_HALF_FLOOR: pick_end = twice > len; break;
        default: pick_end = (twice > len) || (twice == len); break; // halfEven: boundary day index parity ~ floor
        }
        return _ejs_temporal_zoneddatetime_new(pick_end ? end : start, zdt->time_zone, zdt->calendar);
    }
    // sub-day units round the local wall-clock, then reapply via the zone
    ejs_i128 local = ((ejs_i128)_ejs_temporal_iso_date_to_epoch_days(f.year, f.month, f.day)) * NS_PER_DAY
        + time_fields_to_ns(f.hour, f.minute, f.second, f.millisecond, f.microsecond, f.nanosecond);
    local = round_i128_as_if_positive(local, (ejs_i128)(int64_t)increment * unit_ns(unit), mode);
    ejs_i128 epoch = local - f.offset_ns;
    if (epoch < EJS_TEMPORAL_NS_MIN_INSTANT || epoch > EJS_TEMPORAL_NS_MAX_INSTANT)
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "instant out of range");
    return _ejs_temporal_zoneddatetime_new(epoch, zdt->time_zone, zdt->calendar);
}

static EJS_NATIVE_FUNC(_ejs_TemporalZonedDateTime_prototype_withPlainTime) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_ZONEDDATETIME, "Temporal.ZonedDateTime.prototype.withPlainTime");
    EJSTemporalZonedDateTime* zdt = EJSVAL_TO_TEMPORAL_ZONEDDATETIME(*_this);
    ejsval t = argc > 0 ? args[0] : _ejs_undefined;
    int32_t h = 0, mi = 0, s = 0, ms = 0, us = 0, ns = 0;
    if (!EJSVAL_IS_UNDEFINED(t))
        to_temporal_time_fields(t, _ejs_undefined, &h, &mi, &s, &ms, &us, &ns);
    ZDTFields f;
    zdt_fields(zdt, &f);
    ejs_i128 epoch = zone_epoch_from_local(zdt->time_zone,
        iso_datetime_to_epoch_ns(f.year, f.month, f.day, h, mi, s, ms, us, ns), 0);
    if (epoch < EJS_TEMPORAL_NS_MIN_INSTANT || epoch > EJS_TEMPORAL_NS_MAX_INSTANT)
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "instant out of range");
    return _ejs_temporal_zoneddatetime_new(epoch, zdt->time_zone, zdt->calendar);
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainDate_prototype_toZonedDateTime) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINDATE, "Temporal.PlainDate.prototype.toZonedDateTime");
    EJSTemporalPlainDate* pd = EJSVAL_TO_TEMPORAL_PLAINDATE(*_this);
    ejsval item = argc > 0 ? args[0] : _ejs_undefined;
    ejsval tz;
    int32_t h = 0, mi = 0, s = 0, ms = 0, us = 0, ns = 0;
    if (EJSVAL_IS_STRING(item) || EJSVAL_IS_TEMPORAL_ZONEDDATETIME(item)) {
        tz = to_time_zone_identifier(item);
    } else if (EJSVAL_IS_OBJECT(item)) {
        ejsval tz_like = Get(item, _ejs_string_new_utf8("timeZone"));
        if (EJSVAL_IS_UNDEFINED(tz_like)) {
            tz = to_time_zone_identifier(item);
        } else {
            tz = to_time_zone_identifier(tz_like);
            ejsval time = Get(item, _ejs_string_new_utf8("plainTime"));
            if (!EJSVAL_IS_UNDEFINED(time))
                to_temporal_time_fields(time, _ejs_undefined, &h, &mi, &s, &ms, &us, &ns);
        }
    } else {
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "invalid time zone");
        EJS_NOT_REACHED();
        return _ejs_undefined;
    }
    ejs_i128 epoch = zone_epoch_from_local(tz,
        iso_datetime_to_epoch_ns(pd->year, pd->month, pd->day, h, mi, s, ms, us, ns), 0);
    if (epoch < EJS_TEMPORAL_NS_MIN_INSTANT || epoch > EJS_TEMPORAL_NS_MAX_INSTANT)
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "instant out of range");
    return _ejs_temporal_zoneddatetime_new(epoch, tz, pd->calendar);
}

static EJS_NATIVE_FUNC(_ejs_TemporalPlainDateTime_prototype_toZonedDateTime) {
    BRAND_CHECK(EJSVAL_IS_TEMPORAL_PLAINDATETIME, "Temporal.PlainDateTime.prototype.toZonedDateTime");
    EJSTemporalPlainDateTime* pdt = EJSVAL_TO_TEMPORAL_PLAINDATETIME(*_this);
    ejsval tz = to_time_zone_identifier(argc > 0 ? args[0] : _ejs_undefined);
    // disambiguation option read for validation (no DST in our zones)
    ejsval options = get_options_object(argc > 1 ? args[1] : _ejs_undefined);
    static const char* const disambig[] = { "compatible", "earlier", "later", "reject" };
    int disambiguation = get_string_option(options, "disambiguation", disambig, 4, 0);
    ejs_i128 epoch = zone_epoch_from_local(tz,
        iso_datetime_to_epoch_ns(pdt->year, pdt->month, pdt->day, pdt->hour, pdt->minute,
                                 pdt->second, pdt->millisecond, pdt->microsecond, pdt->nanosecond),
        disambiguation);
    if (epoch < EJS_TEMPORAL_NS_MIN_INSTANT || epoch > EJS_TEMPORAL_NS_MAX_INSTANT)
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "instant out of range");
    return _ejs_temporal_zoneddatetime_new(epoch, tz, pdt->calendar);
}

// ------------------------------------------------------------------ Now

static ejs_i128
system_epoch_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return ((ejs_i128)ts.tv_sec) * 1000000000LL + ts.tv_nsec;
}

// time zone for a Temporal.Now.*(tz?) call: undefined -> the system
// zone (accepted as-is), otherwise validated
static ejsval
now_time_zone(uint32_t argc, ejsval* args)
{
    if (argc == 0 || EJSVAL_IS_UNDEFINED(args[0])) {
        char buf[256];
        system_time_zone_id(buf, sizeof(buf));
        return _ejs_string_new_utf8(buf);
    }
    return to_time_zone_identifier(args[0]);
}

static EJS_NATIVE_FUNC(_ejs_Temporal_Now_instant) {
    return _ejs_temporal_instant_new(system_epoch_ns());
}

static EJS_NATIVE_FUNC(_ejs_Temporal_Now_timeZoneId) {
    char buf[256];
    system_time_zone_id(buf, sizeof(buf));
    return _ejs_string_new_utf8(buf);
}

static void
now_fields(uint32_t argc, ejsval* args, ejsval* tz, ZDTFields* f)
{
    *tz = now_time_zone(argc, args);
    ejs_i128 ns = system_epoch_ns();
    f->offset_ns = tz_offset_ns(*tz, ns);
    epoch_ns_to_iso_fields(ns, f->offset_ns, &f->year, &f->month, &f->day,
                           &f->hour, &f->minute, &f->second,
                           &f->millisecond, &f->microsecond, &f->nanosecond);
}

static EJS_NATIVE_FUNC(_ejs_Temporal_Now_plainDateTimeISO) {
    ejsval tz;
    ZDTFields f;
    now_fields(argc, args, &tz, &f);
    return _ejs_temporal_plain_datetime_new(f.year, f.month, f.day, f.hour, f.minute, f.second,
                                            f.millisecond, f.microsecond, f.nanosecond, _ejs_temporal_str_iso8601);
}

static EJS_NATIVE_FUNC(_ejs_Temporal_Now_plainDateISO) {
    ejsval tz;
    ZDTFields f;
    now_fields(argc, args, &tz, &f);
    return _ejs_temporal_plain_date_new(f.year, f.month, f.day, _ejs_temporal_str_iso8601);
}

static EJS_NATIVE_FUNC(_ejs_Temporal_Now_plainTimeISO) {
    ejsval tz;
    ZDTFields f;
    now_fields(argc, args, &tz, &f);
    return _ejs_temporal_plain_time_new(f.hour, f.minute, f.second, f.millisecond, f.microsecond, f.nanosecond);
}

static EJS_NATIVE_FUNC(_ejs_Temporal_Now_zonedDateTimeISO) {
    ejsval tz = now_time_zone(argc, args);
    return _ejs_temporal_zoneddatetime_new(system_epoch_ns(), tz, _ejs_temporal_str_iso8601);
}

// ------------------------------------------------------------------ init

static ejsval
create_constructor(ejsval namespace_obj, const char* name, EJSClosureFunc ctor_fn, int len, ejsval* proto_out)
{
    ejsval proto = _ejs_object_new(_ejs_Object_prototype, &_ejs_Object_specops);
    ejsval ctor = _ejs_function_new_without_proto(_ejs_null, _ejs_string_new_utf8(name), ctor_fn);
    _ejs_object_define_value_property(ctor, _ejs_atom_length, NUMBER_TO_EJSVAL(len),
                                      EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_WRITABLE | EJS_PROP_CONFIGURABLE);
    _ejs_object_define_value_property(ctor, _ejs_atom_prototype, proto,
                                      EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_WRITABLE | EJS_PROP_NOT_CONFIGURABLE);
    _ejs_object_define_value_property(proto, _ejs_atom_constructor, ctor,
                                      EJS_PROP_NOT_ENUMERABLE | EJS_PROP_WRITABLE | EJS_PROP_CONFIGURABLE);
    _ejs_object_define_value_property(namespace_obj, _ejs_string_new_utf8(name), ctor,
                                      EJS_PROP_NOT_ENUMERABLE | EJS_PROP_WRITABLE | EJS_PROP_CONFIGURABLE);
    *proto_out = proto;
    return ctor;
}

void
_ejs_temporal_init(ejsval global)
{
    _ejs_gc_add_root(&_ejs_Temporal);
    _ejs_gc_add_root(&_ejs_Temporal_Now);
    _ejs_gc_add_root(&_ejs_TemporalInstant);
    _ejs_gc_add_root(&_ejs_TemporalInstant_prototype);
    _ejs_gc_add_root(&_ejs_TemporalPlainDate);
    _ejs_gc_add_root(&_ejs_TemporalPlainDate_prototype);
    _ejs_gc_add_root(&_ejs_TemporalPlainTime);
    _ejs_gc_add_root(&_ejs_TemporalPlainTime_prototype);
    _ejs_gc_add_root(&_ejs_TemporalPlainDateTime);
    _ejs_gc_add_root(&_ejs_TemporalPlainDateTime_prototype);
    _ejs_gc_add_root(&_ejs_TemporalPlainYearMonth);
    _ejs_gc_add_root(&_ejs_TemporalPlainYearMonth_prototype);
    _ejs_gc_add_root(&_ejs_TemporalPlainMonthDay);
    _ejs_gc_add_root(&_ejs_TemporalPlainMonthDay_prototype);
    _ejs_gc_add_root(&_ejs_TemporalZonedDateTime);
    _ejs_gc_add_root(&_ejs_TemporalZonedDateTime_prototype);
    _ejs_gc_add_root(&_ejs_TemporalDuration);
    _ejs_gc_add_root(&_ejs_TemporalDuration_prototype);
    _ejs_gc_add_root(&_ejs_temporal_str_iso8601);
    _ejs_gc_add_root(&_ejs_temporal_str_UTC);

    _ejs_temporal_str_iso8601 = _ejs_string_new_utf8("iso8601");
    _ejs_temporal_str_UTC = _ejs_string_new_utf8("UTC");

    // the namespace object
    _ejs_Temporal = _ejs_object_new(_ejs_Object_prototype, &_ejs_Object_specops);
    install_tostringtag(_ejs_Temporal, "Temporal");
    _ejs_object_define_value_property(global, _ejs_string_new_utf8("Temporal"), _ejs_Temporal,
                                      EJS_PROP_NOT_ENUMERABLE | EJS_PROP_WRITABLE | EJS_PROP_CONFIGURABLE);

    // Temporal.Now
    _ejs_Temporal_Now = _ejs_object_new(_ejs_Object_prototype, &_ejs_Object_specops);
    install_tostringtag(_ejs_Temporal_Now, "Temporal.Now");
    _ejs_object_define_value_property(_ejs_Temporal, _ejs_string_new_utf8("Now"), _ejs_Temporal_Now,
                                      EJS_PROP_NOT_ENUMERABLE | EJS_PROP_WRITABLE | EJS_PROP_CONFIGURABLE);
    install_method(_ejs_Temporal_Now, "instant", _ejs_Temporal_Now_instant, 0);
    install_method(_ejs_Temporal_Now, "plainDateTimeISO", _ejs_Temporal_Now_plainDateTimeISO, 0);
    install_method(_ejs_Temporal_Now, "zonedDateTimeISO", _ejs_Temporal_Now_zonedDateTimeISO, 0);
    install_method(_ejs_Temporal_Now, "plainDateISO", _ejs_Temporal_Now_plainDateISO, 0);
    install_method(_ejs_Temporal_Now, "plainTimeISO", _ejs_Temporal_Now_plainTimeISO, 0);
    install_method(_ejs_Temporal_Now, "timeZoneId", _ejs_Temporal_Now_timeZoneId, 0);

    // ---- Instant
    _ejs_TemporalInstant = create_constructor(_ejs_Temporal, "Instant", _ejs_TemporalInstant_impl, 1,
                                              &_ejs_TemporalInstant_prototype);
    install_tostringtag(_ejs_TemporalInstant_prototype, "Temporal.Instant");
    install_method(_ejs_TemporalInstant, "from", _ejs_TemporalInstant_from, 1);
    install_method(_ejs_TemporalInstant, "fromEpochMilliseconds", _ejs_TemporalInstant_fromEpochMilliseconds, 1);
    install_method(_ejs_TemporalInstant, "fromEpochNanoseconds", _ejs_TemporalInstant_fromEpochNanoseconds, 1);
    install_method(_ejs_TemporalInstant, "compare", _ejs_TemporalInstant_compare, 2);
    install_getter(_ejs_TemporalInstant_prototype, "epochMilliseconds", _ejs_TemporalInstant_get_epochMilliseconds);
    install_getter(_ejs_TemporalInstant_prototype, "epochNanoseconds", _ejs_TemporalInstant_get_epochNanoseconds);
    install_method(_ejs_TemporalInstant_prototype, "add", _ejs_TemporalInstant_prototype_add, 1);
    install_method(_ejs_TemporalInstant_prototype, "subtract", _ejs_TemporalInstant_prototype_subtract, 1);
    install_method(_ejs_TemporalInstant_prototype, "until", _ejs_TemporalInstant_prototype_until, 1);
    install_method(_ejs_TemporalInstant_prototype, "since", _ejs_TemporalInstant_prototype_since, 1);
    install_method(_ejs_TemporalInstant_prototype, "round", _ejs_TemporalInstant_prototype_round, 1);
    install_method(_ejs_TemporalInstant_prototype, "equals", _ejs_TemporalInstant_prototype_equals, 1);
    install_method(_ejs_TemporalInstant_prototype, "toString", _ejs_TemporalInstant_prototype_toString, 0);
    install_method(_ejs_TemporalInstant_prototype, "toLocaleString", _ejs_TemporalInstant_prototype_toLocaleString, 0);
    install_method(_ejs_TemporalInstant_prototype, "toJSON", _ejs_TemporalInstant_prototype_toJSON, 0);
    install_method(_ejs_TemporalInstant_prototype, "valueOf", _ejs_Temporal_valueOf_impl, 0);
    install_method(_ejs_TemporalInstant_prototype, "toZonedDateTimeISO", _ejs_TemporalInstant_prototype_toZonedDateTimeISO, 1);

    // ---- PlainDate
    _ejs_TemporalPlainDate = create_constructor(_ejs_Temporal, "PlainDate", _ejs_TemporalPlainDate_impl, 3,
                                                &_ejs_TemporalPlainDate_prototype);
    install_tostringtag(_ejs_TemporalPlainDate_prototype, "Temporal.PlainDate");
    install_method(_ejs_TemporalPlainDate, "from", _ejs_TemporalPlainDate_from, 1);
    install_method(_ejs_TemporalPlainDate, "compare", _ejs_TemporalPlainDate_compare, 2);
    install_getter(_ejs_TemporalPlainDate_prototype, "calendarId", _ejs_TemporalPlainDate_get_calendarId);
    install_getter(_ejs_TemporalPlainDate_prototype, "era", _ejs_TemporalPlainDate_get_era);
    install_getter(_ejs_TemporalPlainDate_prototype, "eraYear", _ejs_TemporalPlainDate_get_eraYear);
    install_getter(_ejs_TemporalPlainDate_prototype, "year", _ejs_TemporalPlainDate_get_year);
    install_getter(_ejs_TemporalPlainDate_prototype, "month", _ejs_TemporalPlainDate_get_month);
    install_getter(_ejs_TemporalPlainDate_prototype, "monthCode", _ejs_TemporalPlainDate_get_monthCode);
    install_getter(_ejs_TemporalPlainDate_prototype, "day", _ejs_TemporalPlainDate_get_day);
    install_getter(_ejs_TemporalPlainDate_prototype, "dayOfWeek", _ejs_TemporalPlainDate_get_dayOfWeek);
    install_getter(_ejs_TemporalPlainDate_prototype, "dayOfYear", _ejs_TemporalPlainDate_get_dayOfYear);
    install_getter(_ejs_TemporalPlainDate_prototype, "weekOfYear", _ejs_TemporalPlainDate_get_weekOfYear);
    install_getter(_ejs_TemporalPlainDate_prototype, "yearOfWeek", _ejs_TemporalPlainDate_get_yearOfWeek);
    install_getter(_ejs_TemporalPlainDate_prototype, "daysInWeek", _ejs_TemporalPlainDate_get_daysInWeek);
    install_getter(_ejs_TemporalPlainDate_prototype, "daysInMonth", _ejs_TemporalPlainDate_get_daysInMonth);
    install_getter(_ejs_TemporalPlainDate_prototype, "daysInYear", _ejs_TemporalPlainDate_get_daysInYear);
    install_getter(_ejs_TemporalPlainDate_prototype, "monthsInYear", _ejs_TemporalPlainDate_get_monthsInYear);
    install_getter(_ejs_TemporalPlainDate_prototype, "inLeapYear", _ejs_TemporalPlainDate_get_inLeapYear);
    install_method(_ejs_TemporalPlainDate_prototype, "toPlainYearMonth", _ejs_TemporalPlainDate_prototype_toPlainYearMonth, 0);
    install_method(_ejs_TemporalPlainDate_prototype, "toPlainMonthDay", _ejs_TemporalPlainDate_prototype_toPlainMonthDay, 0);
    install_method(_ejs_TemporalPlainDate_prototype, "add", _ejs_TemporalPlainDate_prototype_add, 1);
    install_method(_ejs_TemporalPlainDate_prototype, "subtract", _ejs_TemporalPlainDate_prototype_subtract, 1);
    install_method(_ejs_TemporalPlainDate_prototype, "with", _ejs_TemporalPlainDate_prototype_with, 1);
    install_method(_ejs_TemporalPlainDate_prototype, "withCalendar", _ejs_TemporalPlainDate_prototype_withCalendar, 1);
    install_method(_ejs_TemporalPlainDate_prototype, "until", _ejs_TemporalPlainDate_prototype_until, 1);
    install_method(_ejs_TemporalPlainDate_prototype, "since", _ejs_TemporalPlainDate_prototype_since, 1);
    install_method(_ejs_TemporalPlainDate_prototype, "equals", _ejs_TemporalPlainDate_prototype_equals, 1);
    install_method(_ejs_TemporalPlainDate_prototype, "toPlainDateTime", _ejs_TemporalPlainDate_prototype_toPlainDateTime, 0);
    install_method(_ejs_TemporalPlainDate_prototype, "toZonedDateTime", _ejs_TemporalPlainDate_prototype_toZonedDateTime, 1);
    install_method(_ejs_TemporalPlainDate_prototype, "toString", _ejs_TemporalPlainDate_prototype_toString, 0);
    install_method(_ejs_TemporalPlainDate_prototype, "toLocaleString", _ejs_TemporalPlainDate_prototype_toLocaleString, 0);
    install_method(_ejs_TemporalPlainDate_prototype, "toJSON", _ejs_TemporalPlainDate_prototype_toJSON, 0);
    install_method(_ejs_TemporalPlainDate_prototype, "valueOf", _ejs_Temporal_valueOf_impl, 0);

    // ---- PlainTime
    _ejs_TemporalPlainTime = create_constructor(_ejs_Temporal, "PlainTime", _ejs_TemporalPlainTime_impl, 0,
                                                &_ejs_TemporalPlainTime_prototype);
    install_tostringtag(_ejs_TemporalPlainTime_prototype, "Temporal.PlainTime");
    install_method(_ejs_TemporalPlainTime, "from", _ejs_TemporalPlainTime_from, 1);
    install_method(_ejs_TemporalPlainTime, "compare", _ejs_TemporalPlainTime_compare, 2);
    install_getter(_ejs_TemporalPlainTime_prototype, "hour", _ejs_TemporalPlainTime_get_hour);
    install_getter(_ejs_TemporalPlainTime_prototype, "minute", _ejs_TemporalPlainTime_get_minute);
    install_getter(_ejs_TemporalPlainTime_prototype, "second", _ejs_TemporalPlainTime_get_second);
    install_getter(_ejs_TemporalPlainTime_prototype, "millisecond", _ejs_TemporalPlainTime_get_millisecond);
    install_getter(_ejs_TemporalPlainTime_prototype, "microsecond", _ejs_TemporalPlainTime_get_microsecond);
    install_getter(_ejs_TemporalPlainTime_prototype, "nanosecond", _ejs_TemporalPlainTime_get_nanosecond);
    install_method(_ejs_TemporalPlainTime_prototype, "add", _ejs_TemporalPlainTime_prototype_add, 1);
    install_method(_ejs_TemporalPlainTime_prototype, "subtract", _ejs_TemporalPlainTime_prototype_subtract, 1);
    install_method(_ejs_TemporalPlainTime_prototype, "with", _ejs_TemporalPlainTime_prototype_with, 1);
    install_method(_ejs_TemporalPlainTime_prototype, "until", _ejs_TemporalPlainTime_prototype_until, 1);
    install_method(_ejs_TemporalPlainTime_prototype, "since", _ejs_TemporalPlainTime_prototype_since, 1);
    install_method(_ejs_TemporalPlainTime_prototype, "round", _ejs_TemporalPlainTime_prototype_round, 1);
    install_method(_ejs_TemporalPlainTime_prototype, "equals", _ejs_TemporalPlainTime_prototype_equals, 1);
    install_method(_ejs_TemporalPlainTime_prototype, "toString", _ejs_TemporalPlainTime_prototype_toString, 0);
    install_method(_ejs_TemporalPlainTime_prototype, "toLocaleString", _ejs_TemporalPlainTime_prototype_toLocaleString, 0);
    install_method(_ejs_TemporalPlainTime_prototype, "toJSON", _ejs_TemporalPlainTime_prototype_toJSON, 0);
    install_method(_ejs_TemporalPlainTime_prototype, "valueOf", _ejs_Temporal_valueOf_impl, 0);

    // ---- PlainDateTime
    _ejs_TemporalPlainDateTime = create_constructor(_ejs_Temporal, "PlainDateTime", _ejs_TemporalPlainDateTime_impl, 3,
                                                    &_ejs_TemporalPlainDateTime_prototype);
    install_tostringtag(_ejs_TemporalPlainDateTime_prototype, "Temporal.PlainDateTime");
    install_method(_ejs_TemporalPlainDateTime, "from", _ejs_TemporalPlainDateTime_from, 1);
    install_method(_ejs_TemporalPlainDateTime, "compare", _ejs_TemporalPlainDateTime_compare, 2);
    install_getter(_ejs_TemporalPlainDateTime_prototype, "calendarId", _ejs_TemporalPlainDateTime_get_calendarId);
    install_getter(_ejs_TemporalPlainDateTime_prototype, "era", _ejs_TemporalPlainDateTime_get_era);
    install_getter(_ejs_TemporalPlainDateTime_prototype, "eraYear", _ejs_TemporalPlainDateTime_get_eraYear);
    install_getter(_ejs_TemporalPlainDateTime_prototype, "year", _ejs_TemporalPlainDateTime_get_year);
    install_getter(_ejs_TemporalPlainDateTime_prototype, "month", _ejs_TemporalPlainDateTime_get_month);
    install_getter(_ejs_TemporalPlainDateTime_prototype, "monthCode", _ejs_TemporalPlainDateTime_get_monthCode);
    install_getter(_ejs_TemporalPlainDateTime_prototype, "day", _ejs_TemporalPlainDateTime_get_day);
    install_getter(_ejs_TemporalPlainDateTime_prototype, "hour", _ejs_TemporalPlainDateTime_get_hour);
    install_getter(_ejs_TemporalPlainDateTime_prototype, "minute", _ejs_TemporalPlainDateTime_get_minute);
    install_getter(_ejs_TemporalPlainDateTime_prototype, "second", _ejs_TemporalPlainDateTime_get_second);
    install_getter(_ejs_TemporalPlainDateTime_prototype, "millisecond", _ejs_TemporalPlainDateTime_get_millisecond);
    install_getter(_ejs_TemporalPlainDateTime_prototype, "microsecond", _ejs_TemporalPlainDateTime_get_microsecond);
    install_getter(_ejs_TemporalPlainDateTime_prototype, "nanosecond", _ejs_TemporalPlainDateTime_get_nanosecond);
    install_getter(_ejs_TemporalPlainDateTime_prototype, "dayOfWeek", _ejs_TemporalPlainDateTime_get_dayOfWeek);
    install_getter(_ejs_TemporalPlainDateTime_prototype, "dayOfYear", _ejs_TemporalPlainDateTime_get_dayOfYear);
    install_getter(_ejs_TemporalPlainDateTime_prototype, "weekOfYear", _ejs_TemporalPlainDateTime_get_weekOfYear);
    install_getter(_ejs_TemporalPlainDateTime_prototype, "yearOfWeek", _ejs_TemporalPlainDateTime_get_yearOfWeek);
    install_getter(_ejs_TemporalPlainDateTime_prototype, "daysInWeek", _ejs_TemporalPlainDateTime_get_daysInWeek);
    install_getter(_ejs_TemporalPlainDateTime_prototype, "daysInMonth", _ejs_TemporalPlainDateTime_get_daysInMonth);
    install_getter(_ejs_TemporalPlainDateTime_prototype, "daysInYear", _ejs_TemporalPlainDateTime_get_daysInYear);
    install_getter(_ejs_TemporalPlainDateTime_prototype, "monthsInYear", _ejs_TemporalPlainDateTime_get_monthsInYear);
    install_getter(_ejs_TemporalPlainDateTime_prototype, "inLeapYear", _ejs_TemporalPlainDateTime_get_inLeapYear);
    install_method(_ejs_TemporalPlainDateTime_prototype, "with", _ejs_TemporalPlainDateTime_prototype_with, 1);
    install_method(_ejs_TemporalPlainDateTime_prototype, "withPlainTime", _ejs_TemporalPlainDateTime_prototype_withPlainTime, 0);
    install_method(_ejs_TemporalPlainDateTime_prototype, "withCalendar", _ejs_TemporalPlainDateTime_prototype_withCalendar, 1);
    install_method(_ejs_TemporalPlainDateTime_prototype, "add", _ejs_TemporalPlainDateTime_prototype_add, 1);
    install_method(_ejs_TemporalPlainDateTime_prototype, "subtract", _ejs_TemporalPlainDateTime_prototype_subtract, 1);
    install_method(_ejs_TemporalPlainDateTime_prototype, "until", _ejs_TemporalPlainDateTime_prototype_until, 1);
    install_method(_ejs_TemporalPlainDateTime_prototype, "since", _ejs_TemporalPlainDateTime_prototype_since, 1);
    install_method(_ejs_TemporalPlainDateTime_prototype, "round", _ejs_TemporalPlainDateTime_prototype_round, 1);
    install_method(_ejs_TemporalPlainDateTime_prototype, "equals", _ejs_TemporalPlainDateTime_prototype_equals, 1);
    install_method(_ejs_TemporalPlainDateTime_prototype, "toString", _ejs_TemporalPlainDateTime_prototype_toString, 0);
    install_method(_ejs_TemporalPlainDateTime_prototype, "toLocaleString", _ejs_TemporalPlainDateTime_prototype_toLocaleString, 0);
    install_method(_ejs_TemporalPlainDateTime_prototype, "toJSON", _ejs_TemporalPlainDateTime_prototype_toJSON, 0);
    install_method(_ejs_TemporalPlainDateTime_prototype, "valueOf", _ejs_Temporal_valueOf_impl, 0);
    install_method(_ejs_TemporalPlainDateTime_prototype, "toZonedDateTime", _ejs_TemporalPlainDateTime_prototype_toZonedDateTime, 1);
    install_method(_ejs_TemporalPlainDateTime_prototype, "toPlainDate", _ejs_TemporalPlainDateTime_prototype_toPlainDate, 0);
    install_method(_ejs_TemporalPlainDateTime_prototype, "toPlainTime", _ejs_TemporalPlainDateTime_prototype_toPlainTime, 0);

    // ---- PlainYearMonth
    _ejs_TemporalPlainYearMonth = create_constructor(_ejs_Temporal, "PlainYearMonth", _ejs_TemporalPlainYearMonth_impl, 2,
                                                     &_ejs_TemporalPlainYearMonth_prototype);
    install_tostringtag(_ejs_TemporalPlainYearMonth_prototype, "Temporal.PlainYearMonth");
    install_method(_ejs_TemporalPlainYearMonth, "from", _ejs_TemporalPlainYearMonth_from, 1);
    install_method(_ejs_TemporalPlainYearMonth, "compare", _ejs_TemporalPlainYearMonth_compare, 2);
    install_getter(_ejs_TemporalPlainYearMonth_prototype, "calendarId", _ejs_TemporalPlainYearMonth_get_calendarId);
    install_getter(_ejs_TemporalPlainYearMonth_prototype, "era", _ejs_TemporalPlainYearMonth_get_era);
    install_getter(_ejs_TemporalPlainYearMonth_prototype, "eraYear", _ejs_TemporalPlainYearMonth_get_eraYear);
    install_getter(_ejs_TemporalPlainYearMonth_prototype, "year", _ejs_TemporalPlainYearMonth_get_year);
    install_getter(_ejs_TemporalPlainYearMonth_prototype, "month", _ejs_TemporalPlainYearMonth_get_month);
    install_getter(_ejs_TemporalPlainYearMonth_prototype, "monthCode", _ejs_TemporalPlainYearMonth_get_monthCode);
    install_getter(_ejs_TemporalPlainYearMonth_prototype, "daysInMonth", _ejs_TemporalPlainYearMonth_get_daysInMonth);
    install_getter(_ejs_TemporalPlainYearMonth_prototype, "daysInYear", _ejs_TemporalPlainYearMonth_get_daysInYear);
    install_getter(_ejs_TemporalPlainYearMonth_prototype, "monthsInYear", _ejs_TemporalPlainYearMonth_get_monthsInYear);
    install_getter(_ejs_TemporalPlainYearMonth_prototype, "inLeapYear", _ejs_TemporalPlainYearMonth_get_inLeapYear);
    install_method(_ejs_TemporalPlainYearMonth_prototype, "with", _ejs_TemporalPlainYearMonth_prototype_with, 1);
    install_method(_ejs_TemporalPlainYearMonth_prototype, "add", _ejs_TemporalPlainYearMonth_prototype_add, 1);
    install_method(_ejs_TemporalPlainYearMonth_prototype, "subtract", _ejs_TemporalPlainYearMonth_prototype_subtract, 1);
    install_method(_ejs_TemporalPlainYearMonth_prototype, "until", _ejs_TemporalPlainYearMonth_prototype_until, 1);
    install_method(_ejs_TemporalPlainYearMonth_prototype, "since", _ejs_TemporalPlainYearMonth_prototype_since, 1);
    install_method(_ejs_TemporalPlainYearMonth_prototype, "equals", _ejs_TemporalPlainYearMonth_prototype_equals, 1);
    install_method(_ejs_TemporalPlainYearMonth_prototype, "toString", _ejs_TemporalPlainYearMonth_prototype_toString, 0);
    install_method(_ejs_TemporalPlainYearMonth_prototype, "toLocaleString", _ejs_TemporalPlainYearMonth_prototype_toLocaleString, 0);
    install_method(_ejs_TemporalPlainYearMonth_prototype, "toJSON", _ejs_TemporalPlainYearMonth_prototype_toJSON, 0);
    install_method(_ejs_TemporalPlainYearMonth_prototype, "valueOf", _ejs_Temporal_valueOf_impl, 0);
    install_method(_ejs_TemporalPlainYearMonth_prototype, "toPlainDate", _ejs_TemporalPlainYearMonth_prototype_toPlainDate, 1);

    // ---- PlainMonthDay
    _ejs_TemporalPlainMonthDay = create_constructor(_ejs_Temporal, "PlainMonthDay", _ejs_TemporalPlainMonthDay_impl, 2,
                                                    &_ejs_TemporalPlainMonthDay_prototype);
    install_tostringtag(_ejs_TemporalPlainMonthDay_prototype, "Temporal.PlainMonthDay");
    install_method(_ejs_TemporalPlainMonthDay, "from", _ejs_TemporalPlainMonthDay_from, 1);
    install_getter(_ejs_TemporalPlainMonthDay_prototype, "calendarId", _ejs_TemporalPlainMonthDay_get_calendarId);
    install_getter(_ejs_TemporalPlainMonthDay_prototype, "monthCode", _ejs_TemporalPlainMonthDay_get_monthCode);
    install_getter(_ejs_TemporalPlainMonthDay_prototype, "day", _ejs_TemporalPlainMonthDay_get_day);
    install_method(_ejs_TemporalPlainMonthDay_prototype, "with", _ejs_TemporalPlainMonthDay_prototype_with, 1);
    install_method(_ejs_TemporalPlainMonthDay_prototype, "equals", _ejs_TemporalPlainMonthDay_prototype_equals, 1);
    install_method(_ejs_TemporalPlainMonthDay_prototype, "toString", _ejs_TemporalPlainMonthDay_prototype_toString, 0);
    install_method(_ejs_TemporalPlainMonthDay_prototype, "toLocaleString", _ejs_TemporalPlainMonthDay_prototype_toLocaleString, 0);
    install_method(_ejs_TemporalPlainMonthDay_prototype, "toJSON", _ejs_TemporalPlainMonthDay_prototype_toJSON, 0);
    install_method(_ejs_TemporalPlainMonthDay_prototype, "valueOf", _ejs_Temporal_valueOf_impl, 0);
    install_method(_ejs_TemporalPlainMonthDay_prototype, "toPlainDate", _ejs_TemporalPlainMonthDay_prototype_toPlainDate, 1);

    // ---- ZonedDateTime
    _ejs_TemporalZonedDateTime = create_constructor(_ejs_Temporal, "ZonedDateTime", _ejs_TemporalZonedDateTime_impl, 2,
                                                    &_ejs_TemporalZonedDateTime_prototype);
    install_tostringtag(_ejs_TemporalZonedDateTime_prototype, "Temporal.ZonedDateTime");
    install_method(_ejs_TemporalZonedDateTime, "from", _ejs_TemporalZonedDateTime_from, 1);
    install_method(_ejs_TemporalZonedDateTime, "compare", _ejs_TemporalZonedDateTime_compare, 2);
    install_getter(_ejs_TemporalZonedDateTime_prototype, "calendarId", _ejs_TemporalZonedDateTime_get_calendarId);
    install_getter(_ejs_TemporalZonedDateTime_prototype, "timeZoneId", _ejs_TemporalZonedDateTime_get_timeZoneId);
    install_getter(_ejs_TemporalZonedDateTime_prototype, "era", _ejs_TemporalZonedDateTime_get_era);
    install_getter(_ejs_TemporalZonedDateTime_prototype, "eraYear", _ejs_TemporalZonedDateTime_get_eraYear);
    install_getter(_ejs_TemporalZonedDateTime_prototype, "year", _ejs_TemporalZonedDateTime_get_year);
    install_getter(_ejs_TemporalZonedDateTime_prototype, "month", _ejs_TemporalZonedDateTime_get_month);
    install_getter(_ejs_TemporalZonedDateTime_prototype, "monthCode", _ejs_TemporalZonedDateTime_get_monthCode);
    install_getter(_ejs_TemporalZonedDateTime_prototype, "day", _ejs_TemporalZonedDateTime_get_day);
    install_getter(_ejs_TemporalZonedDateTime_prototype, "hour", _ejs_TemporalZonedDateTime_get_hour);
    install_getter(_ejs_TemporalZonedDateTime_prototype, "minute", _ejs_TemporalZonedDateTime_get_minute);
    install_getter(_ejs_TemporalZonedDateTime_prototype, "second", _ejs_TemporalZonedDateTime_get_second);
    install_getter(_ejs_TemporalZonedDateTime_prototype, "millisecond", _ejs_TemporalZonedDateTime_get_millisecond);
    install_getter(_ejs_TemporalZonedDateTime_prototype, "microsecond", _ejs_TemporalZonedDateTime_get_microsecond);
    install_getter(_ejs_TemporalZonedDateTime_prototype, "nanosecond", _ejs_TemporalZonedDateTime_get_nanosecond);
    install_getter(_ejs_TemporalZonedDateTime_prototype, "epochMilliseconds", _ejs_TemporalZonedDateTime_get_epochMilliseconds);
    install_getter(_ejs_TemporalZonedDateTime_prototype, "epochNanoseconds", _ejs_TemporalZonedDateTime_get_epochNanoseconds);
    install_getter(_ejs_TemporalZonedDateTime_prototype, "dayOfWeek", _ejs_TemporalZonedDateTime_get_dayOfWeek);
    install_getter(_ejs_TemporalZonedDateTime_prototype, "dayOfYear", _ejs_TemporalZonedDateTime_get_dayOfYear);
    install_getter(_ejs_TemporalZonedDateTime_prototype, "weekOfYear", _ejs_TemporalZonedDateTime_get_weekOfYear);
    install_getter(_ejs_TemporalZonedDateTime_prototype, "yearOfWeek", _ejs_TemporalZonedDateTime_get_yearOfWeek);
    install_getter(_ejs_TemporalZonedDateTime_prototype, "hoursInDay", _ejs_TemporalZonedDateTime_get_hoursInDay);
    install_getter(_ejs_TemporalZonedDateTime_prototype, "daysInWeek", _ejs_TemporalZonedDateTime_get_daysInWeek);
    install_getter(_ejs_TemporalZonedDateTime_prototype, "daysInMonth", _ejs_TemporalZonedDateTime_get_daysInMonth);
    install_getter(_ejs_TemporalZonedDateTime_prototype, "daysInYear", _ejs_TemporalZonedDateTime_get_daysInYear);
    install_getter(_ejs_TemporalZonedDateTime_prototype, "monthsInYear", _ejs_TemporalZonedDateTime_get_monthsInYear);
    install_getter(_ejs_TemporalZonedDateTime_prototype, "inLeapYear", _ejs_TemporalZonedDateTime_get_inLeapYear);
    install_getter(_ejs_TemporalZonedDateTime_prototype, "offsetNanoseconds", _ejs_TemporalZonedDateTime_get_offsetNanoseconds);
    install_getter(_ejs_TemporalZonedDateTime_prototype, "offset", _ejs_TemporalZonedDateTime_get_offset);
    install_method(_ejs_TemporalZonedDateTime_prototype, "with", _ejs_TemporalZonedDateTime_prototype_with, 1);
    install_method(_ejs_TemporalZonedDateTime_prototype, "withPlainTime", _ejs_TemporalZonedDateTime_prototype_withPlainTime, 0);
    install_method(_ejs_TemporalZonedDateTime_prototype, "withTimeZone", _ejs_TemporalZonedDateTime_prototype_withTimeZone, 1);
    install_method(_ejs_TemporalZonedDateTime_prototype, "withCalendar", _ejs_TemporalZonedDateTime_prototype_withCalendar, 1);
    install_method(_ejs_TemporalZonedDateTime_prototype, "add", _ejs_TemporalZonedDateTime_prototype_add, 1);
    install_method(_ejs_TemporalZonedDateTime_prototype, "subtract", _ejs_TemporalZonedDateTime_prototype_subtract, 1);
    install_method(_ejs_TemporalZonedDateTime_prototype, "until", _ejs_TemporalZonedDateTime_prototype_until, 1);
    install_method(_ejs_TemporalZonedDateTime_prototype, "since", _ejs_TemporalZonedDateTime_prototype_since, 1);
    install_method(_ejs_TemporalZonedDateTime_prototype, "round", _ejs_TemporalZonedDateTime_prototype_round, 1);
    install_method(_ejs_TemporalZonedDateTime_prototype, "equals", _ejs_TemporalZonedDateTime_prototype_equals, 1);
    install_method(_ejs_TemporalZonedDateTime_prototype, "toString", _ejs_TemporalZonedDateTime_prototype_toString, 0);
    install_method(_ejs_TemporalZonedDateTime_prototype, "toLocaleString", _ejs_TemporalZonedDateTime_prototype_toLocaleString, 0);
    install_method(_ejs_TemporalZonedDateTime_prototype, "toJSON", _ejs_TemporalZonedDateTime_prototype_toJSON, 0);
    install_method(_ejs_TemporalZonedDateTime_prototype, "valueOf", _ejs_Temporal_valueOf_impl, 0);
    install_method(_ejs_TemporalZonedDateTime_prototype, "startOfDay", _ejs_TemporalZonedDateTime_prototype_startOfDay, 0);
    install_method(_ejs_TemporalZonedDateTime_prototype, "getTimeZoneTransition", _ejs_TemporalZonedDateTime_prototype_getTimeZoneTransition, 1);
    install_method(_ejs_TemporalZonedDateTime_prototype, "toInstant", _ejs_TemporalZonedDateTime_prototype_toInstant, 0);
    install_method(_ejs_TemporalZonedDateTime_prototype, "toPlainDate", _ejs_TemporalZonedDateTime_prototype_toPlainDate, 0);
    install_method(_ejs_TemporalZonedDateTime_prototype, "toPlainTime", _ejs_TemporalZonedDateTime_prototype_toPlainTime, 0);
    install_method(_ejs_TemporalZonedDateTime_prototype, "toPlainDateTime", _ejs_TemporalZonedDateTime_prototype_toPlainDateTime, 0);

    // ---- Duration
    _ejs_TemporalDuration = create_constructor(_ejs_Temporal, "Duration", _ejs_TemporalDuration_impl, 0,
                                               &_ejs_TemporalDuration_prototype);
    install_tostringtag(_ejs_TemporalDuration_prototype, "Temporal.Duration");
    install_method(_ejs_TemporalDuration, "from", _ejs_TemporalDuration_from, 1);
    install_method(_ejs_TemporalDuration, "compare", _ejs_TemporalDuration_compare, 2);
    install_getter(_ejs_TemporalDuration_prototype, "years", _ejs_TemporalDuration_get_years);
    install_getter(_ejs_TemporalDuration_prototype, "months", _ejs_TemporalDuration_get_months);
    install_getter(_ejs_TemporalDuration_prototype, "weeks", _ejs_TemporalDuration_get_weeks);
    install_getter(_ejs_TemporalDuration_prototype, "days", _ejs_TemporalDuration_get_days);
    install_getter(_ejs_TemporalDuration_prototype, "hours", _ejs_TemporalDuration_get_hours);
    install_getter(_ejs_TemporalDuration_prototype, "minutes", _ejs_TemporalDuration_get_minutes);
    install_getter(_ejs_TemporalDuration_prototype, "seconds", _ejs_TemporalDuration_get_seconds);
    install_getter(_ejs_TemporalDuration_prototype, "milliseconds", _ejs_TemporalDuration_get_milliseconds);
    install_getter(_ejs_TemporalDuration_prototype, "microseconds", _ejs_TemporalDuration_get_microseconds);
    install_getter(_ejs_TemporalDuration_prototype, "nanoseconds", _ejs_TemporalDuration_get_nanoseconds);
    install_getter(_ejs_TemporalDuration_prototype, "sign", _ejs_TemporalDuration_get_sign);
    install_getter(_ejs_TemporalDuration_prototype, "blank", _ejs_TemporalDuration_get_blank);
    install_method(_ejs_TemporalDuration_prototype, "with", _ejs_TemporalDuration_prototype_with, 1);
    install_method(_ejs_TemporalDuration_prototype, "negated", _ejs_TemporalDuration_prototype_negated, 0);
    install_method(_ejs_TemporalDuration_prototype, "abs", _ejs_TemporalDuration_prototype_abs, 0);
    install_method(_ejs_TemporalDuration_prototype, "add", _ejs_TemporalDuration_prototype_add, 1);
    install_method(_ejs_TemporalDuration_prototype, "subtract", _ejs_TemporalDuration_prototype_subtract, 1);
    install_method(_ejs_TemporalDuration_prototype, "round", _ejs_TemporalDuration_prototype_round, 1);
    install_method(_ejs_TemporalDuration_prototype, "total", _ejs_TemporalDuration_prototype_total, 1);
    install_method(_ejs_TemporalDuration_prototype, "toString", _ejs_TemporalDuration_prototype_toString, 0);
    install_method(_ejs_TemporalDuration_prototype, "toJSON", _ejs_TemporalDuration_prototype_toJSON, 0);
    install_method(_ejs_TemporalDuration_prototype, "toLocaleString", _ejs_TemporalDuration_prototype_toLocaleString, 0);
    install_method(_ejs_TemporalDuration_prototype, "valueOf", _ejs_Temporal_valueOf_impl, 0);
}




