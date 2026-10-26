/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief Interface of the formats that a reader reads.
 */

#ifndef SDFIO_SRC_READER_FORMAT_H
#define SDFIO_SRC_READER_FORMAT_H

#include "message.h"
#include "source.h"

typedef struct sdfio_reader_format sdfio_reader_format;

/**
 * A reader format: the functions that read one format of the files.
 *
 * The reader knows the structure of a file, a header, the data area and the
 * trailer, and leaves the details of a format to these functions. The source
 * file of a format names its functions after the members of this structure,
 * without a prefix.
 */
struct sdfio_reader_format
{
    /** The size of what the format keeps for a file between its calls, which
     * the reader allocates and passes as `state`. Zero if it keeps nothing. */
    size_t state_size;
    /** The first character of the magics, which identifies the
     * format. */
    char prefix;
    /** The magics that the format reads, indexed by revision. The
     * format determines the revision of a file by comparing the magic
     * of the file with these. */
    const char *const *magics;
    /** The number of magics in the list. */
    size_t magic_count;

    /**
     * Reads the header at the start of the source and leaves the source at the
     * data area.
     *
     * @param source The source, at the start of the file.
     * @param[out] header Receives the header.
     * @param[out] state The room of `state_size` bytes, zeroed. The format
     * fills it.
     * @param zone The time zone of the caller. May be null.
     * @param[out] message Receives the reason of a failure.
     * @retval SDFIO_ERR_FORMAT The header is damaged.
     * @retval SDFIO_ERR_IO The stream failed.
     */
    sdfio_status (*read_header)(sdfio_source *source, sdfio_header *header,
                                void *state, const sdfio_time_zone *zone,
                                const char **message);

    /**
     * Reads values of the data area.
     *
     * @param source The source.
     * @param header The header of the file.
     * @param state The state of the format for the file, `state_size` bytes.
     * @param[out] dst Receives the values in the data type of the header.
     * @param count The number of values.
     * @param[out] message Receives the reason of a failure.
     * @retval SDFIO_ERR_FORMAT The data area is damaged or ends early.
     * @retval SDFIO_ERR_IO The stream failed.
     */
    sdfio_status (*read_values)(sdfio_source *source,
                                const sdfio_header *header, const void *state,
                                void *dst, size_t count, const char **message);

    /**
     * Ends the data area: passes the values that were not read, and what closes
     * the area.
     *
     * @param source The source.
     * @param header The header of the file.
     * @param complete Whether every row was read.
     * @param values_left The number of values that were not read.
     * @param chunk Room for the format to read the values that it skips into.
     * @param chunk_size The size of `chunk` in bytes.
     * @param[out] message Receives the reason of a failure.
     * @retval SDFIO_ERR_FORMAT The data area is damaged or ends early.
     * @retval SDFIO_ERR_IO The stream failed.
     */
    sdfio_status (*end_data)(sdfio_source *source, const sdfio_header *header,
                             bool complete, size_t values_left, void *chunk,
                             size_t chunk_size, const char **message);

    /**
     * Reads the next part of the trailer into a buffer, with LF as line ends.
     * The first call comes after the data area.
     *
     * @param source The source.
     * @param[in,out] state The state of the format for the file, `state_size`
     * bytes.
     * @param[out] buffer Receives the text.
     * @param capacity The size of `buffer`.
     * @param[out] size Receives the number of bytes written, 0 at the end of
     * the trailer.
     * @param[out] message Receives the reason of a failure.
     * @retval SDFIO_ERR_IO The stream failed.
     */
    sdfio_status (*read_trailer)(sdfio_source *source, void *state,
                                 char *buffer, size_t capacity, size_t *size,
                                 const char **message);
};

/**
 * Adds a format to the reader. The public functions that name a format call
 * it.
 *
 * @param reader The reader.
 * @param format The format.
 * @retval SDFIO_ERR_ARGUMENT The reader is null, or has added the format
 * already.
 * @retval SDFIO_ERR_STATE An attempt to open a file was made earlier.
 */
sdfio_status sdfio_reader_install_format(sdfio_reader *reader,
                                         const sdfio_reader_format *format);

#endif /* SDFIO_SRC_READER_FORMAT_H */
