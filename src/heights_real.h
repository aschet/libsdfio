/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file heights_real.h
 * @brief Height conversion for one floating-point type.
 *
 * This file is included twice by heights.c, once for `float` and once for
 * `double`, so that heights of a type are calculated in that type and a program
 * that uses one of them has no code of the other in its path. The including
 * file defines REAL, REAL_SUFFIX, REAL_MAX, REAL_ABS and REAL_NEARBY, and the
 * functions scale_init, scale_multiply and scale_divide of the type.
 */

#ifndef REAL
#error "Define REAL before including heights_real.h"
#endif

#define REAL_JOIN_(base, suffix) base##_##suffix
#define REAL_JOIN(base, suffix) REAL_JOIN_(base, suffix)
#define REAL_NAME(base) REAL_JOIN(base, REAL_SUFFIX)

static bool REAL_NAME(is_finite)(REAL value)
{
    return REAL_ABS(value) <= REAL_MAX;
}

/**
 * A case of the decoding, for SDFIO_SCALARS(): a coded value is converted
 * to the type of the heights and multiplied by the scale. The lowest value of
 * the type is the bad value.
 */
#define DECODE_CASE(data, type, low, high, floating)                           \
    case data:                                                                 \
        for (size_t i = 0; i < count; ++i)                                     \
        {                                                                      \
            type coded_value;                                                  \
            memcpy(&coded_value,                                               \
                   (const unsigned char *)coded + i * sizeof coded_value,      \
                   sizeof coded_value);                                        \
            REAL height = (REAL)NAN;                                           \
            if (coded_value != (type)(low))                                    \
            {                                                                  \
                if ((floating) && sizeof(type) > sizeof(REAL))                 \
                {                                                              \
                    const double wide = (double)coded_value;                   \
                    if (fabs(wide) <= DBL_MAX                                  \
                        && fabs(wide) > (double)REAL_MAX)                      \
                    {                                                          \
                        return SDFIO_ERR_RANGE;                                \
                    }                                                          \
                }                                                              \
                const REAL number = (REAL)coded_value;                         \
                if (!REAL_NAME(is_finite)(number))                             \
                {                                                              \
                    height = number;                                           \
                }                                                              \
                else if (!REAL_NAME(scale_multiply)(&scale, number, &height))  \
                {                                                              \
                    return SDFIO_ERR_RANGE;                                    \
                }                                                              \
            }                                                                  \
            memcpy((unsigned char *)heights + (ptrdiff_t)i * element, &height, \
                   sizeof height);                                             \
        }                                                                      \
        return SDFIO_OK;

static sdfio_status REAL_NAME(decode)(const sdfio_header *header,
                                      const void *coded, size_t count,
                                      void *heights, ptrdiff_t element)
{
    REAL_NAME(scale_type) scale;
    REAL_NAME(scale_init)(header->z_scale, &scale);
    switch (header->data_type)
    {
        SDFIO_SCALARS(DECODE_CASE)
    }
    return SDFIO_ERR_ARGUMENT;
}

#undef DECODE_CASE

/**
 * A case of the encoding, for SDFIO_SCALARS(): a height is divided by the scale
 * and rounded to a whole number for an integer type, and checked against the
 * range of the type and against the bad value, which is the lowest value of the
 * type. A height that is not a finite number is written as the bad value.
 */
#define ENCODE_CASE(data, type, low, high, floating)                           \
    case data:                                                                 \
        for (size_t i = 0; i < count; ++i)                                     \
        {                                                                      \
            REAL height;                                                       \
            memcpy(&height,                                                    \
                   (const unsigned char *)heights + (ptrdiff_t)i * element,    \
                   sizeof height);                                             \
            type coded_value = (type)(low);                                    \
            if (REAL_NAME(is_finite)(height))                                  \
            {                                                                  \
                REAL quotient = 0;                                             \
                REAL correction = 0;                                           \
                if (!REAL_NAME(scale_divide)(&scale, height, &quotient,        \
                                             &correction))                     \
                {                                                              \
                    return SDFIO_ERR_RANGE;                                    \
                }                                                              \
                if (floating)                                                  \
                {                                                              \
                    const REAL number = quotient + correction;                 \
                    if (!REAL_NAME(is_finite)(number)                          \
                        || (sizeof(type) < sizeof(REAL)                        \
                            && REAL_ABS(number) > (REAL)(high)))               \
                    {                                                          \
                        return SDFIO_ERR_RANGE;                                \
                    }                                                          \
                    coded_value = (type)number;                                \
                }                                                              \
                else                                                           \
                {                                                              \
                    REAL number = REAL_NEARBY(quotient);                       \
                    const REAL difference = (quotient - number) + correction;  \
                    number += difference > (REAL)0.5      ? (REAL)1            \
                              : difference < (REAL) - 0.5 ? (REAL) - 1         \
                                                          : (REAL)0;           \
                    if (number < (REAL)(low)                                   \
                        || number >= (REAL)(high) + (REAL)1)                   \
                    {                                                          \
                        return SDFIO_ERR_RANGE;                                \
                    }                                                          \
                    coded_value = (type)number;                                \
                }                                                              \
                if (coded_value == (type)(low))                                \
                {                                                              \
                    return SDFIO_ERR_BAD_VALUE;                                \
                }                                                              \
            }                                                                  \
            memcpy((unsigned char *)coded + i * sizeof coded_value,            \
                   &coded_value, sizeof coded_value);                          \
        }                                                                      \
        return SDFIO_OK;

static sdfio_status REAL_NAME(encode)(const sdfio_header *header,
                                      const void *heights, ptrdiff_t element,
                                      size_t count, void *coded)
{
    REAL_NAME(scale_type) scale;
    REAL_NAME(scale_init)(header->z_scale, &scale);
    switch (header->data_type)
    {
        SDFIO_SCALARS(ENCODE_CASE)
    }
    return SDFIO_ERR_ARGUMENT;
}

#undef ENCODE_CASE

/* The lowest and the highest height of a matrix, without the heights that are
 * not finite. */
static void REAL_NAME(find_range)(const void *heights, size_t rows,
                                  size_t columns, ptrdiff_t row_stride,
                                  ptrdiff_t element, double *lowest,
                                  double *highest)
{
    REAL low = REAL_MAX;
    REAL high = -REAL_MAX;
    bool found = false;
    for (size_t row = 0; row < rows; ++row)
    {
        const unsigned char *at =
            (const unsigned char *)heights + (ptrdiff_t)row * row_stride;
        for (size_t column = 0; column < columns; ++column)
        {
            REAL height;
            memcpy(&height, at + (ptrdiff_t)column * element, sizeof height);
            if (!REAL_NAME(is_finite)(height))
            {
                continue;
            }
            low = found && low < height ? low : height;
            high = found && high > height ? high : height;
            found = true;
        }
    }
    *lowest = found ? (double)low : (double)NAN;
    *highest = found ? (double)high : (double)NAN;
}

#undef REAL_NAME
#undef REAL_JOIN
#undef REAL_JOIN_
