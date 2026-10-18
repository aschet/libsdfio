/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

#include "internal.h"

/* The spelling of a revision in the magic, in the order of the enumeration. */
static const char *const revision_names[] = {"ISO-1.0", "ISO-2.0", "BCR-1.0"};
enum
{
    REVISION_COUNT = sizeof revision_names / sizeof revision_names[0]
};

/* What a revision allows: one entry for each data type, by its code. */
static const bool allowed[REVISION_COUNT][8] = {
    /* ISO-1.0 */
    [SDF_ISO_1_0] = {[SDF_DATA_INT16] = true,
                     [SDF_DATA_INT32] = true,
                     [SDF_DATA_BINARY64] = true},
    /* ISO-2.0 */
    [SDF_ISO_2_0] = {[SDF_DATA_INT8] = true,
                     [SDF_DATA_INT16] = true,
                     [SDF_DATA_INT32] = true,
                     [SDF_DATA_BINARY32] = true,
                     [SDF_DATA_BINARY64] = true},
    /* BCR-1.0 allows them all. */
    [SDF_BCR_1_0] = {true, true, true, true, true, true, true, true},
};

typedef struct scalar_info
{
    const char *name;
    size_t size;
    double lowest;
    double highest;
    sdf_sample sample;
    bool floating;
} scalar_info;

#define SCALAR_ROW(sample, data, name, type, bits, low, high, floating)        \
    [data] = {name,           sizeof(type), (double)(low),                     \
              (double)(high), sample,       floating},

static const scalar_info scalars[] = {SDFI_SCALARS(SCALAR_ROW)};
enum
{
    SCALAR_COUNT = sizeof scalars / sizeof scalars[0]
};

/* The samples are listed in their own order, which the table by data type
 * does not follow. */
static const sdf_data_type data_of[SCALAR_COUNT] = {
    [SDF_INT8] = SDF_DATA_INT8,        [SDF_UINT8] = SDF_DATA_UINT8,
    [SDF_INT16] = SDF_DATA_INT16,      [SDF_UINT16] = SDF_DATA_UINT16,
    [SDF_INT32] = SDF_DATA_INT32,      [SDF_UINT32] = SDF_DATA_UINT32,
    [SDF_FLOAT32] = SDF_DATA_BINARY32, [SDF_FLOAT64] = SDF_DATA_BINARY64,
};

bool sdfi_valid_sample(sdf_sample sample)
{
    return (int)sample >= 0 && (size_t)sample < SCALAR_COUNT;
}

bool sdfi_valid_data_type(sdf_data_type type)
{
    return (int)type >= 0 && (size_t)type < SCALAR_COUNT;
}

static bool valid_revision(sdf_revision revision)
{
    return (int)revision >= 0 && (size_t)revision < REVISION_COUNT;
}

const char *sdf_revision_name(sdf_revision revision)
{
    return valid_revision(revision) ? revision_names[revision] : "";
}

const char *sdf_data_type_name(sdf_data_type type)
{
    return sdfi_valid_data_type(type) ? scalars[type].name : "";
}

size_t sdf_data_type_size(sdf_data_type type)
{
    return sdfi_valid_data_type(type) ? scalars[type].size : 0;
}

size_t sdf_sample_size(sdf_sample sample)
{
    return sdfi_valid_sample(sample) ? scalars[data_of[sample]].size : 0;
}

sdf_sample sdf_sample_of(sdf_data_type type)
{
    return sdfi_valid_data_type(type) ? scalars[type].sample : SDF_FLOAT64;
}

bool sdf_revision_allows(sdf_revision revision, sdf_data_type type)
{
    return valid_revision(revision) && sdfi_valid_data_type(type)
           && allowed[revision][type];
}

bool sdfi_is_floating(sdf_data_type type)
{
    return sdfi_valid_data_type(type) && scalars[type].floating;
}

double sdfi_lowest(sdf_data_type type)
{
    return sdfi_valid_data_type(type) ? scalars[type].lowest : 0.0;
}

double sdfi_highest(sdf_data_type type)
{
    return sdfi_valid_data_type(type) ? scalars[type].highest : 0.0;
}

bool sdfi_revision_from_text(const char *text, size_t size,
                             sdf_revision *revision)
{
    for (size_t i = 0; i < REVISION_COUNT; ++i)
    {
        if (strlen(revision_names[i]) == size
            && memcmp(revision_names[i], text, size) == 0)
        {
            *revision = (sdf_revision)i;
            return true;
        }
    }
    return false;
}

size_t sdfi_max_binary_count(sdf_revision revision)
{
    return revision == SDF_ISO_2_0 ? UINT32_MAX : UINT16_MAX;
}

size_t sdfi_binary_header_size(sdf_revision revision)
{
    /* The magic, the identifier and the two dates, the counts, four binary64
     * numbers and the bytes of the compression, the data type and the
     * checksum. */
    const size_t fixed =
        SDFI_MAGIC_SIZE + SDFI_ID_SIZE + SDFI_HEADER_DATES * SDFI_DATE_SIZE
        + SDFI_HEADER_DOUBLES * sizeof(double) + SDFI_HEADER_CODES;
    const size_t count_size =
        revision == SDF_ISO_2_0 ? sizeof(uint32_t) : sizeof(uint16_t);
    return fixed + SDFI_HEADER_COUNTS * count_size;
}

bool sdf_rows_reversed(sdf_revision revision)
{
    return revision == SDF_BCR_1_0;
}

size_t sdf_row_y(const sdf_info *info, size_t file_row)
{
    return sdf_rows_reversed(info->revision) ? info->rows - 1 - file_row
                                             : file_row;
}

/* The nominal maximum of a floating point type in BCR-1.0: a number at or
 * above it marks a point that is not measured. */
static const double bcr_bad_binary32 = 3.4e38;
static const double bcr_bad_binary64 = 1.7e308;

double sdf_sentinel(sdf_data_type type, sdf_revision revision)
{
    return sdfi_uses_max_sentinel(revision) ? sdfi_highest(type)
                                            : sdfi_lowest(type);
}

double sdfi_ascii_sentinel(sdf_data_type type, sdf_revision revision)
{
    if (sdfi_uses_max_sentinel(revision) && sdfi_is_floating(type))
    {
        return type == SDF_DATA_BINARY32 ? bcr_bad_binary32 : bcr_bad_binary64;
    }
    return sdf_sentinel(type, revision);
}

sdfi_sentinel_test sdfi_sentinel_test_for(sdf_data_type type,
                                          sdf_revision revision)
{
    sdfi_sentinel_test test;
    test.at_least = sdfi_uses_max_sentinel(revision) && sdfi_is_floating(type);
    test.value = test.at_least ? sdfi_ascii_sentinel(type, revision)
                               : sdf_sentinel(type, revision);
    if (test.at_least && type == SDF_DATA_BINARY32)
    {
        /* The threshold in the precision of the type, so that a number
         * compares as the binary32 it is stored as. */
        test.value = (double)(float)test.value;
    }
    return test;
}

bool sdfi_matches(sdfi_sentinel_test test, double stored)
{
    return test.at_least ? stored >= test.value : stored == test.value;
}

bool sdf_is_sentinel(sdf_data_type type, sdf_revision revision, double stored)
{
    return sdfi_matches(sdfi_sentinel_test_for(type, revision), stored);
}

void sdf_info_init(sdf_info *info)
{
    *info = (sdf_info){0};
    info->revision = SDF_ISO_2_0;
    info->storage = SDF_BINARY;
    info->data_type = SDF_DATA_BINARY64;
    info->x_scale = 1.0;
    info->y_scale = 1.0;
    info->z_scale = SDF_AUTO_Z_SCALE;
    info->z_resolution = SDF_NO_RESOLUTION;
    info->create_date = sdfi_now();
    info->mod_date = info->create_date;
    snprintf(info->manufacturer_id, sizeof info->manufacturer_id, "libsdf");
}

bool sdfi_count(const sdf_info *info, size_t *count)
{
    if (info->columns != 0 && info->rows > SIZE_MAX / info->columns)
    {
        return false;
    }
    *count = info->columns * info->rows;
    return true;
}
