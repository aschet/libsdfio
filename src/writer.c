/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief Life of a writer: creation, options, file start, release.
 */

#include "allocator.h"
#include "data_type.h"
#include "defaults.h"
#include "header.h"
#include "message.h"
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
    sdfio_options options;
    if (!sdfio_options_init(&options, allocator))
    {
        return SDFIO_ERR_ARGUMENT;
    }
    sdfio_writer *handle = sdfio_allocate(&options.allocator, sizeof *handle);
    if (handle == NULL)
    {
        return SDFIO_ERR_MEMORY;
    }
    *handle = (sdfio_writer){.options = options};
    *writer = handle;
    return SDFIO_OK;
}

/* Whether the buffers were created, which the first attempt to create a file
 * does: the options are fixed from then on. */
static bool buffers_created(const sdfio_writer *writer)
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
    if (buffers_created(writer))
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

sdfio_status sdfio_writer_set_buffer(sdfio_writer *writer, size_t size)
{
    const sdfio_status status = require_new(writer);
    return status != SDFIO_OK ? status
                              : sdfio_options_set_buffer(&writer->options, size,
                                                         &writer->message);
}

sdfio_status sdfio_writer_set_chunk(sdfio_writer *writer, size_t count)
{
    const sdfio_status status = require_new(writer);
    return status != SDFIO_OK ? status
                              : sdfio_options_set_chunk(&writer->options, count,
                                                        &writer->message);
}

sdfio_status sdfio_writer_set_time_zone(sdfio_writer *writer,
                                        const sdfio_time_zone *time_zone)
{
    const sdfio_status status = require_new(writer);
    if (status == SDFIO_OK)
    {
        sdfio_options_set_zone(&writer->options, time_zone);
    }
    return status;
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
    const sdfio_allocator allocator = writer->options.allocator;
    sdfio_release(&allocator, writer->coded);
    sdfio_release(&allocator, writer->file_chunk);
    sdfio_release(&allocator, writer->format_state);
    sdfio_release(&allocator, writer);
}

sdfio_status sdfio_writer_reset(sdfio_writer *writer)
{
    if (writer == NULL)
    {
        return SDFIO_ERR_ARGUMENT;
    }
    if (writer->owner.close != NULL)
    {
        writer->owner.close(writer->owner.user);
    }
    writer->owner = (sdfio_owner){0};
    writer->message = NULL;
    writer->format = NULL;
    writer->state = SDFIO_WRITER_NEW;
    writer->header_written = false;
    writer->trailer_open = false;
    writer->trailer_text = (sdfio_crlf_state){0};
    writer->next_row = 0;
    return SDFIO_OK;
}

void sdfio_writer_own(sdfio_writer *writer, sdfio_owner owner)
{
    writer->owner = owner;
}

const char **sdfio_writer_message(sdfio_writer *writer)
{
    return &writer->message;
}

const char *sdfio_writer_error_message(const sdfio_writer *writer)
{
    return writer != NULL && writer->message != NULL ? writer->message : "";
}

sdfio_status sdfio_writer_fail(sdfio_writer *writer, sdfio_status status)
{
    writer->state = SDFIO_WRITER_FAILED;
    return status;
}

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

sdfio_status sdfio_writer_estimate_size(const sdfio_writer *writer,
                                        const sdfio_header *header,
                                        size_t *size)
{
    if (writer == NULL || header == NULL || size == NULL
        || sdfio_header_check(header, NULL) != SDFIO_OK)
    {
        return SDFIO_ERR_ARGUMENT;
    }
    const sdfio_writer_format *format =
        writer_format_for(writer->formats, header->format);
    return format != NULL ? format->estimate_size(header, size)
                          : SDFIO_ERR_UNSUPPORTED;
}

sdfio_status sdfio_writer_check(sdfio_writer *writer,
                                const sdfio_header *header)
{
    if (writer->state != SDFIO_WRITER_NEW)
    {
        return sdfio_fail(&writer->message, SDFIO_ERR_STATE,
                          "The writer has created a file already");
    }
    const sdfio_status status = sdfio_header_check(header, &writer->message);
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
    const size_t file_chunk_bytes =
        format->file_chunk_size(writer->options.chunk_values);
    if (writer->coded == NULL)
    {
        writer->coded = sdfio_allocate(&writer->options.allocator,
                                       writer->options.chunk_values
                                           * SDFIO_LARGEST_CODED_VALUE);
    }
    if (writer->file_chunk == NULL && file_chunk_bytes > 0)
    {
        writer->file_chunk =
            sdfio_allocate(&writer->options.allocator, file_chunk_bytes);
    }
    if (writer->format_state_size < format->state_size)
    {
        sdfio_release(&writer->options.allocator, writer->format_state);
        writer->format_state = NULL;
        writer->format_state_size = 0;
        if (format->state_size > 0)
        {
            writer->format_state =
                sdfio_allocate(&writer->options.allocator, format->state_size);
            writer->format_state_size =
                writer->format_state != NULL ? format->state_size : 0;
        }
    }
    if (writer->coded == NULL
        || (writer->file_chunk == NULL && file_chunk_bytes > 0)
        || writer->format_state_size < format->state_size
        || (writer->sink.buffer == NULL
            && !sdfio_sink_create(&writer->sink, writer->options.buffer_size,
                                  &writer->options.allocator)))
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
                          "No file is created");
    }
    if (writer->state != SDFIO_WRITER_CREATED)
    {
        return sdfio_fail(&writer->message, SDFIO_ERR_STATE,
                          "The file is ended, or an error ended the writing");
    }
    return SDFIO_OK;
}

sdfio_status sdfio_writer_sink_failure(sdfio_writer *writer)
{
    return sdfio_writer_fail(writer, sdfio_sink_failure(&writer->message));
}
