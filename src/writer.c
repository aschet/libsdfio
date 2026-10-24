/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file writer.c
 * @brief Life of a writer: creation, file start and release.
 */

#include "allocation.h"
#include "data_type.h"
#include "defaults.h"
#include "header.h"
#include "writer_file.h"
#include "writer_state.h"
#include <string.h>

sdfio_status sdfio_writer_create(const sdfio_allocator *allocator,
                                 sdfio_writer **writer)
{
    if (writer == NULL)
    {
        return SDFIO_ERR_ARGUMENT;
    }
    *writer = NULL;
    sdfio_allocator used;
    if (!sdfio_allocator_take(allocator, &used))
    {
        return SDFIO_ERR_ARGUMENT;
    }
    sdfio_writer *handle = sdfio_allocate(&used, sizeof *handle);
    if (handle == NULL)
    {
        return SDFIO_ERR_MEMORY;
    }
    *handle = (sdfio_writer){.allocator = used,
                             .chunk_values = SDFIO_DEFAULT_CHUNK,
                             .buffer_size = SDFIO_DEFAULT_BUFFER};
    *writer = handle;
    return SDFIO_OK;
}

/* Whether the buffers were made, which the first attempt to create a file
 * does: the options are fixed from then on. */
static bool buffers_made(const sdfio_writer *writer)
{
    return writer->coded != NULL || writer->sink.buffer != NULL;
}

/* The precondition of every setter: a writer and no attempt to create a file.
 */
static sdfio_status require_new(sdfio_writer *writer)
{
    if (writer == NULL)
    {
        return SDFIO_ERR_ARGUMENT;
    }
    if (buffers_made(writer))
    {
        return sdfio_fail(&writer->message, SDFIO_ERR_STATE,
                          "Options can be set only before a file is created");
    }
    return SDFIO_OK;
}

sdfio_status sdfio_writer_install_format(sdfio_writer *writer,
                                         const sdfio_writer_format *format)
{
    const sdfio_status status = require_new(writer);
    if (status != SDFIO_OK)
    {
        return status;
    }
    for (size_t i = 0; i < SDFIO_FORMAT_MAX; ++i)
    {
        if (writer->formats[i] == format)
        {
            return sdfio_fail(&writer->message, SDFIO_ERR_ARGUMENT,
                              "The format has been added already");
        }
        if (writer->formats[i] == NULL)
        {
            writer->formats[i] = format;
            return SDFIO_OK;
        }
    }
    return sdfio_fail(&writer->message, SDFIO_ERR_ARGUMENT,
                      "The list of formats is full");
}

sdfio_status sdfio_writer_set_buffer_size(sdfio_writer *writer, size_t size)
{
    const sdfio_status status = require_new(writer);
    if (status != SDFIO_OK)
    {
        return status;
    }
    if (size == 0)
    {
        return sdfio_fail(&writer->message, SDFIO_ERR_ARGUMENT,
                          "The buffer needs a size");
    }
    writer->buffer_size =
        size < SDFIO_MINIMUM_BUFFER ? SDFIO_MINIMUM_BUFFER : size;
    return SDFIO_OK;
}

sdfio_status sdfio_writer_set_chunk_values(sdfio_writer *writer, size_t count)
{
    const sdfio_status status = require_new(writer);
    if (status != SDFIO_OK)
    {
        return status;
    }
    if (!sdfio_chunk_valid(count))
    {
        return sdfio_fail(&writer->message, SDFIO_ERR_ARGUMENT,
                          "The number of values of a chunk is 0 or too large");
    }
    writer->chunk_values = count;
    return SDFIO_OK;
}

sdfio_status sdfio_writer_set_time_zone(sdfio_writer *writer,
                                        const sdfio_time_zone *time_zone)
{
    const sdfio_status status = require_new(writer);
    if (status != SDFIO_OK)
    {
        return status;
    }
    writer->zone = time_zone != NULL ? *time_zone : (sdfio_time_zone){0};
    return SDFIO_OK;
}

void sdfio_writer_free(sdfio_writer *writer)
{
    if (writer == NULL)
    {
        return;
    }
    if (writer->owner.close != NULL)
    {
        writer->owner.close(writer->owner.user);
    }
    sdfio_sink_destroy(&writer->sink);
    const sdfio_allocator allocator = writer->allocator;
    sdfio_release(&allocator, writer->coded);
    sdfio_release(&allocator, writer->scratch);
    sdfio_release(&allocator, writer->format_state);
    sdfio_release(&allocator, writer);
}

void sdfio_writer_own(sdfio_writer *writer, sdfio_owner owner)
{
    writer->owner = owner;
}

sdfio_message *sdfio_writer_message(sdfio_writer *writer)
{
    return &writer->message;
}

const char *sdfio_writer_error_message(const sdfio_writer *writer)
{
    return writer != NULL && writer->message.text != NULL ? writer->message.text
                                                          : "";
}

sdfio_status sdfio_writer_fail(sdfio_writer *writer, sdfio_status status)
{
    writer->state = SDFIO_WRITER_FAILED;
    return status;
}

/* The added writer format of a file format, or null. */
static const sdfio_writer_format *
writer_format_for(const sdfio_writer_format *const *formats,
                  sdfio_format format)
{
    for (size_t i = 0; i < SDFIO_FORMAT_MAX && formats[i] != NULL; ++i)
    {
        if (formats[i]->writes == format)
        {
            return formats[i];
        }
    }
    return NULL;
}

sdfio_status sdfio_writer_check(sdfio_writer *writer,
                                const sdfio_header *header)
{
    if (writer->state != SDFIO_WRITER_NEW)
    {
        return sdfio_fail(&writer->message, SDFIO_ERR_STATE,
                          "The writer has created a file already");
    }
    const sdfio_status status =
        sdfio_header_validate_for_writing(header, &writer->message);
    if (status != SDFIO_OK)
    {
        return status;
    }
    if (writer_format_for(writer->formats, header->format) == NULL)
    {
        return sdfio_fail(&writer->message, SDFIO_ERR_UNSUPPORTED,
                          "The format of the header has not been added to "
                          "the writer");
    }
    return SDFIO_OK;
}

/* Checks the header, and prepares the writer for the format of the file. */
static sdfio_status start_stream(sdfio_writer *writer,
                                 const sdfio_stream *stream,
                                 const sdfio_header *header)
{
    const sdfio_status status = sdfio_writer_check(writer, header);
    if (status != SDFIO_OK)
    {
        return status;
    }
    const sdfio_writer_format *format =
        writer_format_for(writer->formats, header->format);
    const size_t scratch = format->scratch_size(writer->chunk_values);
    if (writer->coded == NULL)
    {
        writer->coded =
            sdfio_allocate(&writer->allocator,
                           writer->chunk_values * SDFIO_LARGEST_CODED_VALUE);
    }
    if (writer->scratch == NULL && scratch > 0)
    {
        writer->scratch = sdfio_allocate(&writer->allocator, scratch);
    }
    if (writer->format_state == NULL && format->state_size > 0)
    {
        writer->format_state =
            sdfio_allocate(&writer->allocator, format->state_size);
    }
    if (writer->coded == NULL || (writer->scratch == NULL && scratch > 0)
        || (writer->format_state == NULL && format->state_size > 0)
        || (writer->sink.buffer == NULL
            && !sdfio_sink_create(&writer->sink, writer->buffer_size,
                                  &writer->allocator)))
    {
        return sdfio_fail(&writer->message, SDFIO_ERR_MEMORY, "Out of memory");
    }
    if (format->state_size > 0)
    {
        memset(writer->format_state, 0, format->state_size);
    }
    writer->header = *header;
    writer->format = format;
    sdfio_sink_init(&writer->sink, stream);
    writer->state = SDFIO_WRITER_CREATED;
    return SDFIO_OK;
}

sdfio_status sdfio_create_stream(sdfio_writer *writer,
                                 const sdfio_stream *stream,
                                 const sdfio_header *header)
{
    if (writer == NULL || stream == NULL || stream->write == NULL
        || header == NULL)
    {
        return SDFIO_ERR_ARGUMENT;
    }
    return start_stream(writer, stream, header);
}

sdfio_status sdfio_writer_require_writable(sdfio_writer *writer)
{
    if (writer == NULL)
    {
        return SDFIO_ERR_ARGUMENT;
    }
    if (writer->state == SDFIO_WRITER_NEW)
    {
        return sdfio_fail(&writer->message, SDFIO_ERR_STATE,
                          "Create a file first");
    }
    if (writer->state != SDFIO_WRITER_CREATED)
    {
        return sdfio_fail(&writer->message, SDFIO_ERR_STATE,
                          "The file is ended, or an error ended the writing");
    }
    return SDFIO_OK;
}

sdfio_status sdfio_writer_sink_failed(sdfio_writer *writer)
{
    return sdfio_writer_fail(writer, sdfio_sink_failure(&writer->message));
}
