/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file reader_state.h
 * @brief State of a reader and what its parts share.
 */

#ifndef SDFIO_SRC_READER_STATE_H
#define SDFIO_SRC_READER_STATE_H

#include "defaults.h"
#include "header.h"
#include "message.h"
#include "options.h"
#include "owner.h"
#include "reader_format.h"
#include "source.h"
#include <sdfio/sdfio.h>

/** A stream over memory of the caller, for sdfio_open_memory(). */
typedef struct memory_stream
{
    /** The bytes. */
    const unsigned char *data;
    /** The number of bytes. */
    size_t size;
    /** The index of the next byte. */
    size_t position;
} memory_stream;

/** The state of a reader. */
struct sdfio_reader
{
    /** The text of the last failure. */
    sdfio_message message;
    /** The header of the open file. */
    sdfio_header header;
    /** The buffered stream. */
    sdfio_source source;
    /** What the format of the file keeps for it, or null. */
    void *format_state;
    /** The added formats. */
    const sdfio_reader_format *formats[SDFIO_FORMAT_MAX];
    /** The format of the open file. */
    const sdfio_reader_format *format;
    /** The options of the reader. */
    sdfio_options options;
    /** What a file adds to the reader. */
    sdfio_owner owner;
    /** The stream over memory, if the file is in memory. */
    memory_stream memory;
    /** Whether a file is open. */
    bool opened;
    /** The number of values of the data area. */
    size_t value_count;
    /** The index of the next row to read. */
    size_t next_row;
    /** The index of the next value to read. */
    size_t next_value;
    /** Whether the end of the data area was passed. */
    bool data_done;
    /** Whether an error in the middle of the data lost the position. */
    bool failed;
    /** Whether the reading of the trailer has begun. */
    bool trailer_begun;
    /** The room for a chunk of values. */
    unsigned char *chunk;
};

/**
 * Marks the reader as failed after an error in the data area, because the
 * position in the file is no longer known. The reader refuses further read
 * operations.
 *
 * @param reader The reader.
 * @param status The status of the error.
 * @return `status`.
 */
sdfio_status sdfio_reader_fail(sdfio_reader *reader, sdfio_status status);

/**
 * Verifies the precondition of every read operation: a file is open and no
 * error has ended the reading.
 *
 * @param reader The reader. May be null.
 * @retval SDFIO_ERR_ARGUMENT The reader is null.
 * @retval SDFIO_ERR_STATE No file is open, or an error ended the reading.
 */
sdfio_status sdfio_reader_require_readable(sdfio_reader *reader);

/**
 * Reads the next values of the data area through the format of the file. The
 * values are delivered in the data type of the file.
 *
 * @param reader The reader.
 * @param[out] dst Receives the values.
 * @param count The number of values.
 */
sdfio_status sdfio_reader_take_values(sdfio_reader *reader, void *dst,
                                      size_t count);

/**
 * Completes the data area. The function discards the values that were not
 * requested and consumes what terminates the data area in the format
 * of the file.
 *
 * @param reader The reader.
 */
sdfio_status sdfio_reader_finish_data(sdfio_reader *reader);

#endif /* SDFIO_SRC_READER_STATE_H */
