/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file tagged.c
 * @brief The lines of the form `Name = Value` of a header and of the trailer
 * of ISO-2.0.
 */

#include "internal.h"

bool sdfio_next_line(const char *text, size_t size, size_t *position,
                     const char **line, size_t *line_size)
{
    if (*position >= size)
    {
        return false;
    }
    const size_t start = *position;
    size_t end = start;
    while (end < size && text[end] != '\n')
    {
        ++end;
    }
    *line = text + start;
    *line_size = end - start;
    /* The CR of a CRLF is not part of the line. */
    if (end < size && *line_size > 0 && text[end - 1] == '\r')
    {
        --*line_size;
    }
    *position = end < size ? end + 1 : end;
    return true;
}

static const char line_end[] = "\r\n";

bool sdfio_write_crlf_lines(sdfio_sink *sink, const char *text, size_t size)
{
    size_t position = 0;
    const char *line = NULL;
    size_t line_size = 0;
    while (sdfio_next_line(text, size, &position, &line, &line_size))
    {
        if (!sdfio_sink_write(sink, line, line_size)
            || !sdfio_sink_write(sink, line_end, sizeof line_end - 1))
        {
            return false;
        }
    }
    return true;
}

bool sdfio_append_crlf_lines(sdfio_string *out, const char *text, size_t size)
{
    size_t position = 0;
    const char *line = NULL;
    size_t line_size = 0;
    while (sdfio_next_line(text, size, &position, &line, &line_size))
    {
        if (!sdfio_string_append(out, line, line_size)
            || !sdfio_string_append(out, line_end, sizeof line_end - 1))
        {
            return false;
        }
    }
    return true;
}

void sdfio_lines_to_lf(sdfio_string *text)
{
    size_t used = 0;
    for (size_t i = 0; i < text->size; ++i)
    {
        if (text->data[i] == '\r' && i + 1 < text->size
            && text->data[i + 1] == '\n')
        {
            continue;
        }
        text->data[used++] = text->data[i];
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
    return sdfio_is_digit(c) || (sdfio_lower(c) >= 'a' && sdfio_lower(c) <= 'z')
           || c == '_';
}

/* Splits a line into its name and its value. */
static bool split_line(const char *line, size_t size, const char **name,
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

bool sdfio_tagged_valid(const char *text, size_t size)
{
    size_t position = 0;
    const char *line = NULL;
    size_t line_size = 0;
    while (sdfio_next_line(text, size, &position, &line, &line_size))
    {
        const char *blank = line;
        size_t blank_size = line_size;
        sdfio_trim(&blank, &blank_size);
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

bool sdfio_tagged_next(const char *text, size_t size, size_t *position,
                       const char **name, size_t *name_size, const char **value,
                       size_t *value_size)
{
    const char *line = NULL;
    size_t line_size = 0;
    while (sdfio_next_line(text, size, position, &line, &line_size))
    {
        if (split_line(line, line_size, name, name_size, value, value_size))
        {
            return true;
        }
    }
    return false;
}

bool sdfio_tagged_find(const char *text, size_t size, const char *name,
                       const char **value, size_t *value_size)
{
    const size_t wanted = strlen(name);
    bool found = false;
    size_t position = 0;
    const char *field = NULL;
    const char *content = NULL;
    size_t field_size = 0;
    size_t content_size = 0;
    while (sdfio_tagged_next(text, size, &position, &field, &field_size,
                             &content, &content_size))
    {
        bool same = field_size == wanted;
        for (size_t i = 0; i < wanted && same; ++i)
        {
            same = sdfio_lower(field[i]) == sdfio_lower(name[i]);
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
