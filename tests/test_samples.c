/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* The files of tests/data. What the Python package sdfio, an independent
 * implementation, reads from them is in expected.txt, and this library has to
 * read the same. */

#include "check.h"

#include "internal.h"

enum
{
    FIELD_COUNT = 17,
    LINE_CAPACITY = 4096
};

typedef struct expectation
{
    char path[LINE_CAPACITY];
    char revision[16];
    char storage[16];
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
    char create_date[16];
    char mod_date[16];
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
    snprintf(e->storage, sizeof e->storage, "%s", fields[2]);
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
    snprintf(e->create_date, sizeof e->create_date, "%s", fields[14]);
    snprintf(e->mod_date, sizeof e->mod_date, "%s", fields[15]);
    char *end = fields[16] + strcspn(fields[16], "\r\n");
    *end = '\0';
    snprintf(e->trailer, sizeof e->trailer, "%s", fields[16]);
    return true;
}

static void check_against(const expectation *e)
{
    char path[LINE_CAPACITY + 128];
    snprintf(path, sizeof path, "%s/%s", SDF_TEST_DATA_DIR, e->path);
    printf("%s\n", e->path);

    sdf_reader *reader = sdf_reader_new();
    CHECK(reader != NULL);
    CHECK_OK(sdf_open_file(reader, path));
    const sdf_info *info = sdf_get_info(reader);
    CHECK(info != NULL);
    if (info == NULL)
    {
        sdf_reader_free(reader);
        return;
    }
    CHECK(strcmp(sdf_revision_name(info->revision), e->revision) == 0);
    CHECK(strcmp(info->storage == SDF_ASCII ? "ascii" : "binary", e->storage)
          == 0);
    CHECK(strcmp(sdf_data_type_name(info->data_type), e->type) == 0);
    CHECK(info->rows == e->rows);
    CHECK(info->columns == e->columns);
    CHECK(info->x_scale == e->x_scale);
    CHECK(info->y_scale == e->y_scale);
    CHECK(info->z_scale == e->z_scale);
    CHECK(strcmp(info->manufacturer_id, e->manufacturer_id) == 0);
    char date[SDFI_DATE_SIZE + 1] = {0};
    sdfi_date_format(info->create_date, info->revision, date);
    CHECK(strcmp(date, e->create_date) == 0);
    sdfi_date_format(info->mod_date, info->revision, date);
    CHECK(strcmp(date, e->mod_date) == 0);

    const size_t count = info->rows * info->columns;
    double *heights = malloc(count * sizeof *heights);
    CHECK(heights != NULL);
    CHECK_OK(sdf_read_heights(reader, sdf_metres(SDF_FLOAT64), heights,
                              (ptrdiff_t)(info->columns * sizeof(double))));

    const char *trailer = NULL;
    size_t trailer_size = 0;
    CHECK_OK(sdf_read_trailer(reader, &trailer, &trailer_size));
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
        info->data_type == SDF_DATA_BINARY32 ? 1e-6 : 1e-12;
    CHECK(valid == e->valid);
    CHECK(near_relative(sum, e->sum, 1e-9 + precision));
    CHECK(near_relative(first, e->z_first, precision));
    CHECK(near_relative(last, e->z_last, precision));
    free(heights);
    sdf_reader_free(reader);
}

int main(void)
{
    char path[512];
    snprintf(path, sizeof path, "%s/expected.txt", SDF_TEST_DATA_DIR);
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
