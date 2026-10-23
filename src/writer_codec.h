/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file writer_codec.h
 * @brief Interface of the codecs that write a representation.
 */

#ifndef SDFIO_SRC_WRITER_CODEC_H
#define SDFIO_SRC_WRITER_CODEC_H

#include "message.h"
#include "revision.h"
#include "sink.h"

/** The room for the text of one value in an ASCII data area, with its
 * separator, which is the most that a codec asks for. */
enum
{
    SDFIO_TEXT_PER_VALUE = 32
};

/**
 * A writer codec: the functions that write a representation of the files.
 * The writer knows nothing of the representations, as the reader does not.
 */
struct sdfio_writer_codec
{
    /** The size of what the codec keeps for a file between its calls, which
     * the writer allocates and passes as `state`. Zero if it keeps nothing. */
    size_t state_size;
    /** The version numbers that the codec writes, one for each revision in the
     * order of the enumeration. */
    const char *version_numbers[SDFIO_REVISION_COUNT];
    /** The representation that the codec writes. */
    sdfio_representation representation;

    /**
     * Returns the bytes that the writer has to provide for write_values().
     *
     * @param chunk_values The number of values that a call gets at most.
     */
    size_t (*scratch_size)(size_t chunk_values);

    /**
     * Writes the header.
     *
     * @param sink The sink.
     * @param header The header.
     * @param[out] state The room of `state_size` bytes, zeroed. The codec fills
     * it.
     * @param zone The time zone of the caller. May be null.
     * @param allocator The allocator of the handle.
     * @param[out] message Receives the reason of a failure.
     * @retval SDFIO_ERR_RANGE A date cannot be written.
     * @retval SDFIO_ERR_MEMORY There is no memory.
     * @retval SDFIO_ERR_IO The stream failed.
     */
    sdfio_status (*write_header)(sdfio_sink *sink, const sdfio_header *header,
                                 void *state, const sdfio_time_zone *zone,
                                 const sdfio_allocator *allocator,
                                 sdfio_message *message);

    /**
     * Writes values of the data area.
     *
     * @param sink The sink.
     * @param header The header of the file.
     * @param[in,out] state What the codec keeps for the file, `state_size`
     * bytes.
     * @param values The values in the data type of the header.
     * @param count The number of values.
     * @param scratch Room of scratch_size() bytes.
     * @param chunk_values The number of values that `scratch` serves.
     * @param[out] message Receives the reason of a failure.
     * @retval SDFIO_ERR_IO The stream failed.
     */
    sdfio_status (*write_values)(sdfio_sink *sink, const sdfio_header *header,
                                 void *state, const void *values, size_t count,
                                 void *scratch, size_t chunk_values,
                                 sdfio_message *message);

    /**
     * Closes the data area and opens the trailer record. Called before the
     * first line of the trailer, or alone if the file has none.
     *
     * @param sink The sink.
     * @param[out] message Receives the reason of a failure.
     * @retval SDFIO_ERR_IO The stream failed.
     */
    sdfio_status (*begin_trailer)(sdfio_sink *sink, sdfio_message *message);

    /**
     * Closes the trailer record and so the file. The lines of the trailer
     * were written between begin_trailer() and this call.
     *
     * @param sink The sink.
     * @param[out] message Receives the reason of a failure.
     * @retval SDFIO_ERR_IO The stream failed.
     */
    sdfio_status (*end_trailer)(sdfio_sink *sink, sdfio_message *message);
};

#endif /* SDFIO_SRC_WRITER_CODEC_H */
