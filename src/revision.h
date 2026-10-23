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

/**
 * Tells whether a value is one of the revisions.
 *
 * @param revision The value to test.
 */
bool sdfio_revision_valid(sdfio_revision revision);

/**
 * Tells whether the datetimes of a revision are in UTC.
 *
 * @param revision The revision.
 */
bool sdfio_requires_utc(sdfio_revision revision);

/**
 * Returns the size in bytes of each of the two fields of the binary header that
 * hold the number of columns and the number of rows of the data matrix.
 *
 * @param revision The revision.
 */
size_t sdfio_revision_columns_rows_size(sdfio_revision revision);

/**
 * Returns the largest number of columns, and the largest number of rows, that
 * the header of a revision can hold. The limit follows from the size of the
 * fields of the binary header.
 *
 * @param revision The revision.
 */
size_t sdfio_max_columns_and_rows(sdfio_revision revision);

#endif /* SDFIO_SRC_REVISION_H */
