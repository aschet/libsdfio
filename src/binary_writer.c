/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file binary_writer.c
 * @brief Writing of the binary format.
 *
 * The files store values in little-endian byte order, regardless of the byte
 * order of the host.
 */

#include "binary.h"
#include "byte_order.h"
#include "data_type.h"
#include "datetime.h"
#include "fields.h"
#include "message.h"
#include "sink.h"
#include "writer_format.h"
#include <limits.h>
#include <sdfio/sdfio.h>
#include <string.h>

_Static_assert(SDFIO_VERSION_NUMBER_SIZE <= SDFIO_DATETIME_SIZE
                   && SDFIO_ID_SIZE <= SDFIO_DATETIME_SIZE,
               "a text field of the header fits the buffer of put_text");

static size_t scratch_size(size_t chunk_values)
{
    return sdfio_host_is_little_endian()
               ? 0
               : chunk_values * SDFIO_LARGEST_CODED_VALUE;
}

static const char *const version_numbers[] = {
    [SDFIO_ISO_1_0] = "bISO-1.0", [SDFIO_ISO_2_0] = "bISO-2.0"};

/* Writes a text in a field of a width: a text that is too long is cut, and one
 * that is too short is filled with spaces. */
static bool put_text(sdfio_sink *sink, const char *text, size_t width)
{
    unsigned char field[SDFIO_DATETIME_SIZE];
    const size_t size = strlen(text);
    for (size_t i = 0; i < width; ++i)
    {
        field[i] = (unsigned char)(i < size ? text[i] : ' ');
    }
    return sdfio_sink_write(sink, field, width);
}

static bool put_le(sdfio_sink *sink, size_t size, uint64_t value)
{
    unsigned char bytes[sizeof(uint64_t)];
    for (size_t i = 0; i < size; ++i)
    {
        bytes[i] = (unsigned char)(value & UCHAR_MAX);
        value >>= CHAR_BIT;
    }
    return sdfio_sink_write(sink, bytes, size);
}

static bool put_double(sdfio_sink *sink, double value)
{
    uint64_t bits = 0;
    memcpy(&bits, &value, sizeof bits);
    return put_le(sink, sizeof bits, bits);
}

static sdfio_status write_header(sdfio_sink *sink, const sdfio_header *header,
                                 void *state, const sdfio_time_zone *zone,
                                 const sdfio_allocator *allocator,
                                 sdfio_message *message)
{
    (void)state;
    (void)allocator;
    char created[SDFIO_DATETIME_SIZE + 1] = {0};
    char modified[SDFIO_DATETIME_SIZE + 1] = {0};
    if (!sdfio_datetime_format(header->creation_date, header->revision, zone,
                               created))
    {
        return sdfio_fail(message, SDFIO_ERR_RANGE,
                          "The time zone cannot convert the creation date");
    }
    if (!sdfio_datetime_format(header->modification_date, header->revision,
                               zone, modified))
    {
        return sdfio_fail(message, SDFIO_ERR_RANGE,
                          "The time zone cannot convert the modification date");
    }
    const char *version_number = version_numbers[header->revision];
    const size_t columns_rows_size =
        sdfio_revision_columns_rows_size(header->revision);
    const double resolution =
        header->z_resolution > 0.0 ? header->z_resolution : SDFIO_NO_RESOLUTION;
    const unsigned char codes[SDFIO_BINARY_CODE_FIELDS] = {
        [SDFIO_BINARY_CODE_COMPRESSION] = SDFIO_NONE,
        [SDFIO_BINARY_CODE_DATA_TYPE] = (unsigned char)header->data_type,
        [SDFIO_BINARY_CODE_CHECKSUM] = SDFIO_NONE};
    const bool written =
        put_text(sink, version_number, SDFIO_VERSION_NUMBER_SIZE)
        && put_text(sink, header->manufacturer_id, SDFIO_ID_SIZE)
        && put_text(sink, created, SDFIO_DATETIME_SIZE)
        && put_text(sink, modified, SDFIO_DATETIME_SIZE)
        && put_le(sink, columns_rows_size, header->columns)
        && put_le(sink, columns_rows_size, header->rows)
        && put_double(sink, header->x_scale)
        && put_double(sink, header->y_scale)
        && put_double(sink, header->z_scale) && put_double(sink, resolution)
        && sdfio_sink_write(sink, codes, sizeof codes);
    return written ? SDFIO_OK : sdfio_sink_failure(message);
}

static sdfio_status write_values(sdfio_sink *sink, const sdfio_header *header,
                                 void *state, const void *values, size_t count,
                                 void *scratch, size_t chunk_values,
                                 sdfio_message *message)
{
    (void)state;
    const size_t size = sdfio_data_type_size(header->data_type);
    if (sdfio_host_is_little_endian())
    {
        return sdfio_sink_write(sink, values, count * size)
                   ? SDFIO_OK
                   : sdfio_sink_failure(message);
    }
    for (size_t first = 0; first < count; first += chunk_values)
    {
        const size_t n =
            count - first < chunk_values ? count - first : chunk_values;
        memcpy(scratch, (const unsigned char *)values + first * size, n * size);
        sdfio_fix_endianness(scratch, n, size);
        if (!sdfio_sink_write(sink, scratch, n * size))
        {
            return sdfio_sink_failure(message);
        }
    }
    return SDFIO_OK;
}

static sdfio_status begin_trailer(sdfio_sink *sink, sdfio_message *message)
{
    (void)sink;
    (void)message;
    return SDFIO_OK;
}

static sdfio_status end_trailer(sdfio_sink *sink, sdfio_message *message)
{
    (void)sink;
    (void)message;
    return SDFIO_OK;
}

const sdfio_writer_format sdfio_binary_writer_format = {
    .writes = SDFIO_BINARY,
    .scratch_size = scratch_size,
    .write_header = write_header,
    .write_values = write_values,
    .begin_trailer = begin_trailer,
    .end_trailer = end_trailer,
};
