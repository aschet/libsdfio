/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file dates.c
 * @brief The date field of the header: DDMMYYYYhhmm.
 *
 * ISO-2.0 holds UTC and the other revisions the local clock of the writer,
 * whose time zone is not known to the library: the time zone of the caller
 * converts it, and without one the clock reading is kept as it is. The
 * calendar is computed here, with no function of the C library that has a
 * time zone or a shared result.
 */

#include "internal.h"

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

/* The proleptic Gregorian calendar by the algorithm of Howard Hinnant
 * (chrono-Compatible Low-Level Date Algorithms): eras of 400 years, with the
 * year starting on 1 March so that the leap day is the last day. */
enum
{
    YEARS_PER_ERA = 400,
    DAYS_PER_ERA = 146097,
    DAYS_PER_YEAR = 365,
    DAYS_PER_FOUR_YEARS = 1461,
    DAYS_PER_CENTURY = 36524,
    /* From 0000-03-01 to 1970-01-01 */
    ERA_START_TO_EPOCH = 719468,
    /* The months from March repeat their lengths every five months: 153
     * days. */
    DAYS_PER_FIVE_MONTHS = 153,
    MONTHS_PER_CYCLE = 5,
    CYCLE_ROUNDING = 2,
    MARCH = 3,
    FEBRUARY = 2,
    /* The months that follow December in a year that starts with March. */
    MONTHS_BEFORE_MARCH = 9,
    LAST_MONTH_FROM_MARCH_IN_YEAR = 10
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

static int64_t day_of_year_at_month_start(int64_t month_from_march)
{
    return (DAYS_PER_FIVE_MONTHS * month_from_march + CYCLE_ROUNDING)
           / MONTHS_PER_CYCLE;
}

/* Days since 1970-01-01 of a date of the calendar. */
static int64_t days_from_civil(int64_t year, unsigned month, unsigned day)
{
    year -= month <= FEBRUARY ? 1 : 0;
    const int64_t era = floor_div(year, YEARS_PER_ERA);
    const int64_t year_of_era = year - era * YEARS_PER_ERA;
    const int64_t month_from_march = month >= MARCH
                                         ? (int64_t)month - MARCH
                                         : (int64_t)month + MONTHS_BEFORE_MARCH;
    const int64_t day_of_year =
        day_of_year_at_month_start(month_from_march) + (int64_t)day - 1;
    const int64_t day_of_era = year_of_era * DAYS_PER_YEAR + year_of_era / 4
                               - year_of_era / 100 + day_of_year;
    return era * DAYS_PER_ERA + day_of_era - ERA_START_TO_EPOCH;
}

static void civil_from_days(int64_t days, int64_t *year, unsigned *month,
                            unsigned *day)
{
    days += ERA_START_TO_EPOCH;
    const int64_t era = floor_div(days, DAYS_PER_ERA);
    const int64_t day_of_era = days - era * DAYS_PER_ERA;
    const int64_t year_of_era =
        (day_of_era - day_of_era / (DAYS_PER_FOUR_YEARS - 1)
         + day_of_era / DAYS_PER_CENTURY - day_of_era / (DAYS_PER_ERA - 1))
        / DAYS_PER_YEAR;
    const int64_t day_of_year =
        day_of_era
        - (DAYS_PER_YEAR * year_of_era + year_of_era / 4 - year_of_era / 100);
    const int64_t month_from_march =
        (MONTHS_PER_CYCLE * day_of_year + CYCLE_ROUNDING)
        / DAYS_PER_FIVE_MONTHS;
    *day = (unsigned)(day_of_year - day_of_year_at_month_start(month_from_march)
                      + 1);
    *month = (unsigned)(month_from_march <= LAST_MONTH_FROM_MARCH_IN_YEAR - 1
                            ? month_from_march + MARCH
                            : month_from_march - MONTHS_BEFORE_MARCH);
    *year = year_of_era + era * YEARS_PER_ERA + (*month <= FEBRUARY ? 1 : 0);
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

bool sdfio_date_representable(time_t time)
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

bool sdfio_date_format(time_t time, sdfio_revision revision,
                       const sdfio_time_zone *zone, char out[SDFIO_DATE_SIZE])
{
    if (time == SDFIO_NO_TIME)
    {
        memset(out, '0', SDFIO_DATE_SIZE);
        return true;
    }
    if (!sdfio_requires_utc(revision) && zone != NULL
        && zone->utc_to_local != NULL)
    {
        time = zone->utc_to_local(zone->user, time);
        if (time == SDFIO_NO_TIME || !sdfio_date_representable(time))
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

time_t sdfio_date_parse(const char *text, size_t size, sdfio_revision revision,
                        const sdfio_time_zone *zone)
{
    sdfio_trim(&text, &size);
    if (size != SDFIO_DATE_SIZE)
    {
        return SDFIO_NO_TIME;
    }
    bool all_zero = true;
    for (size_t i = 0; i < size; ++i)
    {
        if (!sdfio_is_digit(text[i]))
        {
            return SDFIO_NO_TIME;
        }
        all_zero = all_zero && text[i] == '0';
    }
    if (all_zero)
    {
        return SDFIO_NO_TIME;
    }
    const int day = digits_at(text, DAY_AT, TWO_DIGITS);
    const int month = digits_at(text, MONTH_AT, TWO_DIGITS);
    const int year = digits_at(text, YEAR_AT, YEAR_DIGITS);
    const int hour = digits_at(text, HOUR_AT, TWO_DIGITS);
    const int minute = digits_at(text, MINUTE_AT, TWO_DIGITS);
    if (year < SDFIO_FIRST_YEAR || month < 1 || month > MONTHS_PER_YEAR
        || day < 1 || hour >= HOURS_PER_DAY || minute >= MINUTES_PER_HOUR)
    {
        return SDFIO_NO_TIME;
    }
    /* A day that the month does not have comes back as another date. */
    const int64_t days = days_from_civil(year, (unsigned)month, (unsigned)day);
    int64_t back_year = 0;
    unsigned back_month = 0;
    unsigned back_day = 0;
    civil_from_days(days, &back_year, &back_month, &back_day);
    if (back_year != year || back_month != (unsigned)month
        || back_day != (unsigned)day)
    {
        return SDFIO_NO_TIME;
    }
    const int64_t clock =
        days * MINUTES_PER_DAY + (int64_t)hour * MINUTES_PER_HOUR + minute;
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
