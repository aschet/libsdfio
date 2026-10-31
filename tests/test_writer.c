/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

#include "check.h"
#include <limits.h>
#include <string.h>

#define COLUMNS 2
#define ROWS 2
#define ESTIMATE_ROWS 5
#define ESTIMATE_COLUMNS 9

#define OK SDFIO_OK
#define STATE SDFIO_ERR_STATE
#define ARGUMENT SDFIO_ERR_ARGUMENT

typedef enum writer_state
{
    WRITER_NEW,
    WRITER_CREATED,
    WRITER_PARTIAL,
    WRITER_FULL,
    WRITER_TRAILER,
    WRITER_ENDED,
    WRITER_FAILED,
    WRITER_STATES
} writer_state;

typedef enum writer_call
{
    WRITER_ADD,
    WRITER_CHUNK,
    WRITER_CREATE,
    WRITER_ROW,
    WRITER_TOO_MANY,
    WRITER_HEIGHTS,
    WRITER_FIELD,
    WRITER_END,
    WRITER_CALLS
} writer_call;

static const sdfio_status writer_results[WRITER_STATES][WRITER_CALLS] = {
    [WRITER_NEW] = {OK, OK, OK, STATE, STATE, STATE, STATE, STATE},
    [WRITER_CREATED] = {STATE, STATE, STATE, OK, ARGUMENT, OK, STATE, STATE},
    [WRITER_PARTIAL] = {STATE, STATE, STATE, OK, ARGUMENT, STATE, STATE, STATE},
    [WRITER_FULL] = {STATE, STATE, STATE, ARGUMENT, ARGUMENT, STATE, OK, OK},
    [WRITER_TRAILER] = {STATE, STATE, STATE, ARGUMENT, ARGUMENT, STATE, OK, OK},
    [WRITER_ENDED] = {STATE, STATE, STATE, STATE, STATE, STATE, STATE, STATE},
    [WRITER_FAILED] = {STATE, STATE, STATE, STATE, STATE, STATE, STATE, STATE}};

static const float state_values[ROWS * COLUMNS] = {1.0F, 2.0F, 3.0F, 4.0F};

static size_t refusing_write(void *user, const void *data, size_t size)
{
    (void)user;
    (void)data;
    (void)size;
    return 0;
}

static sdfio_status write_rows(sdfio_writer *writer, size_t rows)
{
    return sdfio_write_data(writer, sdfio_layout_packed(SDFIO_TYPE_FLOAT),
                            state_values, rows);
}

/* Returns a writer in the state. The stream writes to memory, or refuses every
 * byte for the failed state. */
static sdfio_writer *writer_in(writer_state state, membuf *file)
{
    sdfio_writer *writer = NULL;
    SETUP(sdfio_writer_create(sdfio_default_allocator(), &writer));
    SETUP(sdfio_writer_add_binary(writer));
    if (state == WRITER_FAILED)
    {
        SETUP(sdfio_writer_set_buffer(writer, 1));
    }
    if (state == WRITER_NEW)
    {
        return writer;
    }
    sdfio_stream stream = membuf_stream(file);
    if (state == WRITER_FAILED)
    {
        stream.write = refusing_write;
    }
    CHECK_OK(sdfio_create_stream(writer, &stream, small_header()));
    switch (state)
    {
    case WRITER_PARTIAL:
        CHECK_OK(write_rows(writer, 1));
        break;
    case WRITER_FULL:
    case WRITER_TRAILER:
    case WRITER_ENDED:
        CHECK_OK(write_rows(writer, ROWS));
        break;
    case WRITER_FAILED:
        CHECK_STATUS(SDFIO_ERR_IO, write_rows(writer, 1));
        break;
    case WRITER_NEW:
    case WRITER_CREATED:
    case WRITER_STATES:
        break;
    }
    if (state == WRITER_TRAILER)
    {
        CHECK_OK(sdfio_write_trailer_field(writer, "Name", "value"));
    }
    if (state == WRITER_ENDED)
    {
        CHECK_OK(sdfio_end_file(writer));
    }
    return writer;
}

static sdfio_status writer_call_in(sdfio_writer *writer, writer_call call,
                                   membuf *file)
{
    const sdfio_stream stream = membuf_stream(file);
    switch (call)
    {
    case WRITER_ADD:
        return sdfio_writer_add_ascii(writer);
    case WRITER_CHUNK:
        return sdfio_writer_set_chunk(writer, 16);
    case WRITER_CREATE:
        return sdfio_create_stream(writer, &stream, small_header());
    case WRITER_ROW:
        return write_rows(writer, 1);
    case WRITER_TOO_MANY:
        return write_rows(writer, ROWS + 1);
    case WRITER_HEIGHTS:
        return sdfio_write_heights(
            writer, sdfio_layout_packed(SDFIO_TYPE_FLOAT), state_values);
    case WRITER_FIELD:
        return sdfio_write_trailer_field(writer, "Name", "value");
    case WRITER_END:
        return sdfio_end_file(writer);
    case WRITER_CALLS:
        break;
    }
    return SDFIO_OK;
}

static void check_arguments(void)
{
    const sdfio_allocator broken = {NULL, counting_allocate, NULL};
    sdfio_writer *none = NULL;
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_writer_create(NULL, &none));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_writer_create(&broken, &none));
    CHECK(none == NULL);
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_writer_create(sdfio_default_allocator(), NULL));

    sdfio_writer *writer = NULL;
    SETUP(sdfio_writer_create(sdfio_default_allocator(), &writer));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_writer_set_chunk(writer, (size_t)-1));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_writer_set_buffer(writer, 0));
    SETUP(sdfio_writer_add_binary(writer));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_writer_add_binary(writer));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_writer_add_ascii(NULL));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_writer_set_time_zone(NULL, NULL));
    sdfio_writer_free(writer);

    sdfio_header *header = new_header();
    SETUP(sdfio_header_set_columns(header, 1));
    SETUP(sdfio_header_set_rows(header, 1));
    SETUP(sdfio_header_set_created(header, SDFIO_NO_TIME));
    membuf sink = {0};
    const sdfio_stream stream = membuf_stream(&sink);
    writer = new_writer(all_formats());
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_create_stream(writer, &stream, header));
    CHECK(strstr(sdfio_writer_error_message(writer), "SDFIO_NO_TIME") != NULL);
    SETUP(sdfio_header_set_created(header, 0));
    SETUP(sdfio_header_set_modified(header, SDFIO_NO_TIME));
    CHECK_OK(sdfio_create_stream(writer, &stream, header));
    sdfio_writer_free(writer);
    free(sink.data);

    SETUP(sdfio_header_set_format(header, SDFIO_ASCII));
    writer = new_writer(all_formats());
    sink = (membuf){0};
    CHECK_OK(sdfio_create_stream(writer, &stream, header));
    sdfio_writer_free(writer);
    free(sink.data);
    sink = (membuf){0};
    const test_options binary_only = {.writer_formats = {TEST_BINARY}};
    writer = new_writer(&binary_only);
    CHECK_STATUS(SDFIO_ERR_UNSUPPORTED,
                 sdfio_create_stream(writer, &stream, header));
    sdfio_writer_free(writer);

    SETUP(sdfio_writer_create(sdfio_default_allocator(), &writer));
    CHECK_STATUS(SDFIO_ERR_UNSUPPORTED,
                 sdfio_create_stream(writer, &stream, header));
    sdfio_writer_free(writer);
    free(sink.data);
}

static void check_calls(void)
{
    for (int state = 0; state < WRITER_STATES; ++state)
    {
        for (int call = 0; call < WRITER_CALLS; ++call)
        {
            membuf file = {0};
            sdfio_writer *writer = writer_in((writer_state)state, &file);
            const sdfio_status status =
                writer_call_in(writer, (writer_call)call, &file);
            if (status != writer_results[state][call])
            {
                fprintf(stderr, "writer state %d, call %d: %s, expected %s\n",
                        state, call, sdfio_status_string(status),
                        sdfio_status_string(writer_results[state][call]));
                ++failures;
            }
            ++checks;
            sdfio_writer_free(writer);
            free(file.data);
        }
    }
}

static void check_refusals(void)
{
    membuf file = {0};
    sdfio_writer *writer = writer_in(WRITER_CREATED, &file);
    CHECK_STATUS(STATE, sdfio_end_file(writer));
    CHECK_STATUS(ARGUMENT, write_rows(writer, ROWS + 1));
    CHECK_OK(write_rows(writer, 1));
    CHECK_STATUS(STATE, sdfio_write_trailer_field(writer, "Name", "value"));
    CHECK_OK(write_rows(writer, 1));
    CHECK_STATUS(ARGUMENT, sdfio_write_trailer_field(writer, "Bad Name", "v"));
    CHECK_OK(sdfio_write_trailer_field(writer, "Name", "value"));
    CHECK_OK(sdfio_end_file(writer));
    sdfio_writer_free(writer);
    free(file.data);
}

/* A failed create leaves the writer new. */
static void check_retry(void)
{
    sdfio_writer *writer = new_writer(all_formats());
    sdfio_header *header = small_header();
    membuf file = {0};
    const sdfio_stream stream = membuf_stream(&file);
    SETUP(sdfio_header_set_created(header, SDFIO_NO_TIME));
    CHECK_STATUS(ARGUMENT, sdfio_create_stream(writer, &stream, header));
    SETUP(sdfio_header_set_created(header, 1772600760));
    CHECK_OK(sdfio_create_stream(writer, &stream, header));
    sdfio_writer_free(writer);
    free(file.data);
}

/* Limits across several fields are checked when a file is created. */
static void check_cross_field(void)
{
    sdfio_header *header = new_header();
    SETUP(sdfio_header_set_revision(header, SDFIO_ISO_1_0));
    SETUP(sdfio_header_set_data_type(header, SDFIO_TYPE_INT8));
    membuf file = {0};
    const sdfio_stream stream = membuf_stream(&file);
    sdfio_writer *writer = new_writer(all_formats());
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_create_stream(writer, &stream, header));
    SETUP(sdfio_header_set_data_type(header, SDFIO_TYPE_INT16));
    SETUP(sdfio_header_set_columns(header, (uint32_t)UINT16_MAX + 1));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_create_stream(writer, &stream, header));
    SETUP(sdfio_header_set_columns(header, 1));
    SETUP(sdfio_header_set_rows(header, 1));
    SETUP(sdfio_header_set_created(header, SDFIO_NO_TIME));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_create_stream(writer, &stream, header));
    sdfio_writer_free(writer);
    free(file.data);
}

static size_t half_write(void *user, const void *data, size_t size)
{
    (void)user;
    (void)data;
    return size / 2;
}

static void check_failing_sink(void)
{
    sdfio_header *header = new_header();
    SETUP(sdfio_header_set_columns(header, 4));
    SETUP(sdfio_header_set_rows(header, 4));
    const sdfio_stream stream = {NULL, NULL, half_write};
    sdfio_writer *writer = new_writer(all_formats());
    CHECK_OK(sdfio_create_stream(writer, &stream, header));
    double heights[16] = {0};
    /* A small file is buffered: the failure shows when it is flushed. */
    SETUP(sdfio_write_heights(writer, sdfio_layout_packed(SDFIO_TYPE_DOUBLE),
                              heights));
    CHECK_STATUS(SDFIO_ERR_IO, sdfio_end_file(writer));
    CHECK_STATUS(SDFIO_ERR_STATE, sdfio_end_file(writer));
    sdfio_writer_free(writer);
}

#define LONGEST_MANUFACTURER "ABCDEFGHIJ"

/* Fills `count` values with the longest ASCII text of the type: the smallest
 * integer other than the bad value, or a negative number with the most digits.
 */
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
    SETUP(sdfio_header_set_revision(header, revision));
    SETUP(sdfio_header_set_format(header, format));
    SETUP(sdfio_header_set_data_type(header, type));
    SETUP(sdfio_header_set_columns(header, ESTIMATE_COLUMNS));
    SETUP(sdfio_header_set_rows(header, ESTIMATE_ROWS));
    SETUP(sdfio_header_set_x_scale(header, 1.2345678901234e-300));
    SETUP(sdfio_header_set_y_scale(header, 1.2345678901234e+300));
    SETUP(sdfio_header_set_z_scale(header, 1.2345678901234e-300));
    SETUP(sdfio_header_set_z_resolution(header, 1.2345678901234e-300));
    SETUP(sdfio_header_set_manufacturer(header, LONGEST_MANUFACTURER));
}

static void check_estimate(sdfio_revision revision, sdfio_format format,
                           sdfio_data_type type)
{
    sdfio_header *header = new_header();
    describe(header, revision, format, type);
    sdfio_writer *writer = new_writer(all_formats());
    size_t estimate = 0;
    CHECK_OK(sdfio_writer_estimate_size(writer, header, &estimate));

    unsigned char values[ESTIMATE_COLUMNS * sizeof(double)];
    fill_longest(type, values, ESTIMATE_COLUMNS);
    membuf file = {0};
    const sdfio_stream stream = membuf_stream(&file);
    SETUP(sdfio_create_stream(writer, &stream, header));
    const sdfio_layout layout = *sdfio_layout_packed(type);
    for (size_t row = 0; row < ESTIMATE_ROWS; ++row)
    {
        SETUP(sdfio_write_data(writer, &layout, values, 1));
    }
    SETUP(sdfio_end_file(writer));
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

static void check_estimate_errors(void)
{
    sdfio_header *header = new_header();
    SETUP(sdfio_header_set_columns(header, ESTIMATE_COLUMNS));
    SETUP(sdfio_header_set_rows(header, ESTIMATE_ROWS));
    sdfio_writer *writer = new_writer(all_formats());
    size_t size = 0;
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_writer_estimate_size(NULL, header, &size));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_writer_estimate_size(writer, NULL, &size));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_writer_estimate_size(writer, header, NULL));

    sdfio_header *no_dates = NULL;
    SETUP(sdfio_header_create(sdfio_default_allocator(), &no_dates));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_writer_estimate_size(writer, no_dates, &size));
    sdfio_header_free(no_dates);
    sdfio_writer_free(writer);

    sdfio_writer *binary_only = NULL;
    SETUP(sdfio_writer_create(sdfio_default_allocator(), &binary_only));
    SETUP(sdfio_writer_add_binary(binary_only));
    SETUP(sdfio_header_set_format(header, SDFIO_ASCII));
    CHECK_STATUS(SDFIO_ERR_UNSUPPORTED,
                 sdfio_writer_estimate_size(binary_only, header, &size));
    sdfio_writer_free(binary_only);
}

/* A grid whose size in bytes exceeds size_t. Where size_t has 32 bits, the
 * header is already invalid. */
static void check_estimate_overflow(sdfio_format format)
{
    sdfio_header *header = new_header();
    SETUP(sdfio_header_set_revision(header, SDFIO_ISO_2_0));
    SETUP(sdfio_header_set_format(header, format));
    SETUP(sdfio_header_set_data_type(header, SDFIO_TYPE_DOUBLE));
    SETUP(sdfio_header_set_columns(header, UINT32_MAX));
    SETUP(sdfio_header_set_rows(header, UINT32_MAX));
    sdfio_writer *writer = new_writer(all_formats());
    size_t size = 0;
    const sdfio_status status =
        sdfio_writer_estimate_size(writer, header, &size);
    CHECK(status == SDFIO_ERR_RANGE || status == SDFIO_ERR_ARGUMENT);
    sdfio_writer_free(writer);
}

static void check_estimate_matrix(void)
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
                if (revision_permits(revisions[r], types[t]))
                {
                    check_estimate(revisions[r], formats[f], types[t]);
                }
            }
        }
    }
}
static bool refuse_time(void *user, time_t utc, sdfio_date *local)
{
    (void)user;
    (void)utc;
    (void)local;
    return false;
}

/* A header that cannot be written fails the writer, also when the end of the
 * file writes it, and a second attempt does not end the file. */
static void check_failed_header(void)
{
    sdfio_header *header = new_header();
    SETUP(sdfio_header_set_revision(header, SDFIO_ISO_1_0));
    SETUP(sdfio_header_set_data_type(header, SDFIO_TYPE_INT16));
    SETUP(sdfio_header_set_columns(header, 3));
    SETUP(sdfio_header_set_rows(header, 0));
    SETUP(sdfio_header_set_z_scale(header, 1.0));
    const sdfio_time_zone zone = {NULL, NULL, refuse_time};
    const sdfio_format formats[] = {SDFIO_BINARY, SDFIO_ASCII};
    for (size_t f = 0; f < 2; ++f)
    {
        SETUP(sdfio_header_set_format(header, formats[f]));
        membuf file = {0};
        const sdfio_stream stream = membuf_stream(&file);
        sdfio_writer *writer = new_writer(all_formats());
        SETUP(sdfio_writer_set_time_zone(writer, &zone));
        SETUP(sdfio_create_stream(writer, &stream, header));
        CHECK_STATUS(SDFIO_ERR_RANGE, sdfio_end_file(writer));
        CHECK_STATUS(SDFIO_ERR_STATE, sdfio_end_file(writer));
        CHECK_STATUS(SDFIO_ERR_STATE,
                     sdfio_write_trailer_field(writer, "Name", "value"));
        CHECK(file.size == 0);
        sdfio_writer_free(writer);
        free(file.data);
    }
}

static void check_missing_modification(sdfio_revision revision,
                                       sdfio_format format)
{
    sdfio_header *header = new_header();
    SETUP(sdfio_header_set_revision(header, revision));
    SETUP(sdfio_header_set_format(header, format));
    SETUP(sdfio_header_set_data_type(header, SDFIO_TYPE_DOUBLE));
    SETUP(sdfio_header_set_columns(header, 1));
    SETUP(sdfio_header_set_rows(header, 1));
    SETUP(sdfio_header_set_created(header, 1772600760));
    SETUP(sdfio_header_set_modified(header, SDFIO_NO_TIME));
    const double height = 1e-6F;
    membuf file = {0};
    write_memory_file(&file, header, sdfio_layout_packed(SDFIO_TYPE_DOUBLE),
                      &height);
    sdfio_reader *reader = open_memory_file(&file);
    const sdfio_header *read = sdfio_reader_header(reader);
    CHECK(read != NULL && sdfio_header_modified(read) == 1772600760);
    sdfio_reader_free(reader);
    free(file.data);
}

static void check_missing_modifications(void)
{
    for (int r = SDFIO_ISO_1_0; r <= SDFIO_ISO_2_0; ++r)
    {
        check_missing_modification((sdfio_revision)r, SDFIO_BINARY);
        check_missing_modification((sdfio_revision)r, SDFIO_ASCII);
    }
}

static void check_value_errors(void)
{
    sdfio_header *header = new_header();
    SETUP(sdfio_header_set_data_type(header, SDFIO_TYPE_INT16));
    SETUP(sdfio_header_set_columns(header, 2));
    SETUP(sdfio_header_set_rows(header, 2));
    membuf file = {0};
    const sdfio_stream stream = membuf_stream(&file);

    /* Counts need a z-scale: the writer cannot know what they mean. */
    sdfio_writer *writer = new_writer(all_formats());
    const int16_t coded[4] = {0};
    SETUP(sdfio_create_stream(writer, &stream, header));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_write_data(writer, sdfio_layout_packed(SDFIO_TYPE_INT16),
                                  coded, 1));
    CHECK(strstr(sdfio_writer_error_message(writer), "z-scale") != NULL);
    sdfio_writer_free(writer);

    /* A height that does not fit the z-scale that was asked for. */
    const double heights[4] = {1e-6, 2e-6, 3e-6, 4e-6};
    SETUP(sdfio_header_set_z_scale(header, 1e-12));
    writer = new_writer(all_formats());
    SETUP(sdfio_create_stream(writer, &stream, header));
    CHECK_STATUS(SDFIO_ERR_RANGE,
                 sdfio_write_heights(
                     writer, sdfio_layout_packed(SDFIO_TYPE_DOUBLE), heights));
    sdfio_writer_free(writer);

    /* A height that would be read as a point not measured. */
    const double bad_height[1] = {-32768.0};
    SETUP(sdfio_header_set_z_scale(header, 1.0));
    SETUP(sdfio_header_set_columns(header, 1));
    SETUP(sdfio_header_set_rows(header, 1));
    writer = new_writer(all_formats());
    SETUP(sdfio_create_stream(writer, &stream, header));
    CHECK_STATUS(SDFIO_ERR_BAD_VALUE,
                 sdfio_write_heights(writer,
                                     sdfio_layout_packed(SDFIO_TYPE_DOUBLE),
                                     bad_height));
    sdfio_writer_free(writer);
    free(file.data);
}

static void check_layout(void)
{
    sdfio_header *header = small_header();
    membuf file = {0};
    const sdfio_stream stream = membuf_stream(&file);
    sdfio_writer *writer = new_writer(all_formats());
    SETUP(sdfio_create_stream(writer, &stream, header));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_write_data(writer, NULL, state_values, 1));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_write_heights(writer, NULL, state_values));
    sdfio_writer_free(writer);
    free(file.data);
}

int main(void)
{
    check_arguments();
    check_calls();
    check_refusals();
    check_retry();
    check_cross_field();
    check_failing_sink();
    check_estimate_matrix();
    check_estimate_errors();
    check_estimate_overflow(SDFIO_BINARY);
    check_estimate_overflow(SDFIO_ASCII);
    check_failed_header();
    check_missing_modifications();
    check_value_errors();
    check_layout();
    return finish_tests();
}
