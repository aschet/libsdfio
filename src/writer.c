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
#include "header.h"
#include "settings.h"
#include "writer_file.h"
#include "writer_state.h"
#include <string.h>

sdfio_writer *sdfio_writer_new(const sdfio_config *config)
{
    sdfio_settings settings;
    if (!sdfio_config_resolve(config, &settings)
        || settings.writer_formats[0] == NULL)
    {
        return NULL;
    }
    sdfio_writer *writer = sdfio_allocate(&settings.allocator, sizeof *writer);
    if (writer == NULL)
    {
        return NULL;
    }
    *writer = (sdfio_writer){.allocator = settings.allocator,
                             .zone = settings.zone,
                             .chunk_values = settings.chunk};
    for (size_t i = 0; i < SDFIO_FORMAT_MAX; ++i)
    {
        writer->formats[i] = settings.writer_formats[i];
    }
    writer->coded = sdfio_allocate(&writer->allocator,
                                   settings.chunk * SDFIO_LARGEST_DATA_ENTRY);
    if (writer->coded == NULL
        || !sdfio_sink_create(&writer->sink, settings.buffer,
                              &writer->allocator))
    {
        sdfio_writer_free(writer);
        return NULL;
    }
    return writer;
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

/* The listed writer format of a representation, or null. */
static const sdfio_writer_format *
writer_format_for(const sdfio_writer_format *const *formats,
                  sdfio_representation representation)
{
    for (size_t i = 0; i < SDFIO_FORMAT_MAX && formats[i] != NULL; ++i)
    {
        if (formats[i]->writes == representation)
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
    if (writer_format_for(writer->formats, header->representation) == NULL)
    {
        return sdfio_fail(&writer->message, SDFIO_ERR_UNSUPPORTED,
                          "The format of the representation is not listed in "
                          "the configuration");
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
        writer_format_for(writer->formats, header->representation);
    const size_t scratch = format->scratch_size(writer->chunk_values);
    if (scratch > 0)
    {
        writer->scratch = sdfio_allocate(&writer->allocator, scratch);
        if (writer->scratch == NULL)
        {
            return sdfio_fail(&writer->message, SDFIO_ERR_MEMORY,
                              "Out of memory");
        }
    }
    if (format->state_size > 0)
    {
        writer->format_state =
            sdfio_allocate(&writer->allocator, format->state_size);
        if (writer->format_state == NULL)
        {
            return sdfio_fail(&writer->message, SDFIO_ERR_MEMORY,
                              "Out of memory");
        }
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
