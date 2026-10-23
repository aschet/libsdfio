/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file reader_trailer.c
 * @brief Reading of the trailer.
 */

#include "reader_state.h"

sdfio_status sdfio_read_trailer(sdfio_reader *reader, const char **trailer,
                                size_t *size)
{
    if (reader == NULL || trailer == NULL || size == NULL)
    {
        return SDFIO_ERR_ARGUMENT;
    }
    if (reader->trailer_read && !reader->failed)
    {
        *trailer = reader->trailer_present ? sdfio_string_text(&reader->trailer)
                                           : NULL;
        *size = reader->trailer.size;
        return SDFIO_OK;
    }
    sdfio_status status = sdfio_reader_require_readable(reader);
    if (status != SDFIO_OK)
    {
        return status;
    }
    status = sdfio_reader_finish_data(reader);
    if (status == SDFIO_OK)
    {
        status = reader->format->read_trailer(&reader->source, &reader->trailer,
                                              &reader->trailer_present,
                                              &reader->message);
    }
    if (status != SDFIO_OK)
    {
        return sdfio_reader_fail(reader, status);
    }
    reader->trailer_read = true;
    *trailer =
        reader->trailer_present ? sdfio_string_text(&reader->trailer) : NULL;
    *size = reader->trailer.size;
    return SDFIO_OK;
}
