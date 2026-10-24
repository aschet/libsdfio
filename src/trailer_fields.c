/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file trailer_fields.c
 * @brief Fields of a trailer text.
 */

#include "tagged.h"
#include "text.h"
#include <string.h>

bool sdfio_trailer_next_field(const char *text, size_t size, size_t *position,
                              const char **name, size_t *name_size,
                              const char **value, size_t *value_size)
{
    if (text == NULL || position == NULL || name == NULL || name_size == NULL
        || value == NULL || value_size == NULL)
    {
        return false;
    }
    while (*position < size)
    {
        const char *line = text + *position;
        const size_t left = size - *position;
        const char *newline = memchr(line, '\n', left);
        const size_t line_size =
            newline != NULL ? (size_t)(newline - line) : left;
        *position += newline != NULL ? line_size + 1 : line_size;
        if (sdfio_tagged_split(line, line_size, name, name_size, value,
                               value_size))
        {
            return true;
        }
    }
    return false;
}

static bool same_name(const char *a, const char *b, size_t size)
{
    for (size_t i = 0; i < size; ++i)
    {
        if (sdfio_lower(a[i]) != sdfio_lower(b[i]))
        {
            return false;
        }
    }
    return true;
}

bool sdfio_trailer_find_field(const char *text, size_t size, const char *name,
                              const char **value, size_t *value_size)
{
    if (name == NULL || value == NULL || value_size == NULL)
    {
        return false;
    }
    const size_t name_length = strlen(name);
    size_t position = 0;
    const char *field = NULL;
    size_t field_size = 0;
    while (sdfio_trailer_next_field(text, size, &position, &field, &field_size,
                                    value, value_size))
    {
        if (field_size == name_length && same_name(field, name, name_length))
        {
            return true;
        }
    }
    *value = NULL;
    *value_size = 0;
    return false;
}
