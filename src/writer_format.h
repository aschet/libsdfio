/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief Interface of the formats that a writer writes.
 */

#ifndef SDFIO_SRC_WRITER_FORMAT_H
#define SDFIO_SRC_WRITER_FORMAT_H

#include "sink.h"

typedef struct sdfio_writer_format sdfio_writer_format;

/**
 * A writer format: the functions that write one format of the files.
 * The writer knows the structure of a file, a header, the data area and the
 * trailer, and leaves the details of a format to these functions. The source
 * file of a format names its functions after the members of this structure,
 * without a prefix.
 */
struct sdfio_writer_format
{
    /** The size of what the format keeps for a file between its calls, which
     * the writer allocates and passes as `state`. Zero if it keeps nothing. */
    size_t state_size;
    /** The file format that the writer format writes. */
    sdfio_format writes;
    /** Whether the end of the trailer is a line of its own, so that the line
     * that the text leaves open is terminated before end_trailer(). */
    bool trailer_ends_line;

    /**
     * Returns the size in bytes of the room for a chunk of values as the file
     * stores them, which the writer provides to write_values().
     *
     * @param chunk_values The number of values that a call gets at most.
     */
    size_t (*file_chunk_size)(size_t chunk_values);

    /**
     * Writes the header.
     *
     * @param sink The sink.
     * @param header The header.
     * @param[out] state The room of `state_size` bytes, zeroed. The format
     * fills it.
     * @param zone The time zone of the caller. May be null.
     * @param[out] message Receives the reason of a failure.
     * @retval SDFIO_ERR_RANGE A date cannot be written.
     * @retval SDFIO_ERR_IO The stream failed.
     */
    sdfio_status (*write_header)(sdfio_sink *sink, const sdfio_header *header,
                                 void *state, const sdfio_time_zone *zone,
                                 const char **message);

    /**
     * Writes values of the data area.
     *
     * @param sink The sink.
     * @param header The header of the file.
     * @param[in,out] state The state of the format for the file, `state_size`
     * bytes.
     * @param values The values in the data type of the header.
     * @param count The number of values.
     * @param file_chunk Room of file_chunk_size() bytes, for the chunk as the
     * file stores it.
     * @param chunk_values The number of values that `file_chunk` serves.
     * @param[out] message Receives the reason of a failure.
     * @retval SDFIO_ERR_IO The stream failed.
     */
    sdfio_status (*write_values)(sdfio_sink *sink, const sdfio_header *header,
                                 void *state, const void *values, size_t count,
                                 void *file_chunk, size_t chunk_values,
                                 const char **message);

    /**
     * Closes the data area and opens the trailer record. Called before the
     * first line of the trailer, or alone if the file has none.
     *
     * @param sink The sink.
     * @param[out] message Receives the reason of a failure.
     * @retval SDFIO_ERR_IO The stream failed.
     */
    sdfio_status (*begin_trailer)(sdfio_sink *sink, const char **message);

    /**
     * Closes the trailer record and so the file. The lines of the trailer
     * were written between begin_trailer() and this call.
     *
     * @param sink The sink.
     * @param[out] message Receives the reason of a failure.
     * @retval SDFIO_ERR_IO The stream failed.
     */
    sdfio_status (*end_trailer)(sdfio_sink *sink, const char **message);
};

/**
 * Adds a format to the writer. The public functions that name a format call
 * it.
 *
 * @param writer The writer.
 * @param format The format.
 * @retval SDFIO_ERR_ARGUMENT The writer is null, or has added the format
 * already.
 * @retval SDFIO_ERR_STATE An attempt to create a file was made earlier.
 */
sdfio_status sdfio_writer_install_format(sdfio_writer *writer,
                                         const sdfio_writer_format *format);

#endif /* SDFIO_SRC_WRITER_FORMAT_H */
