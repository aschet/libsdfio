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
    for (size_t i = 0; i < SDFIO_CODEC_MAX && settings.reader_codecs[i] != NULL;
         ++i)
    {
        count += settings.reader_codecs[i]->version_number_count;
    }
    return count;
}

const char *sdfio_version_number(const sdfio_config *config, size_t index)
{
    sdfio_settings settings;
    if (!sdfio_config_resolve(config, &settings))
    {
        return NULL;
    }
    for (size_t i = 0; i < SDFIO_CODEC_MAX && settings.reader_codecs[i] != NULL;
         ++i)
    {
        const sdfio_reader_codec *codec = settings.reader_codecs[i];
        if (index < codec->version_number_count)
        {
            return codec->version_numbers[index];
        }
        index -= codec->version_number_count;
    }
    return NULL;
}
