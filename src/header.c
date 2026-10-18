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

sdf_status sdfi_read_header(sdfi_source *source, sdf_info *info,
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
        return source->failed ? sdfi_fail(message, SDF_ERR_IO,
                                          "The stream could not be read")
                              : sdfi_fail(message, SDF_ERR_FORMAT,
                                          "Not an SDF file, unexpected magic");
    }
    const unsigned char *magic = source->buffer + source->position;
    const char prefix = sdfi_lower((char)magic[0]);
    if (prefix == 'a')
    {
        return sdfi_ascii_parse_header(source, info, point, message);
    }
    if (prefix != 'b')
    {
        return sdfi_fail(message, SDF_ERR_FORMAT,
                         "Not an SDF file, unexpected magic");
    }
    sdf_revision revision = SDF_ISO_2_0;
    const sdf_status status = sdfi_binary_revision(magic, &revision, message);
    if (status != SDF_OK)
    {
        return status;
    }
    unsigned char bytes[SDFI_BINARY_HEADER_MAX];
    const size_t size = sdfi_binary_header_size(revision);
    if (sdfi_source_read(source, bytes, size) != size)
    {
        return source->failed ? sdfi_fail(message, SDF_ERR_IO,
                                          "The stream could not be read")
                              : sdfi_fail(message, SDF_ERR_FORMAT,
                                          "File is too short to contain an "
                                          "SDF header");
    }
    return sdfi_binary_parse(bytes, info, message);
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

sdf_status sdfi_validate(const sdf_info *info, sdfi_message *message)
{
    if (info->revision < SDF_ISO_1_0 || info->revision > SDF_BCR_1_0
        || (info->storage != SDF_BINARY && info->storage != SDF_ASCII)
        || !sdfi_valid_data_type(info->data_type))
    {
        return sdfi_fail(message, SDF_ERR_ARGUMENT,
                         "The revision, the storage or the data type is not "
                         "one of the enumeration");
    }
    if (!sdf_revision_allows(info->revision, info->data_type))
    {
        return sdfi_fail(message, SDF_ERR_ARGUMENT,
                         "Data type %s is not valid for SDF revision %s",
                         sdf_data_type_name(info->data_type),
                         sdf_revision_name(info->revision));
    }
    if (info->z_scale != SDF_AUTO_Z_SCALE && !positive(info->z_scale))
    {
        return sdfi_fail(message, SDF_ERR_ARGUMENT,
                         "z_scale must be a positive number, got %g",
                         info->z_scale);
    }
    if (info->z_resolution != SDF_NO_RESOLUTION
        && !positive(info->z_resolution))
    {
        return sdfi_fail(message, SDF_ERR_ARGUMENT,
                         "z_resolution must be a positive number, got %g",
                         info->z_resolution);
    }
    if (!positive(info->x_scale) || !positive(info->y_scale))
    {
        return sdfi_fail(message, SDF_ERR_ARGUMENT,
                         "x_scale and y_scale must be positive numbers");
    }
    const char *terminator =
        memchr(info->manufacturer_id, '\0', sizeof info->manufacturer_id);
    if (terminator == NULL)
    {
        return sdfi_fail(message, SDF_ERR_ARGUMENT,
                         "manufacturer_id has to end with a 0");
    }
    if (!is_ascii_text(info->manufacturer_id,
                       (size_t)(terminator - info->manufacturer_id)))
    {
        return sdfi_fail(message, SDF_ERR_ARGUMENT,
                         "manufacturer_id must be 7-bit ASCII");
    }
    if ((info->create_date != SDF_NO_TIME
         && !sdfi_date_representable(info->create_date))
        || (info->mod_date != SDF_NO_TIME
            && !sdfi_date_representable(info->mod_date)))
    {
        return sdfi_fail(message, SDF_ERR_ARGUMENT,
                         "The year of a date must be one of %d to %d",
                         SDFI_FIRST_YEAR, SDFI_LAST_YEAR);
    }
    size_t count = 0;
    if (!sdfi_count(info, &count))
    {
        return sdfi_fail(message, SDF_ERR_ARGUMENT,
                         "columns times rows does not fit in size_t");
    }
    if (info->storage == SDF_BINARY
        && (info->columns > sdfi_max_binary_count(info->revision)
            || info->rows > sdfi_max_binary_count(info->revision)))
    {
        return sdfi_fail(message, SDF_ERR_ARGUMENT,
                         "The columns and the rows must be between 0 and %zu "
                         "for SDF revision %s (binary format)",
                         sdfi_max_binary_count(info->revision),
                         sdf_revision_name(info->revision));
    }
    return SDF_OK;
}
