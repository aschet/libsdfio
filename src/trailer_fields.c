/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief Fields of a trailer text.
 */

#include "tagged.h"
#include <string.h>

bool sdfio_trailer_next_field(const char *text, size_t size, size_t *position,
                              size_t *name_at, size_t *name_size,
                              size_t *value_at, size_t *value_size)
{
    if (text == NULL || position == NULL || name_at == NULL || name_size == NULL
        || value_at == NULL || value_size == NULL)
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
        const char *name = NULL;
        const char *value = NULL;
        if (sdfio_tagged_split(line, line_size, &name, name_size, &value,
                               value_size))
        {
            *name_at = (size_t)(name - text);
            *value_at = (size_t)(value - text);
            return true;
        }
    }
    return false;
}

bool sdfio_trailer_find_field(const char *text, size_t size, const char *name,
                              size_t *value_at, size_t *value_size)
{
    if (name == NULL || value_at == NULL || value_size == NULL)
    {
        return false;
    }
    const size_t name_length = strlen(name);
    size_t position = 0;
    size_t field_at = 0;
    size_t field_size = 0;
    while (sdfio_trailer_next_field(text, size, &position, &field_at,
                                    &field_size, value_at, value_size))
    {
        if (sdfio_tagged_name_equal(text + field_at, field_size, name,
                                    name_length))
        {
            return true;
        }
    }
    *value_at = 0;
    *value_size = 0;
    return false;
}
