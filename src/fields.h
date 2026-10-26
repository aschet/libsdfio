/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief Sizes and codes of the header fields.
 */

#ifndef SDFIO_SRC_FIELDS_H
#define SDFIO_SRC_FIELDS_H

#include <sdfio/sdfio.h>

/** The size of a date field, DDMMYYYYhhmm. */
#define SDFIO_DATETIME_SIZE 12
/** The size in bytes of a code of the binary header: the compression type, the
 * data type and the checksum type. */
#define SDFIO_CODE_SIZE 1
/** The code of "no compression" and "no checksum". */
#define SDFIO_NONE 0

#endif /* SDFIO_SRC_FIELDS_H */
