/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* The lines of the form `Name = Value` of a header and of the trailer of
 * ISO-2.0. */

#include "internal.h"

bool sdfi_next_line(const char *text, size_t size, size_t *position,
                    const char **line, size_t *line_size)
{
    if (*position >= size)
    {
        return false;
    }
    const size_t start = *position;
    size_t end = start;
    while (end < size && text[end] != '\n' && text[end] != '\r')
    {
        ++end;
    }
    *line = text + start;
    *line_size = end - start;
    if (end < size && text[end] == '\r' && end + 1 < size
        && text[end + 1] == '\n')
    {
        ++end;
    }
    *position = end < size ? end + 1 : end;
    return true;
}

static const char line_end[] = "\r\n";

bool sdfi_write_crlf_lines(sdfi_sink *sink, const char *text, size_t size)
{
    size_t position = 0;
    const char *line = NULL;
    size_t line_size = 0;
    while (sdfi_next_line(text, size, &position, &line, &line_size))
    {
        if (!sdfi_sink_write(sink, line, line_size)
            || !sdfi_sink_write(sink, line_end, sizeof line_end - 1))
        {
            return false;
        }
    }
    return true;
}

bool sdfi_append_crlf_lines(sdfi_string *out, const char *text, size_t size)
{
    size_t position = 0;
    const char *line = NULL;
    size_t line_size = 0;
    while (sdfi_next_line(text, size, &position, &line, &line_size))
    {
        if (!sdfi_string_append(out, line, line_size)
            || !sdfi_string_append(out, line_end, sizeof line_end - 1))
        {
            return false;
        }
    }
    return true;
}

void sdfi_lines_to_lf(sdfi_string *text)
{
    size_t used = 0;
    for (size_t i = 0; i < text->size; ++i)
    {
        char c = text->data[i];
        if (c == '\r')
        {
            c = '\n';
            if (i + 1 < text->size && text->data[i + 1] == '\n')
            {
                ++i;
            }
        }
        text->data[used++] = c;
    }
    /* The end of the last line is not part of the text. */
    if (used > 0 && text->data[used - 1] == '\n')
    {
        --used;
    }
    text->size = used;
    if (text->data != NULL)
    {
        text->data[used] = '\0';
    }
}

static bool is_name_character(char c)
{
    return sdfi_is_digit(c) || (sdfi_lower(c) >= 'a' && sdfi_lower(c) <= 'z')
           || c == '_';
}

/* Splits a line into its name and its value. */
static bool split_line(const char *line, size_t size, const char **name,
                       size_t *name_size, const char **value,
                       size_t *value_size)
{
    sdfi_trim(&line, &size);
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
    while (at < size && sdfi_is_space(line[at]))
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
    sdfi_trim(value, value_size);
    return true;
}

bool sdfi_tagged_valid(const char *text, size_t size)
{
    size_t position = 0;
    const char *line = NULL;
    size_t line_size = 0;
    while (sdfi_next_line(text, size, &position, &line, &line_size))
    {
        const char *blank = line;
        size_t blank_size = line_size;
        sdfi_trim(&blank, &blank_size);
        const char *name = NULL;
        const char *value = NULL;
        size_t name_size = 0;
        size_t value_size = 0;
        if (blank_size > 0
            && !split_line(line, line_size, &name, &name_size, &value,
                           &value_size))
        {
            return false;
        }
    }
    return true;
}

bool sdfi_tagged_next(const char *text, size_t size, size_t *position,
                      const char **name, size_t *name_size, const char **value,
                      size_t *value_size)
{
    const char *line = NULL;
    size_t line_size = 0;
    while (sdfi_next_line(text, size, position, &line, &line_size))
    {
        if (split_line(line, line_size, name, name_size, value, value_size))
        {
            return true;
        }
    }
    return false;
}

bool sdfi_tagged_find(const char *text, size_t size, const char *name,
                      const char **value, size_t *value_size)
{
    const size_t wanted = strlen(name);
    bool found = false;
    size_t position = 0;
    const char *field = NULL;
    const char *content = NULL;
    size_t field_size = 0;
    size_t content_size = 0;
    while (sdfi_tagged_next(text, size, &position, &field, &field_size,
                            &content, &content_size))
    {
        bool same = field_size == wanted;
        for (size_t i = 0; i < wanted && same; ++i)
        {
            same = sdfi_lower(field[i]) == sdfi_lower(name[i]);
        }
        if (same)
        {
            *value = content;
            *value_size = content_size;
            found = true;
        }
    }
    return found;
}
