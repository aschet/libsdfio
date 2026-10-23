/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file tagged.c
 * @brief Handling of Name = Value fields.
 */

#include "tagged.h"
#include "dynstring.h"
#include "sink.h"
#include "text.h"
#include <ctype.h>
#include <sdfio/sdfio.h>
#include <string.h>

static bool next_line(const char *text, size_t size, size_t *position,
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
    while (next_line(text, size, &position, &line, &line_size))
    {
        if (!sdfio_sink_write(sink, line, line_size)
            || !sdfio_sink_write(sink, line_end, sizeof line_end - 1))
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
        const char c = sdfio_lower(name[i]);
        if (!isdigit((unsigned char)c) && !(c >= 'a' && c <= 'z') && c != '_')
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
        const unsigned char c = (unsigned char)value[i];
        if (c == '\r' || c == '\n' || c >= SDFIO_FIRST_NON_ASCII)
        {
            return false;
        }
    }
    return true;
}
