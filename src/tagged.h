/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file tagged.h
 * @brief Handling of Name = Value fields.
 */

#ifndef SDFIO_SRC_TAGGED_H
#define SDFIO_SRC_TAGGED_H

#include "sink.h"

/**
 * Splits a line of the form `Name = Value` into its name and its value. The
 * name consists of letters, digits and underscores, and white space around the
 * name, the separator and the value is removed.
 *
 * @param line The line, without its line end.
 * @param size The length of the line.
 * @param[out] name Receives the name.
 * @param[out] name_size Receives the length of the name.
 * @param[out] value Receives the value.
 * @param[out] value_size Receives the length of the value.
 * @return False if the line is not of that form.
 */
bool sdfio_tagged_split(const char *line, size_t size, const char **name,
                        size_t *name_size, const char **value,
                        size_t *value_size);

/** What a text that is written in parts leaves open between two parts. */
typedef struct sdfio_crlf_state
{
    /** Whether the last line has characters and no line end yet. */
    bool open;
    /** Whether the last part ended with a CR, which is held back until the
     * next part shows whether an LF follows. */
    bool pending_cr;
} sdfio_crlf_state;

/**
 * Writes a part of a text, with CRLF as every line end. LF and CRLF in the
 * text are line ends. The last line of the part stays open and continues with
 * the next part, so a text that is split anywhere is written as the whole text.
 *
 * @param sink The sink.
 * @param[in,out] state What the parts before left open.
 * @param text The part.
 * @param size The length of the part.
 * @return False if the stream failed.
 */
bool sdfio_crlf_write(sdfio_sink *sink, sdfio_crlf_state *state,
                      const char *text, size_t size);

/**
 * Ends the line that the parts left open with a CRLF, if there is one.
 *
 * @param sink The sink.
 * @param[in,out] state What the parts left open.
 * @return False if the stream failed.
 */
bool sdfio_crlf_end(sdfio_sink *sink, sdfio_crlf_state *state);

/**
 * Writes the CR that ended the last text, as a character of the line, because
 * no text follows.
 *
 * @param sink The sink.
 * @param[in,out] state The state of the text.
 * @return False if the sink failed.
 */
bool sdfio_crlf_flush(sdfio_sink *sink, sdfio_crlf_state *state);

/**
 * Tells whether a text is the name of a field: letters, digits and `_`.
 *
 * @param name The name, 0-terminated.
 */
bool sdfio_tagged_name_valid(const char *name);

/**
 * Tells whether two names of fields are the same, whatever the case.
 *
 * @param a The first name.
 * @param a_size The number of characters of `a`.
 * @param b The second name.
 * @param b_size The number of characters of `b`.
 */
bool sdfio_tagged_name_equal(const char *a, size_t a_size, const char *b,
                             size_t b_size);

/**
 * Tells whether a text is the value of a field: one line of 7-bit ASCII.
 *
 * @param value The value, 0-terminated.
 */
bool sdfio_tagged_value_valid(const char *value);

#endif /* SDFIO_SRC_TAGGED_H */
