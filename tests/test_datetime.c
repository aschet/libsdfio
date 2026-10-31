/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif

#include "check.h"
#include <string.h>

#include "datetime.h"
#include "fields.h"

#include <stdlib.h>
#include <time.h>

#define SECONDS_PER_HOUR 3600
#define SECONDS_PER_DAY 86400
#define DATE_AT 18

static void format(time_t time, sdfio_revision revision,
                   const sdfio_time_zone *zone,
                   char out[SDFIO_DATETIME_SIZE + 1])
{
    memset(out, 0, SDFIO_DATETIME_SIZE + 1);
    CHECK(sdfio_datetime_format(time, revision, zone, out));
}

static time_t as_time(long long seconds)
{
    return (time_t)seconds;
}

static void check_utc(void)
{
    char text[SDFIO_DATETIME_SIZE + 1];
    format(1772600760, SDFIO_ISO_2_0, NULL, text); /* 2026-03-04 05:06 UTC */
    CHECK(strcmp(text, "040320260506") == 0);
    CHECK(sdfio_datetime_parse(text, SDFIO_DATETIME_SIZE, SDFIO_ISO_2_0, NULL)
          == 1772600760);
    format(0, SDFIO_ISO_2_0, NULL, text);
    CHECK(strcmp(text, "010119700000") == 0);
    format(SDFIO_NO_TIME, SDFIO_ISO_2_0, NULL, text);
    CHECK(strcmp(text, "000000000000") == 0);
    CHECK(sdfio_datetime_parse(text, SDFIO_DATETIME_SIZE, SDFIO_ISO_2_0, NULL)
          == SDFIO_NO_TIME);

    CHECK(sdfio_datetime_parse("290220240000", SDFIO_DATETIME_SIZE,
                               SDFIO_ISO_2_0, NULL)
          == 1709164800);
    CHECK(sdfio_datetime_parse("290220230000", SDFIO_DATETIME_SIZE,
                               SDFIO_ISO_2_0, NULL)
          == SDFIO_NO_TIME);
    if (sizeof(time_t) >= sizeof(int64_t))
    {
        CHECK(sdfio_datetime_parse("010100010000", SDFIO_DATETIME_SIZE,
                                   SDFIO_ISO_2_0, NULL)
              == as_time(-62135596800LL));
        format(as_time(-62135596800LL), SDFIO_ISO_2_0, NULL, text);
        CHECK(strcmp(text, "010100010000") == 0);
        format(as_time(253402300740LL), SDFIO_ISO_2_0, NULL,
               text); /* 9999-12-31 */
        CHECK(strcmp(text, "311299992359") == 0);
        CHECK(!sdfio_datetime_representable(as_time(253402300800LL)));
        CHECK(!sdfio_datetime_representable(as_time(-62135596801LL)));
    }
    else
    {
        /* A time_t of 32 bits cannot hold the first and the last year of the
         * field: they read as no date, and do not wrap into another one. */
        CHECK(sdfio_datetime_parse("010100010000", SDFIO_DATETIME_SIZE,
                                   SDFIO_ISO_2_0, NULL)
              == SDFIO_NO_TIME);
        CHECK(sdfio_datetime_parse("311299992359", SDFIO_DATETIME_SIZE,
                                   SDFIO_ISO_2_0, NULL)
              == SDFIO_NO_TIME);
    }

    /* A date that is not recorded, as the character 0 or the byte 0 only, is
     * valid and reads as no date. */
    const char unrecorded[][SDFIO_DATETIME_SIZE + 1] = {
        "000000000000", "000000", "0", {0}};
    for (size_t i = 0; i < sizeof unrecorded / sizeof unrecorded[0]; ++i)
    {
        const size_t size =
            i == 3 ? (size_t)SDFIO_DATETIME_SIZE : strlen(unrecorded[i]);
        CHECK(sdfio_datetime_valid(unrecorded[i], size));
        CHECK(sdfio_datetime_parse(unrecorded[i], size, SDFIO_ISO_2_0, NULL)
              == SDFIO_NO_TIME);
    }
    CHECK(!sdfio_datetime_valid("00000000000", 0));
    CHECK(!sdfio_datetime_valid("0000000000000", 13));
    CHECK(!sdfio_datetime_valid("0000000000x0", SDFIO_DATETIME_SIZE));

    const char *bad[] = {"310220260000",  "320120260000", "011320260000",
                         "010120260060",  "010120262400", "01012026000",
                         "0101202600001", "0101202600x0", "            "};
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; ++i)
    {
        CHECK(sdfio_datetime_parse(bad[i], strlen(bad[i]), SDFIO_ISO_2_0, NULL)
              == SDFIO_NO_TIME);
    }
    format(1772600760 + 59, SDFIO_ISO_2_0, NULL, text);
    CHECK(strcmp(text, "040320260506") == 0);
    /* -1 is SDFIO_NO_TIME: one minute before the epoch is -60. */
    format(-60, SDFIO_ISO_2_0, NULL, text);
    CHECK(strcmp(text, "311219692359") == 0);
}

static void check_without_zone(void)
{
    char text[SDFIO_DATETIME_SIZE + 1];
    static const sdfio_revision local_revisions[] = {SDFIO_ISO_1_0};
    for (size_t i = 0; i < 1; ++i)
    {
        CHECK(sdfio_datetime_parse("040320260506", SDFIO_DATETIME_SIZE,
                                   local_revisions[i], NULL)
              == 1772600760);
        format(1772600760, local_revisions[i], NULL, text);
        CHECK(strcmp(text, "040320260506") == 0);
    }
    const sdfio_time_zone none = {NULL, NULL, NULL};
    CHECK(sdfio_datetime_parse("040320260506", SDFIO_DATETIME_SIZE,
                               SDFIO_ISO_1_0, &none)
          == 1772600760);
}

/* Central European time in 2026: UTC+1, and UTC+2 from 29 March 01:00 UTC to
 * 25 October 01:00 UTC. */
static const time_t summer_begins = 1774746000;
static const time_t summer_ends = 1792890000;

static time_t offset_at(time_t utc)
{
    return utc >= summer_begins && utc < summer_ends ? 2 * SECONDS_PER_HOUR
                                                     : SECONDS_PER_HOUR;
}

static bool central_europe_to_local(void *user, time_t utc, sdfio_date *local)
{
    (void)user;
    return sdfio_date_from_time(utc + offset_at(utc), local) == SDFIO_OK;
}

/* An ambiguous clock reading resolves to its first occurrence, a skipped one
 * to the hour after. */
static time_t central_europe_to_utc(void *user, const sdfio_date *local)
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

static time_t refuse_local(void *user, const sdfio_date *local)
{
    (void)user;
    (void)local;
    return SDFIO_NO_TIME;
}

static bool refuse_utc(void *user, time_t utc, sdfio_date *local)
{
    (void)user;
    (void)utc;
    (void)local;
    return false;
}

static void check_date_conversion(void)
{
    sdfio_date date;
    CHECK_OK(sdfio_date_from_time(1772600760, &date));
    CHECK(date.year == 2026 && date.month == 3 && date.day == 4
          && date.hour == 5 && date.minute == 6);
    time_t utc = 0;
    CHECK_OK(sdfio_date_to_time(&date, &utc));
    CHECK(utc == 1772600760);
    CHECK_OK(sdfio_date_from_time(1772600819, &date));
    CHECK(date.minute == 6);
    CHECK_OK(sdfio_date_from_time(-60, &date));
    CHECK(date.year == 1969 && date.month == 12 && date.day == 31
          && date.hour == 23 && date.minute == 59);
    const sdfio_date last = {9999, 12, 31, 23, 59};
    if (sizeof(time_t) >= 8)
    {
        CHECK_OK(sdfio_date_to_time(&last, &utc));
        CHECK_OK(sdfio_date_from_time(utc, &date));
        CHECK(date.year == 9999 && date.minute == 59);
        CHECK_STATUS(SDFIO_ERR_RANGE, sdfio_date_from_time(utc + 60, &date));
    }
    const sdfio_date not_a_day = {2026, 2, 29, 0, 0};
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_date_to_time(&not_a_day, &utc));
    const sdfio_date zero = {0, 0, 0, 0, 0};
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_date_to_time(&zero, &utc));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_date_to_time(NULL, &utc));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_date_to_time(&last, NULL));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_date_from_time(0, NULL));
}

/* For any zone, a time converts to a clock reading and back unchanged, and the
 * reading equals that of the C library. The times avoid clock changes. */
static void check_system_time_zone(void)
{
    const sdfio_time_zone *zone = sdfio_system_time_zone();
    CHECK(zone != NULL && zone->local_to_utc != NULL
          && zone->utc_to_local != NULL);
    if (zone == NULL || zone->local_to_utc == NULL
        || zone->utc_to_local == NULL)
    {
        return;
    }
    const time_t times[] = {1768478400, 1768521600, 1784203200, 1784246400};
    for (size_t i = 0; i < sizeof times / sizeof times[0]; ++i)
    {
        sdfio_date local;
        CHECK(zone->utc_to_local(zone->user, times[i], &local));
        CHECK(zone->local_to_utc(zone->user, &local) == times[i]);
        struct tm expected;
#ifdef _WIN32
        CHECK(localtime_s(&expected, &times[i]) == 0);
#else
        CHECK(localtime_r(&times[i], &expected) != NULL);
#endif
        CHECK(local.year == expected.tm_year + 1900
              && local.month == expected.tm_mon + 1
              && local.day == expected.tm_mday && local.hour == expected.tm_hour
              && local.minute == expected.tm_min);
    }
}

static void check_time_zone(void)
{
    const sdfio_time_zone zone = {NULL, central_europe_to_utc,
                                  central_europe_to_local};
    char text[SDFIO_DATETIME_SIZE + 1];
    static const sdfio_revision local_revisions[] = {SDFIO_ISO_1_0};
    for (size_t i = 0; i < 1; ++i)
    {
        /* 12:00 on the clock is 10:00 UTC in summer and 11:00 UTC in winter. */
        CHECK(sdfio_datetime_parse("010720261200", SDFIO_DATETIME_SIZE,
                                   local_revisions[i], &zone)
              == 1782900000);
        CHECK(sdfio_datetime_parse("150120261200", SDFIO_DATETIME_SIZE,
                                   local_revisions[i], &zone)
              == 1768474800);
        format(1782900000, local_revisions[i], &zone, text);
        CHECK(strcmp(text, "010720261200") == 0);
        format(1768474800, local_revisions[i], &zone, text);
        CHECK(strcmp(text, "150120261200") == 0);
    }

    /* ISO-2.0 holds UTC: the time zone is not used. */
    format(1782900000, SDFIO_ISO_2_0, &zone, text);
    CHECK(strcmp(text, "010720261000") == 0);
    CHECK(sdfio_datetime_parse("010720261000", SDFIO_DATETIME_SIZE,
                               SDFIO_ISO_2_0, &zone)
          == 1782900000);

    unsigned checked = 0;
    for (time_t time = 1767225600; time < 1767225600 + 365 * SECONDS_PER_DAY;
         time += 5 * SECONDS_PER_HOUR + 1800)
    {
        char field[SDFIO_DATETIME_SIZE + 1];
        char again[SDFIO_DATETIME_SIZE + 1];
        format(time, SDFIO_ISO_1_0, &zone, field);
        const time_t parsed = sdfio_datetime_parse(field, SDFIO_DATETIME_SIZE,
                                                   SDFIO_ISO_1_0, &zone);
        CHECK(parsed != SDFIO_NO_TIME);
        format(parsed, SDFIO_ISO_1_0, &zone, again);
        CHECK(strcmp(field, again) == 0);
        ++checked;
    }
    CHECK(checked > 300);

    const sdfio_time_zone refusing = {NULL, refuse_local, refuse_utc};
    CHECK(sdfio_datetime_parse("010720261200", SDFIO_DATETIME_SIZE,
                               SDFIO_ISO_1_0, &refusing)
          == SDFIO_NO_TIME);
    CHECK(!sdfio_datetime_format(1782900000, SDFIO_ISO_1_0, &refusing, text));
}

/* With a time zone, the writer stores the clock reading and the reader
 * restores the UTC time. Without one, the reading is the date. */
static void check_file(void)
{
    const sdfio_time_zone zone = {NULL, central_europe_to_utc,
                                  central_europe_to_local};
    const test_options with_zone = {.time_zone = &zone, ALL_FORMATS};
    sdfio_header *header = new_header();
    CHECK_OK(sdfio_header_set_revision(header, SDFIO_ISO_1_0));
    CHECK_OK(sdfio_header_set_data_type(header, SDFIO_TYPE_INT16));
    CHECK_OK(sdfio_header_set_columns(header, 1));
    CHECK_OK(sdfio_header_set_rows(header, 1));
    CHECK_OK(sdfio_header_set_created(header, 1782900000));
    CHECK_OK(sdfio_header_set_modified(header, 1768474800));

    membuf sink = {0};
    const sdfio_stream stream = membuf_stream(&sink);
    const float height = 1.0F;
    sdfio_writer *writer = new_writer(&with_zone);
    CHECK_OK(sdfio_create_stream(writer, &stream, header));
    CHECK_OK(sdfio_write_heights(writer, sdfio_layout_packed(SDFIO_TYPE_FLOAT),
                                 &height));
    CHECK_OK(sdfio_end_file(writer));
    sdfio_writer_free(writer);
    CHECK(sink.size > DATE_AT + 2 * SDFIO_DATETIME_SIZE);
    CHECK(memcmp(sink.data + DATE_AT, "010720261200", SDFIO_DATETIME_SIZE)
          == 0);
    CHECK(memcmp(sink.data + DATE_AT + SDFIO_DATETIME_SIZE, "150120261200",
                 SDFIO_DATETIME_SIZE)
          == 0);

    sdfio_reader *reader = new_reader(&with_zone);
    CHECK_OK(sdfio_open_memory(reader, sink.data, sink.size));
    CHECK(sdfio_header_created(sdfio_reader_header(reader)) == 1782900000);
    CHECK(sdfio_header_modified(sdfio_reader_header(reader)) == 1768474800);
    sdfio_reader_free(reader);

    reader = new_reader(all_formats());
    CHECK_OK(sdfio_open_memory(reader, sink.data, sink.size));
    CHECK(sdfio_header_created(sdfio_reader_header(reader)) == 1782907200);
    sdfio_reader_free(reader);

    const sdfio_time_zone refusing = {NULL, refuse_local, refuse_utc};
    const test_options refusing_config = {.time_zone = &refusing, ALL_FORMATS};
    membuf refused = {0};
    const sdfio_stream refused_stream = membuf_stream(&refused);
    writer = new_writer(&refusing_config);
    CHECK_OK(sdfio_create_stream(writer, &refused_stream, header));
    CHECK_STATUS(SDFIO_ERR_RANGE,
                 sdfio_write_heights(
                     writer, sdfio_layout_packed(SDFIO_TYPE_FLOAT), &height));
    sdfio_writer_free(writer);
    free(refused.data);
    free(sink.data);
}

int main(void)
{
    check_utc();
    check_without_zone();
    check_date_conversion();
    check_system_time_zone();
    check_time_zone();
    check_file();
    return finish_tests();
}
