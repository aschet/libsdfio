/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* Numbers as text, whatever the locale: the C functions that print and parse
 * numbers use the decimal point of the locale, and the files always have a
 * dot. */

#include "internal.h"

#include <stdio.h>

bool sdfi_is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f'
           || c == '\r';
}

bool sdfi_is_digit(char c)
{
    return c >= '0' && c <= '9';
}

char sdfi_lower(char c)
{
    if (c >= 'A' && c <= 'Z')
    {
        return (char)(c + ('a' - 'A'));
    }
    return c;
}

bool sdfi_equals_nocase(const char *text, size_t size, const char *lower)
{
    if (strlen(lower) != size)
    {
        return false;
    }
    for (size_t i = 0; i < size; ++i)
    {
        if (sdfi_lower(text[i]) != lower[i])
        {
            return false;
        }
    }
    return true;
}

void sdfi_trim(const char **text, size_t *size)
{
    while (*size > 0 && sdfi_is_space((*text)[0]))
    {
        ++*text;
        --*size;
    }
    while (*size > 0 && sdfi_is_space((*text)[*size - 1]))
    {
        --*size;
    }
}

sdfi_point sdfi_point_now(void)
{
    /* 1.5 printed: the digit 1, the point, the digit 5. */
    const size_t digits_around = 2;
    char probe[SDFI_NUMBER_CAPACITY];
    const int length = snprintf(probe, sizeof probe, "%.1f", 1.5);
    sdfi_point point = {{'.', '\0'}};
    if (length > (int)digits_around && (size_t)length < sizeof probe)
    {
        const size_t size = (size_t)length - digits_around;
        if (size < sizeof point.text)
        {
            memcpy(point.text, probe + 1, size);
            point.text[size] = '\0';
        }
    }
    return point;
}

bool sdfi_parse_number(const sdfi_point *point, const char *text, size_t size,
                       double *out)
{
    sdfi_trim(&text, &size);
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
    if (sdfi_equals_nocase(text, size, "inf")
        || sdfi_equals_nocase(text, size, "infinity"))
    {
        *out = sign * INFINITY;
        return true;
    }
    if (sdfi_equals_nocase(text, size, "nan"))
    {
        *out = NAN;
        return true;
    }

    /* digits [. digits] [e [sign] digits], with a digit before or after the
     * dot */
    size_t i = 0;
    size_t digits = 0;
    while (i < size && sdfi_is_digit(text[i]))
    {
        ++i;
        ++digits;
    }
    size_t dot = size;
    if (i < size && text[i] == '.')
    {
        dot = i;
        ++i;
        while (i < size && sdfi_is_digit(text[i]))
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
        while (i < size && sdfi_is_digit(text[i]))
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
    char buffer[SDFI_NUMBER_CAPACITY];
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

bool sdfi_parse_integer(const char *text, size_t size, long long *out)
{
    sdfi_trim(&text, &size);
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
        if (!sdfi_is_digit(text[i]))
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

size_t sdfi_format_scientific(const sdfi_point *point, char *out,
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
    char raw[SDFI_NUMBER_CAPACITY];
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
    char mantissa[SDFI_NUMBER_CAPACITY];
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
