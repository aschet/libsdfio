/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* The date field: UTC in ISO-2.0, the local clock of the writer in the other
 * revisions, in several time zones. */

#include "check.h"

#include "internal.h"

#include <stdlib.h>
#include <time.h>

enum
{
    SECONDS_PER_HOUR = 3600,
    SECONDS_PER_DAY = 86400
};

static void format(time_t time, sdfio_revision revision,
                   char out[SDFIO_DATE_SIZE + 1])
{
    memset(out, 0, SDFIO_DATE_SIZE + 1);
    sdfio_date_format(time, revision, out);
}

static void check_utc(void)
{
    char text[SDFIO_DATE_SIZE + 1];
    format(1772600760, SDFIO_ISO_2_0, text); /* 2026-03-04 05:06 UTC */
    CHECK(strcmp(text, "040320260506") == 0);
    CHECK(sdfio_date_parse(text, SDFIO_DATE_SIZE, SDFIO_ISO_2_0) == 1772600760);
    format(0, SDFIO_ISO_2_0, text);
    CHECK(strcmp(text, "010119700000") == 0);
    format(SDFIO_NO_TIME, SDFIO_ISO_2_0, text);
    CHECK(strcmp(text, "000000000000") == 0);
    CHECK(sdfio_date_parse(text, SDFIO_DATE_SIZE, SDFIO_ISO_2_0)
          == SDFIO_NO_TIME);

    /* The leap day, and the first and the last day the field has. */
    CHECK(sdfio_date_parse("290220240000", SDFIO_DATE_SIZE, SDFIO_ISO_2_0)
          == 1709164800);
    CHECK(sdfio_date_parse("290220230000", SDFIO_DATE_SIZE, SDFIO_ISO_2_0)
          == SDFIO_NO_TIME);
    CHECK(sdfio_date_parse("010100010000", SDFIO_DATE_SIZE, SDFIO_ISO_2_0)
          == -62135596800);
    format(-62135596800, SDFIO_ISO_2_0, text);
    CHECK(strcmp(text, "010100010000") == 0);
    format(253402300740, SDFIO_ISO_2_0, text); /* 9999-12-31 23:59 */
    CHECK(strcmp(text, "311299992359") == 0);
    CHECK(!sdfio_date_representable(253402300800));
    CHECK(!sdfio_date_representable(-62135596801));

    /* A field that is no date reads as no date. */
    const char *bad[] = {"310220260000",  "320120260000", "011320260000",
                         "010120260060",  "010120262400", "01012026000",
                         "0101202600001", "0101202600x0", "            "};
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; ++i)
    {
        CHECK(sdfio_date_parse(bad[i], strlen(bad[i]), SDFIO_ISO_2_0)
              == SDFIO_NO_TIME);
    }
    /* The seconds are dropped. */
    format(1772600760 + 59, SDFIO_ISO_2_0, text);
    CHECK(strcmp(text, "040320260506") == 0);
    /* -1 is SDFIO_NO_TIME: one minute before the epoch is -60. */
    format(-60, SDFIO_ISO_2_0, text);
    CHECK(strcmp(text, "311219692359") == 0);
}

#if !defined(_WIN32)
static void check_zone(const char *zone)
{
    setenv("TZ", zone, 1);
    tzset();
    unsigned checked = 0;
    for (time_t time = 1735689600;
         time < 1735689600 + 2 * 366 * SECONDS_PER_DAY;
         time += 5 * SECONDS_PER_HOUR + 1800)
    {
        char field[SDFIO_DATE_SIZE + 1];
        format(time, SDFIO_ISO_1_0, field);
        const time_t parsed =
            sdfio_date_parse(field, SDFIO_DATE_SIZE, SDFIO_ISO_1_0);
        CHECK(parsed != SDFIO_NO_TIME);
        /* The local clock is the same; in the hour that a change of the clock
         * repeats, the same clock reading is two instants. */
        char again[SDFIO_DATE_SIZE + 1];
        format(parsed, SDFIO_ISO_1_0, again);
        if (strcmp(field, again) != 0)
        {
            fprintf(stderr, "%s: %s read as %s\n", zone, field, again);
            ++failures;
        }
        const time_t difference = parsed > time ? parsed - time : time - parsed;
        CHECK(difference < 60 || difference == SECONDS_PER_HOUR
              || difference == SECONDS_PER_HOUR + 60 - 60);
        ++checked;
    }
    CHECK(checked > 300);

    /* ISO-2.0 does not depend on the zone, the others do. */
    char utc[SDFIO_DATE_SIZE + 1];
    char local[SDFIO_DATE_SIZE + 1];
    format(1772600760, SDFIO_ISO_2_0, utc);
    format(1772600760, SDFIO_BCR_1_0, local);
    CHECK(strcmp(utc, "040320260506") == 0);
    if (strcmp(zone, "UTC") == 0)
    {
        CHECK(strcmp(local, utc) == 0);
    }
    else
    {
        CHECK(strcmp(local, utc) != 0);
    }
}
#endif

int main(void)
{
    check_utc();
#if !defined(_WIN32)
    const char *zones[] = {
        "UTC",          "Europe/Vienna",       "America/New_York",
        "Asia/Kolkata", "Australia/Lord_Howe", "Pacific/Auckland"};
    for (size_t i = 0; i < sizeof zones / sizeof zones[0]; ++i)
    {
        check_zone(zones[i]);
    }
#endif
    return finish_tests();
}
