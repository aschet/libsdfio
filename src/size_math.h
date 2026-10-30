/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief Arithmetic on sizes that reports an overflow.
 */

#ifndef SDFIO_SRC_SIZE_MATH_H
#define SDFIO_SRC_SIZE_MATH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * Adds a part to a total.
 *
 * @param[in,out] total The total, unchanged on an overflow.
 * @param part The part.
 * @return false if the sum does not fit in size_t.
 */
static inline bool sdfio_size_add(size_t *total, size_t part)
{
    if (part > SIZE_MAX - *total)
    {
        return false;
    }
    *total += part;
    return true;
}

/**
 * Multiplies two sizes.
 *
 * @param a The first factor.
 * @param b The second factor.
 * @param[out] product The product, unchanged on an overflow.
 * @return false if the product does not fit in size_t.
 */
static inline bool sdfio_size_multiply(size_t a, size_t b, size_t *product)
{
    if (a != 0 && b > SIZE_MAX / a)
    {
        return false;
    }
    *product = a * b;
    return true;
}

#endif /* SDFIO_SRC_SIZE_MATH_H */
