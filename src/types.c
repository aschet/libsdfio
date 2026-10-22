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
static const char *const revision_names[] = {"ISO-1.0", "ISO-2.0"};
enum
{
    REVISION_COUNT = sizeof revision_names / sizeof revision_names[0]
};

/* What a revision allows: one entry for each data type, by its code. */
static const bool allowed[REVISION_COUNT][8] = {
    /* ISO-1.0 */
    [SDFIO_ISO_1_0] = {[SDFIO_DATA_INT16] = true,
                       [SDFIO_DATA_INT32] = true,
                       [SDFIO_DATA_BINARY64] = true},
    /* ISO-2.0 */
    [SDFIO_ISO_2_0] = {[SDFIO_DATA_INT8] = true,
                       [SDFIO_DATA_INT16] = true,
                       [SDFIO_DATA_INT32] = true,
                       [SDFIO_DATA_BINARY32] = true,
                       [SDFIO_DATA_BINARY64] = true},
};

/** What the library knows of a data type. */
typedef struct scalar_info
{
    /** The name in a header. */
    const char *name;
    /** The size in bytes. */
    size_t size;
    /** The lowest value. */
    double lowest;
    /** The highest value. */
    double highest;
    /** Whether the type is a floating point type. */
    bool floating;
} scalar_info;

/** A row of the table of the data types, for SDFIO_SCALARS(). */
#define SCALAR_ROW(data, name, type, bits, low, high, floating)                \
    [(data) - FIRST_DATA_TYPE] = {name, sizeof(type), (double)(low),           \
                                  (double)(high), floating},

enum
{
    /* The codes of the standard start here. */
    FIRST_DATA_TYPE = SDFIO_DATA_BINARY32
};
static const scalar_info scalars[] = {SDFIO_SCALARS(SCALAR_ROW)};

static const scalar_info *scalar_of(sdfio_data_type type)
{
    return sdfio_valid_data_type(type) ? &scalars[type - FIRST_DATA_TYPE]
                                       : NULL;
}

bool sdfio_valid_sample(sdfio_sample sample)
{
    return sample == SDFIO_FLOAT32 || sample == SDFIO_FLOAT64;
}

bool sdfio_valid_data_type(sdfio_data_type type)
{
    switch (type)
    {
    case SDFIO_DATA_BINARY32:
    case SDFIO_DATA_INT8:
    case SDFIO_DATA_INT16:
    case SDFIO_DATA_INT32:
    case SDFIO_DATA_BINARY64:
        return true;
    }
    return false;
}

static bool valid_revision(sdfio_revision revision)
{
    return (size_t)revision < REVISION_COUNT;
}

const char *sdfio_revision_name(sdfio_revision revision)
{
    return valid_revision(revision) ? revision_names[revision] : "";
}

const char *sdfio_data_type_name(sdfio_data_type type)
{
    const scalar_info *info = scalar_of(type);
    return info != NULL ? info->name : "";
}

size_t sdfio_data_type_size(sdfio_data_type type)
{
    const scalar_info *info = scalar_of(type);
    return info != NULL ? info->size : 0;
}

size_t sdfio_sample_size(sdfio_sample sample)
{
    switch (sample)
    {
    case SDFIO_FLOAT32:
        return sizeof(float);
    case SDFIO_FLOAT64:
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
    const scalar_info *info = scalar_of(type);
    return info != NULL && info->floating;
}

double sdfio_lowest(sdfio_data_type type)
{
    const scalar_info *info = scalar_of(type);
    return info != NULL ? info->lowest : 0.0;
}

double sdfio_highest(sdfio_data_type type)
{
    const scalar_info *info = scalar_of(type);
    return info != NULL ? info->highest : 0.0;
}

bool sdfio_revision_from_text(const char *text, size_t size,
                              sdfio_revision *revision)
{
    for (size_t i = 0; i < REVISION_COUNT; ++i)
    {
        if (strlen(revision_names[i]) == size
            && memcmp(revision_names[i], text, size) == 0)
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

double sdfio_bad_value(sdfio_data_type type)
{
    return sdfio_lowest(type);
}

bool sdfio_is_bad_value(sdfio_data_type type, double value)
{
    return value == sdfio_bad_value(type);
}

void sdfio_header_init(sdfio_header *header)
{
    *header = (sdfio_header){0};
    header->revision = SDFIO_ISO_2_0;
    header->representation = SDFIO_BINARY;
    header->data_type = SDFIO_DATA_BINARY32;
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
