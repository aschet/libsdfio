/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

#include "check.h"
#include <string.h>

#include "datetime.h"
#include "fields.h"
#include "zone.h"

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

int main(void)
{
    check_utc();
    check_without_zone();
    check_time_zone();
    return finish_tests();
}
