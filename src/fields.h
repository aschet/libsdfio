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
    /** The size of a date field, DDMMYYYYhhmm. */
    SDFIO_DATETIME_SIZE = 12,
    /** The code of "no compression" and "no checksum". */
    SDFIO_NONE = 0
};

#endif /* SDFIO_SRC_FIELDS_H */
