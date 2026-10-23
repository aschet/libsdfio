/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file revision.h
 * @brief Revisions of the format and their rules.
 */

#ifndef SDFIO_SRC_REVISION_H
#define SDFIO_SRC_REVISION_H

#include <sdfio/sdfio.h>

/** The number of revisions that the library supports: ISO-1.0 and ISO-2.0. */
enum
{
    SDFIO_REVISION_COUNT = 2
};

/**
 * Tells whether the dates of a revision are in UTC.
 *
 * @param revision The revision.
 */
static inline bool sdfio_requires_utc(sdfio_revision revision)
{
    return revision == SDFIO_ISO_2_0;
}

/**
 * Returns the largest number of columns, and the largest number of rows, that
 * the header of a revision can hold. The header stores each of the two numbers
 * in 2 bytes in ISO-1.0 and in 4 bytes in ISO-2.0.
 *
 * @param revision The revision.
 */
size_t sdfio_max_columns_and_rows(sdfio_revision revision);

#endif /* SDFIO_SRC_REVISION_H */
