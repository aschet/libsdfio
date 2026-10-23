/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file header.h
 * @brief Operations on the header structure.
 */

#ifndef SDFIO_SRC_HEADER_H
#define SDFIO_SRC_HEADER_H

#include "message.h"
#include <sdfio/sdfio.h>

/**
 * Computes the total number of coded values of the data area: the number of
 * columns times the number of rows of the data matrix that the header
 * describes. The function reports an overflow of size_t instead of wrapping
 * around.
 *
 * @param header The header, with the numbers of columns and rows.
 * @param[out] values Receives the number of coded values.
 * @return False if the number does not fit in size_t.
 */
bool sdfio_coded_values(const sdfio_header *header, size_t *values);

/**
 * Validates a header for the creation of a file.
 *
 * The function verifies that the fields of the header can be represented in a
 * file of its revision and representation. The revision, the representation and
 * the data type are enumerators, and the revision permits the data type. The
 * z-scale is positive or SDFIO_AUTO_Z_SCALE, and the x-scale, the y-scale and
 * the z-resolution are finite. The manufacturer identifier is a terminated
 * 7-bit ASCII string. Both datetimes are set and lie in the supported range of
 * years. The numbers of columns and rows do not exceed the limit of the
 * revision, and their product fits in size_t.
 *
 * @param header The header to validate.
 * @param[out] message Receives the reason of a failure. May be null.
 * @retval SDFIO_OK The header can be written.
 * @retval SDFIO_ERR_ARGUMENT A field of the header is invalid.
 */
sdfio_status sdfio_header_validate_for_writing(const sdfio_header *header,
                                               sdfio_message *message);

#endif /* SDFIO_SRC_HEADER_H */
