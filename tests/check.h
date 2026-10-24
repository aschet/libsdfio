/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* A small harness: a failed check is counted and reported, and the test goes
 * on. The program exits with the number of failures. */

#ifndef SDFIO_TESTS_CHECK_H
#define SDFIO_TESTS_CHECK_H

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "options.h"
#include <sdfio/sdfio.h>

/* The options with them, created where they are used: a program that links the
 * DLL cannot take the address of its formats in a constant initializer. */
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

static inline bool near_relative(double a, double b, double relative)
{
    const double scale = fmax(1e-12, fmax(fabs(a), fabs(b)));
    return fabs(a - b) <= relative * scale || fabs(a - b) <= 1e-18;
}

/* A file in memory that a writer fills. */
/** The size of the binary header of ISO-2.0, as the standard lays it out: the
 * magic of 8 bytes, the identifier of 10, two datetimes of 12, the
 * numbers of columns and of rows of 4 each, four binary64 numbers, and 3 bytes
 * of codes. */
enum
{
    ISO_2_0_BINARY_HEADER_SIZE = 8 + 10 + 2 * 12 + 2 * 4 + 4 * 8 + 3,
    /** Where the number of columns and the number of rows start. */
    ISO_2_0_COLUMNS_AT = 8 + 10 + 2 * 12
};

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

/* The layout of an array whose rows are `row_stride` bytes apart. */
static inline sdfio_layout layout_with_rows(sdfio_data_type type,
                                            ptrdiff_t row_stride)
{
    sdfio_layout layout = *sdfio_layout_packed(type);
    layout.row_stride = row_stride;
    return layout;
}

/* A reader or a writer for options that are valid. */
static inline sdfio_reader *new_reader(const test_options *options)
{
    sdfio_reader *reader = NULL;
    CHECK_OK(create_reader(options, &reader));
    return reader;
}

static inline sdfio_writer *new_writer(const test_options *options)
{
    sdfio_writer *writer = NULL;
    CHECK_OK(create_writer(options, &writer));
    return writer;
}

enum
{
    /* The room for the headers that a test creates. */
    HEADER_ROOM = 256
};

static sdfio_header *created_headers[HEADER_ROOM];
static size_t created_header_count = 0;

static void free_created_headers(void)
{
    for (size_t i = 0; i < created_header_count; ++i)
    {
        sdfio_header_free(created_headers[i]);
    }
}

/* A header with the defaults and dates, which a file needs. The test ends by
 * freeing it, so a test does not pair every header with a call. The function is
 * not thread safe. */
static inline sdfio_header *new_header(void)
{
    sdfio_header *header = NULL;
    CHECK_OK(sdfio_header_create(sdfio_default_allocator(), &header));
    CHECK_OK(sdfio_header_set_created(header, 1772600760));
    CHECK_OK(sdfio_header_set_modified(header, 1772604360));
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

/* Frees a reader and opens the file in memory again, to read it from the
 * start. */
static inline sdfio_reader *membuf_reopen(sdfio_reader *reader,
                                          const membuf *file)
{
    sdfio_reader_free(reader);
    reader = new_reader(all_formats());
    CHECK_OK(sdfio_open_memory(reader, file->data, file->size));
    return reader;
}

enum
{
    /* The room for the trailers of the tests. */
    TRAILER_ROOM = 65536
};

/* The text that a reader returns for a text written to the trailer: CRLF reads
 * as LF, and in an ASCII file the line that is open at the end of the text is
 * terminated, which sdfio_end_file() does there. The text is in a buffer of the
 * function and is valid until the next call. */
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

/* The trailer of a reader, read in calls of `part` bytes at most, or with the
 * whole room for 0. The text is in a buffer of the function, ends with a 0
 * character, and is valid until the next call. */
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
