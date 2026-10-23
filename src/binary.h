/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file binary.h
 * @brief Layout of the header of the binary representation.
 */

#ifndef SDFIO_SRC_BINARY_H
#define SDFIO_SRC_BINARY_H

#include <sdfio/sdfio.h>
#include <stdint.h>

/** The parts of the header that follow the dates. */
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

/**
 * Returns the size in bytes of the number of points and of the number of
 * profiles in the header: 2 in ISO-1.0 and 4 in ISO-2.0.
 *
 * @param revision The revision.
 */
static inline size_t sdfio_binary_extent_size(sdfio_revision revision)
{
    return revision == SDFIO_ISO_2_0 ? sizeof(uint32_t) : sizeof(uint16_t);
}

#endif /* SDFIO_SRC_BINARY_H */
