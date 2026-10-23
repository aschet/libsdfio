/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file read_header.c
 * @brief Selection of the codec and reading of the header.
 */

#include "read_header.h"
#include "fields.h"
#include "message.h"
#include "source.h"
#include <sdfio/sdfio.h>

/* The reader codec whose magics start with a byte. */
static const sdfio_reader_codec *
find_codec(const sdfio_reader_codec *const *codecs, char prefix)
{
    for (size_t i = 0; i < SDFIO_CODEC_MAX && codecs[i] != NULL; ++i)
    {
        if (codecs[i]->prefix == prefix)
        {
            return codecs[i];
        }
    }
    return NULL;
}

sdfio_status sdfio_select_reader_codec(sdfio_source *source,
                                       const sdfio_reader_codec *const *codecs,
                                       const sdfio_reader_codec **codec,
                                       sdfio_message *message)
{
    const size_t available = sdfio_source_fill(source, SDFIO_MAGIC_SIZE);
    if (available < SDFIO_MAGIC_SIZE)
    {
        return sdfio_source_failure(source, message,
                                    "Not an SDF file, unexpected magic");
    }
    const char prefix = (char)source->buffer[source->position];
    if (prefix != 'a' && prefix != 'b')
    {
        return sdfio_fail(message, SDFIO_ERR_FORMAT,
                          "Not an SDF file, unexpected magic");
    }
    *codec = find_codec(codecs, prefix);
    if (*codec == NULL)
    {
        return sdfio_fail(message, SDFIO_ERR_UNSUPPORTED,
                          "The codec of this SDF file is not listed in the "
                          "configuration");
    }
    return SDFIO_OK;
}

sdfio_status sdfio_read_header(sdfio_source *source,
                               const sdfio_reader_codec *codec,
                               sdfio_header *header, void *state,
                               const sdfio_time_zone *zone,
                               sdfio_message *message)
{
    const sdfio_status status =
        codec->read_header(source, header, state, zone, message);
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
