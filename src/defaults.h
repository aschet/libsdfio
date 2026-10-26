/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief Defaults and limits of the options.
 */

#ifndef SDFIO_SRC_DEFAULTS_H
#define SDFIO_SRC_DEFAULTS_H

#include "data_type.h"
#include "writer_format.h"
#include <sdfio/sdfio.h>
#include <stdbool.h>
#include <stdint.h>

/** The defaults of the options, and the limits of the lists. */
/** The number of values a reader or a writer converts at a time. */
#define SDFIO_DEFAULT_CHUNK 1024
/** The size of the buffer of a reader or a writer. */
#define SDFIO_DEFAULT_BUFFER 16384
/** The smallest size of a buffer. */
#define SDFIO_MINIMUM_BUFFER 64
/** The number of formats of a direction that a handle can add: the
 * binary and the ASCII format. */
#define SDFIO_FORMAT_MAX 2

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

#endif /* SDFIO_SRC_DEFAULTS_H */
