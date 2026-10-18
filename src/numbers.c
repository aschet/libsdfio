/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* The characters of text, whatever the locale. */

#include "internal.h"

bool sdfio_is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f'
           || c == '\r';
}

bool sdfio_is_digit(char c)
{
    return c >= '0' && c <= '9';
}

char sdfio_lower(char c)
{
    if (c >= 'A' && c <= 'Z')
    {
        return (char)(c + ('a' - 'A'));
    }
    return c;
}

bool sdfio_equals_nocase(const char *text, size_t size, const char *lower)
{
    if (strlen(lower) != size)
    {
        return false;
    }
    for (size_t i = 0; i < size; ++i)
    {
        if (sdfio_lower(text[i]) != lower[i])
        {
            return false;
        }
    }
    return true;
}

void sdfio_trim(const char **text, size_t *size)
{
    while (*size > 0 && sdfio_is_space((*text)[0]))
    {
        ++*text;
        --*size;
    }
    while (*size > 0 && sdfio_is_space((*text)[*size - 1]))
    {
        --*size;
    }
}
