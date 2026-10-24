/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* The options of a reader or a writer in one structure, so that a test lists
 * them where it makes the handle. The library sets them one by one. */

#ifndef SDFIO_TESTS_OPTIONS_H
#define SDFIO_TESTS_OPTIONS_H

#include <sdfio/sdfio.h>

/* A format that a handle lists, or none. */
typedef enum test_format
{
    TEST_NO_FORMAT,
    TEST_BINARY,
    TEST_ASCII
} test_format;

/* Zero selects the default of an option, and the formats are listed. A null
 * allocator selects the allocator of the library. */
typedef struct test_options
{
    const sdfio_allocator *allocator;
    size_t buffer_size;
    size_t chunk_values;
    const sdfio_time_zone *time_zone;
    test_format reader_formats[2];
    test_format writer_formats[2];
} test_options;

/* The formats of a handle that reads and writes both formats. */
#define ALL_FORMATS                                                            \
    .reader_formats = {TEST_BINARY, TEST_ASCII},                               \
    .writer_formats = {TEST_BINARY, TEST_ASCII}

/* Lists a format of a reader or a writer. */
static inline sdfio_status add_reader_format(sdfio_reader *reader,
                                             test_format format)
{
    switch (format)
    {
    case TEST_BINARY:
        return sdfio_reader_add_binary(reader);
    case TEST_ASCII:
        return sdfio_reader_add_ascii(reader);
    case TEST_NO_FORMAT:
        break;
    }
    return SDFIO_OK;
}

static inline sdfio_status add_writer_format(sdfio_writer *writer,
                                             test_format format)
{
    switch (format)
    {
    case TEST_BINARY:
        return sdfio_writer_add_binary(writer);
    case TEST_ASCII:
        return sdfio_writer_add_ascii(writer);
    case TEST_NO_FORMAT:
        break;
    }
    return SDFIO_OK;
}

/* A reader with the options. The reader is freed if an option fails. */
static inline sdfio_status create_reader(const test_options *options,
                                         sdfio_reader **reader)
{
    sdfio_status status = sdfio_reader_create(options->allocator != NULL
                                                  ? options->allocator
                                                  : sdfio_default_allocator(),
                                              reader);
    for (size_t i = 0; status == SDFIO_OK && i < 2; ++i)
    {
        status = add_reader_format(*reader, options->reader_formats[i]);
    }
    if (status == SDFIO_OK && options->buffer_size != 0)
    {
        status = sdfio_reader_set_buffer(*reader, options->buffer_size);
    }
    if (status == SDFIO_OK && options->chunk_values != 0)
    {
        status = sdfio_reader_set_chunk(*reader, options->chunk_values);
    }
    if (status == SDFIO_OK && options->time_zone != NULL)
    {
        status = sdfio_reader_set_time_zone(*reader, options->time_zone);
    }
    if (status != SDFIO_OK)
    {
        sdfio_reader_free(*reader);
        *reader = NULL;
    }
    return status;
}

/* A writer with the options. The writer is freed if an option fails. */
static inline sdfio_status create_writer(const test_options *options,
                                         sdfio_writer **writer)
{
    sdfio_status status = sdfio_writer_create(options->allocator != NULL
                                                  ? options->allocator
                                                  : sdfio_default_allocator(),
                                              writer);
    for (size_t i = 0; status == SDFIO_OK && i < 2; ++i)
    {
        status = add_writer_format(*writer, options->writer_formats[i]);
    }
    if (status == SDFIO_OK && options->buffer_size != 0)
    {
        status = sdfio_writer_set_buffer(*writer, options->buffer_size);
    }
    if (status == SDFIO_OK && options->chunk_values != 0)
    {
        status = sdfio_writer_set_chunk(*writer, options->chunk_values);
    }
    if (status == SDFIO_OK && options->time_zone != NULL)
    {
        status = sdfio_writer_set_time_zone(*writer, options->time_zone);
    }
    if (status != SDFIO_OK)
    {
        sdfio_writer_free(*writer);
        *writer = NULL;
    }
    return status;
}

#endif /* SDFIO_TESTS_OPTIONS_H */
