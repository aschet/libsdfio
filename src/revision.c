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

/* What a revision allows: one entry for each data type, by its code. */
static const bool allowed[SDFIO_REVISION_COUNT][8] = {
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

static bool valid_revision(sdfio_revision revision)
{
    return (size_t)revision < SDFIO_REVISION_COUNT;
}

bool sdfio_revision_allows(sdfio_revision revision, sdfio_data_type type)
{
    return valid_revision(revision) && sdfio_valid_data_type(type)
           && allowed[revision][type];
}

size_t sdfio_max_columns_and_rows(sdfio_revision revision)
{
    return revision == SDFIO_ISO_2_0 ? UINT32_MAX : UINT16_MAX;
}
