/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file number_text.c
 * @brief Locale-independent number conversion.
 *
 * The functions of the C library that print and parse numbers use the decimal
 * point of the current locale, whereas the files always use `.`. Only the ASCII
 * format uses this code, so a program that does not list that format does not
 * contain it.
 */

#include "number_text.h"
#include "text.h"
#include <ctype.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <stdio.h>

sdfio_decimal_point sdfio_decimal_point_now(void)
{
    sdfio_decimal_point decimal_point = {{'.', '\0'}};
    const struct lconv *locale = localeconv();
    if (locale != NULL && locale->decimal_point != NULL
        && locale->decimal_point[0] != '\0'
        && strlen(locale->decimal_point) < sizeof decimal_point.text)
    {
        memcpy(decimal_point.text, locale->decimal_point,
               strlen(locale->decimal_point) + 1);
    }
    return decimal_point;
}

/* Takes the sign at the start of a text, if there is one, and tells whether it
 * is a minus. */
static bool take_sign(const char **text, size_t *size)
{
    bool negative = false;
    if (*size > 0 && ((*text)[0] == '+' || (*text)[0] == '-'))
    {
        negative = (*text)[0] == '-';
        ++*text;
        --*size;
    }
    return negative;
}

/* Tolerated deviation: any decimal or scientific form of a number is read, not
 * only the normalized form with a signed exponent that the standard has.
 *
 * Checks the text against the grammar of a number and copies it into a buffer
 * with the decimal point of the locale, which strtod() and strtof() want. The
 * sign is not copied. */
static bool scan_number(const sdfio_decimal_point *decimal_point,
                        const char *text, size_t size,
                        char buffer[SDFIO_NUMBER_CAPACITY], size_t *length,
                        bool *negative)
{
    sdfio_trim(&text, &size);
    *negative = take_sign(&text, &size);
    if (size == 0 || text[0] == '+' || text[0] == '-')
    {
        return false;
    }

    /* digits [. digits] [e [sign] digits], with a digit before or after the
     * dot */
    size_t i = 0;
    size_t digits = 0;
    while (i < size && isdigit((unsigned char)text[i]))
    {
        ++i;
        ++digits;
    }
    size_t dot = size;
    if (i < size && text[i] == '.')
    {
        dot = i;
        ++i;
        while (i < size && isdigit((unsigned char)text[i]))
        {
            ++i;
            ++digits;
        }
    }
    if (digits == 0)
    {
        return false;
    }
    if (i < size && (text[i] == 'e' || text[i] == 'E'))
    {
        ++i;
        if (i < size && (text[i] == '+' || text[i] == '-'))
        {
            ++i;
        }
        size_t exponent = 0;
        while (i < size && isdigit((unsigned char)text[i]))
        {
            ++i;
            ++exponent;
        }
        if (exponent == 0)
        {
            return false;
        }
    }
    if (i != size)
    {
        return false;
    }

    const size_t decimal_point_size = strlen(decimal_point->text);
    if (size + decimal_point_size >= SDFIO_NUMBER_CAPACITY)
    {
        return false;
    }
    *length = 0;
    for (size_t k = 0; k < size; ++k)
    {
        if (k == dot)
        {
            memcpy(buffer + *length, decimal_point->text, decimal_point_size);
            *length += decimal_point_size;
        }
        else
        {
            buffer[(*length)++] = text[k];
        }
    }
    buffer[*length] = '\0';
    return true;
}

bool sdfio_parse_number(const sdfio_decimal_point *decimal_point,
                        const char *text, size_t size, double *out)
{
    char buffer[SDFIO_NUMBER_CAPACITY];
    size_t length = 0;
    bool negative = false;
    if (!scan_number(decimal_point, text, size, buffer, &length, &negative))
    {
        return false;
    }
    char *end = NULL;
    const double value = strtod(buffer, &end);
    if (end != buffer + length)
    {
        return false;
    }
    *out = negative ? -value : value;
    return true;
}

bool sdfio_parse_float(const sdfio_decimal_point *decimal_point,
                       const char *text, size_t size, float *out)
{
    char buffer[SDFIO_NUMBER_CAPACITY];
    size_t length = 0;
    bool negative = false;
    if (!scan_number(decimal_point, text, size, buffer, &length, &negative))
    {
        return false;
    }
    char *end = NULL;
    const float value = strtof(buffer, &end);
    if (end != buffer + length || isinf(value))
    {
        return false;
    }
    *out = negative ? -value : value;
    return true;
}

bool sdfio_parse_integer(const char *text, size_t size, long long *out)
{
    sdfio_trim(&text, &size);
    const bool negative = take_sign(&text, &size);
    if (size == 0)
    {
        return false;
    }
    /* The magnitude of the lowest value is one more than the highest. */
    unsigned long long value = 0;
    const unsigned long long limit =
        (unsigned long long)LLONG_MAX + (negative ? 1ULL : 0ULL);
    const unsigned long long base = 10;
    for (size_t i = 0; i < size; ++i)
    {
        if (!isdigit((unsigned char)text[i]))
        {
            return false;
        }
        const unsigned long long digit = (unsigned long long)(text[i] - '0');
        if (value > (limit - digit) / base)
        {
            return false;
        }
        value = value * base + digit;
    }
    *out = negative ? (long long)(0ULL - value) : (long long)value;
    return true;
}

bool sdfio_parse_bounded_integer(const char *text, size_t size, int32_t low,
                                 int32_t high, int32_t *out)
{
    const bool negative = take_sign(&text, &size);
    if (size == 0)
    {
        return false;
    }
    /* The magnitude of the bound, which for the lowest value of a type is one
     * more than the highest. */
    const uint32_t limit =
        negative ? UINT32_C(0) - (uint32_t)low : (uint32_t)high;
    const uint32_t limit_tens = limit / 10U;
    const uint32_t limit_ones = limit % 10U;
    uint32_t value = 0;
    for (size_t i = 0; i < size; ++i)
    {
        const uint32_t digit = (uint32_t)(unsigned char)text[i] - (uint32_t)'0';
        if (digit > 9U || value > limit_tens
            || (value == limit_tens && digit > limit_ones))
        {
            return false;
        }
        value = value * 10U + digit;
    }
    *out = negative ? (int32_t)(UINT32_C(0) - value) : (int32_t)value;
    return true;
}

/* Copies as much of a text as the capacity holds and returns its length. */
static size_t copy_text(char *out, size_t capacity, const char *text,
                        size_t size)
{
    memcpy(out, text, size < capacity ? size : capacity);
    return size;
}

size_t sdfio_format_scientific(const sdfio_decimal_point *decimal_point,
                               char *out, size_t capacity, double value,
                               int digits, int exponent_width)
{
    /* A negative zero is a zero: the format has no sign to keep for it. */
    if (value == 0.0)
    {
        value = 0.0;
    }
    if (isnan(value))
    {
        return copy_text(out, capacity, "nan", 3);
    }
    char raw[SDFIO_NUMBER_CAPACITY];
    const int printed = snprintf(raw, sizeof raw, "%.*e", digits, value);
    if (printed < 0 || (size_t)printed >= sizeof raw)
    {
        return copy_text(out, capacity, "0", 1);
    }
    const size_t length = (size_t)printed;
    const char *marker = memchr(raw, 'e', length);
    if (marker == NULL)
    {
        return copy_text(out, capacity, raw, length); /* inf */
    }

    /* The C standard gives the form of the conversion: an optional minus, one
     * digit, the decimal point of the locale if digits follow, the digits, e, a
     * sign and at least two digits of the exponent. */
    char text[SDFIO_NUMBER_CAPACITY];
    size_t used = raw[0] == '-' ? 2 : 1;
    memcpy(text, raw, used);
    const char *rest = raw + used;
    if (rest < marker)
    {
        text[used++] = '.';
        rest += strlen(decimal_point->text);
        memcpy(text + used, rest, (size_t)(marker - rest));
        used += (size_t)(marker - rest);
    }

    /* The exponent has a sign and at least `exponent_width` digits, which the
     * C library does not promise. */
    const char *exponent = marker + 2;
    const char *end = raw + length;
    while (end - exponent > 1 && exponent[0] == '0')
    {
        ++exponent;
    }
    const size_t exponent_size = (size_t)(end - exponent);
    const size_t width = (size_t)exponent_width;
    const size_t padding = exponent_size < width ? width - exponent_size : 0;
    if (used + 2 + padding + exponent_size > sizeof text)
    {
        return copy_text(out, capacity, "0", 1);
    }
    text[used++] = 'e';
    text[used++] = marker[1] == '-' ? '-' : '+';
    memset(text + used, '0', padding);
    used += padding;
    memcpy(text + used, exponent, exponent_size);
    used += exponent_size;
    return copy_text(out, capacity, text, used);
}
