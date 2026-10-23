/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* The files of tests/data. What the Python package sdfio, an independent
 * implementation, reads from them is in expected.txt, and this library has to
 * read the same. */

#include "check.h"
#include <stdlib.h>
#include <string.h>

#include <sdfio/file.h>

#include "datetime.h"
#include "fields.h"

enum
{
    FIELD_COUNT = 17,
    LINE_CAPACITY = 4096
};

typedef struct expectation
{
    char path[LINE_CAPACITY];
    char revision[16];
    char representation[16];
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

/* The trailer the way expected.txt writes it: LF as \n, a backslash and a tab
 * escaped, and a CR left out. */
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
    snprintf(e->representation, sizeof e->representation, "%s", fields[2]);
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

/* The names that the expectations of the files use. */
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

static void check_against(const expectation *e)
{
    char path[LINE_CAPACITY + 128];
    snprintf(path, sizeof path, "%s/%s", SDFIO_TEST_DATA_DIR, e->path);
    printf("%s\n", e->path);

    sdfio_reader *reader = sdfio_reader_new(all_formats());
    CHECK(reader != NULL);
    CHECK_OK(sdfio_open_file(reader, path));
    const sdfio_header *header = sdfio_get_header(reader);
    CHECK(header != NULL);
    if (header == NULL)
    {
        sdfio_reader_free(reader);
        return;
    }
    CHECK(strcmp(revision_text(header->revision), e->revision) == 0);
    CHECK(strcmp(header->representation == SDFIO_ASCII ? "ascii" : "binary",
                 e->representation)
          == 0);
    CHECK(strcmp(type_text(header->data_type), e->type) == 0);
    CHECK(header->rows == e->rows);
    CHECK(header->columns == e->columns);
    CHECK(header->x_scale == e->x_scale);
    CHECK(header->y_scale == e->y_scale);
    CHECK(header->z_scale == e->z_scale);
    CHECK(strcmp(header->manufacturer_id, e->manufacturer_id) == 0);
    char date[SDFIO_DATETIME_SIZE + 1] = {0};
    CHECK(sdfio_datetime_format(header->creation_date, header->revision, NULL,
                                date));
    CHECK(strcmp(date, e->creation_date) == 0);
    CHECK(sdfio_datetime_format(header->modification_date, header->revision,
                                NULL, date));
    CHECK(strcmp(date, e->modification_date) == 0);

    const size_t count = header->rows * header->columns;
    double *heights = malloc(count * sizeof *heights);
    CHECK(heights != NULL);
    CHECK_OK(sdfio_read_heights(reader, sdfio_layout_packed(SDFIO_TYPE_DOUBLE),
                                heights));

    const char *trailer = NULL;
    size_t trailer_size = 0;
    CHECK_OK(sdfio_read_trailer(reader, &trailer, &trailer_size));
    char escaped[LINE_CAPACITY];
    escape(trailer != NULL ? trailer : "", trailer_size, escaped);
    /* The end of the last line is not part of the trailer in memory. */
    char expected_trailer[LINE_CAPACITY];
    snprintf(expected_trailer, sizeof expected_trailer, "%s", e->trailer);
    const size_t expected_size = strlen(expected_trailer);
    if (expected_size >= 2
        && strcmp(expected_trailer + expected_size - 2, "\\n") == 0)
    {
        expected_trailer[expected_size - 2] = '\0';
    }
    CHECK(strcmp(escaped, expected_trailer) == 0);

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
        header->data_type == SDFIO_TYPE_FLOAT ? 1e-6 : 1e-12;
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
            check_against(e);
            ++files;
        }
        free(e);
    }
    fclose(list);
    CHECK(files > 0);
    return finish_tests();
}
