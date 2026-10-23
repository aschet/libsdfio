/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file settings.c
 * @brief Resolution of the configuration.
 */

#include "settings.h"
#include "data_type.h"
#include "writer_format.h"

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
    size_t readers = 0;
    size_t writers = 0;
    for (size_t i = 0; i < SDFIO_FORMAT_MAX; ++i)
    {
        if (config->reader_formats[i] != NULL)
        {
            settings->reader_formats[readers++] = config->reader_formats[i];
        }
        if (config->writer_formats[i] != NULL)
        {
            settings->writer_formats[writers++] = config->writer_formats[i];
        }
    }
    return settings->chunk <= SIZE_MAX / SDFIO_LARGEST_DATA_ENTRY
           && settings->chunk <= SIZE_MAX / SDFIO_TEXT_PER_VALUE;
}
