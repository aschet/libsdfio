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

/** The revisions of the format. */
enum
{
    SDFIO_REVISION_COUNT = 2
};

/**
 * Returns the spelling of a revision in the magic, for example "ISO-2.0", or an
 * empty string for an invalid value.
 *
 * @param revision The revision.
 */
const char *sdfio_revision_text(sdfio_revision revision);

/**
 * Finds the revision that a name stands for.
 *
 * @param text The name, as in the magic: "ISO-2.0".
 * @param size The length of the name.
 * @param[out] revision Receives the revision.
 * @return False if the name is not one of a revision.
 */
bool sdfio_revision_from_text(const char *text, size_t size,
                              sdfio_revision *revision);

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
 * Returns the largest number of columns or of rows of a revision, which is the
 * extent of the matrix in either direction. The header holds each of them in 2
 * bytes in ISO-1.0 and in 4 bytes in ISO-2.0.
 *
 * @param revision The revision.
 */
size_t sdfio_max_extent(sdfio_revision revision);

#endif /* SDFIO_SRC_REVISION_H */
