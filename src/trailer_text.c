/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file trailer_text.c
 * @brief Reading of the trailer text in parts, with LF line ends.
 */

#include "trailer_text.h"

/* The bytes of the source that are looked at to find a line that consists
 * of `*`, so that the white space around the star is at most 30 bytes. */
#define STAR_WINDOW 32

static bool is_blank(int c)
{
    return c == ' ' || c == '\t';
}

/* Whether the next line consists of `*` and white space. The bytes are not
 * taken. A line with more white space than the window holds is no such line. */
static bool at_star_line(sdfio_source *source)
{
    const size_t buffered = sdfio_source_fill(source, STAR_WINDOW);
    const unsigned char *at = source->buffer + source->position;
    size_t i = 0;
    while (i < buffered && is_blank(at[i]))
    {
        ++i;
    }
    if (i == buffered || at[i] != '*')
    {
        return false;
    }
    ++i;
    while (i < buffered && is_blank(at[i]))
    {
        ++i;
    }
    if (i < buffered)
    {
        return at[i] == '\r' || at[i] == '\n';
    }
    return buffered < STAR_WINDOW; /* the file ends after the star */
}

/* Takes a character of the source into the state. */
static void take_character(sdfio_trailer_state *state, int c)
{
    if (state->pending_cr)
    {
        state->pending_cr = false;
        if (c == '\n')
        {
            ++state->line_ends;
            state->in_line = false;
            return;
        }
        /* A CR without an LF is part of the text. */
        state->held[state->held_count] = '\r';
        ++state->held_count;
        state->in_line = true;
    }
    if (c == '\r')
    {
        state->pending_cr = true;
    }
    else if (c == '\n')
    {
        ++state->line_ends;
        state->in_line = false;
    }
    else
    {
        state->held[state->held_count] = (unsigned char)c;
        ++state->held_count;
        state->in_line = true;
    }
}

/* Ends the text: a CR whose LF did not follow is a character of it. */
static void end_text(sdfio_trailer_state *state)
{
    if (state->pending_cr)
    {
        state->pending_cr = false;
        state->held[state->held_count] = '\r';
        ++state->held_count;
    }
    state->done = true;
}

sdfio_status sdfio_trailer_read(sdfio_source *source,
                                sdfio_trailer_state *state, bool record,
                                char *buffer, size_t capacity, size_t *size,
                                sdfio_message *message)
{
    *size = 0;
    while (*size < capacity)
    {
        if (state->held_count > 0 || (state->done && state->line_ends > 0))
        {
            if (state->line_ends > 0)
            {
                buffer[*size] = '\n';
                ++*size;
                --state->line_ends;
            }
            else
            {
                buffer[*size] = (char)state->held[0];
                ++*size;
                state->held[0] = state->held[1];
                --state->held_count;
            }
            continue;
        }
        if (state->done)
        {
            break;
        }
        if (record && !state->in_line && !state->pending_cr
            && at_star_line(source))
        {
            end_text(state);
            continue;
        }
        const int c = sdfio_source_getc(source);
        if (c < 0)
        {
            if (source->failed)
            {
                return sdfio_source_failure(source, message,
                                            "The trailer cannot be read");
            }
            end_text(state);
            continue;
        }
        take_character(state, c);
    }
    return SDFIO_OK;
}
