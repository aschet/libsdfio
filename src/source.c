/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief Buffer in front of the stream of a reader.
 */

#include "source.h"
#include "allocator.h"
#include "message.h"
#include <sdfio/sdfio.h>
#include <string.h>

bool sdfio_source_create(sdfio_source *source, size_t capacity,
                         const sdfio_allocator *allocator)
{
    source->buffer = sdfio_allocate(allocator, capacity);
    source->capacity = capacity;
    source->allocator = allocator;
    return source->buffer != NULL;
}

void sdfio_source_destroy(sdfio_source *source)
{
    sdfio_release(source->allocator, source->buffer);
    source->buffer = NULL;
}

void sdfio_source_init(sdfio_source *source, const sdfio_stream *io)
{
    source->io = *io;
    source->position = 0;
    source->end = 0;
    source->at_end = false;
    source->failed = false;
    source->no_memory = false;
}

size_t sdfio_source_fill(sdfio_source *source, size_t want)
{
    if (want > source->capacity)
    {
        want = source->capacity;
    }
    if (source->end - source->position >= want)
    {
        return source->end - source->position;
    }
    /* What was consumed is gone from the buffer. */
    if (source->position > 0)
    {
        memmove(source->buffer, source->buffer + source->position,
                source->end - source->position);
        source->end -= source->position;
        source->position = 0;
    }
    while (source->end < want && !source->at_end && !source->failed)
    {
        const size_t count =
            source->io.read(source->io.user, source->buffer + source->end,
                            source->capacity - source->end);
        if (count == SDFIO_STREAM_ERROR)
        {
            source->failed = true;
        }
        else if (count == 0)
        {
            source->at_end = true;
        }
        else
        {
            source->end += count;
        }
    }
    return source->end - source->position;
}

size_t sdfio_source_read(sdfio_source *source, void *dst, size_t size)
{
    unsigned char *out = dst;
    size_t done = 0;
    while (done < size)
    {
        const size_t buffered = source->end - source->position;
        if (buffered > 0)
        {
            const size_t count =
                buffered < size - done ? buffered : size - done;
            memcpy(out + done, source->buffer + source->position, count);
            source->position += count;
            done += count;
            continue;
        }
        if (source->at_end || source->failed)
        {
            break;
        }
        if (size - done >= source->capacity)
        {
            /* A large piece goes straight to its place. */
            source->position = 0;
            source->end = 0;
            const size_t count =
                source->io.read(source->io.user, out + done, size - done);
            if (count == SDFIO_STREAM_ERROR)
            {
                source->failed = true;
                break;
            }
            if (count == 0)
            {
                source->at_end = true;
                break;
            }
            done += count;
            continue;
        }
        if (sdfio_source_fill(source, 1) == 0)
        {
            break;
        }
    }
    return done;
}

sdfio_status sdfio_source_failure(const sdfio_source *source,
                                  const char **message, const char *format_text)
{
    if (source->no_memory)
    {
        return sdfio_fail(message, SDFIO_ERR_MEMORY, "Out of memory");
    }
    if (source->failed)
    {
        return sdfio_fail(message, SDFIO_ERR_IO,
                          "The stream could not be read");
    }
    return sdfio_fail(message, SDFIO_ERR_FORMAT, format_text);
}
