/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#ifndef _ejs_temporal_h_
#define _ejs_temporal_h_

#include "ejs.h"
#include "ejs-value.h"
#include "ejs-object.h"

// Temporal (ES2026).  Epoch instants and internal time-duration math
// use __int128 nanoseconds: the valid instant range is ±8.64e21 ns
// (±1e8 days from epoch), which exceeds int64 but fits comfortably in
// 128 bits.  BigInt conversion happens only at the API boundary.
typedef __int128 ejs_i128;

// ±8.64e21: nsMaxInstant / nsMinInstant
#define EJS_TEMPORAL_NS_MAX_INSTANT (((ejs_i128)8640000000000LL) * 1000000000LL)
#define EJS_TEMPORAL_NS_MIN_INSTANT (-EJS_TEMPORAL_NS_MAX_INSTANT)

typedef struct {
    /* object header */
    EJSObject obj;

    ejs_i128 epoch_ns;
} EJSTemporalInstant;

typedef struct {
    /* object header */
    EJSObject obj;

    ejsval calendar; // calendar id string, currently always "iso8601"
    int32_t year;
    int32_t month;   // 1-12
    int32_t day;     // 1-31
} EJSTemporalPlainDate;

typedef struct {
    /* object header */
    EJSObject obj;

    int32_t hour, minute, second;
    int32_t millisecond, microsecond, nanosecond;
} EJSTemporalPlainTime;

typedef struct {
    /* object header */
    EJSObject obj;

    ejsval calendar;
    int32_t year, month, day;
    int32_t hour, minute, second;
    int32_t millisecond, microsecond, nanosecond;
} EJSTemporalPlainDateTime;

typedef struct {
    /* object header */
    EJSObject obj;

    ejsval calendar;
    int32_t year, month;
    int32_t ref_day; // [[ISODate]].day, reachable through toPlainDate
} EJSTemporalPlainYearMonth;

typedef struct {
    /* object header */
    EJSObject obj;

    ejsval calendar;
    int32_t month, day;
    int32_t ref_year; // [[ISODate]].year, reachable through toPlainDate
} EJSTemporalPlainMonthDay;

typedef struct {
    /* object header */
    EJSObject obj;

    ejsval calendar;
    ejsval time_zone; // time zone id string: IANA name or offset
    ejs_i128 epoch_ns;
} EJSTemporalZonedDateTime;

typedef struct {
    /* object header */
    EJSObject obj;

    // spec [[Years]]..[[Nanoseconds]]: float64-representable integers,
    // all the same sign
    double years, months, weeks, days;
    double hours, minutes, seconds;
    double milliseconds, microseconds, nanoseconds;
} EJSTemporalDuration;

#define EJSVAL_IS_TEMPORAL_INSTANT(v)        (EJSVAL_IS_OBJECT(v) && (EJSVAL_TO_OBJECT(v)->ops == &_ejs_TemporalInstant_specops))
#define EJSVAL_IS_TEMPORAL_PLAINDATE(v)      (EJSVAL_IS_OBJECT(v) && (EJSVAL_TO_OBJECT(v)->ops == &_ejs_TemporalPlainDate_specops))
#define EJSVAL_IS_TEMPORAL_PLAINTIME(v)      (EJSVAL_IS_OBJECT(v) && (EJSVAL_TO_OBJECT(v)->ops == &_ejs_TemporalPlainTime_specops))
#define EJSVAL_IS_TEMPORAL_PLAINDATETIME(v)  (EJSVAL_IS_OBJECT(v) && (EJSVAL_TO_OBJECT(v)->ops == &_ejs_TemporalPlainDateTime_specops))
#define EJSVAL_IS_TEMPORAL_PLAINYEARMONTH(v) (EJSVAL_IS_OBJECT(v) && (EJSVAL_TO_OBJECT(v)->ops == &_ejs_TemporalPlainYearMonth_specops))
#define EJSVAL_IS_TEMPORAL_PLAINMONTHDAY(v)  (EJSVAL_IS_OBJECT(v) && (EJSVAL_TO_OBJECT(v)->ops == &_ejs_TemporalPlainMonthDay_specops))
#define EJSVAL_IS_TEMPORAL_ZONEDDATETIME(v)  (EJSVAL_IS_OBJECT(v) && (EJSVAL_TO_OBJECT(v)->ops == &_ejs_TemporalZonedDateTime_specops))
#define EJSVAL_IS_TEMPORAL_DURATION(v)       (EJSVAL_IS_OBJECT(v) && (EJSVAL_TO_OBJECT(v)->ops == &_ejs_TemporalDuration_specops))

#define EJSVAL_TO_TEMPORAL_INSTANT(v)        ((EJSTemporalInstant*)EJSVAL_TO_OBJECT(v))
#define EJSVAL_TO_TEMPORAL_PLAINDATE(v)      ((EJSTemporalPlainDate*)EJSVAL_TO_OBJECT(v))
#define EJSVAL_TO_TEMPORAL_PLAINTIME(v)      ((EJSTemporalPlainTime*)EJSVAL_TO_OBJECT(v))
#define EJSVAL_TO_TEMPORAL_PLAINDATETIME(v)  ((EJSTemporalPlainDateTime*)EJSVAL_TO_OBJECT(v))
#define EJSVAL_TO_TEMPORAL_PLAINYEARMONTH(v) ((EJSTemporalPlainYearMonth*)EJSVAL_TO_OBJECT(v))
#define EJSVAL_TO_TEMPORAL_PLAINMONTHDAY(v)  ((EJSTemporalPlainMonthDay*)EJSVAL_TO_OBJECT(v))
#define EJSVAL_TO_TEMPORAL_ZONEDDATETIME(v)  ((EJSTemporalZonedDateTime*)EJSVAL_TO_OBJECT(v))
#define EJSVAL_TO_TEMPORAL_DURATION(v)       ((EJSTemporalDuration*)EJSVAL_TO_OBJECT(v))

EJS_BEGIN_DECLS

extern ejsval _ejs_Temporal; // the namespace object
extern ejsval _ejs_Temporal_Now;

extern ejsval _ejs_TemporalInstant;
extern ejsval _ejs_TemporalInstant_prototype;
extern ejsval _ejs_TemporalPlainDate;
extern ejsval _ejs_TemporalPlainDate_prototype;
extern ejsval _ejs_TemporalPlainTime;
extern ejsval _ejs_TemporalPlainTime_prototype;
extern ejsval _ejs_TemporalPlainDateTime;
extern ejsval _ejs_TemporalPlainDateTime_prototype;
extern ejsval _ejs_TemporalPlainYearMonth;
extern ejsval _ejs_TemporalPlainYearMonth_prototype;
extern ejsval _ejs_TemporalPlainMonthDay;
extern ejsval _ejs_TemporalPlainMonthDay_prototype;
extern ejsval _ejs_TemporalZonedDateTime;
extern ejsval _ejs_TemporalZonedDateTime_prototype;
extern ejsval _ejs_TemporalDuration;
extern ejsval _ejs_TemporalDuration_prototype;

extern EJSSpecOps _ejs_TemporalInstant_specops;
extern EJSSpecOps _ejs_TemporalPlainDate_specops;
extern EJSSpecOps _ejs_TemporalPlainTime_specops;
extern EJSSpecOps _ejs_TemporalPlainDateTime_specops;
extern EJSSpecOps _ejs_TemporalPlainYearMonth_specops;
extern EJSSpecOps _ejs_TemporalPlainMonthDay_specops;
extern EJSSpecOps _ejs_TemporalZonedDateTime_specops;
extern EJSSpecOps _ejs_TemporalDuration_specops;

void _ejs_temporal_init(ejsval global);

// ---- shared internals (ejs-temporal-*.c) ----

// BigInt boundary
ejsval  _ejs_temporal_i128_to_bigint(ejs_i128 v);
// throws RangeError unless the bigint is a valid epoch-nanoseconds value
ejs_i128 _ejs_temporal_bigint_to_epoch_ns(ejsval bi);

// ISO calendar math
EJSBool _ejs_temporal_is_valid_iso_date(int32_t year, int32_t month, int32_t day);
EJSBool _ejs_temporal_is_valid_time(int32_t h, int32_t mi, int32_t s, int32_t ms, int32_t us, int32_t ns);
int32_t _ejs_temporal_iso_days_in_month(int32_t year, int32_t month);
EJSBool _ejs_temporal_iso_is_leap_year(int32_t year);
int32_t _ejs_temporal_iso_day_of_week(int32_t year, int32_t month, int32_t day);   // 1=Monday..7=Sunday
int32_t _ejs_temporal_iso_day_of_year(int32_t year, int32_t month, int32_t day);   // 1-based
void    _ejs_temporal_iso_week_of_year(int32_t year, int32_t month, int32_t day, int32_t* week, int32_t* week_year);
// days since 1970-01-01 (can exceed int32 for the proleptic range)
int64_t _ejs_temporal_iso_date_to_epoch_days(int32_t year, int32_t month, int32_t day);
void    _ejs_temporal_epoch_days_to_iso_date(int64_t epoch_days, int32_t* year, int32_t* month, int32_t* day);
// ±(nsMaxInstant + nsPerDay): the representable-ISO-datetime bound
EJSBool _ejs_temporal_iso_datetime_within_limits(int32_t year, int32_t month, int32_t day,
                                                 int32_t h, int32_t mi, int32_t s,
                                                 int32_t ms, int32_t us, int32_t ns);

// instance creation from validated parts (no user-visible lookups)
ejsval _ejs_temporal_instant_new(ejs_i128 epoch_ns);
ejsval _ejs_temporal_plain_date_new(int32_t year, int32_t month, int32_t day, ejsval calendar);
ejsval _ejs_temporal_plain_time_new(int32_t h, int32_t mi, int32_t s, int32_t ms, int32_t us, int32_t ns);
ejsval _ejs_temporal_plain_datetime_new(int32_t year, int32_t month, int32_t day,
                                        int32_t h, int32_t mi, int32_t s,
                                        int32_t ms, int32_t us, int32_t ns, ejsval calendar);
ejsval _ejs_temporal_plain_yearmonth_new(int32_t year, int32_t month, int32_t ref_day, ejsval calendar);
ejsval _ejs_temporal_plain_monthday_new(int32_t month, int32_t day, int32_t ref_year, ejsval calendar);
ejsval _ejs_temporal_zoneddatetime_new(ejs_i128 epoch_ns, ejsval time_zone, ejsval calendar);
ejsval _ejs_temporal_duration_new(double years, double months, double weeks, double days,
                                  double hours, double minutes, double seconds,
                                  double ms, double us, double ns);

EJS_END_DECLS

#endif
