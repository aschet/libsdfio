/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

#include "check.h"
#include <stdlib.h>
#include <string.h>

#define COLUMNS 2
#define ROWS 2
#define SAMPLES SDFIO_TEST_DATA_DIR "/samples/"

#define OK SDFIO_OK
#define STATE SDFIO_ERR_STATE
#define ARGUMENT SDFIO_ERR_ARGUMENT

static const float values[ROWS * COLUMNS] = {1.0F, 2.0F, 3.0F, 4.0F};

typedef enum reader_state
{
    READER_NEW,
    READER_REFUSED,
    READER_DATA,
    READER_PARTIAL,
    READER_ALL,
    READER_TRAILER,
    READER_FAILED,
    READER_STATES
} reader_state;

typedef enum reader_call
{
    READER_ADD,
    READER_CHUNK,
    READER_OPEN,
    READER_ROW,
    READER_TOO_MANY,
    READER_NO_ROWS,
    READER_HEIGHTS,
    READER_TRAILER_PART,
    READER_CALLS
} reader_call;

static const sdfio_status reader_results[READER_STATES][READER_CALLS] = {
    [READER_NEW] = {OK, OK, OK, STATE, STATE, STATE, STATE, STATE},
    [READER_REFUSED] = {STATE, STATE, OK, STATE, STATE, STATE, STATE, STATE},
    [READER_DATA] = {STATE, STATE, STATE, OK, ARGUMENT, OK, OK, OK},
    [READER_PARTIAL] = {STATE, STATE, STATE, OK, ARGUMENT, OK, STATE, OK},
    [READER_ALL] = {STATE, STATE, STATE, ARGUMENT, ARGUMENT, OK, STATE, OK},
    [READER_TRAILER] = {STATE, STATE, STATE, STATE, STATE, OK, STATE, OK},
    [READER_FAILED] = {STATE, STATE, STATE, STATE, STATE, STATE, STATE, STATE}};

static void write_file(membuf *file)
{
    sdfio_writer *writer = new_writer(all_formats());
    const sdfio_stream stream = membuf_stream(file);
    CHECK_OK(sdfio_create_stream(writer, &stream, small_header()));
    SETUP(sdfio_write_data(writer, sdfio_layout_packed(SDFIO_TYPE_FLOAT),
                           values, ROWS));
    CHECK_OK(sdfio_write_trailer_field(writer, "Name", "value"));
    CHECK_OK(sdfio_end_file(writer));
    sdfio_writer_free(writer);
}

static sdfio_status read_rows(sdfio_reader *reader, size_t rows)
{
    float rows_read[ROWS * COLUMNS];
    return sdfio_read_data(reader, sdfio_layout_packed(SDFIO_TYPE_FLOAT),
                           rows_read, rows);
}

/* Returns a reader in the state. For the failed state the file ends in the
 * data area. */
static sdfio_reader *reader_in(reader_state state, const membuf *file)
{
    sdfio_reader *reader = NULL;
    SETUP(sdfio_reader_create(sdfio_default_allocator(), &reader));
    SETUP(sdfio_reader_add_binary(reader));
    if (state == READER_REFUSED)
    {
        static const unsigned char junk[] = "not an sdf file at all, but long";
        CHECK_STATUS(SDFIO_ERR_FORMAT,
                     sdfio_open_memory(reader, junk, sizeof junk));
    }
    if (state == READER_NEW || state == READER_REFUSED)
    {
        return reader;
    }
    if (state == READER_FAILED)
    {
        CHECK_OK(sdfio_open_memory(reader, file->data,
                                   ISO_2_0_BINARY_HEADER_SIZE + 4));
        CHECK_STATUS(SDFIO_ERR_FORMAT, read_rows(reader, ROWS));
        return reader;
    }
    CHECK_OK(sdfio_open_memory(reader, file->data, file->size));
    char part[16];
    size_t size = 0;
    switch (state)
    {
    case READER_PARTIAL:
        CHECK_OK(read_rows(reader, 1));
        break;
    case READER_ALL:
        CHECK_OK(read_rows(reader, ROWS));
        break;
    case READER_TRAILER:
        CHECK_OK(sdfio_read_trailer(reader, part, sizeof part, &size));
        break;
    case READER_NEW:
    case READER_REFUSED:
    case READER_DATA:
    case READER_FAILED:
    case READER_STATES:
        break;
    }
    return reader;
}

static sdfio_status reader_call_in(sdfio_reader *reader, reader_call call,
                                   const membuf *file)
{
    float heights[ROWS * COLUMNS];
    char part[16];
    size_t size = 0;
    switch (call)
    {
    case READER_ADD:
        return sdfio_reader_add_ascii(reader);
    case READER_CHUNK:
        return sdfio_reader_set_chunk(reader, 16);
    case READER_OPEN:
        return sdfio_open_memory(reader, file->data, file->size);
    case READER_ROW:
        return read_rows(reader, 1);
    case READER_TOO_MANY:
        return read_rows(reader, ROWS + 1);
    case READER_NO_ROWS:
        return read_rows(reader, 0);
    case READER_HEIGHTS:
        return sdfio_read_heights(reader, sdfio_layout_packed(SDFIO_TYPE_FLOAT),
                                  heights);
    case READER_TRAILER_PART:
        return sdfio_read_trailer(reader, part, sizeof part, &size);
    case READER_CALLS:
        break;
    }
    return SDFIO_OK;
}

static void check_arguments(void)
{
    const sdfio_allocator broken = {NULL, counting_allocate, NULL};
    sdfio_reader *none = NULL;
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_reader_create(NULL, &none));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_reader_create(&broken, &none));
    CHECK(none == NULL);
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_reader_create(sdfio_default_allocator(), NULL));

    sdfio_reader *reader = NULL;
    SETUP(sdfio_reader_create(sdfio_default_allocator(), &reader));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_reader_set_chunk(reader, (size_t)-1));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_reader_set_chunk(reader, 0));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_reader_set_buffer(reader, 0));
    SETUP(sdfio_reader_add_binary(reader));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_reader_add_binary(reader));
    SETUP(sdfio_reader_add_ascii(reader));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_reader_add_binary(NULL));
    CHECK_OK(sdfio_reader_set_time_zone(reader, NULL));

    static const unsigned char junk[] =
        "not an sdf file at all, but long enough";
    CHECK_STATUS(SDFIO_ERR_FORMAT,
                 sdfio_open_memory(reader, junk, sizeof junk));
    CHECK_STATUS(SDFIO_ERR_STATE, sdfio_reader_set_buffer(reader, 4096));
    CHECK_STATUS(SDFIO_ERR_STATE, sdfio_reader_set_chunk(reader, 16));
    CHECK_STATUS(SDFIO_ERR_STATE, sdfio_reader_set_time_zone(reader, NULL));
    CHECK_STATUS(SDFIO_ERR_STATE, sdfio_reader_add_ascii(reader));
    sdfio_reader_free(reader);

    static const char ascii_magic[] = "aISO-2.0\r\n";
    const test_options binary_only = {.reader_formats = {TEST_BINARY}};
    reader = new_reader(&binary_only);
    CHECK_STATUS(SDFIO_ERR_UNSUPPORTED,
                 sdfio_open_memory(reader, ascii_magic, sizeof ascii_magic));
    sdfio_reader_free(reader);
    reader = new_reader(all_formats());
    CHECK_STATUS(SDFIO_ERR_FORMAT,
                 sdfio_open_memory(reader, ascii_magic, sizeof ascii_magic));
    sdfio_reader_free(reader);

    SETUP(sdfio_reader_create(sdfio_default_allocator(), &reader));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_open_memory(reader, junk, sizeof junk));
    sdfio_reader_free(reader);
}

static void check_calls(void)
{
    membuf file = {0};
    write_file(&file);
    for (int state = 0; state < READER_STATES; ++state)
    {
        for (int call = 0; call < READER_CALLS; ++call)
        {
            sdfio_reader *reader = reader_in((reader_state)state, &file);
            const sdfio_status status =
                reader_call_in(reader, (reader_call)call, &file);
            if (status != reader_results[state][call])
            {
                fprintf(stderr, "reader state %d, call %d: %s, expected %s\n",
                        state, call, sdfio_status_string(status),
                        sdfio_status_string(reader_results[state][call]));
                ++failures;
            }
            ++checks;
            sdfio_reader_free(reader);
        }
    }
    free(file.data);
}

/* The header is available after opening, also after an error. */
static void check_header(void)
{
    membuf file = {0};
    write_file(&file);
    for (int state = 0; state < READER_STATES; ++state)
    {
        sdfio_reader *reader = reader_in((reader_state)state, &file);
        const bool opened = state != READER_NEW && state != READER_REFUSED;
        CHECK((sdfio_reader_header(reader) != NULL) == opened);
        sdfio_reader_free(reader);
    }
    free(file.data);
}

/* A refused call leaves the reader unchanged, and reading at the end of the
 * trailer returns 0 repeatedly. */
static void check_refusals(void)
{
    membuf file = {0};
    write_file(&file);
    sdfio_reader *reader = reader_in(READER_DATA, &file);
    CHECK_STATUS(ARGUMENT, read_rows(reader, ROWS + 1));
    CHECK_OK(read_rows(reader, 1));
    CHECK_STATUS(STATE, sdfio_open_memory(reader, file.data, file.size));
    CHECK_OK(read_rows(reader, 1));
    char text[16];
    size_t size = 0;
    CHECK_OK(sdfio_read_trailer(reader, text, sizeof text, &size));
    CHECK(size > 0);
    CHECK_OK(sdfio_read_trailer(reader, text, sizeof text, &size));
    CHECK(size == 0);
    CHECK_OK(sdfio_read_trailer(reader, text, sizeof text, &size));
    CHECK(size == 0);
    sdfio_reader_free(reader);
    free(file.data);
}

static void check_header_copy(void)
{
    membuf file = {0};
    write_file(&file);
    sdfio_reader *reader = new_reader(all_formats());
    CHECK_OK(sdfio_open_memory(reader, file.data, file.size));
    sdfio_header *kept = new_header();
    SETUP(sdfio_header_copy(kept, sdfio_reader_header(reader)));
    sdfio_reader_free(reader);
    CHECK(sdfio_header_columns(kept) == COLUMNS
          && sdfio_header_rows(kept) == ROWS);
    CHECK(sdfio_header_z_scale(kept) == 1.0);
    CHECK(strcmp(sdfio_header_manufacturer(kept), "libsdfio") == 0);
    free(file.data);
}

typedef struct pipe_source
{
    const unsigned char *data;
    size_t size;
    size_t position;
    size_t piece;
    size_t fail_at;
    size_t calls;
} pipe_source;

static size_t pipe_read(void *user, void *data, size_t size)
{
    pipe_source *pipe = user;
    if (pipe->position >= pipe->fail_at)
    {
        return SDFIO_STREAM_ERROR;
    }
    /* A piece of 0 stands for pieces of varying sizes, 1 to 13 bytes. */
    const size_t piece =
        pipe->piece != 0 ? pipe->piece : 1 + (pipe->calls++ * 5) % 13;
    size_t count = pipe->size - pipe->position;
    count = count < size ? count : size;
    count = count < piece ? count : piece;
    memcpy(data, pipe->data + pipe->position, count);
    pipe->position += count;
    return count;
}

static sdfio_stream pipe_stream(pipe_source *pipe)
{
    sdfio_stream stream = {0};
    stream.user = pipe;
    stream.read = pipe_read;
    return stream;
}

/* Reads of one byte, of varying sizes or of the whole file give the same
 * heights as memory, and only 0 ends the data. */
static void check_pipe_file(const char *path, size_t piece)
{
    size_t size = 0;
    unsigned char *data = load_file(path, &size);
    if (data == NULL)
    {
        return;
    }
    pipe_source pipe = {data, size, 0, piece, SIZE_MAX, 0};
    const sdfio_stream stream = pipe_stream(&pipe);

    sdfio_reader *reader = new_reader(all_formats());
    CHECK_OK(sdfio_open_stream(reader, &stream));
    const sdfio_header *header = sdfio_reader_header(reader);
    CHECK(header != NULL);
    if (header != NULL)
    {
        double *heights = malloc(height_count(header) * sizeof(double));
        SETUP(sdfio_read_heights(reader, sdfio_layout_packed(SDFIO_TYPE_DOUBLE),
                                 heights));
        sdfio_reader *in_memory = new_reader(all_formats());
        CHECK_OK(sdfio_open_memory(in_memory, data, size));
        double *expected = malloc(height_count(header) * sizeof(double));
        SETUP(sdfio_read_heights(
            in_memory, sdfio_layout_packed(SDFIO_TYPE_DOUBLE), expected));
        for (size_t i = 0; i < height_count(header); ++i)
        {
            CHECK(isnan(expected[i]) ? isnan(heights[i])
                                     : heights[i] == expected[i]);
        }
        free(expected);
        sdfio_reader_free(in_memory);
        const char *trailer = NULL;
        size_t trailer_size = 0;
        CHECK_OK(read_trailer_text(reader, 0, &trailer, &trailer_size));
        /* The trailer is at its end, and the stream cannot be read again. */
        const char *again = NULL;
        size_t again_size = 0;
        CHECK_OK(read_trailer_text(reader, 0, &again, &again_size));
        CHECK(again_size == 0);
        CHECK_STATUS(SDFIO_ERR_STATE,
                     sdfio_read_heights(reader,
                                        sdfio_layout_packed(SDFIO_TYPE_DOUBLE),
                                        heights));
        free(heights);
    }
    CHECK_STATUS(SDFIO_ERR_STATE, sdfio_open_stream(reader, &stream));
    sdfio_reader_free(reader);

    /* The trailer can be asked for first: the data is passed over. */
    pipe = (pipe_source){data, size, 0, piece, SIZE_MAX, 0};
    reader = new_reader(all_formats());
    CHECK_OK(sdfio_open_stream(reader, &stream));
    const char *trailer = NULL;
    size_t trailer_size = 0;
    CHECK_OK(read_trailer_text(reader, 0, &trailer, &trailer_size));
    CHECK(trailer != NULL && trailer_size > 0);
    sdfio_reader_free(reader);
    free(data);
}

static void check_failing_stream(void)
{
    size_t size = 0;
    unsigned char *data = load_file(SAMPLES "iso2_surface_binary.sdf", &size);
    if (data == NULL)
    {
        return;
    }
    /* A failure in the header, and one in the data, which is a 32 by 48 matrix
     * of int16 after the header. */
    const size_t data_end = ISO_2_0_BINARY_HEADER_SIZE + 32 * 48 * 2;
    const size_t places[] = {10, 100, data_end - 50};
    for (size_t i = 0; i < sizeof places / sizeof places[0]; ++i)
    {
        pipe_source pipe = {data, size, 0, 7, places[i], 0};
        const sdfio_stream stream = pipe_stream(&pipe);
        sdfio_reader *reader = new_reader(all_formats());
        sdfio_status status = sdfio_open_stream(reader, &stream);
        if (status == SDFIO_OK)
        {
            const sdfio_header *header = sdfio_reader_header(reader);
            double *heights = malloc(height_count(header) * sizeof(double));
            status = sdfio_read_heights(
                reader, sdfio_layout_packed(SDFIO_TYPE_DOUBLE), heights);
            free(heights);
        }
        CHECK(status == SDFIO_ERR_IO);
        /* After an error in the data the reader does not go on. */
        if (status == SDFIO_ERR_IO && sdfio_reader_header(reader) != NULL)
        {
            const char *trailer = NULL;
            size_t trailer_size = 0;
            CHECK_STATUS(SDFIO_ERR_STATE,
                         read_trailer_text(reader, 0, &trailer, &trailer_size));
        }
        sdfio_reader_free(reader);
    }
    free(data);
}

/* A header that claims more data than the stream holds: the search for the
 * trailer reaches the end of the stream. */
/* A header that claims more data than the stream holds: the search for the
 * trailer reaches the end of the stream. */
static void check_huge_matrix(void)
{
    if (sizeof(size_t) < sizeof(uint64_t))
    {
        /* The number of values does not fit in size_t, so the file is
         * refused. */
        return;
    }
    sdfio_header *header = new_header();
    SETUP(sdfio_header_set_format(header, SDFIO_BINARY));
    SETUP(sdfio_header_set_data_type(header, SDFIO_TYPE_DOUBLE));
    SETUP(sdfio_header_set_columns(header, 1));
    SETUP(sdfio_header_set_rows(header, 1));
    SETUP(sdfio_header_set_z_scale(header, 1.0));
    membuf file = {0};
    const sdfio_stream out = membuf_stream(&file);
    const double height = 0.0;
    sdfio_writer *writer = new_writer(all_formats());
    CHECK_OK(sdfio_create_stream(writer, &out, header));
    SETUP(sdfio_write_heights(writer, sdfio_layout_packed(SDFIO_TYPE_DOUBLE),
                              &height));
    SETUP(sdfio_end_file(writer));
    sdfio_writer_free(writer);
    /* The numbers of columns and rows, 4 bytes each in little endian. */
    const uint32_t sizes[] = {UINT32_MAX, 1U << 29};
    for (size_t i = 0; i < 2; ++i)
    {
        for (size_t byte = 0; byte < sizeof(uint32_t); ++byte)
        {
            file.data[ISO_2_0_COLUMNS_AT + i * sizeof(uint32_t) + byte] =
                (unsigned char)(sizes[i] >> (8 * byte));
        }
    }
    pipe_source pipe = {file.data, file.size, 0, SIZE_MAX, SIZE_MAX, 0};
    const sdfio_stream stream = pipe_stream(&pipe);
    sdfio_reader *reader = new_reader(all_formats());
    CHECK_OK(sdfio_open_stream(reader, &stream));
    const char *trailer = NULL;
    size_t trailer_size = 0;
    CHECK(read_trailer_text(reader, 0, &trailer, &trailer_size) != SDFIO_OK);
    sdfio_reader_free(reader);
    free(file.data);
}
static void check_magics(void)
{
    sdfio_reader *all = new_reader(all_formats());
    CHECK(sdfio_reader_magic_count(all) == 4U);
    CHECK(strcmp(sdfio_reader_magic(all, 0), "bISO-1.0") == 0);
    CHECK(strcmp(sdfio_reader_magic(all, 1), "bISO-2.0") == 0);
    CHECK(strcmp(sdfio_reader_magic(all, 2), "aISO-1.0") == 0);
    CHECK(strcmp(sdfio_reader_magic(all, 3), "aISO-2.0") == 0);
    CHECK(sdfio_reader_magic(all, sdfio_reader_magic_count(all)) == NULL);
    sdfio_reader_free(all);

    const test_options ascii_first = {
        .reader_formats = {TEST_ASCII, TEST_BINARY}};
    sdfio_reader *ascii = new_reader(&ascii_first);
    CHECK(strcmp(sdfio_reader_magic(ascii, 0), "aISO-1.0") == 0);
    sdfio_reader_free(ascii);
    const test_options binary_only = {.reader_formats = {TEST_BINARY}};
    sdfio_reader *binary = new_reader(&binary_only);
    CHECK(sdfio_reader_magic_count(binary) == 2U);
    CHECK(sdfio_reader_magic(binary, 2) == NULL);
    sdfio_reader_free(binary);
    CHECK(sdfio_reader_magic_count(NULL) == 0U);
    CHECK(sdfio_reader_magic(NULL, 0) == NULL);
}

static void check_pipe_pieces(void)
{
    const size_t pieces[] = {0, 1, 7, 4096, SIZE_MAX};
    for (size_t i = 0; i < sizeof pieces / sizeof pieces[0]; ++i)
    {
        check_pipe_file(SAMPLES "iso2_surface_binary.sdf", pieces[i]);
        check_pipe_file(SAMPLES "iso2_surface_ascii.sdf", pieces[i]);
        check_pipe_file(SAMPLES "iso1_surface_ascii.sdf", pieces[i]);
        check_pipe_file(SAMPLES "iso1_surface_binary.sdf", pieces[i]);
    }
}

/* An attempt to open a file in a reader that is open leaves the open file as it
 * is, also when the file is larger than the buffer. */
#define SECOND_OPEN_VALUES 4000
static void check_second_open(void)
{
    membuf files[2] = {{0}, {0}};
    float row[SECOND_OPEN_VALUES];
    for (size_t f = 0; f < 2; ++f)
    {
        for (size_t i = 0; i < SECOND_OPEN_VALUES; ++i)
        {
            row[i] = (float)i + (float)f * 100000.0F;
        }
        sdfio_header *header = new_header();
        SETUP(sdfio_header_set_data_type(header, SDFIO_TYPE_FLOAT));
        SETUP(sdfio_header_set_columns(header, SECOND_OPEN_VALUES));
        SETUP(sdfio_header_set_rows(header, 1));
        SETUP(sdfio_header_set_z_scale(header, 1.0));
        const sdfio_stream stream = membuf_stream(&files[f]);
        sdfio_writer *writer = new_writer(all_formats());
        SETUP(sdfio_create_stream(writer, &stream, header));
        SETUP(sdfio_write_data(writer, sdfio_layout_packed(SDFIO_TYPE_FLOAT),
                               row, 1));
        SETUP(sdfio_end_file(writer));
        sdfio_writer_free(writer);
    }
    sdfio_reader *reader = new_reader(all_formats());
    SETUP(sdfio_reader_set_buffer(reader, 1));
    SETUP(sdfio_open_memory(reader, files[0].data, files[0].size));
    CHECK_STATUS(SDFIO_ERR_STATE,
                 sdfio_open_memory(reader, files[1].data, files[1].size));
    SETUP(
        sdfio_read_data(reader, sdfio_layout_packed(SDFIO_TYPE_FLOAT), row, 1));
    size_t different = 0;
    for (size_t i = 0; i < SECOND_OPEN_VALUES; ++i)
    {
        different += row[i] != (float)i ? 1 : 0;
    }
    CHECK(different == 0);
    sdfio_reader_free(reader);
    free(files[0].data);
    free(files[1].data);
}

static void check_layout(void)
{
    membuf file = {0};
    write_file(&file);
    sdfio_reader *reader = open_memory_file(&file);
    float rows_read[ROWS * COLUMNS];
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_read_data(reader, NULL, rows_read, 1));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_read_heights(reader, NULL, rows_read));
    sdfio_reader_free(reader);
    free(file.data);
}

int main(void)
{
    check_arguments();
    check_calls();
    check_header();
    check_refusals();
    check_header_copy();
    check_pipe_pieces();
    check_failing_stream();
    check_huge_matrix();
    check_magics();
    check_second_open();
    check_layout();
    return finish_tests();
}
