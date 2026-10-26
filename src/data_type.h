/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief Data types of the data area: sizes, C types and the check of a code.
 */

#ifndef SDFIO_SRC_DATA_TYPE_H
#define SDFIO_SRC_DATA_TYPE_H

#include <float.h>
#include <sdfio/sdfio.h>

/** The size in bytes of the largest coded value of the data area, which is a
 * binary64 number. */
#define SDFIO_LARGEST_CODED_VALUE 8

/**
 * Table of the data types of the format, for use as an X macro.
 *
 * Each entry lists the data type, the corresponding C type, the lowest and the
 * highest representable value, and whether the type is a floating-point type.
 * The lowest value is the bad value, which labels a non-measured or spurious
 * point. The highest value has no special meaning.
 *
 * @param X The macro that is called for each type.
 */
#define SDFIO_SCALARS(X) SDFIO_INTEGER_SCALARS(X) SDFIO_REAL_SCALARS(X)

/**
 * The integer data types of SDFIO_SCALARS(), for code that has no use for the
 * bounds of the floating-point types, which an integer cannot hold.
 *
 * @param X The macro that is called for each type.
 */
#define SDFIO_INTEGER_SCALARS(X)                                               \
    X(SDFIO_TYPE_INT8, int8_t, INT8_MIN, INT8_MAX, false)                      \
    X(SDFIO_TYPE_INT16, int16_t, INT16_MIN, INT16_MAX, false)                  \
    X(SDFIO_TYPE_INT32, int32_t, INT32_MIN, INT32_MAX, false)

/**
 * The floating-point data types of SDFIO_SCALARS().
 *
 * @param X The macro that is called for each type.
 */
#define SDFIO_REAL_SCALARS(X)                                                  \
    X(SDFIO_TYPE_FLOAT, float, -FLT_MAX, FLT_MAX, true)                        \
    X(SDFIO_TYPE_DOUBLE, double, -DBL_MAX, DBL_MAX, true)

/**
 * Tells whether a data type can serve as the type of heights: SDFIO_TYPE_FLOAT
 * or SDFIO_TYPE_DOUBLE.
 *
 * @param type The value to test.
 */
bool sdfio_heights_type_valid(sdfio_data_type type);

/**
 * Returns the size in bytes of one coded value of a data type, or 0 if the
 * value is not a data type.
 *
 * @param type The type.
 */
size_t sdfio_data_type_size(sdfio_data_type type);

/**
 * Tells whether a value is one of the data types of the format.
 *
 * @param type The value to test.
 */
bool sdfio_data_type_valid(sdfio_data_type type);

/**
 * Finds the data type that a code of a file stands for.
 *
 * @param code The code, as the header of a file gives it.
 * @param[out] type Receives the data type.
 * @return False if the code is no data type.
 */
bool sdfio_data_type_from_code(long long code, sdfio_data_type *type);

/**
 * Tells whether a data type is a floating-point type.
 *
 * @param type The data type. A value that is not a data type is not a
 * floating-point type.
 */
bool sdfio_is_floating(sdfio_data_type type);

#endif /* SDFIO_SRC_DATA_TYPE_H */
