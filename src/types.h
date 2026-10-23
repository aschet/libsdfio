/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file types.h
 * @brief The data types, the revisions and the rules that depend on them.
 */

#ifndef SDFIO_SRC_TYPES_H
#define SDFIO_SRC_TYPES_H

#include "common.h"

/** The size of the largest sample, a binary64. */
enum
{
    SDFIO_LARGEST_SAMPLE = 8
};

/**
 * The data types of the format as a list for X-macros.
 *
 * For each type the list has the data type, its name, its C type, an unsigned
 * type of the same size, its lowest and highest value, and whether it is a
 * floating point type.
 *
 * @param X The macro that is called for each type.
 */
#define SDFIO_SCALARS(X)                                                       \
    X(SDFIO_TYPE_INT8, "int8", int8_t, uint8_t, INT8_MIN, INT8_MAX, false)     \
    X(SDFIO_TYPE_INT16, "int16", int16_t, uint16_t, INT16_MIN, INT16_MAX,      \
      false)                                                                   \
    X(SDFIO_TYPE_INT32, "int32", int32_t, uint32_t, INT32_MIN, INT32_MAX,      \
      false)                                                                   \
    X(SDFIO_TYPE_FLOAT, "binary32", float, uint32_t, -FLT_MAX, FLT_MAX, true)  \
    X(SDFIO_TYPE_DOUBLE, "binary64", double, uint64_t, -DBL_MAX, DBL_MAX, true)

/**
 * The checks of the sizes of the data types, as the file stores them: the type
 * of the program and the unsigned type that holds its bytes are of one size,
 * and no value is larger than the largest that a buffer is made for.
 */
#define SDFIO_CHECK_SCALAR(data, name, ctype, stored, low, high, floating)     \
    _Static_assert(sizeof(ctype) == sizeof(stored),                            \
                   name ": the type and its stored bytes differ in size");     \
    _Static_assert(sizeof(ctype) <= SDFIO_LARGEST_SAMPLE,                      \
                   name ": larger than the largest sample");
SDFIO_SCALARS(SDFIO_CHECK_SCALAR)
#undef SDFIO_CHECK_SCALAR
_Static_assert(sizeof(int8_t) == 1 && sizeof(int16_t) == 2
                   && sizeof(int32_t) == 4 && sizeof(uint64_t) == 8,
               "the integer types of the file have the sizes of the format");
_Static_assert(SDFIO_LARGEST_SAMPLE == sizeof(double),
               "the largest sample is a binary64");

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

/**
 * Returns the spelling of a revision in the magic, for example "ISO-2.0", or an
 * empty string for an invalid value.
 *
 * @param revision The revision.
 */
const char *sdfio_revision_text(sdfio_revision revision);

/**
 * Finds the revision that a name stands for.
 *
 * @param text The name, as in the magic: "ISO-2.0".
 * @param size The length of the name.
 * @param[out] revision Receives the revision.
 * @return False if the name is not one of a revision.
 */
bool sdfio_revision_from_text(const char *text, size_t size,
                              sdfio_revision *revision);

/**
 * Tells whether the dates of a revision are in UTC.
 *
 * @param revision The revision.
 */
static inline bool sdfio_requires_utc(sdfio_revision revision)
{
    return revision == SDFIO_ISO_2_0;
}

/**
 * Returns the largest number of columns or rows of a revision: the header
 * holds them as 2 bytes in ISO-1.0 and as 4 bytes in ISO-2.0.
 *
 * @param revision The revision.
 */
size_t sdfio_max_count(sdfio_revision revision);

/**
 * Returns the size of the binary header of a revision.
 *
 * @param revision The revision.
 */
size_t sdfio_binary_header_size(sdfio_revision revision);

/**
 * Counts the values of the data area, with a check for overflow.
 *
 * @param header The header.
 * @param[out] count Receives columns times rows.
 * @return False if the count does not fit in size_t.
 */
bool sdfio_count(const sdfio_header *header, size_t *count);

#endif /* SDFIO_SRC_TYPES_H */
