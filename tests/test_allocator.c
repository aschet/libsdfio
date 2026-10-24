/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* The memory of the handles: an allocator of the program is used for all of
 * it, and given everything back, also when it fails at any point. A small
 * configuration works. */

#include "check.h"
#include "reader_format.h"
#include "reader_state.h"
#include "writer_format.h"
#include "writer_state.h"
#include <stdlib.h>
#include <string.h>

enum
{
    COLUMNS = 37,
    ROWS = 5,
    ARENA_SIZE = 1U << 16
};

typedef struct counting
{
    /* Allocations to allow before one fails; negative for no failure. */
    long allow;
    long calls;
    long live;
    size_t most;
} counting;

static void *counting_allocate(void *user, size_t size)
{
    counting *state = user;
    ++state->calls;
    if (state->allow >= 0 && state->calls > state->allow)
    {
        return NULL;
    }
    void *memory = malloc(size);
    if (memory != NULL)
    {
        ++state->live;
    }
    return memory;
}

static void counting_release(void *user, void *memory)
{
    counting *state = user;
    --state->live;
    free(memory);
}

static void fill(float *heights)
{
    for (size_t i = 0; i < (size_t)COLUMNS * ROWS; ++i)
    {
        heights[i] = (float)i * 1.0e-7F;
    }
    heights[3] = NAN;
}

/* Writes and reads a file through the configuration. Returns the first status
 * that is not OK, SDFIO_OK if all went well. */
static sdfio_status round_trip(const test_options *config, sdfio_format format,
                               sdfio_data_type type, bool *equal)
{
    static float written[ROWS * COLUMNS];
    static float read[ROWS * COLUMNS];
    fill(written);
    *equal = false;

    sdfio_header *header = new_header();
    CHECK_OK(sdfio_header_set_format(header, format));
    CHECK_OK(sdfio_header_set_data_type(header, type));
    CHECK_OK(sdfio_header_set_columns(header, COLUMNS));
    CHECK_OK(sdfio_header_set_rows(header, ROWS));
    CHECK_OK(sdfio_header_set_x_scale(header, 1.0e-6));
    CHECK_OK(sdfio_header_set_y_scale(header, 1.0e-6));
    CHECK_OK(sdfio_header_set_z_scale(header, SDFIO_AUTO_Z_SCALE));

    membuf sink = {0};
    sdfio_writer *writer = NULL;
    sdfio_status status = create_writer(config, &writer);
    if (status == SDFIO_OK)
    {
        const sdfio_stream stream = membuf_stream(&sink);
        status = sdfio_create_stream(writer, &stream, header);
        if (status == SDFIO_OK)
        {
            status = sdfio_write_heights(
                writer, sdfio_layout_packed(SDFIO_TYPE_FLOAT), written);
        }
        if (status == SDFIO_OK)
        {
            status = sdfio_write_trailer_text(writer, "a = 1\n", 6);
        }
        if (status == SDFIO_OK)
        {
            status = sdfio_end_file(writer);
        }
        sdfio_writer_free(writer);
    }
    if (status != SDFIO_OK)
    {
        free(sink.data);
        return status;
    }

    sdfio_reader *reader = NULL;
    status = create_reader(config, &reader);
    if (status == SDFIO_OK)
    {
        status = sdfio_open_memory(reader, sink.data, sink.size);
        if (status == SDFIO_OK)
        {
            status = sdfio_read_heights(
                reader, sdfio_layout_packed(SDFIO_TYPE_FLOAT), read);
        }
        const char *trailer = NULL;
        size_t trailer_size = 0;
        if (status == SDFIO_OK)
        {
            status = read_trailer_text(reader, 0, &trailer, &trailer_size);
        }
        if (status == SDFIO_OK)
        {
            *equal = trailer_size == 5 && memcmp(trailer, "a = 1", 5) == 0;
            for (size_t i = 0; i < (size_t)COLUMNS * ROWS; ++i)
            {
                if (isnan(written[i]) ? !isnan(read[i])
                                      : fabsf(read[i] - written[i]) > 1.0e-6F)
                {
                    *equal = false;
                }
            }
        }
        sdfio_reader_free(reader);
    }
    free(sink.data);
    return status;
}

static void check_counted(void)
{
    static const sdfio_format formats[] = {SDFIO_BINARY, SDFIO_ASCII};
    for (size_t i = 0; i < sizeof formats / sizeof formats[0]; ++i)
    {
        counting state = {-1, 0, 0, 0};
        const sdfio_allocator allocator = {&state, counting_allocate,
                                           counting_release};
        const test_options config = {.allocator = &allocator, ALL_FORMATS};
        bool equal = false;
        CHECK_OK(round_trip(&config, formats[i], SDFIO_TYPE_INT16, &equal));
        CHECK(equal);
        CHECK(state.calls > 0);
        CHECK(state.live == 0);

        /* Nothing is left behind wherever the memory runs out. */
        const long total = state.calls;
        for (long allow = 0; allow < total; ++allow)
        {
            counting failing = {allow, 0, 0, 0};
            const sdfio_allocator failing_allocator = {
                &failing, counting_allocate, counting_release};
            const test_options failing_config = {
                .allocator = &failing_allocator, ALL_FORMATS};
            const sdfio_status status = round_trip(&failing_config, formats[i],
                                                   SDFIO_TYPE_INT16, &equal);
            CHECK_STATUS(SDFIO_ERR_MEMORY, status);
            CHECK(failing.live == 0);
        }
    }
}

/* A handle that is small: a buffer of a few bytes, a chunk of a few values. */
static void check_small(void)
{
    static const test_options small[] = {
        {.buffer_size = 1, .chunk_values = 1, ALL_FORMATS},
        {.buffer_size = 64, .chunk_values = 3, ALL_FORMATS},
        {.buffer_size = 100, .chunk_values = 7, ALL_FORMATS},
        {.chunk_values = 1, ALL_FORMATS}};
    static const sdfio_data_type types[] = {SDFIO_TYPE_INT16, SDFIO_TYPE_FLOAT};
    for (size_t i = 0; i < sizeof small / sizeof small[0]; ++i)
    {
        for (const sdfio_data_type *type = types; type != types + 2; ++type)
        {
            bool equal = false;
            CHECK_OK(round_trip(&small[i], SDFIO_BINARY, *type, &equal));
            CHECK(equal);
            CHECK_OK(round_trip(&small[i], SDFIO_ASCII, *type, &equal));
            CHECK(equal);
        }
    }
}

/* An arena that never gives anything back, as a program without a heap has. */
/* Memory that suits every type: the union has the alignment of the largest. */
typedef union arena_cell
{
    long long integer;
    long double real;
    void *pointer;
    void (*function)(void);
} arena_cell;

typedef struct arena
{
    arena_cell memory[ARENA_SIZE / sizeof(arena_cell)];
    size_t used;
} arena;

static void *arena_allocate(void *user, size_t size)
{
    arena *state = user;
    const size_t step = sizeof(arena_cell);
    const size_t rounded = (size + step - 1) / step * step;
    if (rounded > ARENA_SIZE - state->used)
    {
        return NULL;
    }
    void *memory = (unsigned char *)state->memory + state->used;
    state->used += rounded;
    return memory;
}

static void arena_release(void *user, void *memory)
{
    (void)user;
    (void)memory;
}

static void check_arena(void)
{
    static arena state;
    const sdfio_allocator allocator = {&state, arena_allocate, arena_release};
    const test_options config = {.allocator = &allocator,
                                 .buffer_size = 256,
                                 .chunk_values = 16,
                                 ALL_FORMATS};
    bool equal = false;
    CHECK_OK(round_trip(&config, SDFIO_BINARY, SDFIO_TYPE_FLOAT, &equal));
    CHECK(equal);
    /* The writer and the reader stay within a few kilobytes of it. */
    CHECK(state.used < ARENA_SIZE);
}

static void check_arguments(void)
{
    /* An allocator is needed, with both of its functions. */
    sdfio_allocator broken = {NULL, counting_allocate, NULL};
    sdfio_reader *reader_none = NULL;
    sdfio_writer *writer_none = NULL;
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_reader_create(NULL, &reader_none));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_writer_create(NULL, &writer_none));
    const sdfio_allocator *standard = sdfio_default_allocator();
    CHECK(sdfio_default_allocator() == standard);
    CHECK(standard != NULL && standard->allocate != NULL
          && standard->release != NULL);
    if (standard != NULL && standard->allocate != NULL
        && standard->release != NULL)
    {
        void *block = standard->allocate(standard->user, 16);
        CHECK(block != NULL);
        standard->release(standard->user, block);
    }
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_reader_create(&broken, &reader_none));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_writer_create(&broken, &writer_none));
    CHECK(reader_none == NULL && writer_none == NULL);

    /* A place for the handle is needed. */
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_reader_create(sdfio_default_allocator(), NULL));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_writer_create(sdfio_default_allocator(), NULL));

    /* The options: a chunk that cannot be counted in bytes, a buffer without a
     * size, a format that is added twice, and more formats than there are. */
    sdfio_reader *reader_created = NULL;
    sdfio_writer *writer_created = NULL;
    CHECK_OK(sdfio_reader_create(sdfio_default_allocator(), &reader_created));
    CHECK_OK(sdfio_writer_create(sdfio_default_allocator(), &writer_created));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_reader_set_chunk(reader_created, (size_t)-1));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_writer_set_chunk(writer_created, (size_t)-1));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_reader_set_chunk(reader_created, 0));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_reader_set_buffer(reader_created, 0));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_writer_set_buffer(writer_created, 0));
    CHECK_OK(sdfio_reader_add_binary(reader_created));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_reader_add_binary(reader_created));
    CHECK_OK(sdfio_reader_add_ascii(reader_created));
    CHECK_OK(sdfio_writer_add_binary(writer_created));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_writer_add_binary(writer_created));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_reader_add_binary(NULL));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_writer_add_ascii(NULL));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_writer_set_time_zone(NULL, NULL));
    CHECK_OK(sdfio_reader_set_time_zone(reader_created, NULL));

    /* The options are set before the first attempt to open a file. */
    static const unsigned char junk[] =
        "not an sdf file at all, but long enough";
    CHECK_STATUS(SDFIO_ERR_FORMAT,
                 sdfio_open_memory(reader_created, junk, sizeof junk));
    CHECK_STATUS(SDFIO_ERR_STATE,
                 sdfio_reader_set_buffer(reader_created, 4096));
    CHECK_STATUS(SDFIO_ERR_STATE, sdfio_reader_set_chunk(reader_created, 16));
    CHECK_STATUS(SDFIO_ERR_STATE,
                 sdfio_reader_set_time_zone(reader_created, NULL));
    CHECK_STATUS(SDFIO_ERR_STATE, sdfio_reader_add_ascii(reader_created));
    sdfio_reader_free(reader_created);
    sdfio_writer_free(writer_created);

    /* A date that is not set cannot be written. */
    sdfio_header *header = new_header();
    CHECK_OK(sdfio_header_set_columns(header, 1));
    CHECK_OK(sdfio_header_set_rows(header, 1));
    CHECK_OK(sdfio_header_set_created(header, SDFIO_NO_TIME));
    sdfio_writer *writer = new_writer(all_formats());
    membuf sink = {0};
    const sdfio_stream stream = membuf_stream(&sink);
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_create_stream(writer, &stream, header));
    CHECK(strstr(sdfio_writer_error_message(writer), "SDFIO_NO_TIME") != NULL);
    CHECK_OK(sdfio_header_set_created(header, 0));
    CHECK_OK(sdfio_header_set_modified(header, SDFIO_NO_TIME));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_create_stream(writer, &stream, header));
    CHECK_OK(sdfio_header_set_modified(header, 0));
    CHECK_OK(sdfio_create_stream(writer, &stream, header));
    sdfio_writer_free(writer);
    free(sink.data);

    /* Text is written with its format and refused without it. */
    CHECK_OK(sdfio_header_set_format(header, SDFIO_ASCII));
    CHECK_OK(sdfio_header_set_columns(header, 1));
    CHECK_OK(sdfio_header_set_rows(header, 1));
    writer = new_writer(all_formats());
    CHECK_OK(sdfio_create_stream(writer, &stream, header));
    sdfio_writer_free(writer);
    free(sink.data);
    sink = (membuf){0};
    const test_options binary_only = {.reader_formats = {TEST_BINARY},
                                      .writer_formats = {TEST_BINARY}};
    writer = new_writer(&binary_only);
    CHECK_STATUS(SDFIO_ERR_UNSUPPORTED,
                 sdfio_create_stream(writer, &stream, header));
    sdfio_writer_free(writer);
    free(sink.data);

    /* A file is read only if its format is added. */
    static const char ascii_version_number[] = "aISO-2.0\r\n";
    sdfio_reader *reader = new_reader(&binary_only);
    CHECK_STATUS(SDFIO_ERR_UNSUPPORTED,
                 sdfio_open_memory(reader, ascii_version_number,
                                   sizeof ascii_version_number));
    sdfio_reader_free(reader);
    reader = new_reader(all_formats());
    CHECK_STATUS(SDFIO_ERR_FORMAT,
                 sdfio_open_memory(reader, ascii_version_number,
                                   sizeof ascii_version_number));
    sdfio_reader_free(reader);

    /* A handle needs a format of its direction before a file is opened or
     * created. */
    sdfio_reader *no_format_reader = NULL;
    CHECK_OK(sdfio_reader_create(sdfio_default_allocator(), &no_format_reader));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_open_memory(no_format_reader, junk, sizeof junk));
    sdfio_reader_free(no_format_reader);
    sdfio_writer *no_format_writer = NULL;
    CHECK_OK(sdfio_writer_create(sdfio_default_allocator(), &no_format_writer));
    sdfio_header *no_format_header = new_header();
    membuf no_format_sink = {0};
    const sdfio_stream no_format_stream = membuf_stream(&no_format_sink);
    CHECK_STATUS(SDFIO_ERR_UNSUPPORTED,
                 sdfio_create_stream(no_format_writer, &no_format_stream,
                                     no_format_header));
    sdfio_writer_free(no_format_writer);
    free(no_format_sink.data);
}

/* The version numbers of a reader: those of its formats, in their order. */
static void check_version_numbers(void)
{
    sdfio_reader *all = new_reader(all_formats());
    CHECK(sdfio_reader_version_count(all) == 4U);
    CHECK(strcmp(sdfio_reader_version(all, 0), "bISO-1.0") == 0);
    CHECK(strcmp(sdfio_reader_version(all, 1), "bISO-2.0") == 0);
    CHECK(strcmp(sdfio_reader_version(all, 2), "aISO-1.0") == 0);
    CHECK(strcmp(sdfio_reader_version(all, 3), "aISO-2.0") == 0);
    for (size_t i = 0; i < sdfio_reader_version_count(all); ++i)
    {
        CHECK(strlen(sdfio_reader_version(all, i))
              == SDFIO_VERSION_NUMBER_SIZE);
    }
    CHECK(sdfio_reader_version(all, sdfio_reader_version_count(all)) == NULL);
    sdfio_reader_free(all);

    const test_options ascii_first = {
        .reader_formats = {TEST_ASCII, TEST_BINARY}};
    sdfio_reader *ascii = new_reader(&ascii_first);
    CHECK(strcmp(sdfio_reader_version(ascii, 0), "aISO-1.0") == 0);
    sdfio_reader_free(ascii);
    const test_options binary_only = {.reader_formats = {TEST_BINARY}};
    sdfio_reader *binary = new_reader(&binary_only);
    CHECK(sdfio_reader_version_count(binary) == 2U);
    CHECK(sdfio_reader_version(binary, 2) == NULL);
    sdfio_reader_free(binary);
    CHECK(sdfio_reader_version_count(NULL) == 0U);
    CHECK(sdfio_reader_version(NULL, 0) == NULL);
}

/* The list of formats has room for the formats of the library only. */
static void check_format_list(void)
{
    sdfio_reader_format first_read = {.prefix = 'x'};
    sdfio_reader_format second_read = {.prefix = 'y'};
    sdfio_reader_format third_read = {.prefix = 'z'};
    sdfio_reader *reader = NULL;
    CHECK_OK(sdfio_reader_create(sdfio_default_allocator(), &reader));
    CHECK_OK(sdfio_reader_install_format(reader, &first_read));
    CHECK_OK(sdfio_reader_install_format(reader, &second_read));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_reader_install_format(reader, &first_read));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_reader_install_format(reader, &third_read));
    sdfio_reader_free(reader);

    sdfio_writer_format first_write = {.writes = SDFIO_BINARY};
    sdfio_writer_format second_write = {.writes = SDFIO_ASCII};
    sdfio_writer_format third_write = {.writes = SDFIO_BINARY};
    sdfio_writer *writer = NULL;
    CHECK_OK(sdfio_writer_create(sdfio_default_allocator(), &writer));
    CHECK_OK(sdfio_writer_install_format(writer, &first_write));
    CHECK_OK(sdfio_writer_install_format(writer, &second_write));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_writer_install_format(writer, &first_write));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_writer_install_format(writer, &third_write));
    sdfio_writer_free(writer);
}

int main(void)
{
    check_format_list();
    check_version_numbers();
    check_counted();
    check_small();
    check_arena();
    check_arguments();
    if (failures != 0)
    {
        fprintf(stderr, "%d of %d checks failed\n", failures, checks);
        return 1;
    }
    return 0;
}
