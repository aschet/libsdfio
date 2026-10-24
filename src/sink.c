/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file sink.c
 * @brief The buffer in front of the stream of a writer.
 */

#include "sink.h"
#include "allocator.h"
#include "message.h"
#include <sdfio/sdfio.h>
#include <string.h>

bool sdfio_sink_create(sdfio_sink *sink, size_t capacity,
                       const sdfio_allocator *allocator)
{
    sink->buffer = sdfio_allocate(allocator, capacity);
    sink->capacity = capacity;
    sink->allocator = allocator;
    return sink->buffer != NULL;
}

void sdfio_sink_destroy(sdfio_sink *sink)
{
    sdfio_release(sink->allocator, sink->buffer);
    sink->buffer = NULL;
}

void sdfio_sink_init(sdfio_sink *sink, const sdfio_stream *io)
{
    sink->io = *io;
    sink->used = 0;
    sink->failed = false;
}

bool sdfio_sink_flush(sdfio_sink *sink)
{
    if (sink->used > 0 && !sink->failed)
    {
        if (sink->io.write == NULL
            || sink->io.write(sink->io.user, sink->buffer, sink->used)
                   != sink->used)
        {
            sink->failed = true;
        }
    }
    sink->used = 0;
    return !sink->failed;
}

bool sdfio_sink_write(sdfio_sink *sink, const void *data, size_t size)
{
    if (sink->failed)
    {
        return false;
    }
    if (size >= sink->capacity)
    {
        if (!sdfio_sink_flush(sink))
        {
            return false;
        }
        if (sink->io.write == NULL
            || sink->io.write(sink->io.user, data, size) != size)
        {
            sink->failed = true;
        }
        return !sink->failed;
    }
    if (size > sink->capacity - sink->used && !sdfio_sink_flush(sink))
    {
        return false;
    }
    memcpy(sink->buffer + sink->used, data, size);
    sink->used += size;
    return true;
}

sdfio_status sdfio_sink_failure(sdfio_message *message)
{
    return sdfio_fail(message, SDFIO_ERR_IO, "The stream could not be written");
}
