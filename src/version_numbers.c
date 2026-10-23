/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file version_numbers.c
 * @brief Version numbers that the listed reader codecs accept.
 */

#include "reader_codec.h"
#include "revision.h"
#include "settings.h"

size_t sdfio_version_number_count(const sdfio_config *config)
{
    sdfio_settings settings;
    if (!sdfio_config_resolve(config, &settings))
    {
        return 0;
    }
    size_t count = 0;
    while (count < SDFIO_CODEC_MAX && settings.reader_codecs[count] != NULL)
    {
        ++count;
    }
    return count * SDFIO_REVISION_COUNT;
}

const char *sdfio_version_number(const sdfio_config *config, size_t index)
{
    sdfio_settings settings;
    if (!sdfio_config_resolve(config, &settings)
        || index >= sdfio_version_number_count(config))
    {
        return NULL;
    }
    return settings.reader_codecs[index / SDFIO_REVISION_COUNT]
        ->version_numbers[index % SDFIO_REVISION_COUNT];
}
