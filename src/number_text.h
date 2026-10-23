/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file number_text.h
 * @brief Locale-independent number conversion.
 */

#ifndef SDFIO_SRC_NUMBER_TEXT_H
#define SDFIO_SRC_NUMBER_TEXT_H

#include <sdfio/sdfio.h>

/** The sizes and the digits of numbers. */
enum
{
    /** The size of the longest number of an ASCII data area. */
    SDFIO_NUMBER_CAPACITY = 64,
    /** The size of the text of a decimal point. */
    SDFIO_POINT_CAPACITY = 8,
    /** The digits after the point of a binary32. */
    SDFIO_FLOAT_DIGITS = 6,
    /** The digits of the exponent of a binary32. */
    SDFIO_FLOAT_EXPONENT = 2,
    /** The digits after the point of a binary64. */
    SDFIO_DOUBLE_DIGITS = 14,
    /** The digits of the exponent of a binary64. */
    SDFIO_DOUBLE_EXPONENT = 3
};

/**
 * The character that the locale uses for the decimal point.
 *
 * It is measured with a print and not read from a table that the C library
 * shares between threads.
 */
typedef struct sdfio_point
{
    /** The characters, 0-terminated. */
    char text[SDFIO_POINT_CAPACITY];
} sdfio_point;

/** Measures the decimal point that the locale has now. */
sdfio_point sdfio_point_now(void);

/**
 * Parses a number, whatever the locale.
 *
 * The text is a number as the format has it, or inf, infinity or nan, with an
 * optional sign. White space around it is ignored.
 *
 * @param point The decimal point of the locale.
 * @param text The text.
 * @param size The length of the text.
 * @param[out] out Receives the number.
 * @return False if the text is not a number.
 */
bool sdfio_parse_number(const sdfio_point *point, const char *text, size_t size,
                        double *out);

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
 * Writes a number as 1.5e+000.
 *
 * @param point The decimal point of the locale.
 * @param[out] out Receives the text, without a terminator.
 * @param capacity The size of `out`.
 * @param value The number.
 * @param digits The digits after the point.
 * @param exponent_width The digits of the exponent.
 * @return The length of the text.
 */
size_t sdfio_format_scientific(const sdfio_point *point, char *out,
                               size_t capacity, double value, int digits,
                               int exponent_width);

#endif /* SDFIO_SRC_NUMBER_TEXT_H */
