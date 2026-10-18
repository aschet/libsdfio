/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* What is written is what is read: every revision, representation and data
 * type, the order of the rows, the trailer and the dates. */

#include "check.h"

#include "internal.h"

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
                     sdfio_storage storage, sdfio_data_type type)
{
    sdfio_header_init(header);
    header->revision = revision;
    header->storage = storage;
    header->data_type = type;
    header->columns = COLUMNS;
    header->rows = ROWS;
    header->x_scale = 2e-6;
    header->y_scale = 3e-6;
    header->z_resolution = 1e-9;
    header->creation_date = 1772600760; /* 2026-03-04 05:06 UTC */
    header->modification_date = 1772604360;
    snprintf(header->manufacturer_id, sizeof header->manufacturer_id, "ACME 7");
}

static void check_roundtrip(sdfio_revision revision, sdfio_storage storage,
                            sdfio_data_type type)
{
    sdfio_header header;
    describe(&header, revision, storage, type);
    double heights[ROWS * COLUMNS];
    make_heights(heights);

    membuf file = {0};
    const sdfio_stream stream = membuf_stream(&file);
    sdfio_writer *writer = sdfio_writer_new();
    CHECK_OK(sdfio_create_stream(writer, &stream, &header));
    CHECK_OK(sdfio_write_heights(writer, sdfio_metres(SDFIO_FLOAT64), heights,
                                 COLUMNS * sizeof(double)));
    CHECK_OK(
        sdfio_write_trailer(writer, tagged_trailer, strlen(tagged_trailer)));
    sdfio_writer_free(writer);

    sdfio_reader *reader = sdfio_reader_new();
    CHECK_OK(sdfio_open_memory(reader, file.data, file.size));
    const sdfio_header *read = sdfio_get_header(reader);
    CHECK(read != NULL);
    if (read == NULL)
    {
        sdfio_reader_free(reader);
        free(file.data);
        return;
    }
    CHECK(read->revision == revision);
    CHECK(read->storage == storage);
    CHECK(read->data_type == type);
    CHECK(read->columns == COLUMNS && read->rows == ROWS);
    CHECK(read->x_scale == header.x_scale && read->y_scale == header.y_scale);
    CHECK(read->z_resolution == header.z_resolution);
    CHECK(read->creation_date == header.creation_date);
    CHECK(read->modification_date == header.modification_date);
    CHECK(strcmp(read->manufacturer_id, header.manufacturer_id) == 0);
    CHECK(read->z_scale > 0.0);

    double back[ROWS * COLUMNS];
    CHECK_OK(sdfio_read_heights(reader, sdfio_metres(SDFIO_FLOAT64), back,
                                COLUMNS * sizeof(double)));
    /* An integer type is as exact as one count; text and binary32 keep
     * about as many digits as they print. */
    double tolerance = 1e-13;
    if (type == SDFIO_DATA_BINARY32)
    {
        tolerance = 1e-6;
    }
    for (size_t i = 0; i < ROWS * COLUMNS; ++i)
    {
        if (isnan(heights[i]))
        {
            CHECK(isnan(back[i]));
        }
        else if (!sdfi_is_floating(type))
        {
            CHECK(fabs(back[i] - heights[i]) <= 0.5 * read->z_scale * 1.0001);
        }
        else
        {
            CHECK(fabs(back[i] - heights[i])
                  <= tolerance * fmax(1e-6, fabs(heights[i])));
        }
    }

    const char *trailer = NULL;
    size_t size = 0;
    CHECK_OK(sdfio_read_trailer(reader, &trailer, &size));
    CHECK(trailer != NULL && size == strlen(tagged_trailer)
          && memcmp(trailer, tagged_trailer, size) == 0);
    const char *value = NULL;
    size_t value_size = 0;
    CHECK(sdfio_trailer_find(trailer, size, "partname", &value, &value_size)
          && value_size == strlen("Sample")
          && memcmp(value, "Sample", value_size) == 0);

    sdfio_reader_free(reader);
    free(file.data);
}

static void check_all_types(void)
{
    const sdfio_revision revisions[] = {SDFIO_ISO_1_0, SDFIO_ISO_2_0,
                                        SDFIO_BCR_1_0};
    const sdfio_storage storages[] = {SDFIO_BINARY, SDFIO_ASCII};
    for (size_t r = 0; r < sizeof revisions / sizeof revisions[0]; ++r)
    {
        for (size_t s = 0; s < sizeof storages / sizeof storages[0]; ++s)
        {
            for (int t = SDFIO_DATA_UINT8; t <= SDFIO_DATA_BINARY64; ++t)
            {
                const sdfio_data_type type = (sdfio_data_type)t;
                /* The unsigned types cannot hold the negative heights. */
                if (!sdfio_revision_allows(revisions[r], type)
                    || type == SDFIO_DATA_UINT8 || type == SDFIO_DATA_UINT16
                    || type == SDFIO_DATA_UINT32)
                {
                    continue;
                }
                check_roundtrip(revisions[r], storages[s], type);
            }
        }
    }
}

/* The rows API moves the stored values as they are, in the order of the file,
 * which is reversed in BCR-1.0. */
static void check_rows(sdfio_revision revision, sdfio_storage storage)
{
    sdfio_header header;
    describe(&header, revision, storage, SDFIO_DATA_INT16);
    header.z_scale = 1e-9;
    int16_t counts[ROWS * COLUMNS];
    for (size_t i = 0; i < ROWS * COLUMNS; ++i)
    {
        counts[i] = (int16_t)((int)i * 37 - 100);
    }
    counts[3] = INT16_MIN; /* the marker of a point not measured in ISO */
    counts[4] = INT16_MAX; /* the marker of BCR-1.0 */

    membuf file = {0};
    const sdfio_stream stream = membuf_stream(&file);
    sdfio_writer *writer = sdfio_writer_new();
    CHECK_OK(sdfio_create_stream(writer, &stream, &header));
    CHECK_OK(sdfio_write_data(writer, counts, 3));
    CHECK_OK(sdfio_write_data(writer, counts + 3 * COLUMNS, ROWS - 3));
    CHECK_OK(sdfio_write_trailer(writer, NULL, 0));
    sdfio_writer_free(writer);

    sdfio_reader *reader = sdfio_reader_new();
    CHECK_OK(sdfio_open_memory(reader, file.data, file.size));
    int16_t back[ROWS * COLUMNS];
    CHECK_OK(sdfio_read_data(reader, back, 2));
    CHECK_OK(sdfio_read_data(reader, back + 2 * COLUMNS, ROWS - 2));
    CHECK(memcmp(counts, back, sizeof counts) == 0);
    const char *trailer = (const char *)1;
    size_t size = 1;
    CHECK_OK(sdfio_read_trailer(reader, &trailer, &size));
    /* A text file closes the trailer record, so it has an empty one. */
    CHECK(storage == SDFIO_ASCII ? (trailer != NULL && size == 0)
                                 : (trailer == NULL && size == 0));

    /* The heights are in the order of y in every revision. */
    sdfio_header as_read = *sdfio_get_header(reader);
    CHECK_OK(sdfio_rewind(reader));
    double heights[ROWS * COLUMNS];
    CHECK_OK(sdfio_read_heights(reader, sdfio_metres(SDFIO_FLOAT64), heights,
                                COLUMNS * sizeof(double)));
    for (size_t file_row = 0; file_row < ROWS; ++file_row)
    {
        const size_t y = sdfio_row_y(&as_read, file_row);
        CHECK(y
              == (revision == SDFIO_BCR_1_0 ? ROWS - 1 - file_row : file_row));
        for (size_t column = 5; column < COLUMNS; ++column)
        {
            const int16_t count = counts[file_row * COLUMNS + column];
            if (!sdfio_is_sentinel(SDFIO_DATA_INT16, revision, count))
            {
                CHECK(near_relative(heights[y * COLUMNS + column], count * 1e-9,
                                    1e-12));
            }
        }
    }
    sdfio_reader_free(reader);
    free(file.data);
}

static void check_errors(void)
{
    sdfio_header header;
    describe(&header, SDFIO_ISO_2_0, SDFIO_BINARY, SDFIO_DATA_INT16);
    double heights[ROWS * COLUMNS];
    make_heights(heights);
    const ptrdiff_t stride = COLUMNS * sizeof(double);
    membuf file = {0};
    const sdfio_stream stream = membuf_stream(&file);

    /* Counts need a z-scale: the writer cannot know what they mean. */
    sdfio_writer *writer = sdfio_writer_new();
    int16_t counts[ROWS * COLUMNS] = {0};
    CHECK_OK(sdfio_create_stream(writer, &stream, &header));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_write_data(writer, counts, 1));
    CHECK(strstr(sdfio_writer_error_message(writer), "z-scale") != NULL);
    sdfio_writer_free(writer);

    /* A height that does not fit the z-scale that was asked for. */
    header.z_scale = 1e-12;
    writer = sdfio_writer_new();
    CHECK_OK(sdfio_create_stream(writer, &stream, &header));
    CHECK_STATUS(SDFIO_ERR_RANGE,
                 sdfio_write_heights(writer, sdfio_metres(SDFIO_FLOAT64),
                                     heights, stride));
    sdfio_writer_free(writer);

    /* A height that would be read as a point not measured. */
    header.z_scale = 1.0;
    double marker[1] = {-32768.0};
    header.columns = 1;
    header.rows = 1;
    writer = sdfio_writer_new();
    CHECK_OK(sdfio_create_stream(writer, &stream, &header));
    CHECK_STATUS(SDFIO_ERR_SENTINEL,
                 sdfio_write_heights(writer, sdfio_metres(SDFIO_FLOAT64),
                                     marker, sizeof(double)));
    sdfio_writer_free(writer);

    /* A description that cannot be written. */
    describe(&header, SDFIO_ISO_1_0, SDFIO_BINARY, SDFIO_DATA_INT8);
    writer = sdfio_writer_new();
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_create_stream(writer, &stream, &header));
    sdfio_writer_free(writer);
    describe(&header, SDFIO_ISO_1_0, SDFIO_BINARY, SDFIO_DATA_INT16);
    header.columns = (size_t)UINT16_MAX + 1;
    writer = sdfio_writer_new();
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_create_stream(writer, &stream, &header));
    sdfio_writer_free(writer);

    /* The end before the rows, and a trailer ISO-2.0 cannot have. */
    describe(&header, SDFIO_ISO_2_0, SDFIO_BINARY, SDFIO_DATA_BINARY64);
    writer = sdfio_writer_new();
    CHECK_OK(sdfio_create_stream(writer, &stream, &header));
    CHECK_STATUS(SDFIO_ERR_STATE, sdfio_write_trailer(writer, NULL, 0));
    CHECK_OK(sdfio_write_heights(writer, sdfio_metres(SDFIO_FLOAT64), heights,
                                 stride));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_write_trailer(writer, "free text", 9));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_write_trailer(writer, "caf\xC3\xA9 = 1", 8));
    CHECK_OK(sdfio_write_trailer(writer, "Note = a", 8));
    CHECK_STATUS(SDFIO_ERR_STATE, sdfio_write_trailer(writer, NULL, 0));
    sdfio_writer_free(writer);
    free(file.data);
}

/* The z-scale a writer chooses uses the range of the type. */
static void check_z_scale(void)
{
    const double scale =
        sdfio_z_scale_for_range(SDFIO_DATA_INT16, SDFIO_ISO_2_0, -80e-6, 80e-6);
    CHECK(near_relative(scale, 80e-6 / 32767.0, 1e-12));
    CHECK(sdfio_z_scale_for_range(SDFIO_DATA_BINARY64, SDFIO_ISO_2_0, -1, 1)
          == 1.0);
    CHECK(sdfio_z_scale_for_range(SDFIO_DATA_INT16, SDFIO_ISO_2_0, NAN, NAN)
          == 1.0);
    /* BCR-1.0 keeps the maximum for the marker. */
    CHECK(sdfio_z_scale_for_range(SDFIO_DATA_INT16, SDFIO_BCR_1_0, 0, 32767.0)
          > 1.0);
}

/* A negative row stride, with the pointer at the last row, turns the matrix
 * upside down without a copy, for both directions. */
static void check_stride(void)
{
    sdfio_header header;
    describe(&header, SDFIO_ISO_2_0, SDFIO_BINARY, SDFIO_DATA_BINARY64);
    double heights[ROWS * COLUMNS];
    make_heights(heights);
    const ptrdiff_t stride = COLUMNS * sizeof(double);

    membuf file = {0};
    const sdfio_stream stream = membuf_stream(&file);
    sdfio_writer *writer = sdfio_writer_new();
    CHECK_OK(sdfio_create_stream(writer, &stream, &header));
    CHECK_OK(sdfio_write_heights(writer, sdfio_metres(SDFIO_FLOAT64),
                                 heights + (ROWS - 1) * COLUMNS, -stride));
    CHECK_OK(sdfio_write_trailer(writer, NULL, 0));
    sdfio_writer_free(writer);

    double up[ROWS * COLUMNS];
    double down[ROWS * COLUMNS];
    sdfio_reader *reader = sdfio_reader_new();
    CHECK_OK(sdfio_open_memory(reader, file.data, file.size));
    CHECK_OK(
        sdfio_read_heights(reader, sdfio_metres(SDFIO_FLOAT64), up, stride));
    CHECK_OK(sdfio_rewind(reader));
    CHECK_OK(sdfio_read_heights(reader, sdfio_metres(SDFIO_FLOAT64),
                                down + (ROWS - 1) * COLUMNS, -stride));
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
    CHECK_OK(sdfio_rewind(reader));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_read_heights(reader, sdfio_metres(SDFIO_FLOAT64), up,
                                    (ptrdiff_t)sizeof(double)));
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
    sdfio_header header;
    describe(&header, SDFIO_ISO_2_0, SDFIO_BINARY, SDFIO_DATA_BINARY64);
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
        sdfio_format format = sdfio_metres(SDFIO_FLOAT64);
        const void *source = plain;
        ptrdiff_t row_stride = COLUMNS * sizeof(double);
        if (i == 1)
        {
            format.element_stride = ROWS * sizeof(double);
            source = columns_first;
            row_stride = sizeof(double);
        }
        else if (i == 2)
        {
            format.element_stride = sizeof(point);
            source = &points[0].z;
            row_stride = COLUMNS * sizeof(point);
        }
        sdfio_writer *writer = sdfio_writer_new();
        CHECK_OK(sdfio_create_stream(writer, &stream, &header));
        CHECK_OK(sdfio_write_heights(writer, format, source, row_stride));
        CHECK_OK(sdfio_write_trailer(writer, NULL, 0));
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
    sdfio_reader *reader = sdfio_reader_new();
    CHECK_OK(sdfio_open_memory(reader, from_plain.data, from_plain.size));
    sdfio_format format = sdfio_metres(SDFIO_FLOAT64);
    format.element_stride = ROWS * sizeof(double);
    CHECK_OK(sdfio_read_heights(reader, format, read_columns, sizeof(double)));
    CHECK_OK(sdfio_rewind(reader));
    format.element_stride = sizeof(point);
    CHECK_OK(sdfio_read_heights(reader, format, &read_points[0].z,
                                COLUMNS * sizeof(point)));
    CHECK_OK(sdfio_rewind(reader));
    format.element_stride = -(ptrdiff_t)sizeof(double);
    CHECK_OK(sdfio_read_heights(reader, format, mirrored + COLUMNS - 1,
                                COLUMNS * sizeof(double)));
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
    format = sdfio_metres(SDFIO_FLOAT64);
    CHECK_OK(sdfio_rewind(reader));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_read_heights(reader, format, scratch, sizeof(double)));
    CHECK(scratch[0] == 0.0 && scratch[ROWS * COLUMNS - 1] == 0.0);
    CHECK(strstr(sdfio_reader_error_message(reader), "overlap") != NULL);
    sdfio_reader_free(reader);

    const sdfio_stream stream = membuf_stream(&from_plain);
    sdfio_writer *writer = sdfio_writer_new();
    CHECK_OK(sdfio_create_stream(writer, &stream, &header));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_write_heights(writer, format, plain, sizeof(double)));
    sdfio_writer_free(writer);
    free(from_plain.data);
    free(from_columns.data);
    free(from_points.data);
}

int main(void)
{
    check_element_stride();
    check_stride();
    check_all_types();
    for (int r = SDFIO_ISO_1_0; r <= SDFIO_BCR_1_0; ++r)
    {
        check_rows((sdfio_revision)r, SDFIO_BINARY);
        check_rows((sdfio_revision)r, SDFIO_ASCII);
    }
    check_errors();
    check_z_scale();
    return finish_tests();
}
