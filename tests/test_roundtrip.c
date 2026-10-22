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
                     sdfio_representation representation, sdfio_data_type type)
{
    sdfio_header_init(header);
    header->revision = revision;
    header->representation = representation;
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

static void check_roundtrip(sdfio_revision revision,
                            sdfio_representation representation,
                            sdfio_data_type type)
{
    sdfio_header header;
    describe(&header, revision, representation, type);
    double heights[ROWS * COLUMNS];
    make_heights(heights);

    membuf file = {0};
    const sdfio_stream stream = membuf_stream(&file);
    sdfio_writer *writer = sdfio_writer_new(NULL);
    CHECK_OK(sdfio_create_stream(writer, &stream, &header));
    CHECK_OK(sdfio_write_heights(writer, SDFIO_FLOAT64, heights));
    CHECK_OK(
        sdfio_write_trailer(writer, tagged_trailer, strlen(tagged_trailer)));
    sdfio_writer_free(writer);

    sdfio_reader *reader = sdfio_reader_new(NULL);
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
    CHECK(read->representation == representation);
    CHECK(read->data_type == type);
    CHECK(read->columns == COLUMNS && read->rows == ROWS);
    CHECK(read->x_scale == header.x_scale && read->y_scale == header.y_scale);
    CHECK(read->z_resolution == header.z_resolution);
    CHECK(read->creation_date == header.creation_date);
    CHECK(read->modification_date == header.modification_date);
    CHECK(strcmp(read->manufacturer_id, header.manufacturer_id) == 0);
    CHECK(read->z_scale > 0.0);

    double back[ROWS * COLUMNS];
    CHECK_OK(sdfio_read_heights(reader, SDFIO_FLOAT64, back));
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
        else if (!sdfio_is_floating(type))
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
    CHECK(sdfio_tagged_find(trailer, size, "partname", &value, &value_size)
          && value_size == strlen("Sample")
          && memcmp(value, "Sample", value_size) == 0);

    sdfio_reader_free(reader);
    free(file.data);
}

static void check_all_types(void)
{
    const sdfio_revision revisions[] = {SDFIO_ISO_1_0, SDFIO_ISO_2_0};
    const sdfio_representation representations[] = {SDFIO_BINARY, SDFIO_ASCII};
    for (size_t r = 0; r < sizeof revisions / sizeof revisions[0]; ++r)
    {
        for (size_t s = 0;
             s < sizeof representations / sizeof representations[0]; ++s)
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
                check_roundtrip(revisions[r], representations[s], type);
            }
        }
    }
}

/* The rows API moves the stored values as they are, in the order of the
 * file. */
static void check_rows(sdfio_revision revision,
                       sdfio_representation representation)
{
    sdfio_header header;
    describe(&header, revision, representation, SDFIO_DATA_INT16);
    header.z_scale = 1e-9;
    int16_t counts[ROWS * COLUMNS];
    for (size_t i = 0; i < ROWS * COLUMNS; ++i)
    {
        counts[i] = (int16_t)((int)i * 37 - 100);
    }
    counts[3] = INT16_MIN; /* the marker of a point not measured in ISO */
    counts[4] = INT16_MAX; /* an ordinary count */

    membuf file = {0};
    const sdfio_stream stream = membuf_stream(&file);
    sdfio_writer *writer = sdfio_writer_new(NULL);
    CHECK_OK(sdfio_create_stream(writer, &stream, &header));
    CHECK_OK(sdfio_write_data(writer, counts, 3));
    CHECK_OK(sdfio_write_data(writer, counts + 3 * COLUMNS, ROWS - 3));
    CHECK_OK(sdfio_write_trailer(writer, NULL, 0));
    sdfio_writer_free(writer);

    sdfio_reader *reader = sdfio_reader_new(NULL);
    CHECK_OK(sdfio_open_memory(reader, file.data, file.size));
    int16_t back[ROWS * COLUMNS];
    CHECK_OK(sdfio_read_data(reader, back, 2));
    CHECK_OK(sdfio_read_data(reader, back + 2 * COLUMNS, ROWS - 2));
    CHECK(memcmp(counts, back, sizeof counts) == 0);
    const char *trailer = (const char *)1;
    size_t size = 1;
    CHECK_OK(sdfio_read_trailer(reader, &trailer, &size));
    /* A text file closes the trailer record, so it has an empty one. */
    CHECK(representation == SDFIO_ASCII ? (trailer != NULL && size == 0)
                                        : (trailer == NULL && size == 0));

    /* The heights are in the order of the file, which is the order of y. */
    reader = membuf_reopen(reader, &file);
    double heights[ROWS * COLUMNS];
    CHECK_OK(sdfio_read_heights(reader, SDFIO_FLOAT64, heights));
    for (size_t file_row = 0; file_row < ROWS; ++file_row)
    {
        const size_t y = file_row;
        for (size_t column = 5; column < COLUMNS; ++column)
        {
            const int16_t count = counts[file_row * COLUMNS + column];
            if (!sdfio_is_bad_value(SDFIO_DATA_INT16, count))
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
    membuf file = {0};
    const sdfio_stream stream = membuf_stream(&file);

    /* Counts need a z-scale: the writer cannot know what they mean. */
    sdfio_writer *writer = sdfio_writer_new(NULL);
    int16_t counts[ROWS * COLUMNS] = {0};
    CHECK_OK(sdfio_create_stream(writer, &stream, &header));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_write_data(writer, counts, 1));
    CHECK(strstr(sdfio_writer_error_message(writer), "z-scale") != NULL);
    sdfio_writer_free(writer);

    /* A height that does not fit the z-scale that was asked for. */
    header.z_scale = 1e-12;
    writer = sdfio_writer_new(NULL);
    CHECK_OK(sdfio_create_stream(writer, &stream, &header));
    CHECK_STATUS(SDFIO_ERR_RANGE,
                 sdfio_write_heights(writer, SDFIO_FLOAT64, heights));
    sdfio_writer_free(writer);

    /* A height that would be read as a point not measured. */
    header.z_scale = 1.0;
    double marker[1] = {-32768.0};
    header.columns = 1;
    header.rows = 1;
    writer = sdfio_writer_new(NULL);
    CHECK_OK(sdfio_create_stream(writer, &stream, &header));
    CHECK_STATUS(SDFIO_ERR_BAD_VALUE,
                 sdfio_write_heights(writer, SDFIO_FLOAT64, marker));
    sdfio_writer_free(writer);

    /* A description that cannot be written. */
    describe(&header, SDFIO_ISO_1_0, SDFIO_BINARY, SDFIO_DATA_INT8);
    writer = sdfio_writer_new(NULL);
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_create_stream(writer, &stream, &header));
    sdfio_writer_free(writer);
    describe(&header, SDFIO_ISO_1_0, SDFIO_BINARY, SDFIO_DATA_INT16);
    header.columns = (size_t)UINT16_MAX + 1;
    writer = sdfio_writer_new(NULL);
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_create_stream(writer, &stream, &header));
    sdfio_writer_free(writer);

    /* The end before the rows, and a trailer ISO-2.0 cannot have. */
    describe(&header, SDFIO_ISO_2_0, SDFIO_BINARY, SDFIO_DATA_BINARY64);
    writer = sdfio_writer_new(NULL);
    CHECK_OK(sdfio_create_stream(writer, &stream, &header));
    CHECK_STATUS(SDFIO_ERR_STATE, sdfio_write_trailer(writer, NULL, 0));
    CHECK_OK(sdfio_write_heights(writer, SDFIO_FLOAT64, heights));
    /* The text of a trailer is not looked at; only a missing one is refused. */
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_write_trailer(writer, NULL, 5));
    CHECK_OK(sdfio_write_trailer(writer, "free text", 9));
    CHECK_STATUS(SDFIO_ERR_STATE, sdfio_write_trailer(writer, NULL, 0));
    sdfio_writer_free(writer);
    free(file.data);
}

/* The z-scale a writer chooses uses the range of the type. */
static void check_z_scale(void)
{
    const double scale =
        sdfio_z_scale_for_range(SDFIO_DATA_INT16, -80e-6, 80e-6);
    CHECK(near_relative(scale, 80e-6 / 32767.0, 1e-12));
    CHECK(sdfio_z_scale_for_range(SDFIO_DATA_BINARY64, -1, 1) == 1.0);
    CHECK(sdfio_z_scale_for_range(SDFIO_DATA_INT16, NAN, NAN) == 1.0);
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
    sdfio_writer *writer = sdfio_writer_new(NULL);
    CHECK_OK(sdfio_create_stream(writer, &stream, &header));
    CHECK_OK(sdfio_write_heights_strided(
        writer, format_with_rows(SDFIO_FLOAT64, -stride),
        heights + (ROWS - 1) * COLUMNS));
    CHECK_OK(sdfio_write_trailer(writer, NULL, 0));
    sdfio_writer_free(writer);

    double up[ROWS * COLUMNS];
    double down[ROWS * COLUMNS];
    sdfio_reader *reader = sdfio_reader_new(NULL);
    CHECK_OK(sdfio_open_memory(reader, file.data, file.size));
    CHECK_OK(sdfio_read_heights(reader, SDFIO_FLOAT64, up));
    reader = membuf_reopen(reader, &file);
    CHECK_OK(sdfio_read_heights_strided(
        reader, format_with_rows(SDFIO_FLOAT64, -stride),
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
                 sdfio_read_heights_strided(
                     reader,
                     format_with_rows(SDFIO_FLOAT64, (ptrdiff_t)sizeof(double)),
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
        sdfio_format format = sdfio_format_packed(SDFIO_FLOAT64);
        const void *source = plain;
        if (i == 1)
        {
            format.element_stride = ROWS * sizeof(double);
            format.row_stride = sizeof(double);
            source = columns_first;
        }
        else if (i == 2)
        {
            format.element_stride = sizeof(point);
            format.row_stride = COLUMNS * sizeof(point);
            source = &points[0].z;
        }
        sdfio_writer *writer = sdfio_writer_new(NULL);
        CHECK_OK(sdfio_create_stream(writer, &stream, &header));
        CHECK_OK(sdfio_write_heights_strided(writer, format, source));
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
    sdfio_reader *reader = sdfio_reader_new(NULL);
    CHECK_OK(sdfio_open_memory(reader, from_plain.data, from_plain.size));
    sdfio_format format = sdfio_format_packed(SDFIO_FLOAT64);
    format.element_stride = ROWS * sizeof(double);
    format.row_stride = sizeof(double);
    CHECK_OK(sdfio_read_heights_strided(reader, format, read_columns));
    reader = membuf_reopen(reader, &from_plain);
    format.element_stride = sizeof(point);
    format.row_stride = COLUMNS * sizeof(point);
    CHECK_OK(sdfio_read_heights_strided(reader, format, &read_points[0].z));
    reader = membuf_reopen(reader, &from_plain);
    format.element_stride = -(ptrdiff_t)sizeof(double);
    format.row_stride = COLUMNS * sizeof(double);
    CHECK_OK(
        sdfio_read_heights_strided(reader, format, mirrored + COLUMNS - 1));
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
    format = format_with_rows(SDFIO_FLOAT64, sizeof(double));
    reader = membuf_reopen(reader, &from_plain);
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_read_heights_strided(reader, format, scratch));
    CHECK(scratch[0] == 0.0 && scratch[ROWS * COLUMNS - 1] == 0.0);
    CHECK(strstr(sdfio_reader_error_message(reader), "overlap") != NULL);
    sdfio_reader_free(reader);

    const sdfio_stream stream = membuf_stream(&from_plain);
    sdfio_writer *writer = sdfio_writer_new(NULL);
    CHECK_OK(sdfio_create_stream(writer, &stream, &header));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_write_heights_strided(writer, format, plain));
    sdfio_writer_free(writer);
    free(from_plain.data);
    free(from_columns.data);
    free(from_points.data);
}

/* An unknown z-resolution is -1, as the standard has it: whatever a program
 * gives that is not positive is written as -1, and whatever a file holds that
 * is not positive is read as -1. */
static void write_with_resolution(membuf *file,
                                  sdfio_representation representation,
                                  double resolution)
{
    sdfio_header header;
    describe(&header, SDFIO_ISO_2_0, representation, SDFIO_DATA_BINARY64);
    header.z_resolution = resolution;
    double heights[ROWS * COLUMNS];
    make_heights(heights);
    const sdfio_stream stream = membuf_stream(file);
    sdfio_writer *writer = sdfio_writer_new(NULL);
    CHECK_OK(sdfio_create_stream(writer, &stream, &header));
    CHECK_OK(sdfio_write_heights(writer, SDFIO_FLOAT64, heights));
    CHECK_OK(sdfio_write_trailer(writer, NULL, 0));
    sdfio_writer_free(writer);
}

static double resolution_read(const membuf *file)
{
    sdfio_reader *reader = sdfio_reader_new(NULL);
    double resolution = 0.0;
    CHECK_OK(sdfio_open_memory(reader, file->data, file->size));
    if (sdfio_get_header(reader) != NULL)
    {
        resolution = sdfio_get_header(reader)->z_resolution;
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
    sdfio_header initial;
    sdfio_header_init(&initial);
    CHECK(SDFIO_NO_RESOLUTION == -1.0);
    CHECK(initial.z_resolution == SDFIO_NO_RESOLUTION);

    const double given[] = {SDFIO_NO_RESOLUTION, 0.0, -5.0, 1e-9};
    const sdfio_representation representations[] = {SDFIO_BINARY, SDFIO_ASCII};
    for (size_t s = 0; s < 2; ++s)
    {
        for (size_t g = 0; g < sizeof given / sizeof given[0]; ++g)
        {
            membuf file = {0};
            write_with_resolution(&file, representations[s], given[g]);
            const double expected =
                given[g] > 0.0 ? given[g] : SDFIO_NO_RESOLUTION;
            CHECK(resolution_read(&file) == expected);
            if (representations[s] == SDFIO_BINARY)
            {
                /* Four doubles and three bytes end the header. */
                const double stored = double_in_file(
                    file.data + sdfio_binary_header_size(SDFIO_ISO_2_0) - 3
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
        put_double_in_file(file.data + sdfio_binary_header_size(SDFIO_ISO_2_0)
                               - 3 - sizeof(double),
                           stored_values[i]);
        CHECK(resolution_read(&file) == SDFIO_NO_RESOLUTION);
        free(file.data);
    }

    /* A number that is no number is refused. */
    const double refused[] = {NAN, INFINITY, -INFINITY};
    for (size_t i = 0; i < sizeof refused / sizeof refused[0]; ++i)
    {
        sdfio_header header;
        describe(&header, SDFIO_ISO_2_0, SDFIO_BINARY, SDFIO_DATA_BINARY64);
        header.z_resolution = refused[i];
        membuf file = {0};
        const sdfio_stream stream = membuf_stream(&file);
        sdfio_writer *writer = sdfio_writer_new(NULL);
        CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                     sdfio_create_stream(writer, &stream, &header));
        sdfio_writer_free(writer);
        free(file.data);
    }
}

/* Rows without a column hold nothing, and a file that declares billions of
 * them is written and read in an instant. */
static void check_rows_without_columns(void)
{
    sdfio_header header;
    describe(&header, SDFIO_ISO_2_0, SDFIO_BINARY, SDFIO_DATA_BINARY32);
    header.columns = 0;
    header.rows = sdfio_max_binary_count(SDFIO_ISO_2_0);

    membuf file = {0};
    const sdfio_stream stream = membuf_stream(&file);
    sdfio_writer *writer = sdfio_writer_new(NULL);
    CHECK_OK(sdfio_create_stream(writer, &stream, &header));
    CHECK_OK(sdfio_write_data(writer, NULL, header.rows));
    CHECK_OK(sdfio_write_trailer(writer, "a = 1\n", 6));
    sdfio_writer_free(writer);

    sdfio_reader *reader = sdfio_reader_new(NULL);
    CHECK_OK(sdfio_open_memory(reader, file.data, file.size));
    CHECK_OK(sdfio_read_data(reader, NULL, header.rows));
    const char *trailer = NULL;
    size_t trailer_size = 0;
    CHECK_OK(sdfio_read_trailer(reader, &trailer, &trailer_size));
    CHECK(trailer_size == 5 && memcmp(trailer, "a = 1", 5) == 0);
    sdfio_reader_free(reader);
    free(file.data);
}

/* What a header starts as: a binary ISO-2.0 file of single precision floats,
 * which holds a float surface without loss and needs no z-scale. */
static void check_defaults(void)
{
    sdfio_header header;
    sdfio_header_init(&header);
    CHECK(header.revision == SDFIO_ISO_2_0);
    CHECK(header.representation == SDFIO_BINARY);
    CHECK(header.data_type == SDFIO_DATA_BINARY32);
    CHECK(header.z_scale == SDFIO_AUTO_Z_SCALE);
    CHECK(header.columns == 0 && header.rows == 0);

    /* A float surface written with the defaults comes back as it was. */
    header.columns = 3;
    header.rows = 2;
    const float heights[6] = {1e-6F, -2.5e-7F, NAN, 3e-7F, 0.0F, 7.5e-6F};
    membuf file = {0};
    const sdfio_stream stream = membuf_stream(&file);
    sdfio_writer *writer = sdfio_writer_new(NULL);
    CHECK_OK(sdfio_create_stream(writer, &stream, &header));
    CHECK_OK(sdfio_write_heights(writer, SDFIO_FLOAT32, heights));
    CHECK_OK(sdfio_write_trailer(writer, NULL, 0));
    sdfio_writer_free(writer);
    float back[6];
    sdfio_reader *reader = sdfio_reader_new(NULL);
    CHECK_OK(sdfio_open_memory(reader, file.data, file.size));
    CHECK_OK(sdfio_read_heights(reader, SDFIO_FLOAT32, back));
    for (size_t i = 0; i < 6; ++i)
    {
        CHECK(isnan(heights[i]) ? isnan(back[i]) : back[i] == heights[i]);
    }
    CHECK(sdfio_get_header(reader)->z_scale == 1.0);
    sdfio_reader_free(reader);
    free(file.data);
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
        {"A = 1\rB = 2", "A = 1\r\nB = 2\r\n", "A = 1\nB = 2"},
        {"A = 1\r\nB = 2", "A = 1\r\nB = 2\r\n", "A = 1\nB = 2"},
        {"A = 1\nB = 2\n", "A = 1\r\nB = 2\r\n", "A = 1\nB = 2"},
        {"A = 1\r\nB = 2\nC = 3", "A = 1\r\nB = 2\r\nC = 3\r\n",
         "A = 1\nB = 2\nC = 3"},
        {"A = 1\n\nB = 2", "A = 1\r\n\r\nB = 2\r\n", "A = 1\n\nB = 2"},
        {"A = 1", "A = 1\r\n", "A = 1"},
    };
    const sdfio_representation representations[] = {SDFIO_BINARY, SDFIO_ASCII};
    for (size_t s = 0; s < 2; ++s)
    {
        for (size_t v = 0; v < sizeof variants / sizeof variants[0]; ++v)
        {
            sdfio_header header;
            describe(&header, SDFIO_ISO_2_0, representations[s],
                     SDFIO_DATA_BINARY64);
            header.columns = 1;
            header.rows = 1;
            const double height = 1e-6;
            membuf file = {0};
            const sdfio_stream stream = membuf_stream(&file);
            sdfio_writer *writer = sdfio_writer_new(NULL);
            CHECK_OK(sdfio_create_stream(writer, &stream, &header));
            CHECK_OK(sdfio_write_heights(writer, SDFIO_FLOAT64, &height));
            CHECK_OK(sdfio_write_trailer(writer, variants[v].written,
                                         strlen(variants[v].written)));
            sdfio_writer_free(writer);

            const size_t data_at =
                sdfio_binary_header_size(SDFIO_ISO_2_0) + sizeof(double);
            if (representations[s] == SDFIO_BINARY)
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

            sdfio_reader *reader = sdfio_reader_new(NULL);
            CHECK_OK(sdfio_open_memory(reader, file.data, file.size));
            const char *trailer = NULL;
            size_t size = 0;
            CHECK_OK(sdfio_read_trailer(reader, &trailer, &size));
            CHECK(trailer != NULL && size == strlen(variants[v].read)
                  && memcmp(trailer, variants[v].read, size) == 0);
            sdfio_reader_free(reader);
            free(file.data);
        }
    }
}

/* A text file is read whatever ends its lines: CR LF as ISO has it, LF of Unix,
 * or the CR of old Macintosh files. */
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
    const char *const line_ends[] = {"\n", "\r"};
    for (size_t r = 0; r < 2; ++r)
    {
        sdfio_header header;
        describe(&header, revisions[r], SDFIO_ASCII, SDFIO_DATA_INT16);
        double heights[ROWS * COLUMNS];
        make_heights(heights);
        membuf crlf = {0};
        const sdfio_stream stream = membuf_stream(&crlf);
        sdfio_writer *writer = sdfio_writer_new(NULL);
        CHECK_OK(sdfio_create_stream(writer, &stream, &header));
        CHECK_OK(sdfio_write_heights(writer, SDFIO_FLOAT64, heights));
        CHECK_OK(sdfio_write_trailer(writer, tagged_trailer,
                                     strlen(tagged_trailer)));
        sdfio_writer_free(writer);

        double expected[ROWS * COLUMNS];
        sdfio_reader *reader = sdfio_reader_new(NULL);
        CHECK_OK(sdfio_open_memory(reader, crlf.data, crlf.size));
        CHECK_OK(sdfio_read_heights(reader, SDFIO_FLOAT64, expected));
        sdfio_reader_free(reader);

        for (size_t e = 0; e < 2; ++e)
        {
            membuf other = with_line_ends(&crlf, line_ends[e]);
            CHECK(other.size < crlf.size);
            double read[ROWS * COLUMNS];
            reader = sdfio_reader_new(NULL);
            CHECK_OK(sdfio_open_memory(reader, other.data, other.size));
            const sdfio_header *got = sdfio_get_header(reader);
            CHECK(got != NULL && got->columns == COLUMNS && got->rows == ROWS
                  && got->x_scale == header.x_scale
                  && got->creation_date == header.creation_date);
            CHECK_OK(sdfio_read_heights(reader, SDFIO_FLOAT64, read));
            for (size_t i = 0; i < ROWS * COLUMNS; ++i)
            {
                CHECK(isnan(expected[i]) ? isnan(read[i])
                                         : read[i] == expected[i]);
            }
            const char *trailer = NULL;
            size_t size = 0;
            CHECK_OK(sdfio_read_trailer(reader, &trailer, &size));
            CHECK(trailer != NULL && size == strlen(tagged_trailer)
                  && memcmp(trailer, tagged_trailer, size) == 0);
            sdfio_reader_free(reader);
            free(other.data);
        }
        free(crlf.data);
    }
}

int main(void)
{
    check_read_line_ends();
    check_trailer_line_ends();
    check_rows_without_columns();
    check_defaults();
    check_resolution();
    check_element_stride();
    check_stride();
    check_all_types();
    for (int r = SDFIO_ISO_1_0; r <= SDFIO_ISO_2_0; ++r)
    {
        check_rows((sdfio_revision)r, SDFIO_BINARY);
        check_rows((sdfio_revision)r, SDFIO_ASCII);
    }
    check_errors();
    check_z_scale();
    return finish_tests();
}
