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
    [SDFIO_ISO_1_0] = {[SDFIO_DATA_INT16] = true,
                       [SDFIO_DATA_INT32] = true,
                       [SDFIO_DATA_BINARY64] = true},
    /* ISO-2.0 */
    [SDFIO_ISO_2_0] = {[SDFIO_DATA_INT8] = true,
                       [SDFIO_DATA_INT16] = true,
                       [SDFIO_DATA_INT32] = true,
                       [SDFIO_DATA_BINARY32] = true,
                       [SDFIO_DATA_BINARY64] = true},
    /* BCR-1.0 allows them all. */
    [SDFIO_BCR_1_0] = {true, true, true, true, true, true, true, true},
};

typedef struct scalar_info
{
    const char *name;
    size_t size;
    double lowest;
    double highest;
    sdfio_sample sample;
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
static const sdfio_data_type data_of[SCALAR_COUNT] = {
    [SDFIO_INT8] = SDFIO_DATA_INT8,
    [SDFIO_UINT8] = SDFIO_DATA_UINT8,
    [SDFIO_INT16] = SDFIO_DATA_INT16,
    [SDFIO_UINT16] = SDFIO_DATA_UINT16,
    [SDFIO_INT32] = SDFIO_DATA_INT32,
    [SDFIO_UINT32] = SDFIO_DATA_UINT32,
    [SDFIO_FLOAT32] = SDFIO_DATA_BINARY32,
    [SDFIO_FLOAT64] = SDFIO_DATA_BINARY64,
};

bool sdfi_valid_sample(sdfio_sample sample)
{
    return (int)sample >= 0 && (size_t)sample < SCALAR_COUNT;
}

bool sdfi_valid_data_type(sdfio_data_type type)
{
    return (int)type >= 0 && (size_t)type < SCALAR_COUNT;
}

static bool valid_revision(sdfio_revision revision)
{
    return (int)revision >= 0 && (size_t)revision < REVISION_COUNT;
}

const char *sdfio_revision_name(sdfio_revision revision)
{
    return valid_revision(revision) ? revision_names[revision] : "";
}

const char *sdfio_data_type_name(sdfio_data_type type)
{
    return sdfi_valid_data_type(type) ? scalars[type].name : "";
}

size_t sdfio_data_type_size(sdfio_data_type type)
{
    return sdfi_valid_data_type(type) ? scalars[type].size : 0;
}

size_t sdfio_sample_size(sdfio_sample sample)
{
    return sdfi_valid_sample(sample) ? scalars[data_of[sample]].size : 0;
}

sdfio_sample sdfio_sample_of(sdfio_data_type type)
{
    return sdfi_valid_data_type(type) ? scalars[type].sample : SDFIO_FLOAT64;
}

bool sdfio_revision_allows(sdfio_revision revision, sdfio_data_type type)
{
    return valid_revision(revision) && sdfi_valid_data_type(type)
           && allowed[revision][type];
}

bool sdfi_is_floating(sdfio_data_type type)
{
    return sdfi_valid_data_type(type) && scalars[type].floating;
}

double sdfi_lowest(sdfio_data_type type)
{
    return sdfi_valid_data_type(type) ? scalars[type].lowest : 0.0;
}

double sdfi_highest(sdfio_data_type type)
{
    return sdfi_valid_data_type(type) ? scalars[type].highest : 0.0;
}

bool sdfi_revision_from_text(const char *text, size_t size,
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

size_t sdfi_max_binary_count(sdfio_revision revision)
{
    return revision == SDFIO_ISO_2_0 ? UINT32_MAX : UINT16_MAX;
}

size_t sdfi_binary_header_size(sdfio_revision revision)
{
    /* The magic, the identifier and the two dates, the counts, four binary64
     * numbers and the bytes of the compression, the data type and the
     * checksum. */
    const size_t fixed =
        SDFI_MAGIC_SIZE + SDFI_ID_SIZE + SDFI_HEADER_DATES * SDFI_DATE_SIZE
        + SDFI_HEADER_DOUBLES * sizeof(double) + SDFI_HEADER_CODES;
    const size_t count_size =
        revision == SDFIO_ISO_2_0 ? sizeof(uint32_t) : sizeof(uint16_t);
    return fixed + SDFI_HEADER_COUNTS * count_size;
}

bool sdfio_rows_reversed(sdfio_revision revision)
{
    return revision == SDFIO_BCR_1_0;
}

size_t sdfio_row_y(const sdfio_header *header, size_t file_row)
{
    return sdfio_rows_reversed(header->revision) ? header->rows - 1 - file_row
                                                 : file_row;
}

/* The nominal maximum of a floating point type in BCR-1.0: a number at or
 * above it marks a point that is not measured. */
static const double bcr_bad_binary32 = 3.4e38;
static const double bcr_bad_binary64 = 1.7e308;

double sdfio_bad_value(sdfio_data_type type, sdfio_revision revision)
{
    return sdfi_bad_is_maximum(revision) ? sdfi_highest(type)
                                         : sdfi_lowest(type);
}

double sdfi_ascii_bad_value(sdfio_data_type type, sdfio_revision revision)
{
    if (sdfi_bad_is_maximum(revision) && sdfi_is_floating(type))
    {
        return type == SDFIO_DATA_BINARY32 ? bcr_bad_binary32
                                           : bcr_bad_binary64;
    }
    return sdfio_bad_value(type, revision);
}

sdfi_bad_test sdfi_bad_test_for(sdfio_data_type type, sdfio_revision revision)
{
    sdfi_bad_test test;
    test.at_least = sdfi_bad_is_maximum(revision) && sdfi_is_floating(type);
    test.value = test.at_least ? sdfi_ascii_bad_value(type, revision)
                               : sdfio_bad_value(type, revision);
    if (test.at_least && type == SDFIO_DATA_BINARY32)
    {
        /* The threshold in the precision of the type, so that a number
         * compares as the binary32 it is stored as. */
        test.value = (double)(float)test.value;
    }
    return test;
}

bool sdfi_matches(sdfi_bad_test test, double coded)
{
    return test.at_least ? coded >= test.value : coded == test.value;
}

bool sdfio_is_bad_value(sdfio_data_type type, sdfio_revision revision,
                        double value)
{
    return sdfi_matches(sdfi_bad_test_for(type, revision), value);
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
    header->creation_date = sdfi_now();
    header->modification_date = header->creation_date;
    snprintf(header->manufacturer_id, sizeof header->manufacturer_id,
             "libsdfio");
}

bool sdfi_count(const sdfio_header *header, size_t *count)
{
    if (header->columns != 0 && header->rows > SIZE_MAX / header->columns)
    {
        return false;
    }
    *count = header->columns * header->rows;
    return true;
}
