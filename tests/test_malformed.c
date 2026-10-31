/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

#include "check.h"
#include <stdlib.h>
#include <string.h>

#include <sdfio/file.h>

#include "fields.h"

#define SAMPLES SDFIO_TEST_DATA_DIR "/samples/iso25178-71-2026/"

/* The positions of the first date and of the z-scale in a binary header of
 * ISO-2.0. */
#define DATES_AT (sizeof "bISO-2.0" - 1 + SDFIO_MANUFACTURER_ID_MAX)
#define Z_SCALE_AT                                                             \
    (DATES_AT + 2 * SDFIO_DATETIME_SIZE + 2 * sizeof(uint32_t)                 \
     + 2 * sizeof(double))

/* Every prefix of a file, and a file with one damaged byte, is refused or read
 * without a crash. */
static void check_damage(const char *path)
{
    size_t size = 0;
    unsigned char *data = load_file(path, &size);
    if (data == NULL)
    {
        return;
    }
    for (size_t length = 0; length < size; length += (length < 400 ? 1 : 7))
    {
        sdfio_reader *reader = new_reader(all_formats());
        if (sdfio_open_memory(reader, data, length) == SDFIO_OK)
        {
            const sdfio_header *header = sdfio_reader_header(reader);
            double *heights =
                malloc(sdfio_header_rows(header) * sdfio_header_columns(header)
                           * sizeof(double)
                       + 1);
            const sdfio_status status = sdfio_read_heights(
                reader, sdfio_layout_packed(SDFIO_TYPE_DOUBLE), heights);
            if (length < size / 2)
            {
                CHECK(status != SDFIO_OK);
            }
            const char *trailer = NULL;
            size_t trailer_size = 0;
            (void)read_trailer_text(reader, 0, &trailer, &trailer_size);
            free(heights);
        }
        sdfio_reader_free(reader);
    }
    uint32_t state = 2463534242u;
    unsigned char *copy = malloc(size);
    for (int round = 0; round < 3000 && copy != NULL; ++round)
    {
        memcpy(copy, data, size);
        for (int hits = 0; hits < 3; ++hits)
        {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            const size_t at = state % (round % 2 == 0 ? 400 : size);
            copy[at < size ? at : 0] = (unsigned char)(state >> 8);
        }
        sdfio_reader *reader = new_reader(all_formats());
        if (sdfio_open_memory(reader, copy, size) == SDFIO_OK)
        {
            const sdfio_header *header = sdfio_reader_header(reader);
            const size_t count =
                sdfio_header_rows(header) * sdfio_header_columns(header);
            /* A damaged count could ask for more than a test should allocate.
             */
            if (count < (size_t)1 << 20)
            {
                double *heights = malloc(count * sizeof(double) + 1);
                (void)sdfio_read_heights(
                    reader, sdfio_layout_packed(SDFIO_TYPE_DOUBLE), heights);
                free(heights);
            }
            const char *trailer = NULL;
            size_t trailer_size = 0;
            (void)read_trailer_text(reader, 0, &trailer, &trailer_size);
        }
        sdfio_reader_free(reader);
    }
    free(copy);
    free(data);
}

static void check_header_errors(void)
{
    size_t size = 0;
    unsigned char *data = load_file(SAMPLES "iso2_surface_binary.sdf", &size);
    sdfio_reader *reader = new_reader(all_formats());
    CHECK_STATUS(SDFIO_ERR_FORMAT, sdfio_open_memory(reader, "", 0));
    sdfio_reader_free(reader);

    reader = new_reader(all_formats());
    CHECK_STATUS(SDFIO_ERR_FORMAT,
                 sdfio_open_memory(reader, "not an sdf file at all", 22));
    CHECK(strlen(sdfio_reader_error_message(reader)) > 0);
    sdfio_reader_free(reader);

    if (data != NULL)
    {
        /* The compression and the checksum bytes end the header, the data type
         * is before the checksum. */
        const size_t header_size = ISO_2_0_BINARY_HEADER_SIZE;
        unsigned char *copy = malloc(size);
        memcpy(copy, data, size);
        copy[header_size - 3] = 1;
        reader = new_reader(all_formats());
        CHECK_STATUS(SDFIO_ERR_FORMAT, sdfio_open_memory(reader, copy, size));
        sdfio_reader_free(reader);
        memcpy(copy, data, size);
        copy[header_size - 1] = 1;
        reader = new_reader(all_formats());
        CHECK_STATUS(SDFIO_ERR_FORMAT, sdfio_open_memory(reader, copy, size));
        sdfio_reader_free(reader);
        memcpy(copy, data, size);
        copy[header_size - 2] = 99;
        reader = new_reader(all_formats());
        CHECK_STATUS(SDFIO_ERR_FORMAT, sdfio_open_memory(reader, copy, size));
        sdfio_reader_free(reader);
        memcpy(copy, data, size);
        memset(copy + DATES_AT, '9', SDFIO_DATETIME_SIZE);
        reader = new_reader(all_formats());
        CHECK_STATUS(SDFIO_ERR_FORMAT, sdfio_open_memory(reader, copy, size));
        sdfio_reader_free(reader);
        memcpy(copy, data, size);
        memset(copy + Z_SCALE_AT, 0, sizeof(double));
        reader = new_reader(all_formats());
        CHECK_STATUS(SDFIO_ERR_FORMAT, sdfio_open_memory(reader, copy, size));
        sdfio_reader_free(reader);
        memcpy(copy, data, size);
        copy[0] = 'B';
        reader = new_reader(all_formats());
        CHECK_STATUS(SDFIO_ERR_FORMAT, sdfio_open_memory(reader, copy, size));
        sdfio_reader_free(reader);
        free(copy);

        /* A byte order mark in front of the magic is not an SDF file.
         */
        unsigned char *marked = malloc(size + 3);
        const unsigned char mark[] = {0xEF, 0xBB, 0xBF};
        memcpy(marked, mark, 3);
        memcpy(marked + 3, data, size);
        reader = new_reader(all_formats());
        CHECK_STATUS(SDFIO_ERR_FORMAT,
                     sdfio_open_memory(reader, marked, size + 3));
        sdfio_reader_free(reader);
        free(marked);
    }
    free(data);

    reader = new_reader(all_formats());
    CHECK_STATUS(SDFIO_ERR_IO,
                 sdfio_open_file(reader, SDFIO_TEST_DATA_DIR "/missing.sdf"));
    CHECK(strstr(sdfio_reader_error_message(reader), "cannot be opened")
          != NULL);
    CHECK(sdfio_reader_header(reader) == NULL);
    CHECK_STATUS(SDFIO_ERR_STATE,
                 sdfio_read_data(reader, sdfio_layout_packed(SDFIO_TYPE_FLOAT),
                                 NULL, 0));
    sdfio_reader_free(reader);

    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_open_file(NULL, "x"));
    sdfio_reader_free(NULL);
    sdfio_writer_free(NULL);
}

static void add_line(char *text, size_t capacity, const char *name,
                     const char *value, const char *line_end)
{
    const size_t used = strlen(text);
    const int written =
        snprintf(text + used, capacity - used, "%s%s%s", name, value, line_end);
    CHECK(written >= 0 && (size_t)written < capacity - used);
}

/* The status of opening an ASCII file whose header is as given. If it opens,
 * `manufacturer` receives the identifier that the reader returns. */
static sdfio_status
read_ascii(const char *magic, const char *id, const char *date,
           const char *points, const char *z_scale, const char *line_end,
           char (*manufacturer)[SDFIO_MANUFACTURER_ID_MAX + 1])
{
    char text[512] = "";
    add_line(text, sizeof text, magic, "", line_end);
    add_line(text, sizeof text, "ManufacID = ", id, line_end);
    add_line(text, sizeof text, "CreateDate = ", date, line_end);
    add_line(text, sizeof text, "ModDate = ", date, line_end);
    add_line(text, sizeof text, "NumPoints = ", points, line_end);
    add_line(text, sizeof text, "NumProfiles = 1", "", line_end);
    add_line(text, sizeof text, "Xscale = 1.00000000000000e-006", "", line_end);
    add_line(text, sizeof text, "Yscale = 1.00000000000000e-006", "", line_end);
    add_line(text, sizeof text, "Zscale = ", z_scale, line_end);
    add_line(text, sizeof text, "Zresolution = -1", "", line_end);
    add_line(text, sizeof text, "Compression = 0", "", line_end);
    add_line(text, sizeof text, "DataType = 7", "", line_end);
    add_line(text, sizeof text, "CheckType = 0", "", line_end);
    add_line(text, sizeof text, "*", "", line_end);
    add_line(text, sizeof text, "1.00000000000000e+000", "", line_end);
    add_line(text, sizeof text, "*", "", line_end);
    add_line(text, sizeof text, "*", "", line_end);
    sdfio_reader *reader = new_reader(all_formats());
    const sdfio_status status = sdfio_open_memory(reader, text, strlen(text));
    if (status == SDFIO_OK && manufacturer != NULL)
    {
        snprintf(*manufacturer, sizeof *manufacturer, "%s",
                 sdfio_header_manufacturer(sdfio_reader_header(reader)));
    }
    sdfio_reader_free(reader);
    return status;
}

static sdfio_status open_ascii(const char *magic, const char *id,
                               const char *date, const char *points,
                               const char *z_scale, const char *line_end)
{
    return read_ascii(magic, id, date, points, z_scale, line_end, NULL);
}

static void check_standard_rules(void)
{
    const char *const crlf = "\r\n";
    const char *const good_z = "1.00000000000000e-009";
    CHECK_OK(open_ascii("aISO-2.0", "test", "010120261200", "1", good_z, crlf));
    CHECK_OK(open_ascii("aISO-2.0", "test", "010120261200", "1", good_z, "\n"));
    /* The magic starts with a lower case a or b, and names a revision.
     */
    CHECK_STATUS(
        SDFIO_ERR_FORMAT,
        open_ascii("AISO-2.0", "test", "010120261200", "1", good_z, crlf));
    CHECK_STATUS(
        SDFIO_ERR_FORMAT,
        open_ascii("aBCR-1.0", "test", "010120261200", "1", good_z, crlf));
    CHECK_STATUS(
        SDFIO_ERR_FORMAT,
        open_ascii("aISO-3.0", "test", "010120261200", "1", good_z, crlf));
    /* A lone CR does not end a line. */
    CHECK_STATUS(
        SDFIO_ERR_FORMAT,
        open_ascii("aISO-2.0", "test", "010120261200", "1", good_z, "\r"));
    /* The z-scale is a non-zero positive number. */
    CHECK_STATUS(SDFIO_ERR_FORMAT, open_ascii("aISO-2.0", "test",
                                              "010120261200", "1", "0", crlf));
    CHECK_STATUS(
        SDFIO_ERR_FORMAT,
        open_ascii("aISO-2.0", "test", "010120261200", "1", "-1.0e-009", crlf));
    /* A date is DDMMYYYYhhmm with the ranges of the fields. */
    const char *const bad_dates[] = {
        "999999999999", "320120261200",  "310220261200",
        "011320261200", "010120262400",  "010120261260",
        "01012026120",  "0101202612000", "01-01-2026 12"};
    for (size_t i = 0; i < sizeof bad_dates / sizeof bad_dates[0]; ++i)
    {
        CHECK_STATUS(
            SDFIO_ERR_FORMAT,
            open_ascii("aISO-2.0", "test", bad_dates[i], "1", good_z, crlf));
    }
    /* An identifier of more than 10 characters is cut. */
    char identifier[SDFIO_MANUFACTURER_ID_MAX + 1] = "";
    CHECK_OK(read_ascii("aISO-2.0", "0123456789", "010120261200", "1", good_z,
                        crlf, &identifier));
    CHECK(strcmp(identifier, "0123456789") == 0);
    CHECK_OK(read_ascii("aISO-2.0", "0123456789ABC", "010120261200", "1",
                        good_z, crlf, &identifier));
    CHECK(strcmp(identifier, "0123456789") == 0);
    /* The number of columns is 2 bytes in ISO-1.0 and 4 bytes in ISO-2.0. */
    CHECK_STATUS(
        SDFIO_ERR_FORMAT,
        open_ascii("aISO-1.0", "test", "010120261200", "65536", good_z, crlf));
    CHECK_STATUS(SDFIO_ERR_FORMAT,
                 open_ascii("aISO-2.0", "test", "010120261200", "4294967296",
                            good_z, crlf));
}

int main(void)
{
    check_header_errors();
    check_damage(SAMPLES "iso2_surface_binary.sdf");
    check_damage(SAMPLES "iso1_surface_ascii.sdf");
    check_damage(SAMPLES "iso1_surface_binary.sdf");
    check_damage(SAMPLES "iso2_surface_ascii.sdf");
    check_standard_rules();
    return finish_tests();
}
