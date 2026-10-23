/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file settings.c
 * @brief A configuration with its defaults filled in.
 */

#include "settings.h"
#include "codec.h"
#include "types.h"

/**
 * Copies a list of codecs of the configuration, of either direction, and
 * clears `ok` if the list has a gap or names a codec twice.
 *
 * @param from The list of the configuration.
 * @param to The list of the settings.
 * @param ok The flag to clear.
 */
#define COPY_CODECS(from, to, ok)                                              \
    do                                                                         \
    {                                                                          \
        size_t listed_count = 0;                                               \
        for (; listed_count < SDFIO_CODEC_MAX && (from)[listed_count] != NULL; \
             ++listed_count)                                                   \
        {                                                                      \
            for (size_t before = 0; before < listed_count; ++before)           \
            {                                                                  \
                (ok) = (ok) && (from)[before] != (from)[listed_count];         \
            }                                                                  \
            (to)[listed_count] = (from)[listed_count];                         \
        }                                                                      \
        for (size_t gap = listed_count; gap < SDFIO_CODEC_MAX; ++gap)          \
        {                                                                      \
            (ok) = (ok) && (from)[gap] == NULL;                                \
        }                                                                      \
    } while (0)

bool sdfio_config_resolve(const sdfio_config *config, sdfio_settings *settings)
{
    *settings = (sdfio_settings){.buffer = SDFIO_DEFAULT_BUFFER,
                                 .chunk = SDFIO_DEFAULT_CHUNK};
    if (config == NULL)
    {
        return false;
    }
    if (config->allocator != NULL)
    {
        if (config->allocator->allocate == NULL
            || config->allocator->release == NULL)
        {
            return false;
        }
        settings->allocator = *config->allocator;
    }
    if (config->time_zone != NULL)
    {
        settings->zone = *config->time_zone;
    }
    if (config->buffer_size != 0)
    {
        settings->buffer = config->buffer_size < SDFIO_MINIMUM_BUFFER
                               ? SDFIO_MINIMUM_BUFFER
                               : config->buffer_size;
    }
    if (config->chunk_values != 0)
    {
        settings->chunk = config->chunk_values;
    }
    bool listed = true;
    COPY_CODECS(config->reader_codecs, settings->reader_codecs, listed);
    COPY_CODECS(config->writer_codecs, settings->writer_codecs, listed);
    return listed && settings->chunk <= SIZE_MAX / SDFIO_LARGEST_SAMPLE
           && settings->chunk <= SIZE_MAX / SDFIO_TEXT_PER_VALUE;
}
