/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file data_type.h
 * @brief Data types of the data area.
 */

#ifndef SDFIO_SRC_DATA_TYPE_H
#define SDFIO_SRC_DATA_TYPE_H

#include "common.h"

/** The size of the largest sample of the data area, a binary64. */
enum
{
    SDFIO_LARGEST_SAMPLE = 8
};

/**
 * The data types of the format as a list for X-macros.
 *
 * For each type the list has the data type, its C type, its lowest and highest
 * value, and whether it is a floating point type. The lowest value is also the
 * bad value, which marks a point that is not measured. The highest value is
 * not special.
 *
 * @param X The macro that is called for each type.
 */
#define SDFIO_SCALARS(X)                                                       \
    X(SDFIO_TYPE_INT8, int8_t, INT8_MIN, INT8_MAX, false)                      \
    X(SDFIO_TYPE_INT16, int16_t, INT16_MIN, INT16_MAX, false)                  \
    X(SDFIO_TYPE_INT32, int32_t, INT32_MIN, INT32_MAX, false)                  \
    X(SDFIO_TYPE_FLOAT, float, -FLT_MAX, FLT_MAX, true)                        \
    X(SDFIO_TYPE_DOUBLE, double, -DBL_MAX, DBL_MAX, true)

/**
 * Tells whether a value is a type of heights: SDFIO_TYPE_FLOAT or
 * SDFIO_TYPE_DOUBLE.
 *
 * @param type The value to test.
 */
bool sdfio_valid_height_type(sdfio_data_type type);

/**
 * Returns the size in bytes of one value of a type, or 0 for an invalid value.
 *
 * @param type The type.
 */
size_t sdfio_data_type_size(sdfio_data_type type);

/**
 * Tells whether a value is a data type.
 *
 * @param type The value to test.
 */
bool sdfio_valid_data_type(sdfio_data_type type);

/**
 * Tells whether a data type is a floating point type.
 *
 * @param type The data type. An invalid value is not one.
 */
bool sdfio_is_floating(sdfio_data_type type);

#endif /* SDFIO_SRC_DATA_TYPE_H */
