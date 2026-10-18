/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* The date field of the header: DDMMYYYYhhmm. ISO-2.0 holds UTC and the other
 * revisions the local clock of the writer; the library has UTC time points.
 * Only localtime_r is used for the local clock, which is safe to call from
 * several threads, and the calendar is computed here. */

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
    MINUTE_AT = 10,
    /* What struct tm counts from. */
    TM_YEAR_BASE = 1900,
    TM_MONTH_BASE = 1
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

time_t sdfi_now(void)
{
    const time_t now = time(NULL);
    if (now == (time_t)-1)
    {
        return SDFIO_NO_TIME;
    }
    return (time_t)(minutes_of(now) * SECONDS_PER_MINUTE);
}

static bool local_time(time_t time, struct tm *out)
{
#if defined(_WIN32)
    return localtime_s(out, &time) == 0;
#else
    return localtime_r(&time, out) != NULL;
#endif
}

/* The minutes of the local clock at a UTC time, counted as if it were UTC. */
static bool local_clock_of(int64_t utc_minutes, int64_t *clock)
{
    struct tm local;
    if (!local_time((time_t)(utc_minutes * SECONDS_PER_MINUTE), &local))
    {
        return false;
    }
    *clock = days_from_civil(local.tm_year + (int64_t)TM_YEAR_BASE,
                             (unsigned)(local.tm_mon + TM_MONTH_BASE),
                             (unsigned)local.tm_mday)
                 * MINUTES_PER_DAY
             + (int64_t)local.tm_hour * MINUTES_PER_HOUR + local.tm_min;
    return true;
}

/* The UTC time at which the local clock shows `clock`: the offset is looked up
 * at the guess and again at the corrected one, which settles it except in the
 * hour that a change of the clock repeats or skips. */
static int64_t utc_of_local_clock(int64_t clock)
{
    int64_t shown = 0;
    if (!local_clock_of(clock, &shown))
    {
        return clock;
    }
    const int64_t first_offset = shown - clock;
    int64_t guess = clock - first_offset;
    if (!local_clock_of(guess, &shown))
    {
        return guess;
    }
    const int64_t second_offset = shown - guess;
    return second_offset == first_offset ? guess : clock - second_offset;
}

bool sdfi_date_representable(time_t time)
{
    int64_t year = 0;
    unsigned month = 0;
    unsigned day = 0;
    civil_from_days(floor_div(minutes_of(time), MINUTES_PER_DAY), &year, &month,
                    &day);
    return year >= SDFI_FIRST_YEAR && year <= SDFI_LAST_YEAR;
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

void sdfi_date_format(time_t time, sdfio_revision revision,
                      char out[SDFI_DATE_SIZE])
{
    if (time == SDFIO_NO_TIME)
    {
        memset(out, '0', SDFI_DATE_SIZE);
        return;
    }
    int64_t clock = minutes_of(time);
    if (!sdfi_requires_utc(revision))
    {
        int64_t local = 0;
        if (local_clock_of(clock, &local))
        {
            clock = local;
        }
    }
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

time_t sdfi_date_parse(const char *text, size_t size, sdfio_revision revision)
{
    sdfi_trim(&text, &size);
    if (size != SDFI_DATE_SIZE)
    {
        return SDFIO_NO_TIME;
    }
    bool all_zero = true;
    for (size_t i = 0; i < size; ++i)
    {
        if (!sdfi_is_digit(text[i]))
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
    if (year < SDFI_FIRST_YEAR || month < 1 || month > MONTHS_PER_YEAR
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
    int64_t clock =
        days * MINUTES_PER_DAY + (int64_t)hour * MINUTES_PER_HOUR + minute;
    if (!sdfi_requires_utc(revision))
    {
        clock = utc_of_local_clock(clock);
    }
    return (time_t)(clock * SECONDS_PER_MINUTE);
}
