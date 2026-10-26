/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief Reading of the binary format: header fields, values of the data type,
 * trailer to the end of the file.
 *
 * Files store values in little-endian byte order, regardless of the byte
 * order of the host.
 */

#include "byte_order.h"
#include "data_type.h"
#include "datetime.h"
#include "fields.h"
#include "header.h"
#include "message.h"
#include "reader_format.h"
#include "revision.h"
#include "source.h"
#include "text.h"
#include "trailer_text.h"
#include <limits.h>
#include <math.h>
#include <sdfio/sdfio.h>
#include <string.h>

static const char *const magics[] = {
    [SDFIO_ISO_1_0] = "bISO-1.0", [SDFIO_ISO_2_0] = "bISO-2.0"};

/* Takes the magic at the start of the source, which names a revision according
 * to the magics that the format lists. The magics may differ in length. */
static bool take_revision(sdfio_source *source, sdfio_revision *revision)
{
    size_t longest = 0;
    for (size_t i = 0; i < sizeof magics / sizeof magics[0]; ++i)
    {
        const size_t length = strlen(magics[i]);
        longest = length > longest ? length : longest;
    }
    const size_t available = sdfio_source_fill(source, longest);
    for (size_t i = 0; i < sizeof magics / sizeof magics[0]; ++i)
    {
        const size_t length = strlen(magics[i]);
        if (length <= available
            && memcmp(source->buffer + source->position, magics[i], length)
                   == 0)
        {
            source->position += length;
            *revision = (sdfio_revision)i;
            return true;
        }
    }
    return false;
}

/* Tolerated deviation: the identifier ends at its first 0 byte.
 *
 * The identifier up to the first 0, without the white space at its end. The
 * standard fills a string that is shorter than its field with spaces; some
 * writers end it in a 0 instead, and the bytes after that are whatever was in
 * memory. */
static void copy_identifier(sdfio_header *header, const unsigned char *field)
{
    const unsigned char *end = memchr(field, '\0', SDFIO_MANUFACTURER_ID_MAX);
    size_t size =
        end != NULL ? (size_t)(end - field) : SDFIO_MANUFACTURER_ID_MAX;
    while (size > 0 && sdfio_is_space((char)field[size - 1]))
    {
        --size;
    }
    memcpy(header->manufacturer_id, field, size);
    header->manufacturer_id[size] = '\0';
}

static sdfio_status read_trailer(sdfio_source *source, void *state,
                                 char *buffer, size_t capacity, size_t *size,
                                 const char **message)
{
    return sdfio_trailer_read(source, state, false, buffer, capacity, size,
                              message);
}

/* Takes bytes of the header from the source. */
static bool take(sdfio_source *source, void *out, size_t size)
{
    return sdfio_source_read(source, out, size) == size;
}

/* Takes an unsigned integer of `size` bytes in little-endian order. */
static bool take_le(sdfio_source *source, size_t size, uint64_t *value)
{
    unsigned char bytes[sizeof(uint64_t)];
    if (size > sizeof bytes || !take(source, bytes, size))
    {
        return false;
    }
    *value = 0;
    for (size_t i = size; i > 0; --i)
    {
        *value = (*value << CHAR_BIT) | bytes[i - 1];
    }
    return true;
}

/* Takes a binary64 number in little-endian order. */
static bool take_double(sdfio_source *source, double *value)
{
    uint64_t bits = 0;
    if (!take_le(source, sizeof bits, &bits))
    {
        return false;
    }
    memcpy(value, &bits, sizeof *value);
    return true;
}

static sdfio_status read_header(sdfio_source *source, sdfio_header *header,
                                void *state, const sdfio_time_zone *zone,
                                const char **message)
{
    (void)state;
    sdfio_revision revision = SDFIO_ISO_2_0;
    if (!take_revision(source, &revision))
    {
        return sdfio_source_failure(source, message,
                                    "The magic of the file is not supported");
    }
    const size_t columns_rows_size = sdfio_revision_columns_rows_size(revision);
    unsigned char identifier[SDFIO_MANUFACTURER_ID_MAX];
    unsigned char created_text[SDFIO_DATETIME_SIZE];
    unsigned char modified_text[SDFIO_DATETIME_SIZE];
    uint64_t columns = 0;
    uint64_t rows = 0;
    double x_scale = 0.0;
    double y_scale = 0.0;
    double z_scale = 0.0;
    double resolution = 0.0;
    uint64_t compression = 0;
    uint64_t code = 0;
    uint64_t check_type = 0;
    const bool complete =
        take(source, identifier, sizeof identifier)
        && take(source, created_text, sizeof created_text)
        && take(source, modified_text, sizeof modified_text)
        && take_le(source, columns_rows_size, &columns)
        && take_le(source, columns_rows_size, &rows)
        && take_double(source, &x_scale) && take_double(source, &y_scale)
        && take_double(source, &z_scale) && take_double(source, &resolution)
        && take_le(source, SDFIO_CODE_SIZE, &compression)
        && take_le(source, SDFIO_CODE_SIZE, &code)
        && take_le(source, SDFIO_CODE_SIZE, &check_type);
    if (!complete)
    {
        return sdfio_source_failure(
            source, message, "The file ends before the header is complete");
    }
    copy_identifier(header, identifier);
    if (!sdfio_datetime_valid((const char *)created_text, SDFIO_DATETIME_SIZE)
        || !sdfio_datetime_valid((const char *)modified_text,
                                 SDFIO_DATETIME_SIZE))
    {
        return sdfio_fail(message, SDFIO_ERR_FORMAT,
                          "A date of the header is not valid");
    }
    const time_t created = sdfio_datetime_parse(
        (const char *)created_text, SDFIO_DATETIME_SIZE, revision, zone);
    const time_t modified = sdfio_datetime_parse(
        (const char *)modified_text, SDFIO_DATETIME_SIZE, revision, zone);
    header->columns = (uint32_t)columns;
    header->rows = (uint32_t)rows;
    header->x_scale = x_scale;
    header->y_scale = y_scale;
    header->z_scale = z_scale;
    if (compression != SDFIO_NONE)
    {
        return sdfio_fail(message, SDFIO_ERR_FORMAT,
                          "The compression type of the file has to be 0");
    }
    if (check_type != SDFIO_NONE)
    {
        return sdfio_fail(message, SDFIO_ERR_FORMAT,
                          "The checksum type of the file has to be 0");
    }
    sdfio_data_type type = SDFIO_TYPE_FLOAT;
    if (!sdfio_data_type_from_code((long long)code, &type))
    {
        return sdfio_fail(message, SDFIO_ERR_FORMAT,
                          "The data type code of the header is unknown");
    }
    if (!sdfio_revision_allows(revision, type))
    {
        return sdfio_fail(
            message, SDFIO_ERR_FORMAT,
            "The data type is not valid for the revision of the file");
    }
    header->revision = revision;
    header->format = SDFIO_BINARY;
    header->data_type = type;
    header->z_resolution = resolution > 0.0 && isfinite(resolution)
                               ? resolution
                               : SDFIO_NO_RESOLUTION;
    header->creation_date = created;
    header->modification_date = modified;
    return SDFIO_OK;
}

static sdfio_status read_values(sdfio_source *source,
                                const sdfio_header *header, const void *state,
                                void *dst, size_t count, const char **message)
{
    (void)state;
    const size_t size = sdfio_data_type_size(header->data_type);
    if (sdfio_source_read(source, dst, count * size) != count * size)
    {
        return sdfio_source_failure(
            source, message, "The file ends before the data area is complete");
    }
    sdfio_convert_byte_order(dst, count, header->data_type);
    return SDFIO_OK;
}

static sdfio_status end_data(sdfio_source *source, const sdfio_header *header,
                             bool complete, size_t values_left, void *chunk,
                             size_t chunk_size, const char **message)
{
    if (complete)
    {
        return SDFIO_OK;
    }
    const size_t left = values_left * sdfio_data_type_size(header->data_type);
    size_t done = 0;
    while (done < left)
    {
        const size_t step = left - done < chunk_size ? left - done : chunk_size;
        if (sdfio_source_read(source, chunk, step) != step)
        {
            return sdfio_source_failure(
                source, message,
                "The file ends before the data area is complete");
        }
        done += step;
    }
    return SDFIO_OK;
}

static const sdfio_reader_format binary_format = {
    .state_size = sizeof(sdfio_trailer_state),
    .prefix = 'b',
    .magics = magics,
    .magic_count = sizeof magics / sizeof magics[0],
    .read_header = read_header,
    .read_values = read_values,
    .end_data = end_data,
    .read_trailer = read_trailer,
};

sdfio_status sdfio_reader_add_binary(sdfio_reader *reader)
{
    return sdfio_reader_install_format(reader, &binary_format);
}
