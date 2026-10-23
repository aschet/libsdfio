/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file data_type.c
 * @brief Data types of the data area.
 */

#include "data_type.h"
#include <float.h>
#include <limits.h>

/* What the format needs of the platform, checked here and nowhere else: bytes
 * of 8 bits, and IEEE 754 binary32 and binary64 for float and double. The
 * integer types of the data area have exact widths, which the standard
 * guarantees for the types of <stdint.h>. */
_Static_assert(CHAR_BIT == 8, "the format counts in 8-bit bytes");
_Static_assert(sizeof(float) == 4 && FLT_RADIX == 2 && FLT_MANT_DIG == 24,
               "float has to be IEEE 754 binary32");
_Static_assert(sizeof(double) == 8 && DBL_MANT_DIG == 53,
               "double has to be IEEE 754 binary64");
_Static_assert(SDFIO_LARGEST_CODED_VALUE == sizeof(double),
               "the largest coded value has to be a binary64");

bool sdfio_valid_heights_type(sdfio_data_type type)
{
    return type == SDFIO_TYPE_FLOAT || type == SDFIO_TYPE_DOUBLE;
}

/* A case of the search for a data type by its code. */
#define CODE_CASE(data, ctype, low, high, floating)                            \
    if (code == (long long)(data))                                             \
    {                                                                          \
        *type = data;                                                          \
        return true;                                                           \
    }

bool sdfio_data_type_from_code(long long code, sdfio_data_type *type)
{
    SDFIO_SCALARS(CODE_CASE)
    return false;
}

#undef CODE_CASE

bool sdfio_valid_data_type(sdfio_data_type type)
{
    switch (type)
    {
    case SDFIO_TYPE_FLOAT:
    case SDFIO_TYPE_INT8:
    case SDFIO_TYPE_INT16:
    case SDFIO_TYPE_INT32:
    case SDFIO_TYPE_DOUBLE:
        return true;
    }
    return false;
}

size_t sdfio_data_type_size(sdfio_data_type type)
{
    switch (type)
    {
    case SDFIO_TYPE_INT8:
        return sizeof(int8_t);
    case SDFIO_TYPE_INT16:
        return sizeof(int16_t);
    case SDFIO_TYPE_INT32:
        return sizeof(int32_t);
    case SDFIO_TYPE_FLOAT:
        return sizeof(float);
    case SDFIO_TYPE_DOUBLE:
        return sizeof(double);
    }
    return 0;
}

bool sdfio_is_floating(sdfio_data_type type)
{
    return type == SDFIO_TYPE_FLOAT || type == SDFIO_TYPE_DOUBLE;
}
