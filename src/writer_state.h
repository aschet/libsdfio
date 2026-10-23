/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file writer_state.h
 * @brief State of a writer and what its parts share.
 */

#ifndef SDFIO_SRC_WRITER_STATE_H
#define SDFIO_SRC_WRITER_STATE_H

#include "message.h"
#include "owner.h"
#include "sink.h"
#include "writer_codec.h"
#include <sdfio/sdfio.h>

/** Where a writer is in the life of a file. */
typedef enum sdfio_writer_phase
{
    /** No file is created. */
    SDFIO_WRITER_NEW,
    /** A file is created and not ended. */
    SDFIO_WRITER_CREATED,
    /** The file is ended. */
    SDFIO_WRITER_ENDED,
    /** An error ended the writing. */
    SDFIO_WRITER_FAILED
} sdfio_writer_phase;

/** The state of a writer. */
struct sdfio_writer
{
    /** The text of the last failure. */
    sdfio_message message;
    /** The header of the file. */
    sdfio_header header;
    /** The buffered stream. */
    sdfio_sink sink;
    /** What the codec of the file keeps for it, or null. */
    void *codec_state;
    /** The listed codecs. */
    const sdfio_writer_codec *codecs[SDFIO_CODEC_MAX];
    /** The codec of the file. */
    const sdfio_writer_codec *codec;
    /** Where the writer is in the life of a file. */
    sdfio_writer_phase state;
    /** Whether the header went to the sink. */
    bool header_written;
    /** Whether the codec opened the trailer record. */
    bool trailer_open;
    /** The index of the next row to write. */
    size_t next_row;
    /** The allocator of the configuration. */
    sdfio_allocator allocator;
    /** The time zone of the configuration. */
    sdfio_time_zone zone;
    /** What a file adds to the writer. */
    sdfio_owner owner;
    /** The number of values converted at a time. */
    size_t chunk_values;
    /** The room for a chunk of values in the data type of the file, made when
     * the file is created. */
    unsigned char *coded;
    /** The room that the codec asks for to write a chunk of values. */
    void *scratch;
};

/**
 * Ends the writing after an error.
 *
 * @param writer The writer.
 * @param status The status of the error.
 * @return `status`.
 */
sdfio_status sdfio_writer_fail(sdfio_writer *writer, sdfio_status status);

/**
 * Ends the writing because the stream failed.
 *
 * @param writer The writer.
 * @return SDFIO_ERR_IO.
 */
sdfio_status sdfio_writer_sink_failed(sdfio_writer *writer);

/**
 * Checks that the writer can be written to.
 *
 * @param writer The writer. May be null.
 * @retval SDFIO_ERR_ARGUMENT The writer is null.
 * @retval SDFIO_ERR_STATE No file is created, the file is ended, or an error
 * ended the writing.
 */
sdfio_status sdfio_writer_require_writable(sdfio_writer *writer);

/**
 * Writes the header, if that is not done, when the z-scale is known.
 *
 * @param writer The writer.
 */
sdfio_status sdfio_writer_start_data(sdfio_writer *writer);

/**
 * Writes values of the data area, as the file stores them.
 *
 * @param writer The writer.
 * @param values The values in the data type of the file.
 * @param count The number of values.
 */
sdfio_status sdfio_writer_put_values(sdfio_writer *writer, const void *values,
                                     size_t count);

#endif /* SDFIO_SRC_WRITER_STATE_H */
