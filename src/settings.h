/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file settings.h
 * @brief Resolved configuration of a handle.
 */

#ifndef SDFIO_SRC_SETTINGS_H
#define SDFIO_SRC_SETTINGS_H

#include "common.h"

/** The defaults of a configuration. */
enum
{
    /** The number of values a reader or a writer converts at a time, if the
     * configuration has none. */
    SDFIO_DEFAULT_CHUNK = 1024,
    /** The size of the buffer of a reader or a writer, if the configuration has
     * none. */
    SDFIO_DEFAULT_BUFFER = 16384,
    /** The smallest size of a buffer. */
    SDFIO_MINIMUM_BUFFER = 64
};

/** A configuration with its defaults filled in. */
typedef struct sdfio_settings
{
    /** The allocator: malloc and free if the configuration has none. */
    sdfio_allocator allocator;
    /** The time zone: no conversion if the configuration has none. */
    sdfio_time_zone zone;
    /** The size of the buffer between the handle and its stream in bytes. */
    size_t buffer;
    /** The number of values converted at a time. */
    size_t chunk;
    /** The reader codecs without the gaps, null after the last. */
    const sdfio_reader_codec *reader_codecs[SDFIO_CODEC_MAX];
    /** The writer codecs without the gaps, null after the last. */
    const sdfio_writer_codec *writer_codecs[SDFIO_CODEC_MAX];
} sdfio_settings;

/**
 * Resolves a configuration with the defaults for what it does not set.
 *
 * @param config The configuration. May be null, which lists no codec.
 * @param[out] settings Receives the settings.
 * @return False if the allocator is not complete, a list of codecs has a gap or
 * a codec twice, or the chunk is too large for its buffers. A list may be
 * empty: the reader and the writer check the one they use.
 */
bool sdfio_config_resolve(const sdfio_config *config, sdfio_settings *settings);

#endif /* SDFIO_SRC_SETTINGS_H */
