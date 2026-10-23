/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file read_header.c
 * @brief Selection of the format and reading of the header.
 */

#include "read_header.h"
#include "message.h"
#include "source.h"
#include <sdfio/sdfio.h>

/* The reader format whose version numbers start with a byte. */
static const sdfio_reader_format *
find_format(const sdfio_reader_format *const *formats, char prefix)
{
    for (size_t i = 0; i < SDFIO_FORMAT_MAX && formats[i] != NULL; ++i)
    {
        if (formats[i]->prefix == prefix)
        {
            return formats[i];
        }
    }
    return NULL;
}

sdfio_status sdfio_select_reader_format(
    sdfio_source *source, const sdfio_reader_format *const *formats,
    const sdfio_reader_format **format, sdfio_message *message)
{
    const size_t available =
        sdfio_source_fill(source, SDFIO_VERSION_NUMBER_SIZE);
    if (available < SDFIO_VERSION_NUMBER_SIZE)
    {
        return sdfio_source_failure(
            source, message, "Not an SDF file, unexpected version number");
    }
    const char prefix = (char)source->buffer[source->position];
    if (prefix != 'a' && prefix != 'b')
    {
        return sdfio_fail(message, SDFIO_ERR_FORMAT,
                          "Not an SDF file, unexpected version number");
    }
    *format = find_format(formats, prefix);
    if (*format == NULL)
    {
        return sdfio_fail(message, SDFIO_ERR_UNSUPPORTED,
                          "The format of this SDF file is not listed in the "
                          "configuration");
    }
    return SDFIO_OK;
}

sdfio_status sdfio_read_header(sdfio_source *source,
                               const sdfio_reader_format *format,
                               sdfio_header *header, void *state,
                               const sdfio_time_zone *zone,
                               sdfio_message *message)
{
    const sdfio_status status =
        format->read_header(source, header, state, zone, message);
    /* The z-scale is a non-zero positive number. */
    if (status == SDFIO_OK
        && !(header->z_scale > 0.0 && isfinite(header->z_scale)))
    {
        return sdfio_fail(message, SDFIO_ERR_FORMAT,
                          "The z-scale of an SDF file has to be a positive "
                          "number");
    }
    return status;
}
