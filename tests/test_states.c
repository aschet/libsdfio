/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* The states of a reader and of a writer: the result of each call in each
 * state, as the guide gives it. */

#include "check.h"

#define COLUMNS 2
#define ROWS 2

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

static const float values[ROWS * COLUMNS] = {1.0F, 2.0F, 3.0F, 4.0F};

static sdfio_header *describe(void)
{
    sdfio_header *header = new_header();
    CHECK_OK(sdfio_header_set_data_type(header, SDFIO_TYPE_FLOAT));
    CHECK_OK(sdfio_header_set_columns(header, COLUMNS));
    CHECK_OK(sdfio_header_set_rows(header, ROWS));
    CHECK_OK(sdfio_header_set_z_scale(header, 1.0));
    return header;
}

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
                            values, rows);
}

/* A writer in a state. The stream is the memory of the file, or refuses every
 * byte for the failed writer. */
static sdfio_writer *writer_in(writer_state state, membuf *file)
{
    sdfio_writer *writer = NULL;
    CHECK_OK(sdfio_writer_create(sdfio_default_allocator(), &writer));
    CHECK_OK(sdfio_writer_add_binary(writer));
    if (state == WRITER_FAILED)
    {
        CHECK_OK(sdfio_writer_set_buffer(writer, 1));
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
    CHECK_OK(sdfio_create_stream(writer, &stream, describe()));
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
        return sdfio_create_stream(writer, &stream, describe());
    case WRITER_ROW:
        return write_rows(writer, 1);
    case WRITER_TOO_MANY:
        return write_rows(writer, ROWS + 1);
    case WRITER_HEIGHTS:
        return sdfio_write_heights(
            writer, sdfio_layout_packed(SDFIO_TYPE_FLOAT), values);
    case WRITER_FIELD:
        return sdfio_write_trailer_field(writer, "Name", "value");
    case WRITER_END:
        return sdfio_end_file(writer);
    case WRITER_CALLS:
        break;
    }
    return SDFIO_OK;
}

static void check_writer_calls(void)
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

/* A call that is refused for its state or its arguments changes nothing. */
static void check_writer_refusals(void)
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

/* A create that fails leaves the writer new. */
static void check_writer_retry(void)
{
    sdfio_writer *writer = new_writer(all_formats());
    sdfio_header *header = describe();
    membuf file = {0};
    const sdfio_stream stream = membuf_stream(&file);
    CHECK_OK(sdfio_header_set_created(header, SDFIO_NO_TIME));
    CHECK_STATUS(ARGUMENT, sdfio_create_stream(writer, &stream, header));
    CHECK_OK(sdfio_header_set_created(header, 1772600760));
    CHECK_OK(sdfio_create_stream(writer, &stream, header));
    sdfio_writer_free(writer);
    free(file.data);
}

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

/* A binary file of the header and the rows. */
static void write_file(membuf *file)
{
    sdfio_writer *writer = new_writer(all_formats());
    const sdfio_stream stream = membuf_stream(file);
    CHECK_OK(sdfio_create_stream(writer, &stream, describe()));
    CHECK_OK(write_rows(writer, ROWS));
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

/* A reader in a state. The file of the failed reader ends in the data area. */
static sdfio_reader *reader_in(reader_state state, const membuf *file)
{
    sdfio_reader *reader = NULL;
    CHECK_OK(sdfio_reader_create(sdfio_default_allocator(), &reader));
    CHECK_OK(sdfio_reader_add_binary(reader));
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

static void check_reader_calls(void)
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

/* The header is available from the open on, also after an error. */
static void check_reader_header(void)
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

/* A call that is refused for its state or its arguments changes nothing, and
 * the end of the trailer repeats. */
static void check_reader_refusals(void)
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

int main(void)
{
    check_writer_calls();
    check_writer_refusals();
    check_writer_retry();
    check_reader_calls();
    check_reader_header();
    check_reader_refusals();
    return finish_tests();
}
