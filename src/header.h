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
 * Computes the number of values of the data area, which holds one value for
 * each point, with a check for overflow.
 *
 * @param header The header.
 * @param[out] total Receives columns times rows.
 * @return False if the number does not fit in size_t.
 */
bool sdfio_value_total(const sdfio_header *header, size_t *total);

/**
 * Checks what makes a file that cannot be written.
 *
 * @param header The header.
 * @param[out] message Receives the reason of a failure.
 * @retval SDFIO_ERR_ARGUMENT A field of the header is invalid.
 */
sdfio_status sdfio_validate(const sdfio_header *header, sdfio_message *message);

#endif /* SDFIO_SRC_HEADER_H */
