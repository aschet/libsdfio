/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief Writing of a text in parts with CRLF line ends.
 */

#ifndef SDFIO_SRC_CRLF_H
#define SDFIO_SRC_CRLF_H

#include "sink.h"
#include <stdbool.h>
#include <stddef.h>

/** State of a text that is written in parts: the line that stays open between
 * two parts. */
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
 * @param[in,out] state The state after the parts before.
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
 * @param[in,out] state The state after the parts.
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

#endif /* SDFIO_SRC_CRLF_H */
