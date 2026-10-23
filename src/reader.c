/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file reader.c
 * @brief Life of a reader: creation, opening and release.
 */

#include "data_type.h"
#include "header.h"
#include "memory.h"
#include "read_header.h"
#include "reader_file.h"
#include "reader_state.h"
#include "settings.h"
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

sdfio_reader *sdfio_reader_new(const sdfio_config *config)
{
    sdfio_settings settings;
    if (!sdfio_config_resolve(config, &settings)
        || settings.reader_codecs[0] == NULL)
    {
        return NULL;
    }
    sdfio_reader *reader = sdfio_allocate(&settings.allocator, sizeof *reader);
    if (reader == NULL)
    {
        return NULL;
    }
    *reader = (sdfio_reader){.allocator = settings.allocator,
                             .zone = settings.zone,
                             .chunk_values = settings.chunk};
    for (size_t i = 0; i < SDFIO_CODEC_MAX; ++i)
    {
        reader->codecs[i] = settings.reader_codecs[i];
    }
    reader->trailer.allocator = &reader->allocator;
    reader->chunk = sdfio_allocate(&reader->allocator,
                                   settings.chunk * SDFIO_LARGEST_SAMPLE);
    if (reader->chunk == NULL
        || !sdfio_source_create(&reader->source, settings.buffer,
                                &reader->allocator))
    {
        sdfio_reader_free(reader);
        return NULL;
    }
    return reader;
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
    sdfio_string_free(&reader->trailer);
    sdfio_source_destroy(&reader->source);
    const sdfio_allocator allocator = reader->allocator;
    sdfio_release(&allocator, reader->chunk);
    sdfio_release(&allocator, reader->codec_state);
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

static sdfio_status open_source(sdfio_reader *reader,
                                const sdfio_stream *stream)
{
    if (reader->opened)
    {
        return sdfio_fail(&reader->message, SDFIO_ERR_STATE,
                          "The reader is open already");
    }
    sdfio_source_init(&reader->source, stream);
    sdfio_status status = sdfio_select_reader_codec(
        &reader->source, reader->codecs, &reader->codec, &reader->message);
    if (status != SDFIO_OK)
    {
        return status;
    }
    sdfio_release(&reader->allocator, reader->codec_state);
    reader->codec_state = NULL;
    if (reader->codec->state_size > 0)
    {
        reader->codec_state =
            sdfio_allocate(&reader->allocator, reader->codec->state_size);
        if (reader->codec_state == NULL)
        {
            return sdfio_fail(&reader->message, SDFIO_ERR_MEMORY,
                              "Out of memory");
        }
        memset(reader->codec_state, 0, reader->codec->state_size);
    }
    status =
        sdfio_read_header(&reader->source, reader->codec, &reader->header,
                          reader->codec_state, &reader->zone, &reader->message);
    if (status != SDFIO_OK)
    {
        return status;
    }
    const sdfio_header *header = &reader->header;
    const size_t type_size = sdfio_data_type_size(header->data_type);
    if (!sdfio_matrix_elements(header, &reader->value_count)
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
