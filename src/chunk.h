/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file chunk.h
 * @brief Division of values into chunks.
 */

#ifndef SDFIO_SRC_CHUNK_H
#define SDFIO_SRC_CHUNK_H

#include <stddef.h>

/**
 * Returns the number of values in the chunk that starts at a value: a full
 * chunk, or what is left of the values.
 *
 * @param total The number of values.
 * @param first The index of the first value of the chunk.
 * @param chunk The number of values in a full chunk.
 */
static inline size_t sdfio_chunk_count(size_t total, size_t first, size_t chunk)
{
    return total - first < chunk ? total - first : chunk;
}

#endif /* SDFIO_SRC_CHUNK_H */
