/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file fields.h
 * @brief The fields of the header and the limits of the format.
 */

#ifndef SDFIO_SRC_FIELDS_H
#define SDFIO_SRC_FIELDS_H

#include "common.h"

/** The fields of the file. */
enum
{
    /** Where the name of the revision starts in the magic. */
    SDFIO_MAGIC_NAME_AT = 1,
    /** The size of the name of the revision in the magic. */
    SDFIO_MAGIC_NAME_SIZE = 3,
    /** Where the dash after the name is in the magic. */
    SDFIO_MAGIC_DASH_AT = 4,
    /** Where the major number of the revision is in the magic. */
    SDFIO_MAGIC_MAJOR_AT = 5,
    /** Where the dot after the major number is in the magic. */
    SDFIO_MAGIC_DOT_AT = 6,
    /** Where the minor number of the revision is in the magic. */
    SDFIO_MAGIC_MINOR_AT = 7,
    /** The size of the identifier in the header of a binary file. */
    SDFIO_ID_SIZE = 10,
    /** The size of a date field, DDMMYYYYhhmm. */
    SDFIO_DATE_SIZE = 12,
    /** The number of dates in the header of a binary file. */
    SDFIO_HEADER_DATES = 2,
    /** The number of counts (columns and rows) in the header of a binary file.
     */
    SDFIO_HEADER_COUNTS = 2,
    /** The number of binary64 numbers in the header of a binary file. */
    SDFIO_HEADER_DOUBLES = 4,
    /** The number of bytes of codes (compression, data type and checksum) in
     * the header of a binary file. */
    SDFIO_HEADER_CODES = 3,
    /** The size of the largest binary header, that of ISO-2.0. */
    SDFIO_BINARY_HEADER_MAX = 85,
    /** The code of "no compression" and "no checksum". */
    SDFIO_NONE = 0,
    /** The longest trailer. */
    SDFIO_MAX_TRAILER = 1 << 28
};

_Static_assert(SDFIO_BINARY_HEADER_MAX
                   == SDFIO_MAGIC_SIZE + SDFIO_ID_SIZE
                          + SDFIO_HEADER_DATES * SDFIO_DATE_SIZE
                          + SDFIO_HEADER_COUNTS * sizeof(uint32_t)
                          + SDFIO_HEADER_DOUBLES * sizeof(double)
                          + SDFIO_HEADER_CODES,
               "the largest binary header is the sum of its fields");

#endif /* SDFIO_SRC_FIELDS_H */
