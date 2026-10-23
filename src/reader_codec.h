/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file reader_codec.h
 * @brief Interface of the codecs that read a representation.
 */

#ifndef SDFIO_SRC_READER_CODEC_H
#define SDFIO_SRC_READER_CODEC_H

#include "dynstring.h"
#include "message.h"
#include "revision.h"
#include "source.h"

/**
 * A reader codec: the functions that read a representation of the files.
 *
 * The reader knows nothing of the representations. It calls the codecs that
 * the configuration lists, so that the code of a codec that no program names
 * is not linked.
 */
struct sdfio_reader_codec
{
    /** The size of what the codec keeps for a file between its calls, which
     * the reader allocates and passes as `state`. Zero if it keeps nothing. */
    size_t state_size;
    /** The first character of the version numbers, which identifies the
     * representation. */
    char prefix;
    /** The version numbers that the codec reads, one for each revision in the
     * order of the enumeration. The codec determines the revision of a file by
     * comparing the version number of the file with these. */
    const char *version_numbers[SDFIO_REVISION_COUNT];

    /**
     * Reads the header at the start of the source and leaves the source at the
     * data area.
     *
     * @param source The source, at the start of the file.
     * @param[out] header Receives the header.
     * @param[out] state The room of `state_size` bytes, zeroed. The codec fills
     * it.
     * @param zone The time zone of the caller. May be null.
     * @param[out] message Receives the reason of a failure.
     * @retval SDFIO_ERR_FORMAT The header is damaged.
     * @retval SDFIO_ERR_IO The stream failed.
     */
    sdfio_status (*read_header)(sdfio_source *source, sdfio_header *header,
                                void *state, const sdfio_time_zone *zone,
                                sdfio_message *message);

    /**
     * Reads values of the data area.
     *
     * @param source The source.
     * @param header The header of the file.
     * @param state What the codec keeps for the file, `state_size` bytes.
     * @param[out] dst Receives the values in the data type of the header.
     * @param count The number of values.
     * @param[out] message Receives the reason of a failure.
     * @retval SDFIO_ERR_FORMAT The data area is damaged or ends early.
     * @retval SDFIO_ERR_IO The stream failed.
     */
    sdfio_status (*read_values)(sdfio_source *source,
                                const sdfio_header *header, const void *state,
                                void *dst, size_t count,
                                sdfio_message *message);

    /**
     * Ends the data area: passes the values that were not read, and what closes
     * the area.
     *
     * @param source The source.
     * @param header The header of the file.
     * @param complete Whether every row was read.
     * @param values_left The number of values that were not read.
     * @param scratch Room for the codec to read into.
     * @param scratch_size The size of `scratch` in bytes.
     * @param[out] message Receives the reason of a failure.
     * @retval SDFIO_ERR_FORMAT The data area is damaged or ends early.
     * @retval SDFIO_ERR_IO The stream failed.
     */
    sdfio_status (*end_data)(sdfio_source *source, const sdfio_header *header,
                             bool complete, size_t values_left, void *scratch,
                             size_t scratch_size, sdfio_message *message);

    /**
     * Reads the trailer.
     *
     * @param source The source, after the data area.
     * @param[in,out] out Receives the trailer.
     * @param[out] present False if the file ends with the data.
     * @param[out] message Receives the reason of a failure.
     * @retval SDFIO_ERR_FORMAT The trailer is damaged.
     * @retval SDFIO_ERR_MEMORY There is no memory.
     * @retval SDFIO_ERR_IO The stream failed.
     */
    sdfio_status (*read_trailer)(sdfio_source *source, sdfio_string *out,
                                 bool *present, sdfio_message *message);
};

#endif /* SDFIO_SRC_READER_CODEC_H */
