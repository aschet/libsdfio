/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef SDFIO_TESTS_ZONE_H
#define SDFIO_TESTS_ZONE_H

#include "check.h"
#include <time.h>

#define SECONDS_PER_HOUR 3600
#define SECONDS_PER_DAY 86400

/* Central European time in 2026: UTC+1, and UTC+2 from 29 March 01:00 UTC to
 * 25 October 01:00 UTC. */
#define SUMMER_BEGINS 1774746000
#define SUMMER_ENDS 1792890000

static inline time_t offset_at(time_t utc)
{
    return utc >= SUMMER_BEGINS && utc < SUMMER_ENDS ? 2 * SECONDS_PER_HOUR
                                                     : SECONDS_PER_HOUR;
}

static inline bool central_europe_to_local(void *user, time_t utc,
                                           sdfio_date *local)
{
    (void)user;
    return sdfio_date_from_time(utc + offset_at(utc), local) == SDFIO_OK;
}

/* An ambiguous clock reading resolves to its first occurrence, a skipped one
 * to the hour after. */
static inline time_t central_europe_to_utc(void *user, const sdfio_date *local)
{
    (void)user;
    time_t clock = 0;
    if (sdfio_date_to_time(local, &clock) != SDFIO_OK)
    {
        return SDFIO_NO_TIME;
    }
    const time_t summer = clock - 2 * SECONDS_PER_HOUR;
    return offset_at(summer) == 2 * SECONDS_PER_HOUR ? summer
                                                     : clock - SECONDS_PER_HOUR;
}

static inline time_t refuse_local(void *user, const sdfio_date *local)
{
    (void)user;
    (void)local;
    return SDFIO_NO_TIME;
}

static inline bool refuse_utc(void *user, time_t utc, sdfio_date *local)
{
    (void)user;
    (void)utc;
    (void)local;
    return false;
}

#endif /* SDFIO_TESTS_ZONE_H */
