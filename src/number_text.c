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
#include <stdlib.h>
#include <string.h>

#include <stdio.h>

sdfio_point sdfio_point_now(void)
{
    sdfio_point point = {{'.', '\0'}};
    const struct lconv *locale = localeconv();
    if (locale != NULL && locale->decimal_point != NULL
        && locale->decimal_point[0] != '\0'
        && strlen(locale->decimal_point) < sizeof point.text)
    {
        memcpy(point.text, locale->decimal_point,
               strlen(locale->decimal_point) + 1);
    }
    return point;
}

bool sdfio_parse_number(const sdfio_point *point, const char *text, size_t size,
                        double *out)
{
    sdfio_trim(&text, &size);
    bool negative = false;
    if (size > 0 && (text[0] == '+' || text[0] == '-'))
    {
        negative = text[0] == '-';
        ++text;
        --size;
    }
    if (size == 0 || text[0] == '+' || text[0] == '-')
    {
        return false;
    }
    const double sign = negative ? -1.0 : 1.0;

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

    /* strtod wants the point of the locale. */
    const size_t point_size = strlen(point->text);
    char buffer[SDFIO_NUMBER_CAPACITY];
    if (size + point_size >= sizeof buffer)
    {
        return false;
    }
    size_t length = 0;
    for (size_t k = 0; k < size; ++k)
    {
        if (k == dot)
        {
            memcpy(buffer + length, point->text, point_size);
            length += point_size;
        }
        else
        {
            buffer[length++] = text[k];
        }
    }
    buffer[length] = '\0';
    char *end = NULL;
    const double value = strtod(buffer, &end);
    if (end != buffer + length)
    {
        return false;
    }
    *out = sign * value;
    return true;
}

bool sdfio_parse_integer(const char *text, size_t size, long long *out)
{
    sdfio_trim(&text, &size);
    bool negative = false;
    if (size > 0 && (text[0] == '+' || text[0] == '-'))
    {
        negative = text[0] == '-';
        ++text;
        --size;
    }
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

size_t sdfio_format_scientific(const sdfio_point *point, char *out,
                               size_t capacity, double value, int digits,
                               int exponent_width)
{
    /* A negative zero is a zero: the format has no sign to keep for it. */
    if (value == 0.0)
    {
        value = 0.0;
    }
    if (isnan(value))
    {
        return (size_t)snprintf(out, capacity, "nan");
    }
    char raw[SDFIO_NUMBER_CAPACITY];
    const int length = snprintf(raw, sizeof raw, "%.*e", digits, value);
    if (length < 0 || (size_t)length >= sizeof raw)
    {
        return (size_t)snprintf(out, capacity, "0");
    }
    char *marker = strchr(raw, 'e');
    if (marker == NULL)
    {
        return (size_t)snprintf(out, capacity, "%s", raw); /* inf */
    }
    *marker = '\0';

    /* The mantissa with a dot. */
    char mantissa[SDFIO_NUMBER_CAPACITY];
    size_t used = 0;
    const size_t point_size = strlen(point->text);
    for (const char *p = raw; *p != '\0';)
    {
        if (point_size > 0 && strncmp(p, point->text, point_size) == 0)
        {
            mantissa[used++] = '.';
            p += point_size;
        }
        else
        {
            mantissa[used++] = *p++;
        }
    }
    mantissa[used] = '\0';

    /* The exponent has a sign and at least `exponent_width` digits, which the
     * C library does not promise. */
    const char sign = marker[1] == '-' ? '-' : '+';
    const char *exponent = marker + 2;
    while (exponent[0] == '0' && strlen(exponent) > 1)
    {
        ++exponent;
    }
    const size_t exponent_size = strlen(exponent);
    const size_t width = (size_t)exponent_width;
    const size_t padding = exponent_size < width ? width - exponent_size : 0;
    return (size_t)snprintf(out, capacity, "%se%c%.*s%s", mantissa, sign,
                            (int)padding, "0000000000", exponent);
}
