/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief Writing of a text in parts with CRLF line ends.
 */

#include "crlf.h"
#include "sink.h"
#include <string.h>

static const char line_end[] = "\r\n";
static const char carriage_return = '\r';

static bool write_line_end(sdfio_sink *sink)
{
    return sdfio_sink_write(sink, line_end, sizeof line_end - 1);
}

bool sdfio_crlf_write(sdfio_sink *sink, sdfio_crlf_state *state,
                      const char *text, size_t size)
{
    size_t position = 0;
    if (size > 0 && state->pending_cr)
    {
        /* The CR that ended the last part is the first half of a line end if
         * an LF follows, and a character of the line otherwise. */
        state->pending_cr = false;
        if (text[0] == '\n')
        {
            state->open = false;
            ++position;
            if (!write_line_end(sink))
            {
                return false;
            }
        }
        else if (!sdfio_sink_write(sink, &carriage_return, 1))
        {
            return false;
        }
    }
    while (position < size)
    {
        const char *newline = memchr(text + position, '\n', size - position);
        const size_t stop = newline != NULL ? (size_t)(newline - text) : size;
        /* A CR before the LF belongs to the line end, and a CR at the end of
         * the part is held back until the next part. */
        const bool carriage = stop > position && text[stop - 1] == '\r';
        const size_t content = carriage ? stop - 1 : stop;
        if (content > position
            && !sdfio_sink_write(sink, text + position, content - position))
        {
            return false;
        }
        if (newline == NULL)
        {
            state->open = true;
            state->pending_cr = carriage;
            return true;
        }
        state->open = false;
        position = stop + 1;
        if (!write_line_end(sink))
        {
            return false;
        }
    }
    return true;
}

bool sdfio_crlf_end(sdfio_sink *sink, sdfio_crlf_state *state)
{
    if (!state->open)
    {
        return true;
    }
    const bool held = state->pending_cr;
    *state = (sdfio_crlf_state){0};
    return (!held || sdfio_sink_write(sink, &carriage_return, 1))
           && write_line_end(sink);
}

bool sdfio_crlf_flush(sdfio_sink *sink, sdfio_crlf_state *state)
{
    const bool held = state->pending_cr;
    state->pending_cr = false;
    return !held || sdfio_sink_write(sink, &carriage_return, 1);
}
