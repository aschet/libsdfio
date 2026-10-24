/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file trailer_text.h
 * @brief Reading of the trailer text in parts, with LF line ends.
 */

#ifndef SDFIO_SRC_TRAILER_TEXT_H
#define SDFIO_SRC_TRAILER_TEXT_H

#include "message.h"
#include "source.h"
#include <stdbool.h>
#include <stddef.h>

/**
 * What a reader keeps between the calls that read the trailer. The zeroed
 * state is the start of the trailer.
 */
typedef struct sdfio_trailer_state
{
    /** The line ends that were read and are not written yet. */
    size_t line_ends;
    /** Characters that were read and are not written yet. */
    unsigned char held[2];
    /** The number of characters in `held`. */
    size_t held_count;
    /** Whether a CR was read whose LF may follow. */
    bool pending_cr;
    /** Whether a line has begun. */
    bool in_line;
    /** Whether the end of the trailer was read. */
    bool done;
} sdfio_trailer_state;

/**
 * Reads the next part of the text of the trailer into a buffer.
 *
 * Every line end of the text is LF, whether the file has CRLF or LF. Nothing
 * else of the text changes: the white space before it and the line ends at its
 * end are part of it.
 *
 * @param source The source, after the data area.
 * @param[in,out] state What is kept between the calls.
 * @param record Whether the trailer is a record of an ASCII file: a line that
 * consists of `*` ends the trailer. Otherwise the trailer is the rest of the
 * file.
 * @param[out] buffer Receives the text.
 * @param capacity The size of `buffer` in bytes.
 * @param[out] size Receives the number of bytes written, 0 at the end.
 * @param[out] message Receives the reason of a failure.
 * @retval SDFIO_ERR_IO The stream failed.
 */
sdfio_status sdfio_trailer_read(sdfio_source *source,
                                sdfio_trailer_state *state, bool record,
                                char *buffer, size_t capacity, size_t *size,
                                sdfio_message *message);

#endif /* SDFIO_SRC_TRAILER_TEXT_H */
