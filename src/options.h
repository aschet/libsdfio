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

#ifndef SDFIO_SRC_OPTIONS_H
#define SDFIO_SRC_OPTIONS_H

#include "message.h"
#include <sdfio/sdfio.h>

/** The options of a handle, which are fixed by the first file. */
typedef struct sdfio_options
{
    /** The allocator of the handle. */
    sdfio_allocator allocator;
    /** The time zone of the dates of ISO-1.0 files. */
    sdfio_time_zone zone;
    /** The number of values converted at a time. */
    size_t chunk_values;
    /** The size of the buffer between the handle and its stream in bytes. */
    size_t buffer_size;
} sdfio_options;

/**
 * Sets the options to their defaults, with an allocator.
 *
 * @param[out] options The options.
 * @param allocator The allocator, which is copied.
 * @return False if the allocator is null or lacks a callback.
 */
bool sdfio_options_init(sdfio_options *options,
                        const sdfio_allocator *allocator);

/**
 * Sets the size of the buffer. A size below the minimum becomes the minimum.
 *
 * @param[in,out] options The options.
 * @param size The size, not 0.
 * @param[out] message Receives the reason of a failure.
 * @retval SDFIO_ERR_ARGUMENT The size is 0.
 */
sdfio_status sdfio_options_set_buffer(sdfio_options *options, size_t size,
                                      sdfio_message *message);

/**
 * Sets the number of values converted at a time.
 *
 * @param[in,out] options The options.
 * @param count The number of values.
 * @param[out] message Receives the reason of a failure.
 * @retval SDFIO_ERR_ARGUMENT The number is 0 or too large.
 */
sdfio_status sdfio_options_set_chunk(sdfio_options *options, size_t count,
                                     sdfio_message *message);

/**
 * Sets the time zone.
 *
 * @param[in,out] options The options.
 * @param time_zone The time zone, or null for none.
 */
void sdfio_options_set_zone(sdfio_options *options,
                            const sdfio_time_zone *time_zone);

#endif /* SDFIO_SRC_OPTIONS_H */
