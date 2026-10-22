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

#include "internal.h"

sdfio_status sdfio_read_header(sdfio_source *source, sdfio_header *header,
                               const sdfio_point *point,
                               const sdfio_time_zone *zone,
                               sdfio_message *message)
{
    const size_t available = sdfio_source_fill(source, SDFIO_MAGIC_SIZE);
    if (available < SDFIO_MAGIC_SIZE)
    {
        return source->failed ? sdfio_fail(message, SDFIO_ERR_IO,
                                           "The stream could not be read")
                              : sdfio_fail(message, SDFIO_ERR_FORMAT,
                                           "Not an SDF file, unexpected magic");
    }
    const unsigned char *magic = source->buffer + source->position;
    const char prefix = (char)magic[0];
    if (prefix == 'a')
    {
        return sdfio_ascii_parse_header(source, header, point, zone, message);
    }
    if (prefix != 'b')
    {
        return sdfio_fail(message, SDFIO_ERR_FORMAT,
                          "Not an SDF file, unexpected magic");
    }
    sdfio_revision revision = SDFIO_ISO_2_0;
    const sdfio_status status =
        sdfio_binary_revision(magic, &revision, message);
    if (status != SDFIO_OK)
    {
        return status;
    }
    unsigned char bytes[SDFIO_BINARY_HEADER_MAX];
    const size_t size = sdfio_binary_header_size(revision);
    if (sdfio_source_read(source, bytes, size) != size)
    {
        return source->failed ? sdfio_fail(message, SDFIO_ERR_IO,
                                           "The stream could not be read")
                              : sdfio_fail(message, SDFIO_ERR_FORMAT,
                                           "File is too short to contain an "
                                           "SDF header");
    }
    return sdfio_binary_parse(bytes, header, zone, message);
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
    if (header->representation == SDFIO_ASCII && !sdfio_has_text())
    {
        return sdfio_fail(message, SDFIO_ERR_UNSUPPORTED,
                          "Text SDF files are not supported by this build");
    }
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
    if (!positive(header->x_scale) || !positive(header->y_scale))
    {
        return sdfio_fail(message, SDFIO_ERR_ARGUMENT,
                          "x_scale and y_scale must be positive numbers");
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
    if (header->representation == SDFIO_BINARY
        && (header->columns > sdfio_max_binary_count(header->revision)
            || header->rows > sdfio_max_binary_count(header->revision)))
    {
        return sdfio_fail(message, SDFIO_ERR_ARGUMENT,
                          "The columns or the rows do not fit the binary "
                          "header of the SDF revision");
    }
    return SDFIO_OK;
}
