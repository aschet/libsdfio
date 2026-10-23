/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file header.c
 * @brief The header of a file: finding the representation, and the rules a
 * header has to follow to be written.
 */

#include "header.h"
#include "codec.h"
#include "common.h"
#include "dates.h"
#include "stream.h"
#include "text.h"
#include "types.h"

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

sdfio_status sdfio_read_header(sdfio_source *source,
                               const sdfio_reader_codec *const *codecs,
                               const sdfio_reader_codec **codec,
                               sdfio_header *header, sdfio_codec_state *state,
                               const sdfio_time_zone *zone,
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
    const sdfio_status status =
        (*codec)->read_header(source, header, state, zone, message);
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

static bool is_ascii_text(const char *text, size_t size)
{
    for (size_t i = 0; i < size; ++i)
    {
        if ((unsigned char)text[i] >= SDFIO_FIRST_NON_ASCII)
        {
            return false;
        }
    }
    return true;
}

static bool positive(double value)
{
    return value > 0.0 && isfinite(value);
}

sdfio_status sdfio_validate(const sdfio_header *header, sdfio_message *message)
{
    if ((unsigned)header->revision > (unsigned)SDFIO_ISO_2_0
        || (header->representation != SDFIO_BINARY
            && header->representation != SDFIO_ASCII)
        || !sdfio_valid_data_type(header->data_type))
    {
        return sdfio_fail(
            message, SDFIO_ERR_ARGUMENT,
            "The revision, the representation or the data type is not "
            "one of the enumeration");
    }
    if (!sdfio_revision_allows(header->revision, header->data_type))
    {
        return sdfio_fail(message, SDFIO_ERR_ARGUMENT,
                          "The data type is not valid for the SDF revision");
    }
    if (header->z_scale != SDFIO_AUTO_Z_SCALE && !positive(header->z_scale))
    {
        return sdfio_fail(message, SDFIO_ERR_ARGUMENT,
                          "z_scale must be a positive number");
    }
    /* A resolution that is not positive is not known. */
    if (!isfinite(header->z_resolution))
    {
        return sdfio_fail(message, SDFIO_ERR_ARGUMENT,
                          "z_resolution must be a finite number");
    }
    /* The standard puts no limit on the scales: the y_scale of a profile is
     * meaningless. */
    if (!isfinite(header->x_scale) || !isfinite(header->y_scale))
    {
        return sdfio_fail(message, SDFIO_ERR_ARGUMENT,
                          "x_scale and y_scale must be finite numbers");
    }
    const char *terminator =
        memchr(header->manufacturer_id, '\0', sizeof header->manufacturer_id);
    if (terminator == NULL)
    {
        return sdfio_fail(message, SDFIO_ERR_ARGUMENT,
                          "manufacturer_id has to end with a 0");
    }
    if (!is_ascii_text(header->manufacturer_id,
                       (size_t)(terminator - header->manufacturer_id)))
    {
        return sdfio_fail(message, SDFIO_ERR_ARGUMENT,
                          "manufacturer_id must be 7-bit ASCII");
    }
    if (header->creation_date == SDFIO_NO_TIME
        || header->modification_date == SDFIO_NO_TIME)
    {
        return sdfio_fail(message, SDFIO_ERR_ARGUMENT,
                          "A file needs a creation date and a modification "
                          "date: SDFIO_NO_TIME cannot be written");
    }
    if (!sdfio_date_representable(header->creation_date)
        || !sdfio_date_representable(header->modification_date))
    {
        return sdfio_fail(message, SDFIO_ERR_ARGUMENT,
                          "The year of a date is out of the range of the "
                          "SDF files");
    }
    size_t count = 0;
    if (!sdfio_count(header, &count))
    {
        return sdfio_fail(message, SDFIO_ERR_ARGUMENT,
                          "columns times rows does not fit in size_t");
    }
    if (header->columns > sdfio_max_count(header->revision)
        || header->rows > sdfio_max_count(header->revision))
    {
        return sdfio_fail(message, SDFIO_ERR_ARGUMENT,
                          "The columns or the rows do not fit the header of "
                          "the SDF revision");
    }
    return SDFIO_OK;
}
