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

#include <sdfio/sdfio.h>

/* The configuration of a handle that reads and writes both representations. */
#define ALL_CODECS                                                             \
    .reader_codecs = {&sdfio_binary_reader_codec, &sdfio_ascii_reader_codec},  \
    .writer_codecs = {&sdfio_binary_writer_codec, &sdfio_ascii_writer_codec}
static const sdfio_config all_codecs = {ALL_CODECS};

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

/* The format of an array whose rows are `row_stride` bytes apart. */
static inline sdfio_format format_with_rows(sdfio_data_type type,
                                            ptrdiff_t row_stride)
{
    sdfio_format format = sdfio_format_packed(type);
    format.row_stride = row_stride;
    return format;
}

/* Frees a reader and opens the file in memory again, to read it from the
 * start. */
static inline sdfio_reader *membuf_reopen(sdfio_reader *reader,
                                          const membuf *file)
{
    sdfio_reader_free(reader);
    reader = sdfio_reader_new(&all_codecs);
    CHECK_OK(sdfio_open_memory(reader, file->data, file->size));
    return reader;
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
