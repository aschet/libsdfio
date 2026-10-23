/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* The date field: UTC in ISO-2.0, the local clock of the writer in the other
 * revisions, which the time zone of the caller converts. */

#include "check.h"
#include <string.h>

#include "datetime.h"
#include "fields.h"

#include <stdlib.h>
#include <time.h>

enum
{
    SECONDS_PER_HOUR = 3600,
    SECONDS_PER_DAY = 86400,
    /* Where the fields of the dates are in a binary file. */
    DATE_AT = 18
};

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

    /* The leap day, and the first and the last day the field has. */
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

    /* A field that is no date reads as no date. */
    const char *bad[] = {"310220260000",  "320120260000", "011320260000",
                         "010120260060",  "010120262400", "01012026000",
                         "0101202600001", "0101202600x0", "            "};
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; ++i)
    {
        CHECK(sdfio_datetime_parse(bad[i], strlen(bad[i]), SDFIO_ISO_2_0, NULL)
              == SDFIO_NO_TIME);
    }
    /* The seconds are dropped. */
    format(1772600760 + 59, SDFIO_ISO_2_0, NULL, text);
    CHECK(strcmp(text, "040320260506") == 0);
    /* -1 is SDFIO_NO_TIME: one minute before the epoch is -60. */
    format(-60, SDFIO_ISO_2_0, NULL, text);
    CHECK(strcmp(text, "311219692359") == 0);
}

/* The clock reading of ISO-1.0 is kept as it is without a time zone. */
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
    /* A time zone with no callback converts nothing. */
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

static time_t central_europe_to_local(void *user, time_t utc)
{
    (void)user;
    return utc + offset_at(utc);
}

/* In the hour that a change of the clock repeats, the first reading is taken;
 * an hour that it skips is read as the hour after. */
static time_t central_europe_to_utc(void *user, time_t local)
{
    (void)user;
    const time_t summer = local - 2 * SECONDS_PER_HOUR;
    return offset_at(summer) == 2 * SECONDS_PER_HOUR ? summer
                                                     : local - SECONDS_PER_HOUR;
}

static time_t refuse(void *user, time_t time)
{
    (void)user;
    (void)time;
    return SDFIO_NO_TIME;
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

    /* Every instant of the year comes back as the same clock reading, also
     * across both changes of the clock. */
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

    /* A callback that cannot convert: a date reads as not recorded, and a
     * date cannot be written. */
    const sdfio_time_zone refusing = {NULL, refuse, refuse};
    CHECK(sdfio_datetime_parse("010720261200", SDFIO_DATETIME_SIZE,
                               SDFIO_ISO_1_0, &refusing)
          == SDFIO_NO_TIME);
    CHECK(!sdfio_datetime_format(1782900000, SDFIO_ISO_1_0, &refusing, text));
}

typedef struct memory_sink
{
    unsigned char *data;
    size_t size;
} memory_sink;

static size_t sink_write(void *user, const void *data, size_t size)
{
    memory_sink *sink = user;
    unsigned char *grown = realloc(sink->data, sink->size + size);
    if (grown == NULL)
    {
        return 0;
    }
    memcpy(grown + sink->size, data, size);
    sink->data = grown;
    sink->size += size;
    return size;
}

/* The writer stores the clock of the time zone, and the reader gives the UTC
 * time back; without a time zone the clock reading is the date. */
static void check_file(void)
{
    const sdfio_time_zone zone = {NULL, central_europe_to_utc,
                                  central_europe_to_local};
    const sdfio_config with_zone = {.time_zone = &zone, ALL_CODECS};
    sdfio_header header;
    sdfio_header_init(&header);
    header.revision = SDFIO_ISO_1_0;
    header.data_type = SDFIO_TYPE_INT16;
    header.columns = 1;
    header.rows = 1;
    header.creation_date = 1782900000;
    header.modification_date = 1768474800;

    memory_sink sink = {NULL, 0};
    const sdfio_stream stream = {&sink, NULL, sink_write};
    const float height = 1.0F;
    sdfio_writer *writer = sdfio_writer_new(&with_zone);
    CHECK_OK(sdfio_create_stream(writer, &stream, &header));
    CHECK_OK(sdfio_write_heights(writer, SDFIO_TYPE_FLOAT, &height));
    CHECK_OK(sdfio_end_trailer(writer));
    sdfio_writer_free(writer);
    CHECK(sink.size > DATE_AT + 2 * SDFIO_DATETIME_SIZE);
    CHECK(memcmp(sink.data + DATE_AT, "010720261200", SDFIO_DATETIME_SIZE)
          == 0);
    CHECK(memcmp(sink.data + DATE_AT + SDFIO_DATETIME_SIZE, "150120261200",
                 SDFIO_DATETIME_SIZE)
          == 0);

    sdfio_reader *reader = sdfio_reader_new(&with_zone);
    CHECK_OK(sdfio_open_memory(reader, sink.data, sink.size));
    CHECK(sdfio_get_header(reader)->creation_date == 1782900000);
    CHECK(sdfio_get_header(reader)->modification_date == 1768474800);
    sdfio_reader_free(reader);

    reader = sdfio_reader_new(all_codecs());
    CHECK_OK(sdfio_open_memory(reader, sink.data, sink.size));
    CHECK(sdfio_get_header(reader)->creation_date == 1782907200);
    sdfio_reader_free(reader);

    /* A writer whose time zone refuses the date creates no file. */
    const sdfio_time_zone refusing = {NULL, refuse, refuse};
    const sdfio_config refusing_config = {.time_zone = &refusing, ALL_CODECS};
    memory_sink refused = {NULL, 0};
    const sdfio_stream refused_stream = {&refused, NULL, sink_write};
    writer = sdfio_writer_new(&refusing_config);
    CHECK_OK(sdfio_create_stream(writer, &refused_stream, &header));
    CHECK_STATUS(SDFIO_ERR_RANGE,
                 sdfio_write_heights(writer, SDFIO_TYPE_FLOAT, &height));
    sdfio_writer_free(writer);
    free(refused.data);
    free(sink.data);
}

int main(void)
{
    check_utc();
    check_without_zone();
    check_time_zone();
    check_file();
    return finish_tests();
}
