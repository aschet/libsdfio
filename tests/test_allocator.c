/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* The memory of the handles: an allocator of the program is used for all of
 * it, and given everything back, also when it fails at any point. A small
 * configuration works. This is what a program without a heap relies on. */

#include "check.h"
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
static sdfio_status round_trip(const sdfio_config *config,
                               sdfio_representation representation,
                               sdfio_data_type type, bool *equal)
{
    static float written[ROWS * COLUMNS];
    static float read[ROWS * COLUMNS];
    fill(written);
    *equal = false;

    sdfio_header header;
    sdfio_header_init(&header);
    header.representation = representation;
    header.data_type = type;
    header.columns = COLUMNS;
    header.rows = ROWS;
    header.x_scale = 1.0e-6;
    header.y_scale = 1.0e-6;
    header.z_scale = SDFIO_AUTO_Z_SCALE;

    membuf sink = {0};
    sdfio_writer *writer = NULL;
    sdfio_status status = sdfio_writer_create(config, &writer);
    if (status == SDFIO_OK)
    {
        const sdfio_stream stream = membuf_stream(&sink);
        status = sdfio_create_stream(writer, &stream, &header);
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
            status = sdfio_end_trailer(writer);
        }
        sdfio_writer_free(writer);
    }
    if (status != SDFIO_OK)
    {
        free(sink.data);
        return status;
    }

    sdfio_reader *reader = NULL;
    status = sdfio_reader_create(config, &reader);
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
    static const sdfio_representation representations[] = {SDFIO_BINARY,
                                                           SDFIO_ASCII};
    for (size_t i = 0; i < sizeof representations / sizeof representations[0];
         ++i)
    {
        counting state = {-1, 0, 0, 0};
        const sdfio_allocator allocator = {&state, counting_allocate,
                                           counting_release};
        const sdfio_config config = {.allocator = &allocator, ALL_FORMATS};
        bool equal = false;
        CHECK_OK(
            round_trip(&config, representations[i], SDFIO_TYPE_INT16, &equal));
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
            const sdfio_config failing_config = {
                .allocator = &failing_allocator, ALL_FORMATS};
            const sdfio_status status = round_trip(
                &failing_config, representations[i], SDFIO_TYPE_INT16, &equal);
            CHECK_STATUS(SDFIO_ERR_MEMORY, status);
            CHECK(failing.live == 0);
        }
    }
}

/* A handle that is small: a buffer of a few bytes, a chunk of a few values. */
static void check_small(void)
{
    static const sdfio_config small[] = {
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
    const sdfio_config config = {.allocator = &allocator,
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
    /* An allocator needs both of its functions. */
    sdfio_allocator broken = {NULL, counting_allocate, NULL};
    sdfio_config config = {.allocator = &broken, ALL_FORMATS};
    sdfio_reader *reader_none = NULL;
    sdfio_writer *writer_none = NULL;
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_reader_create(&config, &reader_none));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_writer_create(&config, &writer_none));
    CHECK(reader_none == NULL && writer_none == NULL);

    /* A chunk that cannot be counted in bytes. */
    config = (sdfio_config){.chunk_values = (size_t)-1, ALL_FORMATS};
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_reader_create(&config, &reader_none));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_writer_create(&config, &writer_none));

    /* A place for the handle is needed. */
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_reader_create(all_formats(), NULL));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_writer_create(all_formats(), NULL));

    /* A date that is not set cannot be written. */
    sdfio_header header;
    sdfio_header_init(&header);
    header.columns = 1;
    header.rows = 1;
    header.creation_date = SDFIO_NO_TIME;
    sdfio_writer *writer = new_writer(all_formats());
    membuf sink = {0};
    const sdfio_stream stream = membuf_stream(&sink);
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_create_stream(writer, &stream, &header));
    CHECK(strstr(sdfio_writer_error_message(writer), "SDFIO_NO_TIME") != NULL);
    header.creation_date = 0;
    header.modification_date = SDFIO_NO_TIME;
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_create_stream(writer, &stream, &header));
    header.modification_date = 0;
    CHECK_OK(sdfio_create_stream(writer, &stream, &header));
    sdfio_writer_free(writer);
    free(sink.data);

    /* Text is written with its format and refused without it. */
    sdfio_header_init(&header);
    header.representation = SDFIO_ASCII;
    header.columns = 1;
    header.rows = 1;
    writer = new_writer(all_formats());
    CHECK_OK(sdfio_create_stream(writer, &stream, &header));
    sdfio_writer_free(writer);
    free(sink.data);
    sink = (membuf){0};
    const sdfio_config binary_only = {
        .reader_formats = {&sdfio_binary_reader_format},
        .writer_formats = {&sdfio_binary_writer_format}};
    writer = new_writer(&binary_only);
    CHECK_STATUS(SDFIO_ERR_UNSUPPORTED,
                 sdfio_create_stream(writer, &stream, &header));
    sdfio_writer_free(writer);
    free(sink.data);

    /* A file is read only if its format is listed. */
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

    /* A handle needs a format of its direction. */
    sdfio_reader *reader_made = NULL;
    sdfio_writer *writer_made = NULL;
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_reader_create(NULL, &reader_made));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_writer_create(NULL, &writer_made));
    const sdfio_config none = {0};
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_reader_create(&none, &reader_made));
    const sdfio_config readers_only = {
        .reader_formats = {&sdfio_binary_reader_format}};
    CHECK_OK(sdfio_reader_create(&readers_only, &reader_made));
    CHECK(reader_made != NULL);
    sdfio_reader_free(reader_made);
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_writer_create(&readers_only, &writer_made));
    const sdfio_config writers_only = {
        .writer_formats = {&sdfio_binary_writer_format}};
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_reader_create(&writers_only, &reader_made));
    CHECK(reader_made == NULL && writer_made == NULL);
}

/* The version numbers of a configuration: those of its formats, in their order.
 */
static void check_version_numbers(void)
{
    CHECK(sdfio_version_number_count(all_formats()) == 4U);
    CHECK(strcmp(sdfio_version_number(all_formats(), 0), "bISO-1.0") == 0);
    CHECK(strcmp(sdfio_version_number(all_formats(), 1), "bISO-2.0") == 0);
    CHECK(strcmp(sdfio_version_number(all_formats(), 2), "aISO-1.0") == 0);
    CHECK(strcmp(sdfio_version_number(all_formats(), 3), "aISO-2.0") == 0);
    for (size_t i = 0; i < sdfio_version_number_count(all_formats()); ++i)
    {
        CHECK(strlen(sdfio_version_number(all_formats(), i))
              == SDFIO_VERSION_NUMBER_SIZE);
    }
    CHECK(sdfio_version_number(all_formats(),
                               sdfio_version_number_count(all_formats()))
          == NULL);

    const sdfio_config ascii_first = {
        .reader_formats = {&sdfio_ascii_reader_format,
                           &sdfio_binary_reader_format}};
    CHECK(strcmp(sdfio_version_number(&ascii_first, 0), "aISO-1.0") == 0);
    const sdfio_config binary_only = {
        .reader_formats = {&sdfio_binary_reader_format}};
    CHECK(sdfio_version_number_count(&binary_only) == 2U);
    CHECK(sdfio_version_number(&binary_only, 2) == NULL);
    CHECK(sdfio_version_number_count(NULL) == 0U);
    CHECK(sdfio_version_number(NULL, 0) == NULL);
}

int main(void)
{
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
