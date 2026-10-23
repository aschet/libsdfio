/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file fields.h
 * @brief Header fields and format limits.
 */

#ifndef SDFIO_SRC_FIELDS_H
#define SDFIO_SRC_FIELDS_H

#include <sdfio/sdfio.h>

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
    /** The code of "no compression" and "no checksum". */
    SDFIO_NONE = 0,
    /** The longest trailer. */
    SDFIO_MAX_TRAILER = 1 << 28
};

#endif /* SDFIO_SRC_FIELDS_H */
