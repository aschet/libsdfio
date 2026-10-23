/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file revision.c
 * @brief Revisions of the format and their rules.
 */

#include "revision.h"
#include "data_type.h"
#include <limits.h>

/* The properties that differ between the revisions. */
typedef struct revision_rules
{
    /* Whether the datetimes are in UTC. */
    bool utc;
    /* The size in bytes of the number of columns and of the number of rows in
     * the binary header. */
    size_t columns_rows_size;
    /* The data types that the revision permits, as a set of bits that the
     * code of a data type selects. */
    unsigned data_types;
} revision_rules;

/* The bit of a data type in a set of data types. */
#define TYPE_BIT(type) (1U << (type))

static const revision_rules rules[] = {
    [SDFIO_ISO_1_0] = {.utc = false,
                       .columns_rows_size = 2,
                       .data_types = TYPE_BIT(SDFIO_TYPE_INT16)
                                     | TYPE_BIT(SDFIO_TYPE_INT32)
                                     | TYPE_BIT(SDFIO_TYPE_DOUBLE)},
    [SDFIO_ISO_2_0] = {.utc = true,
                       .columns_rows_size = 4,
                       .data_types = TYPE_BIT(SDFIO_TYPE_INT8)
                                     | TYPE_BIT(SDFIO_TYPE_INT16)
                                     | TYPE_BIT(SDFIO_TYPE_INT32)
                                     | TYPE_BIT(SDFIO_TYPE_FLOAT)
                                     | TYPE_BIT(SDFIO_TYPE_DOUBLE)},
};

bool sdfio_revision_valid(sdfio_revision revision)
{
    return (size_t)revision < sizeof rules / sizeof rules[0];
}

bool sdfio_revision_allows(sdfio_revision revision, sdfio_data_type type)
{
    return sdfio_revision_valid(revision) && sdfio_valid_data_type(type)
           && (rules[revision].data_types & TYPE_BIT(type)) != 0;
}

bool sdfio_requires_utc(sdfio_revision revision)
{
    return sdfio_revision_valid(revision) && rules[revision].utc;
}

size_t sdfio_revision_columns_rows_size(sdfio_revision revision)
{
    return rules[revision].columns_rows_size;
}

size_t sdfio_max_columns_and_rows(sdfio_revision revision)
{
    const size_t size = rules[revision].columns_rows_size;
    return size >= sizeof(size_t) ? SIZE_MAX
                                  : ((size_t)1 << (CHAR_BIT * size)) - 1;
}
