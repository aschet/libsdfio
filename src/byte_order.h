/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief Conversion between the little-endian byte order of files and that of
 * the host.
 */

#ifndef SDFIO_SRC_BYTE_ORDER_H
#define SDFIO_SRC_BYTE_ORDER_H

#include <sdfio/sdfio.h>

/** Tells whether the machine stores the least significant byte first. */
bool sdfio_host_is_little_endian(void);

/**
 * Converts values between the byte order of the host and the file order
 * (little endian), in place. The conversion is the same in both directions.
 * The function reverses the bytes of each value, whether it is an integer or a
 * floating-point number. A value of one byte is left as it is.
 *
 * @param[in,out] values The values.
 * @param count The number of values.
 * @param type The data type of the values.
 */
void sdfio_convert_byte_order(void *values, size_t count, sdfio_data_type type);

#endif /* SDFIO_SRC_BYTE_ORDER_H */
