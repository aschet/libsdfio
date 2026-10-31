/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef SDFIO_TESTS_CHECK_H
#define SDFIO_TESTS_CHECK_H

#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_options.h"
#include <sdfio/sdfio.h>

/* Options of a reader or a writer that add the binary and the ASCII format. */
#define all_formats() (&(const test_options){ALL_FORMATS})

static int failures = 0;
static int checks = 0;

#define CHECK(condition)                                                       \
    do                                                                         \
    {                                                                          \
        ++checks;                                                              \
        if (!(condition))                                                      \
        {                                                                      \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__,   \
                    #condition);                                               \
            ++failures;                                                        \
        }                                                                      \
    } while (0)

#define CHECK_STATUS(expected, call)                                           \
    do                                                                         \
    {                                                                          \
        const sdfio_status actual_status = (call);                             \
        ++checks;                                                              \
        if (actual_status != (expected))                                       \
        {                                                                      \
            fprintf(stderr, "%s:%d: %s returned \"%s\", expected \"%s\"\n",    \
                    __FILE__, __LINE__, #call,                                 \
                    sdfio_status_string(actual_status),                        \
                    sdfio_status_string(expected));                            \
            ++failures;                                                        \
        }                                                                      \
    } while (0)

#define CHECK_OK(call) CHECK_STATUS(SDFIO_OK, call)

#define SETUP(call)                                                            \
    do                                                                         \
    {                                                                          \
        const sdfio_status setup_status = (call);                              \
        if (setup_status != SDFIO_OK)                                          \
        {                                                                      \
            fprintf(stderr, "%s:%d: setup %s returned \"%s\"\n", __FILE__,     \
                    __LINE__, #call, sdfio_status_string(setup_status));       \
            exit(EXIT_FAILURE);                                                \
        }                                                                      \
    } while (0)

static inline bool near_relative(double a, double b, double relative)
{
    const double scale = fmax(1e-12, fmax(fabs(a), fabs(b)));
    return fabs(a - b) <= relative * scale || fabs(a - b) <= 1e-18;
}

/* Reads a file completely, followed by a 0 byte that the size does not include.
 * If the file cannot be read, the check fails and the result is null. */
static inline unsigned char *load_file(const char *path, size_t *size)
{
    *size = 0;
    FILE *file = fopen(path, "rb");
    CHECK(file != NULL);
    if (file == NULL)
    {
        return NULL;
    }
    fseek(file, 0, SEEK_END);
    const long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    unsigned char *bytes = length < 0 ? NULL : malloc((size_t)length + 1);
    CHECK(bytes != NULL);
    if (bytes != NULL
        && fread(bytes, 1, (size_t)length, file) == (size_t)length)
    {
        bytes[length] = 0;
        *size = (size_t)length;
    }
    else
    {
        free(bytes);
        bytes = NULL;
    }
    fclose(file);
    return bytes;
}

typedef struct counting
{
    /* Allocations to allow before one fails; negative for no failure. */
    long allow;
    /* The calls of the allocation function, also the failed ones. */
    long calls;
    long released;
    long live;
} counting;

#define COUNTING_UNLIMITED {-1, 0, 0, 0}

static inline void *counting_allocate(void *user, size_t size)
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

static inline void counting_release(void *user, void *memory)
{
    counting *state = user;
    ++state->released;
    --state->live;
    free(memory);
}

/* The widths of the identifier and of a date in the header, in bytes. */
#define IDENTIFIER_SIZE 10
#define DATE_SIZE 12

/* The data types that the revisions permit: ISO-1.0 has int16, int32 and
 * binary64, ISO-2.0 adds int8 and binary32. */
static inline bool revision_permits(sdfio_revision revision,
                                    sdfio_data_type type)
{
    return type == SDFIO_TYPE_INT16 || type == SDFIO_TYPE_INT32
           || type == SDFIO_TYPE_DOUBLE
           || (revision == SDFIO_ISO_2_0
               && (type == SDFIO_TYPE_INT8 || type == SDFIO_TYPE_FLOAT));
}

/** The size of the binary header of ISO-2.0, as the standard lays it out: the
 * magic of 8 bytes, the identifier of 10, two datetimes of 12, the
 * numbers of columns and of rows of 4 each, four binary64 numbers, and 3 bytes
 * of codes. */
#define ISO_2_0_BINARY_HEADER_SIZE (8 + 10 + 2 * 12 + 2 * 4 + 4 * 8 + 3)
#define ISO_2_0_COLUMNS_AT (8 + 10 + 2 * 12)

static inline void put_double_in_file(unsigned char *bytes, double value)
{
    uint64_t bits = 0;
    memcpy(&bits, &value, sizeof bits);
    for (size_t i = 0; i < sizeof bits; ++i)
    {
        bytes[i] = (unsigned char)(bits >> (CHAR_BIT * i));
    }
}

typedef struct membuf
{
    unsigned char *data;
    size_t size;
    size_t capacity;
} membuf;

static inline size_t membuf_write(void *user, const void *data, size_t size)
{
    membuf *buffer = user;
    if (buffer->size + size > buffer->capacity)
    {
        size_t capacity = buffer->capacity == 0 ? 256 : buffer->capacity;
        while (capacity < buffer->size + size)
        {
            capacity *= 2;
        }
        unsigned char *grown = realloc(buffer->data, capacity);
        if (grown == NULL)
        {
            return 0;
        }
        buffer->data = grown;
        buffer->capacity = capacity;
    }
    memcpy(buffer->data + buffer->size, data, size);
    buffer->size += size;
    return size;
}

static inline sdfio_stream membuf_stream(membuf *buffer)
{
    sdfio_stream stream = {0};
    stream.user = buffer;
    stream.write = membuf_write;
    return stream;
}

static inline sdfio_layout layout_with_rows(sdfio_data_type type,
                                            ptrdiff_t row_stride)
{
    sdfio_layout layout = *sdfio_layout_packed(type);
    layout.row_stride = row_stride;
    return layout;
}

/* The number of heights of a header, computed in size_t so that the product of
 * the unsigned 32-bit rows and columns cannot overflow before the conversion.
 */
static inline size_t height_count(const sdfio_header *header)
{
    return (size_t)sdfio_header_rows(header) * sdfio_header_columns(header);
}

static inline sdfio_reader *new_reader(const test_options *options)
{
    sdfio_reader *reader = NULL;
    SETUP(create_reader(options, &reader));
    return reader;
}

static inline sdfio_writer *new_writer(const test_options *options)
{
    sdfio_writer *writer = NULL;
    SETUP(create_writer(options, &writer));
    return writer;
}

#define HEADER_ROOM 256

static sdfio_header *created_headers[HEADER_ROOM];
static size_t created_header_count = 0;

static void free_created_headers(void)
{
    for (size_t i = 0; i < created_header_count; ++i)
    {
        sdfio_header_free(created_headers[i]);
    }
}

/* Creates a header with the default fields and both dates, which a file
 * requires. The header is freed when the program ends, so a test does not free
 * it. The function is not thread safe. */
static inline sdfio_header *new_header(void)
{
    sdfio_header *header = NULL;
    SETUP(sdfio_header_create(sdfio_default_allocator(), &header));
    SETUP(sdfio_header_set_created(header, 1772600760));
    SETUP(sdfio_header_set_modified(header, 1772604360));
    if (created_header_count == 0)
    {
        atexit(free_created_headers);
    }
    if (created_header_count < HEADER_ROOM)
    {
        created_headers[created_header_count++] = header;
    }
    else
    {
        fprintf(stderr, "check.h: more than %d headers\n", HEADER_ROOM);
        exit(EXIT_FAILURE);
    }
    return header;
}

/* A header of 2 columns and 2 rows of float values with the z-scale 1. */
static inline sdfio_header *small_header(void)
{
    sdfio_header *header = new_header();
    SETUP(sdfio_header_set_data_type(header, SDFIO_TYPE_FLOAT));
    SETUP(sdfio_header_set_columns(header, 2));
    SETUP(sdfio_header_set_rows(header, 2));
    SETUP(sdfio_header_set_z_scale(header, 1.0));
    return header;
}

/* A reader with all formats that has opened the file in memory. */
static inline sdfio_reader *open_memory_file(const membuf *file)
{
    sdfio_reader *reader = new_reader(all_formats());
    SETUP(sdfio_open_memory(reader, file->data, file->size));
    return reader;
}

/* Writes a file with all formats to memory: the header and the heights of the
 * layout. */
static inline void write_memory_file(membuf *file, const sdfio_header *header,
                                     const sdfio_layout *layout,
                                     const void *heights)
{
    sdfio_writer *writer = new_writer(all_formats());
    const sdfio_stream stream = membuf_stream(file);
    SETUP(sdfio_create_stream(writer, &stream, header));
    SETUP(sdfio_write_heights(writer, layout, heights));
    SETUP(sdfio_end_file(writer));
    sdfio_writer_free(writer);
}

/* Frees the reader and returns a new one that has opened the file in memory
 * again, so that it reads from the start. */
static inline sdfio_reader *membuf_reopen(sdfio_reader *reader,
                                          const membuf *file)
{
    sdfio_reader_free(reader);
    reader = new_reader(all_formats());
    SETUP(sdfio_open_memory(reader, file->data, file->size));
    return reader;
}

#define TRAILER_ROOM 65536

/* Converts a trailer text as written to the text that a reader returns: CRLF
 * becomes LF, and in an ASCII file a last line without a line end gets one, as
 * sdfio_end_file() ends it. The result is in a static buffer and is valid until
 * the next call. */
static inline const char *expected_read(sdfio_format format,
                                        const char *written)
{
    static char text[4096];
    size_t size = 0;
    for (size_t i = 0; written[i] != '\0'; ++i)
    {
        if (written[i] != '\r' || written[i + 1] != '\n')
        {
            text[size++] = written[i];
        }
    }
    if (format == SDFIO_ASCII && size > 0 && text[size - 1] != '\n')
    {
        text[size++] = '\n';
    }
    text[size] = '\0';
    return text;
}

/* Reads the whole trailer of a reader, in calls of at most `part` bytes, or of
 * as many bytes as fit for 0. The text is in a static buffer, ends with a 0
 * character and is valid until the next call. */
static inline sdfio_status read_trailer_text(sdfio_reader *reader, size_t part,
                                             const char **text, size_t *size)
{
    static char room[TRAILER_ROOM];
    size_t used = 0;
    for (;;)
    {
        size_t capacity = sizeof room - 1 - used;
        if (part != 0 && part < capacity)
        {
            capacity = part;
        }
        size_t got = 0;
        const sdfio_status status =
            sdfio_read_trailer(reader, room + used, capacity, &got);
        if (status != SDFIO_OK)
        {
            return status;
        }
        if (got == 0)
        {
            break;
        }
        used += got;
    }
    room[used] = '\0';
    *text = room;
    *size = used;
    return SDFIO_OK;
}

static inline int finish_tests(void)
{
    if (failures != 0)
    {
        fprintf(stderr, "%d of %d checks failed\n", failures, checks);
        return EXIT_FAILURE;
    }
    printf("%d checks passed\n", checks);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

#endif /* SDFIO_TESTS_CHECK_H */
