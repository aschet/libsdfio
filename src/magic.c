/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file magic.c
 * @brief Magics of the listed codecs.
 */

#include "reader_codec.h"
#include "revision.h"
#include "settings.h"

size_t sdfio_magic_count(const sdfio_config *config)
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

const char *sdfio_magic(const sdfio_config *config, size_t index)
{
    sdfio_settings settings;
    if (!sdfio_config_resolve(config, &settings)
        || index >= sdfio_magic_count(config))
    {
        return NULL;
    }
    return settings.reader_codecs[index / SDFIO_REVISION_COUNT]
        ->magics[index % SDFIO_REVISION_COUNT];
}
