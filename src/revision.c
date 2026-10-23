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
#include <string.h>

/* The spelling of a revision in the magic, in the order of the enumeration. */
static const char *const revision_texts[] = {"ISO-1.0", "ISO-2.0"};
_Static_assert(sizeof revision_texts / sizeof revision_texts[0]
                   == SDFIO_REVISION_COUNT,
               "a text for each revision");

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

const char *sdfio_revision_text(sdfio_revision revision)
{
    return valid_revision(revision) ? revision_texts[revision] : "";
}

bool sdfio_revision_allows(sdfio_revision revision, sdfio_data_type type)
{
    return valid_revision(revision) && sdfio_valid_data_type(type)
           && allowed[revision][type];
}

bool sdfio_revision_from_text(const char *text, size_t size,
                              sdfio_revision *revision)
{
    for (size_t i = 0; i < SDFIO_REVISION_COUNT; ++i)
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

size_t sdfio_max_extent(sdfio_revision revision)
{
    return revision == SDFIO_ISO_2_0 ? UINT32_MAX : UINT16_MAX;
}
