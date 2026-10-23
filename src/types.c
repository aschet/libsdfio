/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file types.c
 * @brief The tables of the types and revisions, and the rules that depend on
 * them.
 */

#include "internal.h"

/* The spelling of a revision in the magic, in the order of the enumeration. */
static const char *const revision_texts[] = {"ISO-1.0", "ISO-2.0"};
enum
{
    REVISION_COUNT = sizeof revision_texts / sizeof revision_texts[0]
};

/* What a revision allows: one entry for each data type, by its code. */
static const bool allowed[REVISION_COUNT][8] = {
    /* ISO-1.0 */
    [SDFIO_ISO_1_0] = {[SDFIO_TYPE_INT16] = true,
                       [SDFIO_TYPE_INT32] = true,
                       [SDFIO_TYPE_DOUBLE] = true},
    /* ISO-2.0 */
    [SDFIO_ISO_2_0] = {[SDFIO_TYPE_INT8] = true,
                       [SDFIO_TYPE_INT16] = true,
                       [SDFIO_TYPE_INT32] = true,
                       [SDFIO_TYPE_FLOAT] = true,
                       [SDFIO_TYPE_DOUBLE] = true},
};

size_t sdfio_magic_count(const sdfio_config *config)
{
    sdfio_settings settings;
    if (!sdfio_config_resolve(config, &settings))
    {
        return 0;
    }
    size_t count = 0;
    while (count < SDFIO_CODEC_MAX && settings.codecs[count] != NULL)
    {
        ++count;
    }
    return count * REVISION_COUNT;
}

const char *sdfio_magic(const sdfio_config *config, size_t index)
{
    sdfio_settings settings;
    if (!sdfio_config_resolve(config, &settings)
        || index >= sdfio_magic_count(config))
    {
        return NULL;
    }
    return settings.codecs[index / REVISION_COUNT]
        ->magics[index % REVISION_COUNT];
}

bool sdfio_valid_height_type(sdfio_data_type type)
{
    return type == SDFIO_TYPE_FLOAT || type == SDFIO_TYPE_DOUBLE;
}

bool sdfio_valid_data_type(sdfio_data_type type)
{
    switch (type)
    {
    case SDFIO_TYPE_FLOAT:
    case SDFIO_TYPE_INT8:
    case SDFIO_TYPE_INT16:
    case SDFIO_TYPE_INT32:
    case SDFIO_TYPE_DOUBLE:
        return true;
    }
    return false;
}

static bool valid_revision(sdfio_revision revision)
{
    return (size_t)revision < REVISION_COUNT;
}

const char *sdfio_revision_text(sdfio_revision revision)
{
    return valid_revision(revision) ? revision_texts[revision] : "";
}

size_t sdfio_data_type_size(sdfio_data_type type)
{
    switch (type)
    {
    case SDFIO_TYPE_INT8:
        return sizeof(int8_t);
    case SDFIO_TYPE_INT16:
        return sizeof(int16_t);
    case SDFIO_TYPE_INT32:
        return sizeof(int32_t);
    case SDFIO_TYPE_FLOAT:
        return sizeof(float);
    case SDFIO_TYPE_DOUBLE:
        return sizeof(double);
    }
    return 0;
}

bool sdfio_revision_allows(sdfio_revision revision, sdfio_data_type type)
{
    return valid_revision(revision) && sdfio_valid_data_type(type)
           && allowed[revision][type];
}

bool sdfio_is_floating(sdfio_data_type type)
{
    return type == SDFIO_TYPE_FLOAT || type == SDFIO_TYPE_DOUBLE;
}

bool sdfio_revision_from_text(const char *text, size_t size,
                              sdfio_revision *revision)
{
    for (size_t i = 0; i < REVISION_COUNT; ++i)
    {
        if (strlen(revision_texts[i]) == size
            && memcmp(revision_texts[i], text, size) == 0)
        {
            *revision = (sdfio_revision)i;
            return true;
        }
    }
    return false;
}

size_t sdfio_max_count(sdfio_revision revision)
{
    return revision == SDFIO_ISO_2_0 ? UINT32_MAX : UINT16_MAX;
}

size_t sdfio_binary_header_size(sdfio_revision revision)
{
    /* The magic, the identifier and the two dates, the counts, four binary64
     * numbers and the bytes of the compression, the data type and the
     * checksum. */
    const size_t fixed =
        SDFIO_MAGIC_SIZE + SDFIO_ID_SIZE + SDFIO_HEADER_DATES * SDFIO_DATE_SIZE
        + SDFIO_HEADER_DOUBLES * sizeof(double) + SDFIO_HEADER_CODES;
    const size_t count_size =
        revision == SDFIO_ISO_2_0 ? sizeof(uint32_t) : sizeof(uint16_t);
    return fixed + SDFIO_HEADER_COUNTS * count_size;
}

void sdfio_header_init(sdfio_header *header)
{
    *header = (sdfio_header){0};
    header->revision = SDFIO_ISO_2_0;
    header->representation = SDFIO_BINARY;
    header->data_type = SDFIO_TYPE_FLOAT;
    header->x_scale = 1.0;
    header->y_scale = 1.0;
    header->z_scale = SDFIO_AUTO_Z_SCALE;
    header->z_resolution = SDFIO_NO_RESOLUTION;
    header->creation_date = sdfio_now();
    header->modification_date = header->creation_date;
    static const char producer[] = "libsdfio";
    memcpy(header->manufacturer_id, producer, sizeof producer);
}

bool sdfio_count(const sdfio_header *header, size_t *count)
{
    if (header->columns != 0 && header->rows > SIZE_MAX / header->columns)
    {
        return false;
    }
    *count = header->columns * header->rows;
    return true;
}
