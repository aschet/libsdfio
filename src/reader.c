/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file reader.c
 * @brief Life of a reader: creation, opening and release.
 */

#include "allocation.h"
#include "data_type.h"
#include "defaults.h"
#include "header.h"
#include "read_header.h"
#include "reader_file.h"
#include "reader_state.h"
#include <string.h>

static size_t memory_read(void *user, void *data, size_t size)
{
    memory_stream *memory = user;
    const size_t left = memory->size - memory->position;
    const size_t count = size < left ? size : left;
    if (count > 0)
    {
        memcpy(data, memory->data + memory->position, count);
    }
    memory->position += count;
    return count;
}

sdfio_status sdfio_reader_create(const sdfio_allocator *allocator,
                                 sdfio_reader **reader)
{
    if (reader == NULL)
    {
        return SDFIO_ERR_ARGUMENT;
    }
    *reader = NULL;
    sdfio_allocator used;
    if (!sdfio_allocator_resolve(allocator, &used))
    {
        return SDFIO_ERR_ARGUMENT;
    }
    sdfio_reader *handle = sdfio_allocate(&used, sizeof *handle);
    if (handle == NULL)
    {
        return SDFIO_ERR_MEMORY;
    }
    *handle = (sdfio_reader){.allocator = used,
                             .chunk_values = SDFIO_DEFAULT_CHUNK,
                             .buffer_size = SDFIO_DEFAULT_BUFFER};
    *reader = handle;
    return SDFIO_OK;
}

/* Whether the buffers were made, which the first attempt to open a file does:
 * the options are fixed from then on. */
static bool buffers_made(const sdfio_reader *reader)
{
    return reader->chunk != NULL || reader->source.buffer != NULL;
}

/* The precondition of every setter: a reader and no attempt to open a file. */
static sdfio_status require_new(sdfio_reader *reader)
{
    if (reader == NULL)
    {
        return SDFIO_ERR_ARGUMENT;
    }
    if (buffers_made(reader))
    {
        return sdfio_fail(&reader->message, SDFIO_ERR_STATE,
                          "Options can be set only before a file is opened");
    }
    return SDFIO_OK;
}

sdfio_status sdfio_reader_install_format(sdfio_reader *reader,
                                         const sdfio_reader_format *format)
{
    const sdfio_status status = require_new(reader);
    if (status != SDFIO_OK)
    {
        return status;
    }
    for (size_t i = 0; i < SDFIO_FORMAT_MAX; ++i)
    {
        if (reader->formats[i] == format)
        {
            return sdfio_fail(&reader->message, SDFIO_ERR_ARGUMENT,
                              "The format has been added already");
        }
        if (reader->formats[i] == NULL)
        {
            reader->formats[i] = format;
            return SDFIO_OK;
        }
    }
    return sdfio_fail(&reader->message, SDFIO_ERR_ARGUMENT,
                      "The list of formats is full");
}

sdfio_status sdfio_reader_set_buffer_size(sdfio_reader *reader, size_t size)
{
    const sdfio_status status = require_new(reader);
    if (status != SDFIO_OK)
    {
        return status;
    }
    if (size == 0)
    {
        return sdfio_fail(&reader->message, SDFIO_ERR_ARGUMENT,
                          "The buffer needs a size");
    }
    reader->buffer_size =
        size < SDFIO_MINIMUM_BUFFER ? SDFIO_MINIMUM_BUFFER : size;
    return SDFIO_OK;
}

sdfio_status sdfio_reader_set_chunk_values(sdfio_reader *reader, size_t count)
{
    const sdfio_status status = require_new(reader);
    if (status != SDFIO_OK)
    {
        return status;
    }
    if (!sdfio_chunk_valid(count))
    {
        return sdfio_fail(&reader->message, SDFIO_ERR_ARGUMENT,
                          "The number of values of a chunk is 0 or too large");
    }
    reader->chunk_values = count;
    return SDFIO_OK;
}

sdfio_status sdfio_reader_set_time_zone(sdfio_reader *reader,
                                        const sdfio_time_zone *time_zone)
{
    const sdfio_status status = require_new(reader);
    if (status != SDFIO_OK)
    {
        return status;
    }
    reader->zone = time_zone != NULL ? *time_zone : (sdfio_time_zone){0};
    return SDFIO_OK;
}

size_t sdfio_reader_version_number_count(const sdfio_reader *reader)
{
    size_t count = 0;
    for (size_t i = 0;
         reader != NULL && i < SDFIO_FORMAT_MAX && reader->formats[i] != NULL;
         ++i)
    {
        count += reader->formats[i]->version_number_count;
    }
    return count;
}

const char *sdfio_reader_version_number(const sdfio_reader *reader,
                                        size_t index)
{
    for (size_t i = 0;
         reader != NULL && i < SDFIO_FORMAT_MAX && reader->formats[i] != NULL;
         ++i)
    {
        const sdfio_reader_format *format = reader->formats[i];
        if (index < format->version_number_count)
        {
            return format->version_numbers[index];
        }
        index -= format->version_number_count;
    }
    return NULL;
}

void sdfio_reader_free(sdfio_reader *reader)
{
    if (reader == NULL)
    {
        return;
    }
    if (reader->owner.close != NULL)
    {
        reader->owner.close(reader->owner.user);
    }
    sdfio_source_destroy(&reader->source);
    const sdfio_allocator allocator = reader->allocator;
    sdfio_release(&allocator, reader->chunk);
    sdfio_release(&allocator, reader->format_state);
    sdfio_release(&allocator, reader);
}

void sdfio_reader_own(sdfio_reader *reader, sdfio_owner owner)
{
    reader->owner = owner;
}

sdfio_message *sdfio_reader_message(sdfio_reader *reader)
{
    return &reader->message;
}

bool sdfio_reader_is_open(const sdfio_reader *reader)
{
    return reader->opened;
}

const char *sdfio_reader_error_message(const sdfio_reader *reader)
{
    return reader != NULL && reader->message.text != NULL ? reader->message.text
                                                          : "";
}

const sdfio_header *sdfio_get_header(const sdfio_reader *reader)
{
    return reader != NULL && reader->opened ? &reader->header : NULL;
}

sdfio_status sdfio_reader_fail(sdfio_reader *reader, sdfio_status status)
{
    reader->failed = true;
    return status;
}

/* Selects the format of the stream by its version number, and reads the
 * header. */
static sdfio_status open_source(sdfio_reader *reader,
                                const sdfio_stream *stream)
{
    if (reader->opened)
    {
        return sdfio_fail(&reader->message, SDFIO_ERR_STATE,
                          "The reader is open already");
    }
    if (reader->formats[0] == NULL)
    {
        return sdfio_fail(&reader->message, SDFIO_ERR_ARGUMENT,
                          "List a format before a file is opened");
    }
    if (reader->chunk == NULL)
    {
        reader->chunk =
            sdfio_allocate(&reader->allocator,
                           reader->chunk_values * SDFIO_LARGEST_CODED_VALUE);
    }
    if (reader->chunk == NULL
        || (reader->source.buffer == NULL
            && !sdfio_source_create(&reader->source, reader->buffer_size,
                                    &reader->allocator)))
    {
        return sdfio_fail(&reader->message, SDFIO_ERR_MEMORY, "Out of memory");
    }
    sdfio_source_init(&reader->source, stream);
    sdfio_status status = sdfio_select_reader_format(
        &reader->source, reader->formats, &reader->format, &reader->message);
    if (status != SDFIO_OK)
    {
        return status;
    }
    sdfio_release(&reader->allocator, reader->format_state);
    reader->format_state = NULL;
    if (reader->format->state_size > 0)
    {
        reader->format_state =
            sdfio_allocate(&reader->allocator, reader->format->state_size);
        if (reader->format_state == NULL)
        {
            return sdfio_fail(&reader->message, SDFIO_ERR_MEMORY,
                              "Out of memory");
        }
        memset(reader->format_state, 0, reader->format->state_size);
    }
    status = sdfio_read_header(&reader->source, reader->format, &reader->header,
                               reader->format_state, &reader->zone,
                               &reader->message);
    if (status != SDFIO_OK)
    {
        return status;
    }
    const sdfio_header *header = &reader->header;
    const size_t type_size = sdfio_data_type_size(header->data_type);
    if (!sdfio_coded_values(header, &reader->value_count)
        || reader->value_count > SIZE_MAX / type_size)
    {
        return sdfio_fail(&reader->message, SDFIO_ERR_FORMAT,
                          "The SDF file has more points than can be counted");
    }
    reader->next_row = 0;
    reader->next_value = 0;
    reader->data_done = false;
    reader->failed = false;
    reader->opened = true;
    return SDFIO_OK;
}

sdfio_status sdfio_open_stream(sdfio_reader *reader, const sdfio_stream *stream)
{
    if (reader == NULL || stream == NULL || stream->read == NULL)
    {
        return SDFIO_ERR_ARGUMENT;
    }
    return open_source(reader, stream);
}

sdfio_status sdfio_open_memory(sdfio_reader *reader, const void *data,
                               size_t size)
{
    if (reader == NULL || (data == NULL && size > 0))
    {
        return SDFIO_ERR_ARGUMENT;
    }
    reader->memory = (memory_stream){data, size, 0};
    const sdfio_stream stream = {&reader->memory, memory_read, NULL};
    return open_source(reader, &stream);
}

sdfio_status sdfio_reader_require_readable(sdfio_reader *reader)
{
    if (reader == NULL)
    {
        return SDFIO_ERR_ARGUMENT;
    }
    if (!reader->opened)
    {
        return sdfio_fail(&reader->message, SDFIO_ERR_STATE,
                          "Open a file first");
    }
    if (reader->failed)
    {
        return sdfio_fail(&reader->message, SDFIO_ERR_STATE,
                          "An error ended the reading");
    }
    return SDFIO_OK;
}
