/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief Reading of the trailer in parts.
 */

#include "reader_state.h"

sdfio_status sdfio_read_trailer(sdfio_reader *reader, char *buffer,
                                size_t capacity, size_t *size)
{
    if (reader == NULL || buffer == NULL || capacity == 0 || size == NULL)
    {
        return SDFIO_ERR_ARGUMENT;
    }
    sdfio_status status = sdfio_reader_require_readable(reader);
    if (status != SDFIO_OK)
    {
        return status;
    }
    if (!reader->trailer_begun)
    {
        status = sdfio_reader_finish_data(reader);
        if (status != SDFIO_OK)
        {
            return sdfio_reader_fail(reader, status);
        }
        reader->trailer_begun = true;
    }
    /* Tolerated deviation: the text of a trailer is not checked for the tagged
     * format that ISO-2.0 asks for, nor for the 7-bit ASCII encoding. */
    status =
        reader->format->read_trailer(&reader->source, reader->format_state,
                                     buffer, capacity, size, &reader->message);
    return status == SDFIO_OK ? status : sdfio_reader_fail(reader, status);
}
