/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* The memory of the handles: an allocator of the program is used for all of
 * it, and given everything back, also when it fails at any point. A small
 * configuration works. This is what a program without a heap relies on. */

#include "check.h"

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

typedef struct memory_sink
{
    unsigned char *data;
    size_t size;
} memory_sink;

static size_t sink_write(void *user, const void *data, size_t size)
{
    memory_sink *sink = user;
    unsigned char *grown = realloc(sink->data, sink->size + size);
    if (grown == NULL)
    {
        return 0;
    }
    memcpy(grown + sink->size, data, size);
    sink->data = grown;
    sink->size += size;
    return size;
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

    memory_sink sink = {0};
    sdfio_status status = SDFIO_ERR_MEMORY;
    sdfio_writer *writer = sdfio_writer_new(config);
    if (writer != NULL)
    {
        const sdfio_stream stream = {&sink, NULL, sink_write};
        status = sdfio_create_stream(writer, &stream, &header);
        if (status == SDFIO_OK)
        {
            status = sdfio_write_heights(writer, SDFIO_TYPE_FLOAT, written);
        }
        if (status == SDFIO_OK)
        {
            status = sdfio_write_trailer(writer, "a = 1\n", 6);
        }
        sdfio_writer_free(writer);
    }
    if (status != SDFIO_OK)
    {
        free(sink.data);
        return status;
    }

    status = SDFIO_ERR_MEMORY;
    sdfio_reader *reader = sdfio_reader_new(config);
    if (reader != NULL)
    {
        status = sdfio_open_memory(reader, sink.data, sink.size);
        if (status == SDFIO_OK)
        {
            status = sdfio_read_heights(reader, SDFIO_TYPE_FLOAT, read);
        }
        const char *trailer = NULL;
        size_t trailer_size = 0;
        if (status == SDFIO_OK)
        {
            status = sdfio_read_trailer(reader, &trailer, &trailer_size);
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
        const sdfio_config config = {.allocator = &allocator,
                                     .codecs = ALL_CODECS};
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
                .allocator = &failing_allocator, .codecs = ALL_CODECS};
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
        {.buffer_size = 1, .chunk_values = 1, .codecs = ALL_CODECS},
        {.buffer_size = 64, .chunk_values = 3, .codecs = ALL_CODECS},
        {.buffer_size = 100, .chunk_values = 7, .codecs = ALL_CODECS},
        {.chunk_values = 1, .codecs = ALL_CODECS}};
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
                                 .codecs = ALL_CODECS};
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
    sdfio_config config = {.allocator = &broken, .codecs = ALL_CODECS};
    CHECK(sdfio_reader_new(&config) == NULL);
    CHECK(sdfio_writer_new(&config) == NULL);

    /* A chunk that cannot be counted in bytes. */
    config = (sdfio_config){.chunk_values = (size_t)-1, .codecs = ALL_CODECS};
    CHECK(sdfio_reader_new(&config) == NULL);
    CHECK(sdfio_writer_new(&config) == NULL);

    /* A date that is not set cannot be written. */
    sdfio_header header;
    sdfio_header_init(&header);
    header.columns = 1;
    header.rows = 1;
    header.creation_date = SDFIO_NO_TIME;
    sdfio_writer *writer = sdfio_writer_new(&all_codecs);
    memory_sink sink = {0};
    const sdfio_stream stream = {&sink, NULL, sink_write};
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

    /* Text is written with its codec and refused without it. */
    sdfio_header_init(&header);
    header.representation = SDFIO_ASCII;
    header.columns = 1;
    header.rows = 1;
    writer = sdfio_writer_new(&all_codecs);
    CHECK_OK(sdfio_create_stream(writer, &stream, &header));
    sdfio_writer_free(writer);
    free(sink.data);
    sink = (memory_sink){0};
    const sdfio_config binary_only = {.codecs = {&sdfio_binary_codec}};
    writer = sdfio_writer_new(&binary_only);
    CHECK_STATUS(SDFIO_ERR_UNSUPPORTED,
                 sdfio_create_stream(writer, &stream, &header));
    sdfio_writer_free(writer);
    free(sink.data);

    /* A file is read only if its codec is registered. */
    static const char ascii_magic[] = "aISO-2.0\r\n";
    sdfio_reader *reader = sdfio_reader_new(&binary_only);
    CHECK_STATUS(SDFIO_ERR_UNSUPPORTED,
                 sdfio_open_memory(reader, ascii_magic, sizeof ascii_magic));
    sdfio_reader_free(reader);
    reader = sdfio_reader_new(&all_codecs);
    CHECK_STATUS(SDFIO_ERR_FORMAT,
                 sdfio_open_memory(reader, ascii_magic, sizeof ascii_magic));
    sdfio_reader_free(reader);

    /* A configuration needs a codec, each only once and without a gap. */
    CHECK(sdfio_reader_new(NULL) == NULL);
    CHECK(sdfio_writer_new(NULL) == NULL);
    const sdfio_config none = {0};
    CHECK(sdfio_reader_new(&none) == NULL);
    const sdfio_config twice = {
        .codecs = {&sdfio_binary_codec, &sdfio_binary_codec}};
    CHECK(sdfio_writer_new(&twice) == NULL);
    const sdfio_config gap = {.codecs = {NULL, &sdfio_binary_codec}};
    CHECK(sdfio_reader_new(&gap) == NULL);
}

/* The magics of a configuration: those of its codecs, in their order. */
static void check_magics(void)
{
    CHECK(sdfio_magic_count(&all_codecs) == 4U);
    CHECK(strcmp(sdfio_magic(&all_codecs, 0), "bISO-1.0") == 0);
    CHECK(strcmp(sdfio_magic(&all_codecs, 1), "bISO-2.0") == 0);
    CHECK(strcmp(sdfio_magic(&all_codecs, 2), "aISO-1.0") == 0);
    CHECK(strcmp(sdfio_magic(&all_codecs, 3), "aISO-2.0") == 0);
    for (size_t i = 0; i < sdfio_magic_count(&all_codecs); ++i)
    {
        CHECK(strlen(sdfio_magic(&all_codecs, i)) == SDFIO_MAGIC_SIZE);
    }
    CHECK(sdfio_magic(&all_codecs, sdfio_magic_count(&all_codecs)) == NULL);

    const sdfio_config ascii_first = {
        .codecs = {&sdfio_ascii_codec, &sdfio_binary_codec}};
    CHECK(strcmp(sdfio_magic(&ascii_first, 0), "aISO-1.0") == 0);
    const sdfio_config binary_only = {.codecs = {&sdfio_binary_codec}};
    CHECK(sdfio_magic_count(&binary_only) == 2U);
    CHECK(sdfio_magic(&binary_only, 2) == NULL);
    CHECK(sdfio_magic_count(NULL) == 0U);
    CHECK(sdfio_magic(NULL, 0) == NULL);
}

int main(void)
{
    check_magics();
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
