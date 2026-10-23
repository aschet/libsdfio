/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file codec.h
 * @brief Interfaces of the representation codecs.
 */

#ifndef SDFIO_SRC_CODEC_H
#define SDFIO_SRC_CODEC_H

#include "decimal.h"
#include "source.h"
#include "sink.h"

/** The room for the text of one value in an ASCII data area, with its
 * separator, which is the most that a codec asks for. */
enum
{
    SDFIO_TEXT_PER_VALUE = 32
};

/**
 * What the codecs keep for one file between their calls. The binary codec
 * keeps nothing.
 */
typedef struct sdfio_codec_state
{
    /** The decimal point of the locale, measured when the file was opened or
     * created. */
    sdfio_point point;
    /** The column of the next value that an ASCII writer writes. */
    size_t column;
} sdfio_codec_state;

/**
 * A reader codec: the functions that read a representation of the files.
 *
 * The reader knows nothing of the representations. It calls the codecs that
 * the configuration lists, so that the code of a codec that no program names
 * is not linked.
 */
struct sdfio_reader_codec
{
    /** The first byte of the magics. */
    char prefix;
    /** The magics, one for each revision in the order of the enumeration. */
    const char *magics[2];

    /**
     * Reads the header at the start of the source and leaves the source at the
     * data area.
     *
     * @param source The source, at the start of the file.
     * @param[out] header Receives the header.
     * @param[out] state Receives what the codec keeps for the file.
     * @param zone The time zone of the caller. May be null.
     * @param[out] message Receives the reason of a failure.
     * @retval SDFIO_ERR_FORMAT The header is damaged.
     * @retval SDFIO_ERR_IO The stream failed.
     */
    sdfio_status (*read_header)(sdfio_source *source, sdfio_header *header,
                                sdfio_codec_state *state,
                                const sdfio_time_zone *zone,
                                sdfio_message *message);

    /**
     * Reads values of the data area.
     *
     * @param source The source.
     * @param header The header of the file.
     * @param state What the codec keeps for the file.
     * @param[out] dst Receives the values in the data type of the header.
     * @param count The number of values.
     * @param[out] message Receives the reason of a failure.
     * @retval SDFIO_ERR_FORMAT The data area is damaged or ends early.
     * @retval SDFIO_ERR_IO The stream failed.
     */
    sdfio_status (*read_values)(sdfio_source *source,
                                const sdfio_header *header,
                                const sdfio_codec_state *state, void *dst,
                                size_t count, sdfio_message *message);

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
     * @retval SDFIO_ERR_FORMAT The trailer is damaged or too long.
     * @retval SDFIO_ERR_MEMORY There is no memory.
     * @retval SDFIO_ERR_IO The stream failed.
     */
    sdfio_status (*read_trailer)(sdfio_source *source, sdfio_string *out,
                                 bool *present, sdfio_message *message);
};

/**
 * A writer codec: the functions that write a representation of the files.
 * The writer knows nothing of the representations, as the reader does not.
 */
struct sdfio_writer_codec
{
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
     * @param[out] state Receives what the codec keeps for the file.
     * @param zone The time zone of the caller. May be null.
     * @param allocator The allocator of the handle.
     * @param[out] message Receives the reason of a failure.
     * @retval SDFIO_ERR_RANGE A date cannot be written.
     * @retval SDFIO_ERR_MEMORY There is no memory.
     * @retval SDFIO_ERR_IO The stream failed.
     */
    sdfio_status (*write_header)(sdfio_sink *sink, const sdfio_header *header,
                                 sdfio_codec_state *state,
                                 const sdfio_time_zone *zone,
                                 const sdfio_allocator *allocator,
                                 sdfio_message *message);

    /**
     * Writes values of the data area.
     *
     * @param sink The sink.
     * @param header The header of the file.
     * @param[in,out] state What the codec keeps for the file.
     * @param values The values in the data type of the header.
     * @param count The number of values.
     * @param scratch Room of scratch_size() bytes.
     * @param chunk_values The number of values that `scratch` serves.
     * @param[out] message Receives the reason of a failure.
     * @retval SDFIO_ERR_IO The stream failed.
     */
    sdfio_status (*write_values)(sdfio_sink *sink, const sdfio_header *header,
                                 sdfio_codec_state *state, const void *values,
                                 size_t count, void *scratch,
                                 size_t chunk_values, sdfio_message *message);

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

#endif /* SDFIO_SRC_CODEC_H */
