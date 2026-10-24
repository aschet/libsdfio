/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file binary.h
 * @brief Layout of the binary header.
 */

#ifndef SDFIO_SRC_BINARY_H
#define SDFIO_SRC_BINARY_H

#include <sdfio/sdfio.h>
#include <stdint.h>

/** Layout of the binary header after the fields of the number of columns and
 * the number of rows: four binary64 numbers followed by three bytes of codes.
 */
enum
{
    /** The number of binary64 numbers: the scales of x, y and z, and the
     * resolution of z. */
    SDFIO_BINARY_REAL_FIELDS = 4,
    /** The index of the byte of the compression type among the bytes that end
     * the header. */
    SDFIO_BINARY_CODE_COMPRESSION = 0,
    /** The index of the byte of the data type. */
    SDFIO_BINARY_CODE_DATA_TYPE = 1,
    /** The index of the byte of the checksum type. */
    SDFIO_BINARY_CODE_CHECKSUM = 2,
    /** The number of bytes that end the header. */
    SDFIO_BINARY_CODE_FIELDS = 3
};

#endif /* SDFIO_SRC_BINARY_H */
