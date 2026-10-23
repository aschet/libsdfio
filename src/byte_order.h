/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file byte_order.h
 * @brief The byte order of the machine and of the files.
 */

#ifndef SDFIO_SRC_BYTE_ORDER_H
#define SDFIO_SRC_BYTE_ORDER_H

#include "common.h"

/** Tells whether the machine stores the least significant byte first. */
bool sdfio_host_is_little_endian(void);

/**
 * Puts values in file order (little endian), or back, in place.
 *
 * @param[in,out] values The values.
 * @param count The number of values.
 * @param size The size of a value in bytes.
 */
void sdfio_fix_endianness(void *values, size_t count, size_t size);

#endif /* SDFIO_SRC_BYTE_ORDER_H */
