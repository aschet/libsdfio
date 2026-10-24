/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file defaults.h
 * @brief Defaults and limits of the options of a reader and a writer.
 */

#ifndef SDFIO_SRC_DEFAULTS_H
#define SDFIO_SRC_DEFAULTS_H

#include "data_type.h"
#include "writer_format.h"
#include <sdfio/sdfio.h>
#include <stdbool.h>
#include <stdint.h>

/** The defaults of the options, and the limits of the lists. */
enum
{
    /** The number of values a reader or a writer converts at a time. */
    SDFIO_DEFAULT_CHUNK = 1024,
    /** The size of the buffer of a reader or a writer. */
    SDFIO_DEFAULT_BUFFER = 16384,
    /** The smallest size of a buffer. */
    SDFIO_MINIMUM_BUFFER = 64,
    /** The number of formats of a direction that a handle can add: the
     * binary and the ASCII format. */
    SDFIO_FORMAT_MAX = 2
};

/**
 * Tells whether a number of values is a valid size of a chunk: not 0, and not
 * so large that the buffers of a chunk cannot be counted in bytes.
 *
 * @param chunk The number of values.
 */
static inline bool sdfio_chunk_valid(size_t chunk)
{
    return chunk > 0 && chunk <= SIZE_MAX / SDFIO_LARGEST_CODED_VALUE
           && chunk <= SIZE_MAX / SDFIO_TEXT_PER_VALUE;
}

/**
 * Tells whether an allocator can be used: both callbacks are set.
 *
 * @param allocator The allocator.
 */
static inline bool sdfio_allocator_valid(const sdfio_allocator *allocator)
{
    return allocator->allocate != NULL && allocator->release != NULL;
}

/**
 * Takes the allocator argument of a create function.
 *
 * @param given The argument.
 * @param[out] used Receives a copy of the allocator.
 * @return False if the argument is null or lacks a callback.
 */
static inline bool sdfio_allocator_take(const sdfio_allocator *given,
                                        sdfio_allocator *used)
{
    if (given == NULL || !sdfio_allocator_valid(given))
    {
        return false;
    }
    *used = *given;
    return true;
}

#endif /* SDFIO_SRC_DEFAULTS_H */
