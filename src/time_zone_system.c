/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief Time zone of the machine.
 */

#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif

#include <sdfio/types.h>
#include <string.h>
#include <time.h>

static bool local_fields(time_t utc, struct tm *fields)
{
#ifdef _WIN32
    return localtime_s(fields, &utc) == 0;
#else
    return localtime_r(&utc, fields) != NULL;
#endif
}

static time_t system_local_to_utc(void *user, const sdfio_date *local)
{
    (void)user;
    struct tm fields;
    memset(&fields, 0, sizeof fields);
    fields.tm_year = local->year - 1900;
    fields.tm_mon = local->month - 1;
    fields.tm_mday = local->day;
    fields.tm_hour = local->hour;
    fields.tm_min = local->minute;
    fields.tm_isdst = -1;
    return mktime(&fields);
}

static bool system_utc_to_local(void *user, time_t utc, sdfio_date *local)
{
    (void)user;
    struct tm fields;
    if (!local_fields(utc, &fields))
    {
        return false;
    }
    local->year = fields.tm_year + 1900;
    local->month = fields.tm_mon + 1;
    local->day = fields.tm_mday;
    local->hour = fields.tm_hour;
    local->minute = fields.tm_min;
    return true;
}

const sdfio_time_zone *sdfio_system_time_zone(void)
{
    static const sdfio_time_zone zone = {NULL, system_local_to_utc,
                                         system_utc_to_local};
    return &zone;
}
