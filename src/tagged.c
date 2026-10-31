/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief Name = Value lines: splitting and checks.
 */

#include "tagged.h"
#include "text.h"
#include <ctype.h>

static bool is_name_character(char c)
{
    return isdigit((unsigned char)c)
           || (sdfio_lower(c) >= 'a' && sdfio_lower(c) <= 'z') || c == '_';
}

bool sdfio_tagged_split(const char *line, size_t size, const char **name,
                        size_t *name_size, const char **value,
                        size_t *value_size)
{
    sdfio_trim(&line, &size);
    size_t end = 0;
    while (end < size && is_name_character(line[end]))
    {
        ++end;
    }
    if (end == 0)
    {
        return false;
    }
    size_t at = end;
    while (at < size && sdfio_is_space(line[at]))
    {
        ++at;
    }
    if (at >= size || line[at] != '=')
    {
        return false;
    }
    *name = line;
    *name_size = end;
    *value = line + at + 1;
    *value_size = size - at - 1;
    sdfio_trim(value, value_size);
    return true;
}

bool sdfio_tagged_name_valid(const char *name)
{
    if (name[0] == '\0')
    {
        return false;
    }
    for (size_t i = 0; name[i] != '\0'; ++i)
    {
        if (!is_name_character(name[i]))
        {
            return false;
        }
    }
    return true;
}

bool sdfio_tagged_name_equal(const char *a, size_t a_size, const char *b,
                             size_t b_size)
{
    if (a_size != b_size)
    {
        return false;
    }
    for (size_t i = 0; i < a_size; ++i)
    {
        if (sdfio_lower(a[i]) != sdfio_lower(b[i]))
        {
            return false;
        }
    }
    return true;
}

bool sdfio_tagged_value_valid(const char *value)
{
    for (size_t i = 0; value[i] != '\0'; ++i)
    {
        const char c = value[i];
        if (c == '\r' || c == '\n')
        {
            return false;
        }
    }
    return true;
}
