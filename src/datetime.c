/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file datetime.c
 * @brief Date field of the header, DDMMYYYYhhmm.
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

enum
{
    /** The first year that a date field can hold. */
    SDFIO_FIRST_YEAR = 1,
    /** The last year that a date field can hold. */
    SDFIO_LAST_YEAR = 9999
};

enum
{
    SECONDS_PER_MINUTE = 60,
    MINUTES_PER_HOUR = 60,
    HOURS_PER_DAY = 24,
    MINUTES_PER_DAY = MINUTES_PER_HOUR * HOURS_PER_DAY,
    MONTHS_PER_YEAR = 12,
    TWO_DIGITS = 2,
    YEAR_DIGITS = 4,
    DAY_AT = 0,
    MONTH_AT = 2,
    YEAR_AT = 4,
    HOUR_AT = 8,
    MINUTE_AT = 10
};

/* The proleptic Gregorian calendar. Days are counted from 1970-01-01. */
enum
{
    DAYS_PER_YEAR = 365,
    /* The leap days of 400 years: 100 by four, less 4 by 100, plus 1 by 400. */
    DAYS_PER_400_YEARS = 400 * DAYS_PER_YEAR + 97,
    YEARS_PER_400_YEARS = 400,
    /* From 0001-01-01 to 1970-01-01 */
    YEAR_ONE_TO_EPOCH = 719162,
    FEBRUARY = 2
};

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

time_t sdfio_now(void)
{
    const time_t now = time(NULL);
    if (now == (time_t)-1)
    {
        return SDFIO_NO_TIME;
    }
    return (time_t)(minutes_of(now) * SECONDS_PER_MINUTE);
}

bool sdfio_datetime_representable(time_t time)
{
    int64_t year = 0;
    unsigned month = 0;
    unsigned day = 0;
    civil_from_days(floor_div(minutes_of(time), MINUTES_PER_DAY), &year, &month,
                    &day);
    return year >= SDFIO_FIRST_YEAR && year <= SDFIO_LAST_YEAR;
}

/* A non-negative number as decimal digits, filled with zeros. */
static void put_digits(char *out, int value, size_t width)
{
    for (size_t i = width; i > 0; --i)
    {
        out[i - 1] = (char)('0' + value % 10);
        value /= 10;
    }
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
    put_digits(out + DAY_AT, (int)day, TWO_DIGITS);
    put_digits(out + MONTH_AT, (int)month, TWO_DIGITS);
    put_digits(out + YEAR_AT, (int)year, YEAR_DIGITS);
    put_digits(out + HOUR_AT, (int)(minute_of_day / MINUTES_PER_HOUR),
               TWO_DIGITS);
    put_digits(out + MINUTE_AT, (int)(minute_of_day % MINUTES_PER_HOUR),
               TWO_DIGITS);
    return true;
}

static int digits_at(const char *text, size_t at, size_t count)
{
    int number = 0;
    for (size_t i = 0; i < count; ++i)
    {
        number = number * 10 + (text[at + i] - '0');
    }
    return number;
}

/** The fields of a date in the text DDMMYYYYhhmm. */
typedef struct date_fields
{
    /** The year, 1 to 9999. */
    int year;
    /** The month, 1 to 12. */
    int month;
    /** The day of the month. */
    int day;
    /** The hour, 0 to 23. */
    int hour;
    /** The minute, 0 to 59. */
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
    fields->day = digits_at(text, DAY_AT, TWO_DIGITS);
    fields->month = digits_at(text, MONTH_AT, TWO_DIGITS);
    fields->year = digits_at(text, YEAR_AT, YEAR_DIGITS);
    fields->hour = digits_at(text, HOUR_AT, TWO_DIGITS);
    fields->minute = digits_at(text, MINUTE_AT, TWO_DIGITS);
    return fields->year >= SDFIO_FIRST_YEAR && fields->month >= 1
           && fields->month <= MONTHS_PER_YEAR && fields->day >= 1
           && (unsigned)fields->day
                  <= days_in_month(fields->year, (unsigned)fields->month)
           && fields->hour < HOURS_PER_DAY && fields->minute < MINUTES_PER_HOUR;
}

bool sdfio_datetime_is_valid(const char *text, size_t size)
{
    date_fields fields;
    return read_date(text, size, &fields);
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
