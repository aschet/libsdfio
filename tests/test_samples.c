/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* Reads each file listed in tests/data/expected.txt and compares its header,
 * heights and trailer with the listed values. */

#include "check.h"
#include <stdlib.h>
#include <string.h>

#include <sdfio/file.h>

#define FIELD_COUNT 17
#define LINE_CAPACITY 4096

typedef struct expectation
{
    char path[LINE_CAPACITY];
    char revision[16];
    char format[16];
    char type[16];
    size_t rows;
    size_t columns;
    size_t valid;
    double sum;
    double z_first;
    double z_last;
    double x_scale;
    double y_scale;
    double z_scale;
    char manufacturer_id[64];
    char creation_date[16];
    char modification_date[16];
    char trailer[LINE_CAPACITY];
} expectation;

/* Escapes a trailer as expected.txt does: LF as \n, backslash and tab escaped,
 * CR omitted. */
static void escape(const char *trailer, size_t size, char *out)
{
    size_t used = 0;
    for (size_t i = 0; i < size; ++i)
    {
        switch (trailer[i])
        {
        case '\\':
            out[used++] = '\\';
            out[used++] = '\\';
            break;
        case '\t':
            out[used++] = '\\';
            out[used++] = 't';
            break;
        case '\n':
            out[used++] = '\\';
            out[used++] = 'n';
            break;
        case '\r':
            break;
        default:
            out[used++] = trailer[i];
        }
    }
    out[used] = '\0';
}

static bool parse_line(char *line, expectation *e)
{
    char *fields[FIELD_COUNT];
    size_t count = 0;
    char *start = line;
    for (;;)
    {
        char *tab = strchr(start, '\t');
        fields[count++] = start;
        if (tab == NULL || count == FIELD_COUNT)
        {
            break;
        }
        *tab = '\0';
        start = tab + 1;
    }
    if (count != FIELD_COUNT)
    {
        return false;
    }
    snprintf(e->path, sizeof e->path, "%s", fields[0]);
    snprintf(e->revision, sizeof e->revision, "%s", fields[1]);
    snprintf(e->format, sizeof e->format, "%s", fields[2]);
    snprintf(e->type, sizeof e->type, "%s", fields[3]);
    e->rows = strtoul(fields[4], NULL, 10);
    e->columns = strtoul(fields[5], NULL, 10);
    e->valid = strtoul(fields[6], NULL, 10);
    e->sum = strtod(fields[7], NULL);
    e->z_first = strtod(fields[8], NULL);
    e->z_last = strtod(fields[9], NULL);
    e->x_scale = strtod(fields[10], NULL);
    e->y_scale = strtod(fields[11], NULL);
    e->z_scale = strtod(fields[12], NULL);
    snprintf(e->manufacturer_id, sizeof e->manufacturer_id, "%s", fields[13]);
    snprintf(e->creation_date, sizeof e->creation_date, "%s", fields[14]);
    snprintf(e->modification_date, sizeof e->modification_date, "%s",
             fields[15]);
    char *end = fields[16] + strcspn(fields[16], "\r\n");
    *end = '\0';
    snprintf(e->trailer, sizeof e->trailer, "%s", fields[16]);
    return true;
}

/* The revision names of expected.txt. */
static const char *revision_text(sdfio_revision revision)
{
    return revision == SDFIO_ISO_1_0 ? "ISO-1.0" : "ISO-2.0";
}

static const char *type_text(sdfio_data_type type)
{
    switch (type)
    {
    case SDFIO_TYPE_FLOAT:
        return "binary32";
    case SDFIO_TYPE_INT8:
        return "int8";
    case SDFIO_TYPE_INT16:
        return "int16";
    case SDFIO_TYPE_INT32:
        return "int32";
    case SDFIO_TYPE_DOUBLE:
        return "binary64";
    }
    return "";
}

/* Whether a time is the date field of expected.txt: day, month, year, hour and
 * minute in UTC. */
static bool date_matches(time_t time, const char *expected)
{
    sdfio_date date;
    char text[DATE_SIZE + 1];
    return sdfio_date_from_time(time, &date) == SDFIO_OK
           && snprintf(text, sizeof text, "%02d%02d%04d%02d%02d", date.day,
                       date.month, date.year, date.hour, date.minute)
                  == DATE_SIZE
           && strcmp(text, expected) == 0;
}

static void check_sample(const expectation *e)
{
    char path[LINE_CAPACITY + 128];
    snprintf(path, sizeof path, "%s/%s", SDFIO_TEST_DATA_DIR, e->path);
    printf("%s\n", e->path);

    sdfio_reader *reader = new_reader(all_formats());
    CHECK(reader != NULL);
    CHECK_OK(sdfio_open_file(reader, path));
    const sdfio_header *header = sdfio_reader_header(reader);
    CHECK(header != NULL);
    if (header == NULL)
    {
        sdfio_reader_free(reader);
        return;
    }
    CHECK(strcmp(revision_text(sdfio_header_revision(header)), e->revision)
          == 0);
    CHECK(
        strcmp(sdfio_header_format(header) == SDFIO_ASCII ? "ascii" : "binary",
               e->format)
        == 0);
    CHECK(strcmp(type_text(sdfio_header_data_type(header)), e->type) == 0);
    CHECK(sdfio_header_rows(header) == e->rows);
    CHECK(sdfio_header_columns(header) == e->columns);
    CHECK(sdfio_header_x_scale(header) == e->x_scale);
    CHECK(sdfio_header_y_scale(header) == e->y_scale);
    CHECK(sdfio_header_z_scale(header) == e->z_scale);
    CHECK(strcmp(sdfio_header_manufacturer(header), e->manufacturer_id) == 0);
    CHECK(date_matches(sdfio_header_created(header), e->creation_date));
    CHECK(date_matches(sdfio_header_modified(header), e->modification_date));

    const size_t count = height_count(header);
    double *heights = malloc(count * sizeof *heights);
    CHECK(heights != NULL);
    CHECK_OK(sdfio_read_heights(reader, sdfio_layout_packed(SDFIO_TYPE_DOUBLE),
                                heights));

    const char *trailer = NULL;
    size_t trailer_size = 0;
    CHECK_OK(read_trailer_text(reader, 0, &trailer, &trailer_size));
    char escaped[LINE_CAPACITY];
    escape(trailer != NULL ? trailer : "", trailer_size, escaped);
    CHECK(strcmp(escaped, e->trailer) == 0);

    size_t valid = 0;
    double sum = 0.0;
    double first = NAN;
    double last = NAN;
    for (size_t i = 0; i < count; ++i)
    {
        if (isfinite(heights[i]))
        {
            ++valid;
            sum += heights[i];
            if (isnan(first))
            {
                first = heights[i];
            }
            last = heights[i];
        }
    }
    /* Text is parsed into the data type of the file: a binary32 file is
     * rounded to single precision, which the reference does not do. */
    const double precision =
        sdfio_header_data_type(header) == SDFIO_TYPE_FLOAT ? 1e-6 : 1e-12;
    CHECK(valid == e->valid);
    CHECK(near_relative(sum, e->sum, 1e-9 + precision));
    CHECK(near_relative(first, e->z_first, precision));
    CHECK(near_relative(last, e->z_last, precision));
    free(heights);
    sdfio_reader_free(reader);
}

int main(void)
{
    char path[512];
    snprintf(path, sizeof path, "%s/expected.txt", SDFIO_TEST_DATA_DIR);
    FILE *list = fopen(path, "r");
    CHECK(list != NULL);
    if (list == NULL)
    {
        return finish_tests();
    }
    static char line[LINE_CAPACITY];
    size_t files = 0;
    while (fgets(line, sizeof line, list) != NULL)
    {
        expectation *e = calloc(1, sizeof *e);
        if (e != NULL && parse_line(line, e))
        {
            check_sample(e);
            ++files;
        }
        free(e);
    }
    fclose(list);
    CHECK(files > 0);
    return finish_tests();
}
