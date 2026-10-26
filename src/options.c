/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief Options that readers and writers share: allocator, time zone, buffer
 * size and chunk size.
 */

#include "options.h"
#include "allocator.h"
#include "defaults.h"
#include "message.h"

bool sdfio_options_init(sdfio_options *options,
                        const sdfio_allocator *allocator)
{
    *options = (sdfio_options){.chunk_values = SDFIO_DEFAULT_CHUNK,
                               .buffer_size = SDFIO_DEFAULT_BUFFER};
    return sdfio_allocator_take(allocator, &options->allocator);
}

sdfio_status sdfio_options_set_buffer(sdfio_options *options, size_t size,
                                      const char **message)
{
    if (size == 0)
    {
        return sdfio_fail(message, SDFIO_ERR_ARGUMENT,
                          "The size of the buffer is 0");
    }
    options->buffer_size =
        size < SDFIO_MINIMUM_BUFFER ? SDFIO_MINIMUM_BUFFER : size;
    return SDFIO_OK;
}

sdfio_status sdfio_options_set_chunk(sdfio_options *options, size_t count,
                                     const char **message)
{
    if (!sdfio_chunk_valid(count))
    {
        return sdfio_fail(message, SDFIO_ERR_ARGUMENT,
                          "The number of values of a chunk is 0 or too large");
    }
    options->chunk_values = count;
    return SDFIO_OK;
}

void sdfio_options_set_zone(sdfio_options *options,
                            const sdfio_time_zone *time_zone)
{
    options->zone = time_zone != NULL ? *time_zone : (sdfio_time_zone){0};
}
