/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* What is written is what is read: every revision, format and data
 * type, the order of the rows, the trailer and the dates. */

#include "check.h"
#include "data_type.h"
#include "revision.h"
#include <limits.h>
#include <string.h>

#include "tagged.h"

#include <stdlib.h>

enum
{
    ROWS = 7,
    COLUMNS = 11
};

static const char tagged_trailer[] = "OperatorName = Tester\nPartName = Sample";

static double height(size_t row, size_t column)
{
    return 1e-6 * sin(0.7 * (double)row) * cos(0.4 * (double)column)
           + 2e-7 * (double)row - 3e-7;
}

static bool is_missing(size_t row, size_t column)
{
    return row == 2 || (column == 5 && row != 0);
}

static void make_heights(double *heights)
{
    for (size_t row = 0; row < ROWS; ++row)
    {
        for (size_t column = 0; column < COLUMNS; ++column)
        {
            heights[row * COLUMNS + column] =
                is_missing(row, column) ? NAN : height(row, column);
        }
    }
}

static void describe(sdfio_header *header, sdfio_revision revision,
                     sdfio_format format, sdfio_data_type type)
{
    CHECK_OK(sdfio_header_set_z_scale(header, SDFIO_AUTO_Z_SCALE));
    CHECK_OK(sdfio_header_set_revision(header, revision));
    CHECK_OK(sdfio_header_set_format(header, format));
    CHECK_OK(sdfio_header_set_data_type(header, type));
    CHECK_OK(sdfio_header_set_columns(header, COLUMNS));
    CHECK_OK(sdfio_header_set_rows(header, ROWS));
    CHECK_OK(sdfio_header_set_x_scale(header, 2e-6));
    CHECK_OK(sdfio_header_set_y_scale(header, 3e-6));
    CHECK_OK(sdfio_header_set_z_resolution(header, 1e-9));
    CHECK_OK(sdfio_header_set_created(header,
                                      1772600760)); /* 2026-03-04 05:06 UTC */
    CHECK_OK(sdfio_header_set_modified(header, 1772604360));
    CHECK_OK(sdfio_header_set_manufacturer(header, "ACME 7"));
}

static void check_roundtrip(sdfio_revision revision, sdfio_format format,
                            sdfio_data_type type)
{
    sdfio_header *header = new_header();
    describe(header, revision, format, type);
    double heights[ROWS * COLUMNS];
    make_heights(heights);

    membuf file = {0};
    const sdfio_stream stream = membuf_stream(&file);
    sdfio_writer *writer = new_writer(all_formats());
    CHECK_OK(sdfio_create_stream(writer, &stream, header));
    CHECK_OK(sdfio_write_heights(writer, sdfio_layout_packed(SDFIO_TYPE_DOUBLE),
                                 heights));
    CHECK_OK(sdfio_write_trailer_text(writer, tagged_trailer,
                                      strlen(tagged_trailer)));
    CHECK_OK(sdfio_end_file(writer));
    sdfio_writer_free(writer);

    sdfio_reader *reader = new_reader(all_formats());
    CHECK_OK(sdfio_open_memory(reader, file.data, file.size));
    const sdfio_header *read = sdfio_reader_header(reader);
    CHECK(read != NULL);
    if (read == NULL)
    {
        sdfio_reader_free(reader);
        free(file.data);
        return;
    }
    CHECK(sdfio_header_revision(read) == revision);
    CHECK(sdfio_header_format(read) == format);
    CHECK(sdfio_header_data_type(read) == type);
    CHECK(sdfio_header_columns(read) == COLUMNS
          && sdfio_header_rows(read) == ROWS);
    CHECK(sdfio_header_x_scale(read) == sdfio_header_x_scale(header)
          && sdfio_header_y_scale(read) == sdfio_header_y_scale(header));
    CHECK(sdfio_header_z_resolution(read) == sdfio_header_z_resolution(header));
    CHECK(sdfio_header_created(read) == sdfio_header_created(header));
    CHECK(sdfio_header_modified(read) == sdfio_header_modified(header));
    CHECK(strcmp(sdfio_header_manufacturer(read),
                 sdfio_header_manufacturer(header))
          == 0);
    CHECK(sdfio_header_z_scale(read) > 0.0);

    double back[ROWS * COLUMNS];
    CHECK_OK(sdfio_read_heights(reader, sdfio_layout_packed(SDFIO_TYPE_DOUBLE),
                                back));
    /* An integer type is as exact as one coded value; text and binary32 keep
     * about as many digits as they print. */
    double tolerance = 1e-13;
    if (type == SDFIO_TYPE_FLOAT)
    {
        tolerance = 1e-6;
    }
    for (size_t i = 0; i < ROWS * COLUMNS; ++i)
    {
        if (isnan(heights[i]))
        {
            CHECK(isnan(back[i]));
        }
        else if (!sdfio_is_floating(type))
        {
            CHECK(fabs(back[i] - heights[i])
                  <= 0.5 * sdfio_header_z_scale(read) * 1.0001);
        }
        else
        {
            CHECK(fabs(back[i] - heights[i])
                  <= tolerance * fmax(1e-6, fabs(heights[i])));
        }
    }

    const char *trailer = NULL;
    size_t size = 0;
    CHECK_OK(read_trailer_text(reader, 0, &trailer, &size));
    CHECK(trailer != NULL && size == strlen(tagged_trailer)
          && memcmp(trailer, tagged_trailer, size) == 0);
    const char *second = memchr(trailer, '\n', size);
    const char *name = NULL;
    const char *value = NULL;
    size_t name_size = 0;
    size_t value_size = 0;
    CHECK(second != NULL
          && sdfio_tagged_split(second + 1,
                                size - (size_t)(second + 1 - trailer), &name,
                                &name_size, &value, &value_size)
          && name_size == strlen("PartName")
          && memcmp(name, "PartName", name_size) == 0
          && value_size == strlen("Sample")
          && memcmp(value, "Sample", value_size) == 0);

    sdfio_reader_free(reader);
    free(file.data);
}

static void check_all_types(void)
{
    const sdfio_revision revisions[] = {SDFIO_ISO_1_0, SDFIO_ISO_2_0};
    const sdfio_format formats[] = {SDFIO_BINARY, SDFIO_ASCII};
    for (size_t r = 0; r < sizeof revisions / sizeof revisions[0]; ++r)
    {
        for (size_t s = 0; s < sizeof formats / sizeof formats[0]; ++s)
        {
            for (int t = SDFIO_TYPE_FLOAT; t <= SDFIO_TYPE_DOUBLE; ++t)
            {
                const sdfio_data_type type = (sdfio_data_type)t;
                if (!sdfio_revision_allows(revisions[r], type))
                {
                    continue;
                }
                check_roundtrip(revisions[r], formats[s], type);
            }
        }
    }
}

/* The values of the data area move through a buffer that has a gap between
 * the elements, and through one that holds the rows upside down. */
static void check_strided_rows(sdfio_revision revision, sdfio_format format)
{
    typedef struct point
    {
        int16_t other;
        int16_t value;
    } point;
    sdfio_header *header = new_header();
    describe(header, revision, format, SDFIO_TYPE_INT16);
    CHECK_OK(sdfio_header_set_z_scale(header, 1e-9));
    point source[ROWS * COLUMNS];
    for (size_t i = 0; i < ROWS * COLUMNS; ++i)
    {
        source[i].other = -1;
        source[i].value = (int16_t)((int)i * 29 - 300);
    }
    sdfio_layout layout = sdfio_layout_packed(SDFIO_TYPE_INT16);
    layout.element_stride = sizeof(point);
    layout.row_stride = COLUMNS * sizeof(point);

    membuf file = {0};
    const sdfio_stream stream = membuf_stream(&file);
    sdfio_writer *writer = new_writer(all_formats());
    CHECK_OK(sdfio_create_stream(writer, &stream, header));
    CHECK_OK(sdfio_write_data(writer, layout, &source[0].value, ROWS));
    CHECK_OK(sdfio_end_file(writer));
    sdfio_writer_free(writer);

    sdfio_reader *reader = new_reader(all_formats());
    CHECK_OK(sdfio_open_memory(reader, file.data, file.size));
    point read[ROWS * COLUMNS] = {{0, 0}};
    layout.row_stride = -(ptrdiff_t)(COLUMNS * sizeof(point));
    CHECK_OK(sdfio_read_data(reader, layout, &read[(ROWS - 1) * COLUMNS].value,
                             ROWS));
    for (size_t y = 0; y < ROWS; ++y)
    {
        for (size_t x = 0; x < COLUMNS; ++x)
        {
            CHECK(read[(ROWS - 1 - y) * COLUMNS + x].value
                  == source[y * COLUMNS + x].value);
            CHECK(read[y * COLUMNS + x].other == 0);
        }
    }
    sdfio_reader_free(reader);
    free(file.data);
}

/* The rows API moves the stored values as they are, in the order of the
 * file. */
static void check_rows(sdfio_revision revision, sdfio_format format)
{
    sdfio_header *header = new_header();
    describe(header, revision, format, SDFIO_TYPE_INT16);
    CHECK_OK(sdfio_header_set_z_scale(header, 1e-9));
    int16_t coded[ROWS * COLUMNS];
    for (size_t i = 0; i < ROWS * COLUMNS; ++i)
    {
        coded[i] = (int16_t)((int)i * 37 - 100);
    }
    coded[3] = INT16_MIN; /* the marker of a point not measured in ISO */
    coded[4] = INT16_MAX; /* an ordinary coded value */

    membuf file = {0};
    const sdfio_stream stream = membuf_stream(&file);
    sdfio_writer *writer = new_writer(all_formats());
    CHECK_OK(sdfio_create_stream(writer, &stream, header));
    CHECK_OK(sdfio_write_data(writer, sdfio_layout_packed(SDFIO_TYPE_INT16),
                              coded, 3));
    CHECK_OK(sdfio_write_data(writer, sdfio_layout_packed(SDFIO_TYPE_INT16),
                              coded + 3 * COLUMNS, ROWS - 3));
    CHECK_OK(sdfio_end_file(writer));
    sdfio_writer_free(writer);

    sdfio_reader *reader = new_reader(all_formats());
    CHECK_OK(sdfio_open_memory(reader, file.data, file.size));
    int16_t back[ROWS * COLUMNS];
    CHECK_OK(sdfio_read_data(reader, sdfio_layout_packed(SDFIO_TYPE_INT16),
                             back, 2));
    CHECK_OK(sdfio_read_data(reader, sdfio_layout_packed(SDFIO_TYPE_INT16),
                             back + 2 * COLUMNS, ROWS - 2));
    CHECK(memcmp(coded, back, sizeof coded) == 0);
    const char *trailer = (const char *)1;
    size_t size = 1;
    CHECK_OK(read_trailer_text(reader, 0, &trailer, &size));
    CHECK(trailer != NULL && size == 0);

    /* The heights are in the order of the file, which is the order of y. */
    reader = membuf_reopen(reader, &file);
    double heights[ROWS * COLUMNS];
    CHECK_OK(sdfio_read_heights(reader, sdfio_layout_packed(SDFIO_TYPE_DOUBLE),
                                heights));
    for (size_t file_row = 0; file_row < ROWS; ++file_row)
    {
        const size_t y = file_row;
        for (size_t column = 5; column < COLUMNS; ++column)
        {
            const int16_t value = coded[file_row * COLUMNS + column];
            if (value != SDFIO_BAD_INT16)
            {
                CHECK(near_relative(heights[y * COLUMNS + column], value * 1e-9,
                                    1e-12));
            }
        }
    }
    sdfio_reader_free(reader);
    free(file.data);
}

/* The functions of a C type check that the file has the data type of the type,
 * and the compiler checks the pointer. */
static void check_typed_rows(void)
{
    sdfio_header *header = new_header();
    describe(header, SDFIO_ISO_2_0, SDFIO_BINARY, SDFIO_TYPE_INT16);
    CHECK_OK(sdfio_header_set_z_scale(header, 1e-9));
    const int16_t coded[ROWS * COLUMNS] = {1, 2, 3};
    const float floats[ROWS * COLUMNS] = {0};
    membuf file = {0};
    const sdfio_stream stream = membuf_stream(&file);
    sdfio_writer *writer = new_writer(all_formats());
    CHECK_STATUS(SDFIO_ERR_STATE,
                 sdfio_write_data(writer, sdfio_layout_packed(SDFIO_TYPE_INT16),
                                  coded, 1));
    CHECK_OK(sdfio_create_stream(writer, &stream, header));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_write_data(writer, sdfio_layout_packed(SDFIO_TYPE_FLOAT),
                                  floats, 1));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_write_data(writer, sdfio_layout_packed(SDFIO_TYPE_INT8),
                                  NULL, 1));
    CHECK_OK(sdfio_write_data(writer, sdfio_layout_packed(SDFIO_TYPE_INT16),
                              coded, ROWS));
    CHECK_OK(sdfio_end_file(writer));
    sdfio_writer_free(writer);

    sdfio_reader *reader = new_reader(all_formats());
    int16_t back[ROWS * COLUMNS] = {0};
    CHECK_STATUS(SDFIO_ERR_STATE,
                 sdfio_read_data(reader, sdfio_layout_packed(SDFIO_TYPE_INT16),
                                 back, 1));
    CHECK_OK(sdfio_open_memory(reader, file.data, file.size));
    float wrong[ROWS * COLUMNS];
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_read_data(reader, sdfio_layout_packed(SDFIO_TYPE_FLOAT),
                                 wrong, 1));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_read_data(reader, sdfio_layout_packed(SDFIO_TYPE_DOUBLE),
                                 NULL, 1));
    CHECK_OK(sdfio_read_data(reader, sdfio_layout_packed(SDFIO_TYPE_INT16),
                             back, ROWS));
    CHECK(memcmp(coded, back, sizeof coded) == 0);
    sdfio_reader_free(reader);
    free(file.data);
}

/* The y-scale of a profile is meaningless: whatever it is, the file is
 * written. */
static void check_profile_scale(void)
{
    static const double scales[] = {0.0, -1.0, 1e-6, 1e300};
    for (size_t i = 0; i < sizeof scales / sizeof scales[0]; ++i)
    {
        sdfio_header *header = new_header();
        describe(header, SDFIO_ISO_2_0, SDFIO_BINARY, SDFIO_TYPE_DOUBLE);
        CHECK_OK(sdfio_header_set_rows(header, 1));
        CHECK_OK(sdfio_header_set_y_scale(header, scales[i]));
        const double heights[COLUMNS] = {1e-6};
        membuf file = {0};
        const sdfio_stream stream = membuf_stream(&file);
        sdfio_writer *writer = new_writer(all_formats());
        CHECK_OK(sdfio_create_stream(writer, &stream, header));
        CHECK_OK(sdfio_write_heights(
            writer, sdfio_layout_packed(SDFIO_TYPE_DOUBLE), heights));
        CHECK_OK(sdfio_end_file(writer));
        sdfio_writer_free(writer);
        sdfio_reader *reader = new_reader(all_formats());
        CHECK_OK(sdfio_open_memory(reader, file.data, file.size));
        CHECK(sdfio_header_y_scale(sdfio_reader_header(reader)) == scales[i]);
        sdfio_reader_free(reader);
        free(file.data);
    }
}

static void check_errors(void)
{
    sdfio_header *header = new_header();
    describe(header, SDFIO_ISO_2_0, SDFIO_BINARY, SDFIO_TYPE_INT16);
    double heights[ROWS * COLUMNS];
    make_heights(heights);
    membuf file = {0};
    const sdfio_stream stream = membuf_stream(&file);

    /* Counts need a z-scale: the writer cannot know what they mean. */
    sdfio_writer *writer = new_writer(all_formats());
    int16_t coded[ROWS * COLUMNS] = {0};
    CHECK_OK(sdfio_create_stream(writer, &stream, header));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_write_data(writer, sdfio_layout_packed(SDFIO_TYPE_INT16),
                                  coded, 1));
    CHECK(strstr(sdfio_writer_error_message(writer), "z-scale") != NULL);
    sdfio_writer_free(writer);

    /* A height that does not fit the z-scale that was asked for. */
    CHECK_OK(sdfio_header_set_z_scale(header, 1e-12));
    writer = new_writer(all_formats());
    CHECK_OK(sdfio_create_stream(writer, &stream, header));
    CHECK_STATUS(SDFIO_ERR_RANGE,
                 sdfio_write_heights(
                     writer, sdfio_layout_packed(SDFIO_TYPE_DOUBLE), heights));
    sdfio_writer_free(writer);

    /* A height that would be read as a point not measured. */
    CHECK_OK(sdfio_header_set_z_scale(header, 1.0));
    double marker[1] = {-32768.0};
    CHECK_OK(sdfio_header_set_columns(header, 1));
    CHECK_OK(sdfio_header_set_rows(header, 1));
    writer = new_writer(all_formats());
    CHECK_OK(sdfio_create_stream(writer, &stream, header));
    CHECK_STATUS(SDFIO_ERR_BAD_VALUE,
                 sdfio_write_heights(
                     writer, sdfio_layout_packed(SDFIO_TYPE_DOUBLE), marker));
    sdfio_writer_free(writer);

    /* A description that cannot be written. */
    describe(header, SDFIO_ISO_1_0, SDFIO_BINARY, SDFIO_TYPE_INT8);
    writer = new_writer(all_formats());
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_create_stream(writer, &stream, header));
    sdfio_writer_free(writer);
    describe(header, SDFIO_ISO_1_0, SDFIO_BINARY, SDFIO_TYPE_INT16);
    CHECK_OK(sdfio_header_set_columns(header, (size_t)UINT16_MAX + 1));
    writer = new_writer(all_formats());
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_create_stream(writer, &stream, header));
    sdfio_writer_free(writer);

    /* The end before the rows, and a trailer ISO-2.0 cannot have. */
    describe(header, SDFIO_ISO_2_0, SDFIO_BINARY, SDFIO_TYPE_DOUBLE);
    writer = new_writer(all_formats());
    CHECK_OK(sdfio_create_stream(writer, &stream, header));
    CHECK_STATUS(SDFIO_ERR_STATE, sdfio_end_file(writer));
    CHECK_OK(sdfio_write_heights(writer, sdfio_layout_packed(SDFIO_TYPE_DOUBLE),
                                 heights));
    /* The text of a trailer is not looked at; only a missing one is refused. */
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_write_trailer_text(writer, NULL, 5));
    CHECK_OK(sdfio_write_trailer_text(writer, "free text", 9));
    CHECK_OK(sdfio_end_file(writer));
    CHECK_STATUS(SDFIO_ERR_STATE, sdfio_end_file(writer));
    sdfio_writer_free(writer);
    free(file.data);
}

/* The z-scale a writer chooses uses the range of the type. */
static void check_z_scale(void)
{
    const double scale =
        sdfio_z_scale_for_range(SDFIO_TYPE_INT16, -80e-6, 80e-6);
    CHECK(near_relative(scale, 80e-6 / 32767.0, 1e-12));
    CHECK(sdfio_z_scale_for_range(SDFIO_TYPE_DOUBLE, -1, 1) == 1.0);
    CHECK(sdfio_z_scale_for_range(SDFIO_TYPE_INT16, NAN, NAN) == 1.0);
}

/* A negative row stride, with the pointer at the last row, turns the matrix
 * upside down without a copy, for both directions. */
static void check_stride(void)
{
    sdfio_header *header = new_header();
    describe(header, SDFIO_ISO_2_0, SDFIO_BINARY, SDFIO_TYPE_DOUBLE);
    double heights[ROWS * COLUMNS];
    make_heights(heights);
    const ptrdiff_t stride = COLUMNS * sizeof(double);

    membuf file = {0};
    const sdfio_stream stream = membuf_stream(&file);
    sdfio_writer *writer = new_writer(all_formats());
    CHECK_OK(sdfio_create_stream(writer, &stream, header));
    CHECK_OK(sdfio_write_heights(writer,
                                 layout_with_rows(SDFIO_TYPE_DOUBLE, -stride),
                                 heights + (ROWS - 1) * COLUMNS));
    CHECK_OK(sdfio_end_file(writer));
    sdfio_writer_free(writer);

    double up[ROWS * COLUMNS];
    double down[ROWS * COLUMNS];
    sdfio_reader *reader = new_reader(all_formats());
    CHECK_OK(sdfio_open_memory(reader, file.data, file.size));
    CHECK_OK(
        sdfio_read_heights(reader, sdfio_layout_packed(SDFIO_TYPE_DOUBLE), up));
    reader = membuf_reopen(reader, &file);
    CHECK_OK(sdfio_read_heights(reader,
                                layout_with_rows(SDFIO_TYPE_DOUBLE, -stride),
                                down + (ROWS - 1) * COLUMNS));
    for (size_t row = 0; row < ROWS; ++row)
    {
        for (size_t column = 0; column < COLUMNS; ++column)
        {
            const double written = heights[(ROWS - 1 - row) * COLUMNS + column];
            const double read_up = up[row * COLUMNS + column];
            const double read_down = down[(ROWS - 1 - row) * COLUMNS + column];
            CHECK(isnan(written)
                      ? isnan(read_up) && isnan(read_down)
                      : read_up == written
                            && read_down == up[row * COLUMNS + column]);
        }
    }
    /* A stride smaller than a row would overwrite the memory. */
    reader = membuf_reopen(reader, &file);
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_read_heights(reader,
                                    layout_with_rows(SDFIO_TYPE_DOUBLE,
                                                     (ptrdiff_t)sizeof(double)),
                                    up));
    sdfio_reader_free(reader);
    free(file.data);
}

/* The heights of a file in the layouts of other libraries: written from and
 * read into points, a column-major matrix and a mirrored one, they give the
 * same file and the same numbers as the plain matrix. */
typedef struct point
{
    double x;
    double y;
    double z;
    double w;
} point;

static void check_element_stride(void)
{
    sdfio_header *header = new_header();
    describe(header, SDFIO_ISO_2_0, SDFIO_BINARY, SDFIO_TYPE_DOUBLE);
    double plain[ROWS * COLUMNS];
    make_heights(plain);

    /* The same heights in a column-major matrix and in points. */
    double columns_first[ROWS * COLUMNS];
    point points[ROWS * COLUMNS];
    for (size_t y = 0; y < ROWS; ++y)
    {
        for (size_t x = 0; x < COLUMNS; ++x)
        {
            columns_first[x * ROWS + y] = plain[y * COLUMNS + x];
            points[y * COLUMNS + x] =
                (point){1.0, 2.0, plain[y * COLUMNS + x], 4.0};
        }
    }

    membuf from_plain = {0};
    membuf from_columns = {0};
    membuf from_points = {0};
    membuf *const files[] = {&from_plain, &from_columns, &from_points};
    for (size_t i = 0; i < 3; ++i)
    {
        const sdfio_stream stream = membuf_stream(files[i]);
        sdfio_layout layout = sdfio_layout_packed(SDFIO_TYPE_DOUBLE);
        const void *source = plain;
        if (i == 1)
        {
            layout.element_stride = ROWS * sizeof(double);
            layout.row_stride = sizeof(double);
            source = columns_first;
        }
        else if (i == 2)
        {
            layout.element_stride = sizeof(point);
            layout.row_stride = COLUMNS * sizeof(point);
            source = &points[0].z;
        }
        sdfio_writer *writer = new_writer(all_formats());
        CHECK_OK(sdfio_create_stream(writer, &stream, header));
        CHECK_OK(sdfio_write_heights(writer, layout, source));
        CHECK_OK(sdfio_end_file(writer));
        sdfio_writer_free(writer);
    }
    CHECK(from_columns.size == from_plain.size
          && memcmp(from_columns.data, from_plain.data, from_plain.size) == 0);
    CHECK(from_points.size == from_plain.size
          && memcmp(from_points.data, from_plain.data, from_plain.size) == 0);

    /* Read back into each layout. */
    double read_columns[ROWS * COLUMNS];
    point read_points[ROWS * COLUMNS];
    double mirrored[ROWS * COLUMNS];
    for (size_t i = 0; i < ROWS * COLUMNS; ++i)
    {
        read_points[i] = (point){-1.0, -2.0, 0.0, -4.0};
    }
    sdfio_reader *reader = new_reader(all_formats());
    CHECK_OK(sdfio_open_memory(reader, from_plain.data, from_plain.size));
    sdfio_layout layout = sdfio_layout_packed(SDFIO_TYPE_DOUBLE);
    layout.element_stride = ROWS * sizeof(double);
    layout.row_stride = sizeof(double);
    CHECK_OK(sdfio_read_heights(reader, layout, read_columns));
    reader = membuf_reopen(reader, &from_plain);
    layout.element_stride = sizeof(point);
    layout.row_stride = COLUMNS * sizeof(point);
    CHECK_OK(sdfio_read_heights(reader, layout, &read_points[0].z));
    reader = membuf_reopen(reader, &from_plain);
    layout.element_stride = -(ptrdiff_t)sizeof(double);
    layout.row_stride = COLUMNS * sizeof(double);
    CHECK_OK(sdfio_read_heights(reader, layout, mirrored + COLUMNS - 1));
    for (size_t y = 0; y < ROWS; ++y)
    {
        for (size_t x = 0; x < COLUMNS; ++x)
        {
            const double expected = plain[y * COLUMNS + x];
            const double columns_value = read_columns[x * ROWS + y];
            const point p = read_points[y * COLUMNS + x];
            const double mirror = mirrored[y * COLUMNS + (COLUMNS - 1 - x)];
            CHECK(isnan(expected)
                      ? isnan(columns_value) && isnan(p.z) && isnan(mirror)
                      : columns_value == expected && p.z == expected
                            && mirror == expected);
            CHECK(p.x == -1.0 && p.y == -2.0 && p.w == -4.0);
        }
    }

    /* Strides that make the elements overlap are refused, and nothing is
     * written. */
    double scratch[ROWS * COLUMNS] = {0};
    layout = layout_with_rows(SDFIO_TYPE_DOUBLE, sizeof(double));
    reader = membuf_reopen(reader, &from_plain);
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_read_heights(reader, layout, scratch));
    CHECK(scratch[0] == 0.0 && scratch[ROWS * COLUMNS - 1] == 0.0);
    CHECK(strstr(sdfio_reader_error_message(reader), "overlap") != NULL);
    sdfio_reader_free(reader);

    const sdfio_stream stream = membuf_stream(&from_plain);
    sdfio_writer *writer = new_writer(all_formats());
    CHECK_OK(sdfio_create_stream(writer, &stream, header));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_write_heights(writer, layout, plain));
    sdfio_writer_free(writer);
    free(from_plain.data);
    free(from_columns.data);
    free(from_points.data);
}

/* An unknown z-resolution is -1, as the standard has it: whatever a program
 * gives that is not positive is written as -1, and whatever a file holds that
 * is not positive is read as -1. */
static void write_with_resolution(membuf *file, sdfio_format format,
                                  double resolution)
{
    sdfio_header *header = new_header();
    describe(header, SDFIO_ISO_2_0, format, SDFIO_TYPE_DOUBLE);
    CHECK_OK(sdfio_header_set_z_resolution(header, resolution));
    double heights[ROWS * COLUMNS];
    make_heights(heights);
    const sdfio_stream stream = membuf_stream(file);
    sdfio_writer *writer = new_writer(all_formats());
    CHECK_OK(sdfio_create_stream(writer, &stream, header));
    CHECK_OK(sdfio_write_heights(writer, sdfio_layout_packed(SDFIO_TYPE_DOUBLE),
                                 heights));
    CHECK_OK(sdfio_end_file(writer));
    sdfio_writer_free(writer);
}

static double resolution_read(const membuf *file)
{
    sdfio_reader *reader = new_reader(all_formats());
    double resolution = 0.0;
    CHECK_OK(sdfio_open_memory(reader, file->data, file->size));
    if (sdfio_reader_header(reader) != NULL)
    {
        resolution = sdfio_header_z_resolution(sdfio_reader_header(reader));
    }
    sdfio_reader_free(reader);
    return resolution;
}

/* A double as the file holds it: 8 bytes, the least significant first. */
static double double_in_file(const unsigned char *bytes)
{
    uint64_t bits = 0;
    for (size_t i = 0; i < sizeof bits; ++i)
    {
        bits |= (uint64_t)bytes[i] << (CHAR_BIT * i);
    }
    double value = 0.0;
    memcpy(&value, &bits, sizeof value);
    return value;
}

static void put_double_in_file(unsigned char *bytes, double value)
{
    uint64_t bits = 0;
    memcpy(&bits, &value, sizeof bits);
    for (size_t i = 0; i < sizeof bits; ++i)
    {
        bytes[i] = (unsigned char)(bits >> (CHAR_BIT * i));
    }
}

static void check_resolution(void)
{
    sdfio_header *initial = new_header();
    CHECK(SDFIO_NO_RESOLUTION == -1.0);
    CHECK(sdfio_header_z_resolution(initial) == SDFIO_NO_RESOLUTION);

    const double given[] = {SDFIO_NO_RESOLUTION, 0.0, -5.0, 1e-9};
    const sdfio_format formats[] = {SDFIO_BINARY, SDFIO_ASCII};
    for (size_t s = 0; s < 2; ++s)
    {
        for (size_t g = 0; g < sizeof given / sizeof given[0]; ++g)
        {
            membuf file = {0};
            write_with_resolution(&file, formats[s], given[g]);
            const double expected =
                given[g] > 0.0 ? given[g] : SDFIO_NO_RESOLUTION;
            CHECK(resolution_read(&file) == expected);
            if (formats[s] == SDFIO_BINARY)
            {
                /* Four doubles and three bytes end the header. */
                const double stored =
                    double_in_file(file.data + ISO_2_0_BINARY_HEADER_SIZE - 3
                                   - sizeof(double));
                CHECK(stored == expected);
            }
            free(file.data);
        }
    }

    /* A file that holds another number that is not positive. */
    const double stored_values[] = {-2.0, 0.0, -1e-9};
    for (size_t i = 0; i < sizeof stored_values / sizeof stored_values[0]; ++i)
    {
        membuf file = {0};
        write_with_resolution(&file, SDFIO_BINARY, 1e-9);
        put_double_in_file(file.data + ISO_2_0_BINARY_HEADER_SIZE - 3
                               - sizeof(double),
                           stored_values[i]);
        CHECK(resolution_read(&file) == SDFIO_NO_RESOLUTION);
        free(file.data);
    }

    /* A number that is no number is refused, and the header keeps the value
     * that it had. */
    const double refused[] = {NAN, INFINITY, -INFINITY};
    for (size_t i = 0; i < sizeof refused / sizeof refused[0]; ++i)
    {
        sdfio_header *header = new_header();
        CHECK_OK(sdfio_header_set_z_resolution(header, 1e-9));
        CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                     sdfio_header_set_z_resolution(header, refused[i]));
        CHECK(sdfio_header_z_resolution(header) == (double)1e-9);
    }
}

/* Rows without a column hold nothing, and a file that declares billions of
 * them is written and read in an instant. */
static void check_rows_without_columns(void)
{
    sdfio_header *header = new_header();
    describe(header, SDFIO_ISO_2_0, SDFIO_BINARY, SDFIO_TYPE_FLOAT);
    CHECK_OK(sdfio_header_set_columns(header, 0));
    CHECK_OK(sdfio_header_set_rows(header,
                                   sdfio_max_columns_and_rows(SDFIO_ISO_2_0)));

    membuf file = {0};
    const sdfio_stream stream = membuf_stream(&file);
    sdfio_writer *writer = new_writer(all_formats());
    CHECK_OK(sdfio_create_stream(writer, &stream, header));
    CHECK_OK(sdfio_write_data(
        writer, sdfio_layout_packed(sdfio_header_data_type(header)), NULL,
        sdfio_header_rows(header)));
    CHECK_OK(sdfio_write_trailer_text(writer, "a = 1\n", 6));
    CHECK_OK(sdfio_end_file(writer));
    sdfio_writer_free(writer);

    sdfio_reader *reader = new_reader(all_formats());
    CHECK_OK(sdfio_open_memory(reader, file.data, file.size));
    CHECK_OK(sdfio_read_data(
        reader, sdfio_layout_packed(sdfio_header_data_type(header)), NULL,
        sdfio_header_rows(header)));
    const char *trailer = NULL;
    size_t trailer_size = 0;
    CHECK_OK(read_trailer_text(reader, 0, &trailer, &trailer_size));
    CHECK(trailer_size == 5 && memcmp(trailer, "a = 1", 5) == 0);
    sdfio_reader_free(reader);
    free(file.data);
}

/* What a header starts as: a binary ISO-2.0 file of single precision floats,
 * which holds a float surface without loss and needs no z-scale. */
static void check_defaults(void)
{
    sdfio_header *header = new_header();
    CHECK(sdfio_header_revision(header) == SDFIO_ISO_2_0);
    CHECK(sdfio_header_format(header) == SDFIO_BINARY);
    CHECK(sdfio_header_data_type(header) == SDFIO_TYPE_FLOAT);
    CHECK(sdfio_header_z_scale(header) == SDFIO_AUTO_Z_SCALE);
    CHECK(sdfio_header_columns(header) == 0 && sdfio_header_rows(header) == 0);

    /* A float surface written with the defaults comes back as it was. */
    CHECK_OK(sdfio_header_set_columns(header, 3));
    CHECK_OK(sdfio_header_set_rows(header, 2));
    const float heights[6] = {1e-6F, -2.5e-7F, NAN, 3e-7F, 0.0F, 7.5e-6F};
    membuf file = {0};
    const sdfio_stream stream = membuf_stream(&file);
    sdfio_writer *writer = new_writer(all_formats());
    CHECK_OK(sdfio_create_stream(writer, &stream, header));
    CHECK_OK(sdfio_write_heights(writer, sdfio_layout_packed(SDFIO_TYPE_FLOAT),
                                 heights));
    CHECK_OK(sdfio_end_file(writer));
    sdfio_writer_free(writer);
    float back[6];
    sdfio_reader *reader = new_reader(all_formats());
    CHECK_OK(sdfio_open_memory(reader, file.data, file.size));
    CHECK_OK(sdfio_read_heights(reader, sdfio_layout_packed(SDFIO_TYPE_FLOAT),
                                back));
    for (size_t i = 0; i < 6; ++i)
    {
        CHECK(isnan(heights[i]) ? isnan(back[i]) : back[i] == heights[i]);
    }
    CHECK(sdfio_header_z_scale(sdfio_reader_header(reader)) == 1.0);
    sdfio_reader_free(reader);
    free(file.data);
}

/* The digits of the integers of an ASCII data area, up to the extremes of the
 * widest type. The lowest value of a type is the bad value, which is BAD. */
static void check_integer_text(void)
{
    sdfio_header *header = new_header();
    CHECK_OK(sdfio_header_set_format(header, SDFIO_ASCII));
    CHECK_OK(sdfio_header_set_data_type(header, SDFIO_TYPE_INT32));
    CHECK_OK(sdfio_header_set_z_scale(header, 1.0));
    CHECK_OK(sdfio_header_set_columns(header, 9));
    CHECK_OK(sdfio_header_set_rows(header, 1));
    const int32_t coded[9] = {0,       7,         -7,         10,       99999,
                              -100000, INT32_MAX, -INT32_MAX, INT32_MIN};
    membuf file = {0};
    const sdfio_stream stream = membuf_stream(&file);
    sdfio_writer *writer = new_writer(all_formats());
    CHECK_OK(sdfio_create_stream(writer, &stream, header));
    CHECK_OK(sdfio_write_data(writer, sdfio_layout_packed(SDFIO_TYPE_INT32),
                              coded, 1));
    CHECK_OK(sdfio_end_file(writer));
    sdfio_writer_free(writer);
    char *text = malloc(file.size + 1);
    CHECK(text != NULL);
    if (text != NULL)
    {
        memcpy(text, file.data, file.size);
        text[file.size] = '\0';
        CHECK(strstr(text,
                     "0 7 -7 10 99999 -100000 2147483647 -2147483647 BAD\r\n")
              != NULL);
        free(text);
    }
    free(file.data);
}

/* An identifier of the 10 characters that the field has comes back in both
 * formats. */
static void check_long_identifier(void)
{
    static const sdfio_format formats[] = {SDFIO_BINARY, SDFIO_ASCII};
    for (size_t i = 0; i < sizeof formats / sizeof formats[0]; ++i)
    {
        sdfio_header *header = new_header();
        CHECK_OK(sdfio_header_set_format(header, formats[i]));
        CHECK_OK(sdfio_header_set_columns(header, 1));
        CHECK_OK(sdfio_header_set_rows(header, 1));
        CHECK_OK(sdfio_header_set_manufacturer(header, "0123456789"));
        const float height = 1e-6F;
        membuf file = {0};
        const sdfio_stream stream = membuf_stream(&file);
        sdfio_writer *writer = new_writer(all_formats());
        CHECK_OK(sdfio_create_stream(writer, &stream, header));
        CHECK_OK(sdfio_write_heights(
            writer, sdfio_layout_packed(SDFIO_TYPE_FLOAT), &height));
        CHECK_OK(sdfio_end_file(writer));
        sdfio_writer_free(writer);
        sdfio_reader *reader = new_reader(all_formats());
        CHECK_OK(sdfio_open_memory(reader, file.data, file.size));
        const sdfio_header *back = sdfio_reader_header(reader);
        CHECK(back != NULL
              && strcmp(sdfio_header_manufacturer(back), "0123456789") == 0);
        sdfio_reader_free(reader);
        free(file.data);
    }
}

/* Every line end that is written is CRLF, as the standard has it, whatever the
 * program put in memory; what is read back has LF. */
static bool only_crlf(const unsigned char *bytes, size_t size)
{
    for (size_t i = 0; i < size; ++i)
    {
        const bool lf_alone =
            bytes[i] == '\n' && (i == 0 || bytes[i - 1] != '\r');
        const bool cr_alone =
            bytes[i] == '\r' && (i + 1 == size || bytes[i + 1] != '\n');
        if (lf_alone || cr_alone)
        {
            return false;
        }
    }
    return true;
}

static void check_trailer_line_ends(void)
{
    struct variant
    {
        const char *written;
        const char *in_file; /* the bytes of a binary trailer */
        const char *read;
    };
    const struct variant variants[] = {
        {"A = 1\nB = 2", "A = 1\r\nB = 2\r\n", "A = 1\nB = 2"},
        {"A = 1\r\nB = 2", "A = 1\r\nB = 2\r\n", "A = 1\nB = 2"},
        {"A = 1\nB = 2\n", "A = 1\r\nB = 2\r\n", "A = 1\nB = 2"},
        {"A = 1\r\nB = 2\nC = 3", "A = 1\r\nB = 2\r\nC = 3\r\n",
         "A = 1\nB = 2\nC = 3"},
        {"A = 1\n\nB = 2", "A = 1\r\n\r\nB = 2\r\n", "A = 1\n\nB = 2"},
        {"A = 1", "A = 1\r\n", "A = 1"},
    };
    const sdfio_format formats[] = {SDFIO_BINARY, SDFIO_ASCII};
    for (size_t s = 0; s < 2; ++s)
    {
        for (size_t v = 0; v < sizeof variants / sizeof variants[0]; ++v)
        {
            sdfio_header *header = new_header();
            describe(header, SDFIO_ISO_2_0, formats[s], SDFIO_TYPE_DOUBLE);
            CHECK_OK(sdfio_header_set_columns(header, 1));
            CHECK_OK(sdfio_header_set_rows(header, 1));
            const double height = 1e-6;
            membuf file = {0};
            const sdfio_stream stream = membuf_stream(&file);
            sdfio_writer *writer = new_writer(all_formats());
            CHECK_OK(sdfio_create_stream(writer, &stream, header));
            CHECK_OK(sdfio_write_heights(
                writer, sdfio_layout_packed(SDFIO_TYPE_DOUBLE), &height));
            CHECK_OK(sdfio_write_trailer_text(writer, variants[v].written,
                                              strlen(variants[v].written)));
            CHECK_OK(sdfio_end_file(writer));
            sdfio_writer_free(writer);

            const size_t data_at = ISO_2_0_BINARY_HEADER_SIZE + sizeof(double);
            if (formats[s] == SDFIO_BINARY)
            {
                CHECK(file.size == data_at + strlen(variants[v].in_file));
                CHECK(memcmp(file.data + data_at, variants[v].in_file,
                             strlen(variants[v].in_file))
                      == 0);
                CHECK(only_crlf(file.data + data_at, file.size - data_at));
            }
            else
            {
                CHECK(only_crlf(file.data, file.size));
            }

            sdfio_reader *reader = new_reader(all_formats());
            CHECK_OK(sdfio_open_memory(reader, file.data, file.size));
            const char *trailer = NULL;
            size_t size = 0;
            CHECK_OK(read_trailer_text(reader, 0, &trailer, &size));
            CHECK(trailer != NULL && size == strlen(variants[v].read)
                  && memcmp(trailer, variants[v].read, size) == 0);
            sdfio_reader_free(reader);
            free(file.data);
        }
    }
}

/* A text file is read whatever ends its lines: CR LF as ISO has it, LF of Unix,
 * which the writers of some programs use. */
static membuf with_line_ends(const membuf *file, const char *line_end)
{
    membuf out = {0};
    for (size_t i = 0; i < file->size; ++i)
    {
        if (file->data[i] == '\r' && i + 1 < file->size
            && file->data[i + 1] == '\n')
        {
            membuf_write(&out, line_end, strlen(line_end));
            ++i;
        }
        else
        {
            membuf_write(&out, &file->data[i], 1);
        }
    }
    return out;
}

static void check_read_line_ends(void)
{
    const sdfio_revision revisions[] = {SDFIO_ISO_1_0, SDFIO_ISO_2_0};
    const char *const line_ends[] = {"\n"};
    for (size_t r = 0; r < 2; ++r)
    {
        sdfio_header *header = new_header();
        describe(header, revisions[r], SDFIO_ASCII, SDFIO_TYPE_INT16);
        double heights[ROWS * COLUMNS];
        make_heights(heights);
        membuf crlf = {0};
        const sdfio_stream stream = membuf_stream(&crlf);
        sdfio_writer *writer = new_writer(all_formats());
        CHECK_OK(sdfio_create_stream(writer, &stream, header));
        CHECK_OK(sdfio_write_heights(
            writer, sdfio_layout_packed(SDFIO_TYPE_DOUBLE), heights));
        CHECK_OK(sdfio_write_trailer_text(writer, tagged_trailer,
                                          strlen(tagged_trailer)));
        CHECK_OK(sdfio_end_file(writer));
        sdfio_writer_free(writer);

        double expected[ROWS * COLUMNS];
        sdfio_reader *reader = new_reader(all_formats());
        CHECK_OK(sdfio_open_memory(reader, crlf.data, crlf.size));
        CHECK_OK(sdfio_read_heights(
            reader, sdfio_layout_packed(SDFIO_TYPE_DOUBLE), expected));
        sdfio_reader_free(reader);

        for (size_t e = 0; e < 1; ++e)
        {
            membuf other = with_line_ends(&crlf, line_ends[e]);
            CHECK(other.size < crlf.size);
            double read[ROWS * COLUMNS];
            reader = new_reader(all_formats());
            CHECK_OK(sdfio_open_memory(reader, other.data, other.size));
            const sdfio_header *got = sdfio_reader_header(reader);
            CHECK(got != NULL && sdfio_header_columns(got) == COLUMNS
                  && sdfio_header_rows(got) == ROWS
                  && sdfio_header_x_scale(got) == sdfio_header_x_scale(header)
                  && sdfio_header_created(got) == sdfio_header_created(header));
            CHECK_OK(sdfio_read_heights(
                reader, sdfio_layout_packed(SDFIO_TYPE_DOUBLE), read));
            for (size_t i = 0; i < ROWS * COLUMNS; ++i)
            {
                CHECK(isnan(expected[i]) ? isnan(read[i])
                                         : read[i] == expected[i]);
            }
            const char *trailer = NULL;
            size_t size = 0;
            CHECK_OK(read_trailer_text(reader, 0, &trailer, &size));
            CHECK(trailer != NULL && size == strlen(tagged_trailer)
                  && memcmp(trailer, tagged_trailer, size) == 0);
            sdfio_reader_free(reader);
            free(other.data);
        }
        free(crlf.data);
    }
}

/* An attempt to open a file in a reader that is open leaves the open file as it
 * is, also when the file is larger than the buffer. */
static void check_second_open(void)
{
    enum
    {
        VALUES = 4000
    };
    membuf files[2] = {{0}, {0}};
    float values[VALUES];
    for (size_t f = 0; f < 2; ++f)
    {
        for (size_t i = 0; i < VALUES; ++i)
        {
            values[i] = (float)i + (float)f * 100000.0F;
        }
        sdfio_header *header = new_header();
        CHECK_OK(sdfio_header_set_data_type(header, SDFIO_TYPE_FLOAT));
        CHECK_OK(sdfio_header_set_columns(header, VALUES));
        CHECK_OK(sdfio_header_set_rows(header, 1));
        CHECK_OK(sdfio_header_set_z_scale(header, 1.0));
        const sdfio_stream stream = membuf_stream(&files[f]);
        sdfio_writer *writer = new_writer(all_formats());
        CHECK_OK(sdfio_create_stream(writer, &stream, header));
        CHECK_OK(sdfio_write_data(writer, sdfio_layout_packed(SDFIO_TYPE_FLOAT),
                                  values, 1));
        CHECK_OK(sdfio_end_file(writer));
        sdfio_writer_free(writer);
    }
    sdfio_reader *reader = new_reader(all_formats());
    CHECK_OK(sdfio_reader_set_buffer(reader, 1));
    CHECK_OK(sdfio_open_memory(reader, files[0].data, files[0].size));
    CHECK_STATUS(SDFIO_ERR_STATE,
                 sdfio_open_memory(reader, files[1].data, files[1].size));
    CHECK_OK(sdfio_read_data(reader, sdfio_layout_packed(SDFIO_TYPE_FLOAT),
                             values, 1));
    size_t different = 0;
    for (size_t i = 0; i < VALUES; ++i)
    {
        different += values[i] != (float)i ? 1 : 0;
    }
    CHECK(different == 0);
    sdfio_reader_free(reader);
    free(files[0].data);
    free(files[1].data);
}

static time_t refuse_time(void *user, time_t time)
{
    (void)user;
    (void)time;
    return SDFIO_NO_TIME;
}

/* A header that cannot be written fails the writer, also if it is the end of
 * the file that writes it, and the file is not ended by a second try. */
static void check_failed_header(void)
{
    sdfio_header *header = new_header();
    CHECK_OK(sdfio_header_set_revision(header, SDFIO_ISO_1_0));
    CHECK_OK(sdfio_header_set_data_type(header, SDFIO_TYPE_INT16));
    CHECK_OK(sdfio_header_set_columns(header, 3));
    CHECK_OK(sdfio_header_set_rows(header, 0));
    CHECK_OK(sdfio_header_set_z_scale(header, 1.0));
    const sdfio_time_zone zone = {NULL, NULL, refuse_time};
    const sdfio_format formats[] = {SDFIO_BINARY, SDFIO_ASCII};
    for (size_t f = 0; f < 2; ++f)
    {
        CHECK_OK(sdfio_header_set_format(header, formats[f]));
        membuf file = {0};
        const sdfio_stream stream = membuf_stream(&file);
        sdfio_writer *writer = new_writer(all_formats());
        CHECK_OK(sdfio_writer_set_time_zone(writer, &zone));
        CHECK_OK(sdfio_create_stream(writer, &stream, header));
        CHECK_STATUS(SDFIO_ERR_RANGE, sdfio_end_file(writer));
        CHECK_STATUS(SDFIO_ERR_STATE, sdfio_end_file(writer));
        CHECK_STATUS(SDFIO_ERR_STATE,
                     sdfio_write_trailer_field(writer, "Name", "value"));
        CHECK(file.size == 0);
        sdfio_writer_free(writer);
        free(file.data);
    }
}

/* The scales are finite numbers in a file, as they are in a header. */
static void check_scales_of_files(void)
{
    sdfio_header *header = new_header();
    CHECK_OK(sdfio_header_set_columns(header, 2));
    CHECK_OK(sdfio_header_set_rows(header, 1));
    CHECK_OK(sdfio_header_set_z_scale(header, 1.0));
    const double not_finite[] = {NAN, INFINITY, -INFINITY};
    for (size_t i = 0; i < sizeof not_finite / sizeof not_finite[0]; ++i)
    {
        for (size_t scale = 0; scale < 2; ++scale)
        {
            membuf file = {0};
            const sdfio_stream stream = membuf_stream(&file);
            sdfio_writer *writer = new_writer(all_formats());
            const float values[2] = {1.0F, 2.0F};
            CHECK_OK(sdfio_create_stream(writer, &stream, header));
            CHECK_OK(sdfio_write_data(
                writer, sdfio_layout_packed(SDFIO_TYPE_FLOAT), values, 1));
            CHECK_OK(sdfio_end_file(writer));
            sdfio_writer_free(writer);
            put_double_in_file(file.data + ISO_2_0_COLUMNS_AT + 2 * 4
                                   + scale * sizeof(double),
                               not_finite[i]);
            sdfio_reader *reader = new_reader(all_formats());
            CHECK_STATUS(SDFIO_ERR_FORMAT,
                         sdfio_open_memory(reader, file.data, file.size));
            sdfio_reader_free(reader);
            free(file.data);
        }
    }

    /* The text of a number that is too large for a double. */
    CHECK_OK(sdfio_header_set_format(header, SDFIO_ASCII));
    membuf file = {0};
    const sdfio_stream stream = membuf_stream(&file);
    sdfio_writer *writer = new_writer(all_formats());
    const float values[2] = {1.0F, 2.0F};
    CHECK_OK(sdfio_create_stream(writer, &stream, header));
    CHECK_OK(sdfio_write_data(writer, sdfio_layout_packed(SDFIO_TYPE_FLOAT),
                              values, 1));
    CHECK_OK(sdfio_end_file(writer));
    sdfio_writer_free(writer);
    char *at = strstr((char *)file.data, "Xscale = ");
    CHECK(at != NULL);
    if (at != NULL)
    {
        static const char huge[] = "9.99999999999999e+999";
        memcpy(at + sizeof "Xscale = " - 1, huge, sizeof huge - 1);
    }
    sdfio_reader *reader = new_reader(all_formats());
    CHECK_STATUS(SDFIO_ERR_FORMAT,
                 sdfio_open_memory(reader, file.data, file.size));
    sdfio_reader_free(reader);
    free(file.data);
}

int main(void)
{
    check_second_open();
    check_failed_header();
    check_scales_of_files();
    check_read_line_ends();
    check_trailer_line_ends();
    check_rows_without_columns();
    check_defaults();
    check_long_identifier();
    check_integer_text();
    check_resolution();
    check_element_stride();
    check_stride();
    check_all_types();
    for (int r = SDFIO_ISO_1_0; r <= SDFIO_ISO_2_0; ++r)
    {
        check_rows((sdfio_revision)r, SDFIO_BINARY);
        check_rows((sdfio_revision)r, SDFIO_ASCII);
        check_strided_rows((sdfio_revision)r, SDFIO_BINARY);
        check_strided_rows((sdfio_revision)r, SDFIO_ASCII);
    }
    check_errors();
    check_typed_rows();
    check_profile_scale();
    check_z_scale();
    return finish_tests();
}
