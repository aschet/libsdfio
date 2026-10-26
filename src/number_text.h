/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief Locale-independent number conversion.
 */

#ifndef SDFIO_SRC_NUMBER_TEXT_H
#define SDFIO_SRC_NUMBER_TEXT_H

#include <sdfio/sdfio.h>

/** The size of the longest number of an ASCII data area. */
#define SDFIO_NUMBER_CAPACITY 64
/** The size of the text of a decimal point. */
#define SDFIO_DECIMAL_POINT_CAPACITY 8
/** The digits after the point of a binary32. */
#define SDFIO_FLOAT_DIGITS 6
/** The digits of the exponent of a binary32. */
#define SDFIO_FLOAT_EXPONENT 2
/** The digits after the point of a binary64. */
#define SDFIO_DOUBLE_DIGITS 14
/** The digits of the exponent of a binary64. */
#define SDFIO_DOUBLE_EXPONENT 3

/**
 * The character that the locale uses for the decimal point.
 *
 * It is measured with a print and not read from a table that the C library
 * shares between threads.
 */
typedef struct sdfio_decimal_point
{
    /** The characters, 0-terminated. */
    char text[SDFIO_DECIMAL_POINT_CAPACITY];
} sdfio_decimal_point;

/** Measures the decimal point that the locale has now. */
sdfio_decimal_point sdfio_decimal_point_now(void);

/**
 * Parses a number, whatever the locale.
 *
 * The text is a number as the format has it, or inf, infinity or nan, with an
 * optional sign. White space around it is ignored.
 *
 * @param decimal_point The decimal point of the locale.
 * @param text The text.
 * @param size The length of the text.
 * @param[out] out Receives the number.
 * @return False if the text is not a number.
 */
bool sdfio_parse_number(const sdfio_decimal_point *decimal_point,
                        const char *text, size_t size, double *out);

/**
 * Parses a number as sdfio_parse_number() does, rounded once to the nearest
 * float, which rounding a double to a float would not guarantee.
 *
 * @param decimal_point The decimal point of the locale.
 * @param text The text.
 * @param size The length of the text.
 * @param[out] out Receives the number.
 * @return False if the text is not a number or the number is too large for a
 * float.
 */
bool sdfio_parse_float(const sdfio_decimal_point *decimal_point,
                       const char *text, size_t size, float *out);

/**
 * Parses an integer in decimal digits, with an optional sign.
 *
 * @param text The text.
 * @param size The length of the text.
 * @param[out] out Receives the integer.
 * @return False if the text is not an integer or does not fit.
 */
bool sdfio_parse_integer(const char *text, size_t size, long long *out);

/**
 * Parses an integer in decimal digits, with an optional sign, that lies within
 * bounds. The text has no white space around it. The function is meant for the
 * values of a data area, which are many: it calculates in 32 bits.
 *
 * @param text The text.
 * @param size The length of the text.
 * @param low The lowest value that is accepted, at most 0.
 * @param high The highest value that is accepted, at least 0.
 * @param[out] out Receives the integer.
 * @return False if the text is not an integer or lies outside the bounds.
 */
bool sdfio_parse_bounded_integer(const char *text, size_t size, int32_t low,
                                 int32_t high, int32_t *out);

/**
 * Writes a number as 1.5e+000.
 *
 * @param decimal_point The decimal point of the locale.
 * @param[out] out Receives the text, without a terminator. A text that is
 * longer than the capacity is cut off.
 * @param capacity The size of `out`.
 * @param value The number.
 * @param digits The digits after the point.
 * @param exponent_width The digits of the exponent.
 * @return The length of the text, which can exceed the capacity.
 */
size_t sdfio_format_scientific(const sdfio_decimal_point *decimal_point,
                               char *out, size_t capacity, double value,
                               int digits, int exponent_width);

#endif /* SDFIO_SRC_NUMBER_TEXT_H */
