/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* The header of a file: finding the representation, and the rules a header has
 * to follow to be written. */

#include "internal.h"

/* The mark that some editors put in front of a text. */
static const unsigned char byte_order_mark[] = {0xEF, 0xBB, 0xBF};

sdfio_status sdfi_read_header(sdfi_source *source, sdfio_header *header,
                              const sdfi_point *point, sdfi_message *message)
{
    size_t available = sdfi_source_fill(source, sizeof byte_order_mark);
    if (available >= sizeof byte_order_mark
        && memcmp(source->buffer + source->position, byte_order_mark,
                  sizeof byte_order_mark)
               == 0)
    {
        source->position += sizeof byte_order_mark;
    }
    available = sdfi_source_fill(source, SDFI_MAGIC_SIZE);
    if (available < SDFI_MAGIC_SIZE)
    {
        return source->failed ? sdfi_fail(message, SDFIO_ERR_IO,
                                          "The stream could not be read")
                              : sdfi_fail(message, SDFIO_ERR_FORMAT,
                                          "Not an SDF file, unexpected magic");
    }
    const unsigned char *magic = source->buffer + source->position;
    const char prefix = sdfi_lower((char)magic[0]);
    if (prefix == 'a')
    {
        return sdfi_ascii_parse_header(source, header, point, message);
    }
    if (prefix != 'b')
    {
        return sdfi_fail(message, SDFIO_ERR_FORMAT,
                         "Not an SDF file, unexpected magic");
    }
    sdfio_revision revision = SDFIO_ISO_2_0;
    const sdfio_status status = sdfi_binary_revision(magic, &revision, message);
    if (status != SDFIO_OK)
    {
        return status;
    }
    unsigned char bytes[SDFI_BINARY_HEADER_MAX];
    const size_t size = sdfi_binary_header_size(revision);
    if (sdfi_source_read(source, bytes, size) != size)
    {
        return source->failed ? sdfi_fail(message, SDFIO_ERR_IO,
                                          "The stream could not be read")
                              : sdfi_fail(message, SDFIO_ERR_FORMAT,
                                          "File is too short to contain an "
                                          "SDF header");
    }
    return sdfi_binary_parse(bytes, header, message);
}

static bool is_ascii_text(const char *text, size_t size)
{
    for (size_t i = 0; i < size; ++i)
    {
        if ((unsigned char)text[i] >= SDFI_FIRST_NON_ASCII)
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

sdfio_status sdfi_validate(const sdfio_header *header, sdfi_message *message)
{
    if (header->revision < SDFIO_ISO_1_0 || header->revision > SDFIO_BCR_1_0
        || (header->storage != SDFIO_BINARY && header->storage != SDFIO_ASCII)
        || !sdfi_valid_data_type(header->data_type))
    {
        return sdfi_fail(message, SDFIO_ERR_ARGUMENT,
                         "The revision, the storage or the data type is not "
                         "one of the enumeration");
    }
    if (!sdfio_revision_allows(header->revision, header->data_type))
    {
        return sdfi_fail(message, SDFIO_ERR_ARGUMENT,
                         "Data type %s is not valid for SDF revision %s",
                         sdfio_data_type_name(header->data_type),
                         sdfio_revision_name(header->revision));
    }
    if (header->z_scale != SDFIO_AUTO_Z_SCALE && !positive(header->z_scale))
    {
        return sdfi_fail(message, SDFIO_ERR_ARGUMENT,
                         "z_scale must be a positive number, got %g",
                         header->z_scale);
    }
    /* A resolution that is not positive is not known. */
    if (!isfinite(header->z_resolution))
    {
        return sdfi_fail(message, SDFIO_ERR_ARGUMENT,
                         "z_resolution must be a finite number, got %g",
                         header->z_resolution);
    }
    if (!positive(header->x_scale) || !positive(header->y_scale))
    {
        return sdfi_fail(message, SDFIO_ERR_ARGUMENT,
                         "x_scale and y_scale must be positive numbers");
    }
    const char *terminator =
        memchr(header->manufacturer_id, '\0', sizeof header->manufacturer_id);
    if (terminator == NULL)
    {
        return sdfi_fail(message, SDFIO_ERR_ARGUMENT,
                         "manufacturer_id has to end with a 0");
    }
    if (!is_ascii_text(header->manufacturer_id,
                       (size_t)(terminator - header->manufacturer_id)))
    {
        return sdfi_fail(message, SDFIO_ERR_ARGUMENT,
                         "manufacturer_id must be 7-bit ASCII");
    }
    if ((header->creation_date != SDFIO_NO_TIME
         && !sdfi_date_representable(header->creation_date))
        || (header->modification_date != SDFIO_NO_TIME
            && !sdfi_date_representable(header->modification_date)))
    {
        return sdfi_fail(message, SDFIO_ERR_ARGUMENT,
                         "The year of a date must be one of %d to %d",
                         SDFI_FIRST_YEAR, SDFI_LAST_YEAR);
    }
    size_t count = 0;
    if (!sdfi_count(header, &count))
    {
        return sdfi_fail(message, SDFIO_ERR_ARGUMENT,
                         "columns times rows does not fit in size_t");
    }
    if (header->storage == SDFIO_BINARY
        && (header->columns > sdfi_max_binary_count(header->revision)
            || header->rows > sdfi_max_binary_count(header->revision)))
    {
        return sdfi_fail(message, SDFIO_ERR_ARGUMENT,
                         "The columns and the rows must be between 0 and %zu "
                         "for SDF revision %s (binary format)",
                         sdfi_max_binary_count(header->revision),
                         sdfio_revision_name(header->revision));
    }
    return SDFIO_OK;
}
