/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief Conversion between the date field DDMMYYYYhhmm and time_t.
 *
 * ISO-2.0 holds UTC and the other revisions the local clock of the writer,
 * whose time zone is not known to the library: the time zone of the caller
 * converts it, and without one the clock reading is kept as it is. The
 * calendar is computed here, with no function of the C library that has a
 * time zone or a shared result.
 */

#include "datetime.h"
#include "fields.h"
#include "revision.h"
#include "text.h"
#include <ctype.h>
#include <string.h>

/* The first year that a date field can hold. */
#define FIRST_YEAR 1
/* The last year that a date field can hold. */
#define LAST_YEAR 9999

#define SECONDS_PER_MINUTE 60
#define MINUTES_PER_HOUR 60
#define HOURS_PER_DAY 24
#define MINUTES_PER_DAY ((int64_t)MINUTES_PER_HOUR * HOURS_PER_DAY)
#define MONTHS_PER_YEAR 12

/* The proleptic Gregorian calendar. Days are counted from 1970-01-01. */
#define DAYS_PER_YEAR 365
/* The leap days of 400 years: 100 by four, less 4 by 100, plus 1 by 400. */
#define DAYS_PER_400_YEARS (400 * DAYS_PER_YEAR + 97)
#define YEARS_PER_400_YEARS 400
/* From 0001-01-01 to 1970-01-01 */
#define YEAR_ONE_TO_EPOCH 719162
#define FEBRUARY 2

/* The quotient rounded toward negative infinity, where C rounds toward 0. */
static int64_t floor_div(int64_t a, int64_t b)
{
    int64_t q = a / b;
    if ((a % b != 0) && ((a < 0) != (b < 0)))
    {
        --q;
    }
    return q;
}

static bool is_leap_year(int64_t year)
{
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

static unsigned days_in_month(int64_t year, unsigned month)
{
    static const unsigned days[MONTHS_PER_YEAR] = {31, 28, 31, 30, 31, 30,
                                                   31, 31, 30, 31, 30, 31};
    return days[month - 1] + (month == FEBRUARY && is_leap_year(year) ? 1 : 0);
}

/* The days from 0001-01-01 to the first day of a year. */
static int64_t days_before_year(int64_t year)
{
    const int64_t past = year - 1;
    return past * DAYS_PER_YEAR + floor_div(past, 4) - floor_div(past, 100)
           + floor_div(past, 400);
}

/* Days since 1970-01-01 of a date of the calendar. */
static int64_t days_from_civil(int64_t year, unsigned month, unsigned day)
{
    int64_t days =
        days_before_year(year) - YEAR_ONE_TO_EPOCH + (int64_t)day - 1;
    for (unsigned before = 1; before < month; ++before)
    {
        days += days_in_month(year, before);
    }
    return days;
}

/* The date of a day count since 1970-01-01. */
static void civil_from_days(int64_t days, int64_t *year, unsigned *month,
                            unsigned *day)
{
    const int64_t since_year_one = days + YEAR_ONE_TO_EPOCH;
    /* The mean length of a year gives the year, or one off. */
    int64_t guess =
        floor_div(since_year_one * YEARS_PER_400_YEARS, DAYS_PER_400_YEARS) + 1;
    while (days_before_year(guess) > since_year_one)
    {
        --guess;
    }
    while (days_before_year(guess + 1) <= since_year_one)
    {
        ++guess;
    }
    int64_t left = since_year_one - days_before_year(guess);
    unsigned count = 1;
    while (left >= days_in_month(guess, count))
    {
        left -= days_in_month(guess, count);
        ++count;
    }
    *year = guess;
    *month = count;
    *day = (unsigned)left + 1;
}

/* The whole minutes of a time, rounded down for a time before 1970. */
static int64_t minutes_of(time_t time)
{
    return floor_div((int64_t)time, SECONDS_PER_MINUTE);
}

/* Seconds as a time_t, if the type has room for them: a time_t of 32 bits ends
 * in the year 2038, and a date beyond it must not wrap into another one. */
static bool seconds_to_time(int64_t seconds, time_t *out)
{
    const time_t converted = (time_t)seconds;
    if ((int64_t)converted != seconds)
    {
        return false;
    }
    *out = converted;
    return true;
}

/* The positions and the widths of the fields in the text DDMMYYYYhhmm. */
#define DAY_AT 0
#define DAY_WIDTH 2
#define MONTH_AT (DAY_AT + DAY_WIDTH)
#define MONTH_WIDTH 2
#define YEAR_AT (MONTH_AT + MONTH_WIDTH)
#define YEAR_WIDTH 4
#define HOUR_AT (YEAR_AT + YEAR_WIDTH)
#define HOUR_WIDTH 2
#define MINUTE_AT (HOUR_AT + HOUR_WIDTH)
#define MINUTE_WIDTH 2

_Static_assert(MINUTE_AT + MINUTE_WIDTH == SDFIO_DATETIME_SIZE,
               "the fields have to fill the date");

/* Writes a number as `width` decimal digits with leading zeros. Digits above
 * the width are dropped. */
static void put_digits(char *out, unsigned value, size_t width)
{
    while (width > 0)
    {
        --width;
        out[width] = (char)('0' + value % 10);
        value /= 10;
    }
}

/* The number that `width` decimal digits give. */
static int get_digits(const char *text, size_t width)
{
    int value = 0;
    for (size_t i = 0; i < width; ++i)
    {
        value = value * 10 + (text[i] - '0');
    }
    return value;
}

time_t sdfio_datetime_round(time_t time)
{
    return (time_t)(minutes_of(time) * SECONDS_PER_MINUTE);
}

bool sdfio_datetime_representable(time_t time)
{
    int64_t year = 0;
    unsigned month = 0;
    unsigned day = 0;
    civil_from_days(floor_div(minutes_of(time), MINUTES_PER_DAY), &year, &month,
                    &day);
    return year >= FIRST_YEAR && year <= LAST_YEAR;
}

bool sdfio_datetime_format(time_t time, sdfio_revision revision,
                           const sdfio_time_zone *zone,
                           char out[SDFIO_DATETIME_SIZE])
{
    if (time == SDFIO_NO_TIME)
    {
        memset(out, '0', SDFIO_DATETIME_SIZE);
        return true;
    }
    if (!sdfio_requires_utc(revision) && zone != NULL
        && zone->utc_to_local != NULL)
    {
        time = zone->utc_to_local(zone->user, time);
        if (time == SDFIO_NO_TIME || !sdfio_datetime_representable(time))
        {
            return false;
        }
    }
    const int64_t clock = minutes_of(time);
    const int64_t days = floor_div(clock, MINUTES_PER_DAY);
    const int64_t minute_of_day = clock - days * MINUTES_PER_DAY;
    int64_t year = 0;
    unsigned month = 0;
    unsigned day = 0;
    civil_from_days(days, &year, &month, &day);
    put_digits(out + DAY_AT, day, DAY_WIDTH);
    put_digits(out + MONTH_AT, month, MONTH_WIDTH);
    put_digits(out + YEAR_AT, (unsigned)year, YEAR_WIDTH);
    put_digits(out + HOUR_AT, (unsigned)(minute_of_day / MINUTES_PER_HOUR),
               HOUR_WIDTH);
    put_digits(out + MINUTE_AT, (unsigned)(minute_of_day % MINUTES_PER_HOUR),
               MINUTE_WIDTH);
    return true;
}

/* The fields of a date in the text DDMMYYYYhhmm. */
typedef struct date_fields
{
    /* The year, 1 to 9999. */
    int year;
    /* The month, 1 to 12. */
    int month;
    /* The day of the month. */
    int day;
    /* The hour, 0 to 23. */
    int hour;
    /* The minute, 0 to 59. */
    int minute;
} date_fields;

/* The fields of a date, if the text has the ranges of the standard and a day
 * that the month has. */
static bool read_date(const char *text, size_t size, date_fields *fields)
{
    sdfio_trim(&text, &size);
    if (size != SDFIO_DATETIME_SIZE)
    {
        return false;
    }
    for (size_t i = 0; i < size; ++i)
    {
        if (!isdigit((unsigned char)text[i]))
        {
            return false;
        }
    }
    fields->day = get_digits(text + DAY_AT, DAY_WIDTH);
    fields->month = get_digits(text + MONTH_AT, MONTH_WIDTH);
    fields->year = get_digits(text + YEAR_AT, YEAR_WIDTH);
    fields->hour = get_digits(text + HOUR_AT, HOUR_WIDTH);
    fields->minute = get_digits(text + MINUTE_AT, MINUTE_WIDTH);
    return fields->year >= FIRST_YEAR && fields->month >= 1
           && fields->month <= MONTHS_PER_YEAR && fields->day >= 1
           && (unsigned)fields->day
                  <= days_in_month(fields->year, (unsigned)fields->month)
           && fields->hour < HOURS_PER_DAY && fields->minute < MINUTES_PER_HOUR;
}

/* Tolerated deviation: a date of the character 0 only, or of the byte 0 only,
 * is a date that is not recorded.
 *
 * The text that exporters write for a date that they do not record. */
static bool is_not_recorded(const char *text, size_t size)
{
    if (size == 0 || size > SDFIO_DATETIME_SIZE)
    {
        return false;
    }
    for (size_t i = 1; i < size; ++i)
    {
        if (text[i] != text[0])
        {
            return false;
        }
    }
    return text[0] == '0' || text[0] == '\0';
}

bool sdfio_datetime_valid(const char *text, size_t size)
{
    date_fields fields;
    return is_not_recorded(text, size) || read_date(text, size, &fields);
}

time_t sdfio_datetime_parse(const char *text, size_t size,
                            sdfio_revision revision,
                            const sdfio_time_zone *zone)
{
    date_fields fields;
    if (!read_date(text, size, &fields))
    {
        return SDFIO_NO_TIME;
    }
    const int64_t days = days_from_civil(fields.year, (unsigned)fields.month,
                                         (unsigned)fields.day);
    const int64_t clock = days * MINUTES_PER_DAY
                          + (int64_t)fields.hour * MINUTES_PER_HOUR
                          + fields.minute;
    time_t date = 0;
    if (!seconds_to_time(clock * SECONDS_PER_MINUTE, &date))
    {
        return SDFIO_NO_TIME;
    }
    if (!sdfio_requires_utc(revision) && zone != NULL
        && zone->local_to_utc != NULL)
    {
        date = zone->local_to_utc(zone->user, date);
    }
    return date;
}
