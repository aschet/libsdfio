/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

#include "check.h"
#include <limits.h>
#include <string.h>

#define ROWS 5
#define COLUMNS 9
#define LONGEST_MANUFACTURER "ABCDEFGHIJ"

/* Fills `count` values of the data type with the longest text that the ASCII
 * format writes for it: the smallest integer that is not the bad value, and a
 * negative number with the most digits for a floating-point type. */
static void fill_longest(sdfio_data_type type, unsigned char *values,
                         size_t count)
{
    for (size_t i = 0; i < count; ++i)
    {
        switch (type)
        {
        case SDFIO_TYPE_INT8:
        {
            const int8_t value = INT8_MIN + 1;
            memcpy(values + i * sizeof value, &value, sizeof value);
            break;
        }
        case SDFIO_TYPE_INT16:
        {
            const int16_t value = INT16_MIN + 1;
            memcpy(values + i * sizeof value, &value, sizeof value);
            break;
        }
        case SDFIO_TYPE_INT32:
        {
            const int32_t value = INT32_MIN + 1;
            memcpy(values + i * sizeof value, &value, sizeof value);
            break;
        }
        case SDFIO_TYPE_FLOAT:
        {
            const float value = -1.234567e-30F;
            memcpy(values + i * sizeof value, &value, sizeof value);
            break;
        }
        case SDFIO_TYPE_DOUBLE:
        {
            const double value = -1.2345678901234e-300;
            memcpy(values + i * sizeof value, &value, sizeof value);
            break;
        }
        }
    }
}

static void describe(sdfio_header *header, sdfio_revision revision,
                     sdfio_format format, sdfio_data_type type)
{
    CHECK_OK(sdfio_header_set_revision(header, revision));
    CHECK_OK(sdfio_header_set_format(header, format));
    CHECK_OK(sdfio_header_set_data_type(header, type));
    CHECK_OK(sdfio_header_set_columns(header, COLUMNS));
    CHECK_OK(sdfio_header_set_rows(header, ROWS));
    CHECK_OK(sdfio_header_set_x_scale(header, 1.2345678901234e-300));
    CHECK_OK(sdfio_header_set_y_scale(header, 1.2345678901234e+300));
    CHECK_OK(sdfio_header_set_z_scale(header, 1.2345678901234e-300));
    CHECK_OK(sdfio_header_set_z_resolution(header, 1.2345678901234e-300));
    CHECK_OK(sdfio_header_set_manufacturer(header, LONGEST_MANUFACTURER));
}

static void check_estimate(sdfio_revision revision, sdfio_format format,
                           sdfio_data_type type)
{
    sdfio_header *header = new_header();
    describe(header, revision, format, type);
    sdfio_writer *writer = new_writer(all_formats());
    size_t estimate = 0;
    CHECK_OK(sdfio_writer_estimate_size(writer, header, &estimate));

    unsigned char values[COLUMNS * sizeof(double)];
    fill_longest(type, values, COLUMNS);
    membuf file = {0};
    const sdfio_stream stream = membuf_stream(&file);
    CHECK_OK(sdfio_create_stream(writer, &stream, header));
    const sdfio_layout layout = *sdfio_layout_packed(type);
    for (size_t row = 0; row < ROWS; ++row)
    {
        CHECK_OK(sdfio_write_data(writer, &layout, values, 1));
    }
    CHECK_OK(sdfio_end_file(writer));
    sdfio_writer_free(writer);

    if (format == SDFIO_BINARY)
    {
        CHECK(estimate == file.size);
    }
    else
    {
        CHECK(estimate >= file.size);
        CHECK(estimate <= file.size + file.size / 4);
    }
    free(file.data);
}

static void check_errors(void)
{
    sdfio_header *header = new_header();
    CHECK_OK(sdfio_header_set_columns(header, COLUMNS));
    CHECK_OK(sdfio_header_set_rows(header, ROWS));
    sdfio_writer *writer = new_writer(all_formats());
    size_t size = 0;
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_writer_estimate_size(NULL, header, &size));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_writer_estimate_size(writer, NULL, &size));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_writer_estimate_size(writer, header, NULL));

    sdfio_header *no_dates = NULL;
    CHECK_OK(sdfio_header_create(sdfio_default_allocator(), &no_dates));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_writer_estimate_size(writer, no_dates, &size));
    sdfio_header_free(no_dates);
    sdfio_writer_free(writer);

    sdfio_writer *binary_only = NULL;
    CHECK_OK(sdfio_writer_create(sdfio_default_allocator(), &binary_only));
    CHECK_OK(sdfio_writer_add_binary(binary_only));
    CHECK_OK(sdfio_header_set_format(header, SDFIO_ASCII));
    CHECK_STATUS(SDFIO_ERR_UNSUPPORTED,
                 sdfio_writer_estimate_size(binary_only, header, &size));
    sdfio_writer_free(binary_only);
}

/* A grid whose size in bytes does not fit in size_t. A header whose number of
 * values does not fit is invalid already, which is the case where size_t has 32
 * bits. */
static void check_overflow(sdfio_format format)
{
    sdfio_header *header = new_header();
    CHECK_OK(sdfio_header_set_revision(header, SDFIO_ISO_2_0));
    CHECK_OK(sdfio_header_set_format(header, format));
    CHECK_OK(sdfio_header_set_data_type(header, SDFIO_TYPE_DOUBLE));
    CHECK_OK(sdfio_header_set_columns(header, UINT32_MAX));
    CHECK_OK(sdfio_header_set_rows(header, UINT32_MAX));
    sdfio_writer *writer = new_writer(all_formats());
    size_t size = 0;
    const sdfio_status status =
        sdfio_writer_estimate_size(writer, header, &size);
    CHECK(status == SDFIO_ERR_RANGE || status == SDFIO_ERR_ARGUMENT);
    sdfio_writer_free(writer);
}

int main(void)
{
    const sdfio_revision revisions[] = {SDFIO_ISO_1_0, SDFIO_ISO_2_0};
    const sdfio_format formats[] = {SDFIO_BINARY, SDFIO_ASCII};
    const sdfio_data_type types[] = {SDFIO_TYPE_FLOAT, SDFIO_TYPE_DOUBLE,
                                     SDFIO_TYPE_INT8, SDFIO_TYPE_INT16,
                                     SDFIO_TYPE_INT32};
    for (size_t r = 0; r < sizeof revisions / sizeof revisions[0]; ++r)
    {
        for (size_t f = 0; f < sizeof formats / sizeof formats[0]; ++f)
        {
            for (size_t t = 0; t < sizeof types / sizeof types[0]; ++t)
            {
                if (sdfio_revision_allows(revisions[r], types[t]))
                {
                    check_estimate(revisions[r], formats[f], types[t]);
                }
            }
        }
    }
    check_errors();
    check_overflow(SDFIO_BINARY);
    check_overflow(SDFIO_ASCII);
    return finish_tests();
}
