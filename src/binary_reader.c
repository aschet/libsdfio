/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file binary_reader.c
 * @brief Reading of the binary format.
 *
 * The files store values in little-endian byte order, regardless of the byte
 * order of the host.
 */

#include "binary.h"
#include "byte_order.h"
#include "data_type.h"
#include "datetime.h"
#include "fields.h"
#include "header.h"
#include "message.h"
#include "reader_format.h"
#include "reader_state.h"
#include "revision.h"
#include "source.h"
#include "tagged.h"
#include "text.h"
#include "trailer_text.h"
#include <limits.h>
#include <sdfio/sdfio.h>
#include <string.h>

/* The unsigned integer of `size` bytes in little-endian order. */
static uint64_t get_le(const unsigned char *bytes, size_t size)
{
    uint64_t value = 0;
    for (size_t i = size; i > 0; --i)
    {
        value = (value << CHAR_BIT) | bytes[i - 1];
    }
    return value;
}

/* The binary64 number in little-endian order. */
static double get_double(const unsigned char *bytes)
{
    const uint64_t bits = get_le(bytes, sizeof(double));
    double value = 0.0;
    memcpy(&value, &bits, sizeof value);
    return value;
}

static const char *const version_numbers[] = {
    [SDFIO_ISO_1_0] = "bISO-1.0", [SDFIO_ISO_2_0] = "bISO-2.0"};

/* The revision whose version number the bytes are, according to the version
 * numbers that the format lists. */
static bool revision_of(const unsigned char *version_number,
                        sdfio_revision *revision)
{
    for (size_t i = 0; i < sizeof version_numbers / sizeof version_numbers[0];
         ++i)
    {
        if (strncmp((const char *)version_number, version_numbers[i],
                    SDFIO_VERSION_NUMBER_SIZE)
            == 0)
        {
            *revision = (sdfio_revision)i;
            return true;
        }
    }
    return false;
}

/* Tolerated deviation: the identifier ends at its first 0 byte.
 *
 * The identifier up to the first 0, without the white space at its end. The
 * standard fills a string that is shorter than its field with spaces; the
 * writer of MountainsMap ends it in a 0 instead, and the bytes after that are
 * whatever was in memory. */
static void copy_identifier(sdfio_header *header, const unsigned char *field)
{
    const unsigned char *end = memchr(field, '\0', SDFIO_ID_SIZE);
    size_t size = end != NULL ? (size_t)(end - field) : SDFIO_ID_SIZE;
    while (size > 0 && sdfio_is_space((char)field[size - 1]))
    {
        --size;
    }
    memcpy(header->manufacturer_id, field, size);
    header->manufacturer_id[size] = '\0';
}

static sdfio_status read_trailer(sdfio_source *source, void *state,
                                 char *buffer, size_t capacity, size_t *size,
                                 sdfio_message *message)
{
    return sdfio_trailer_read(source, state, false, buffer, capacity, size,
                              message);
}

/* Takes bytes of the header from the source. */
static bool take(sdfio_source *source, void *out, size_t size)
{
    return sdfio_source_read(source, out, size) == size;
}

static sdfio_status read_header(sdfio_source *source, sdfio_header *header,
                                void *state, const sdfio_time_zone *zone,
                                sdfio_message *message)
{
    (void)state;
    unsigned char version_number[SDFIO_VERSION_NUMBER_SIZE];
    if (!take(source, version_number, sizeof version_number))
    {
        return sdfio_source_failure(
            source, message, "Not an SDF file, unexpected version number");
    }
    sdfio_revision revision = SDFIO_ISO_2_0;
    if (!revision_of(version_number, &revision))
    {
        return sdfio_fail(message, SDFIO_ERR_FORMAT,
                          "Unknown or unsupported SDF version number");
    }
    const size_t columns_rows_size = sdfio_revision_columns_rows_size(revision);
    unsigned char identifier[SDFIO_ID_SIZE];
    unsigned char created_text[SDFIO_DATETIME_SIZE];
    unsigned char modified_text[SDFIO_DATETIME_SIZE];
    unsigned char columns_field[sizeof(uint32_t)];
    unsigned char rows_field[sizeof(uint32_t)];
    unsigned char scales[SDFIO_BINARY_REAL_FIELDS][sizeof(double)];
    unsigned char codes[SDFIO_BINARY_CODE_FIELDS];
    bool complete = take(source, identifier, sizeof identifier)
                    && take(source, created_text, sizeof created_text)
                    && take(source, modified_text, sizeof modified_text)
                    && take(source, columns_field, columns_rows_size)
                    && take(source, rows_field, columns_rows_size);
    for (size_t i = 0; complete && i < SDFIO_BINARY_REAL_FIELDS; ++i)
    {
        complete = take(source, scales[i], sizeof scales[i]);
    }
    if (!complete || !take(source, codes, sizeof codes))
    {
        return sdfio_source_failure(source, message,
                                    "File is too short to contain an SDF "
                                    "header");
    }
    copy_identifier(header, identifier);
    if (!sdfio_datetime_is_valid((const char *)created_text,
                                 SDFIO_DATETIME_SIZE)
        || !sdfio_datetime_is_valid((const char *)modified_text,
                                    SDFIO_DATETIME_SIZE))
    {
        return sdfio_fail(message, SDFIO_ERR_FORMAT,
                          "A date of the SDF header is not valid");
    }
    const time_t created = sdfio_datetime_parse(
        (const char *)created_text, SDFIO_DATETIME_SIZE, revision, zone);
    const time_t modified = sdfio_datetime_parse(
        (const char *)modified_text, SDFIO_DATETIME_SIZE, revision, zone);
    header->columns = (uint32_t)get_le(columns_field, columns_rows_size);
    header->rows = (uint32_t)get_le(rows_field, columns_rows_size);
    header->x_scale = get_double(scales[0]);
    header->y_scale = get_double(scales[1]);
    header->z_scale = get_double(scales[2]);
    const double resolution = get_double(scales[3]);
    const unsigned compression = codes[SDFIO_BINARY_CODE_COMPRESSION];
    const unsigned code = codes[SDFIO_BINARY_CODE_DATA_TYPE];
    const unsigned check_type = codes[SDFIO_BINARY_CODE_CHECKSUM];
    if (compression != SDFIO_NONE)
    {
        return sdfio_fail(message, SDFIO_ERR_FORMAT,
                          "The compression type of an SDF file has to be 0");
    }
    if (check_type != SDFIO_NONE)
    {
        return sdfio_fail(message, SDFIO_ERR_FORMAT,
                          "The checksum type of an SDF file has to be 0");
    }
    sdfio_data_type type = SDFIO_TYPE_FLOAT;
    if (!sdfio_data_type_from_code(code, &type))
    {
        return sdfio_fail(message, SDFIO_ERR_FORMAT,
                          "Unknown SDF data type code");
    }
    if (!sdfio_revision_allows(revision, type))
    {
        return sdfio_fail(message, SDFIO_ERR_FORMAT,
                          "The data type is not valid for the SDF revision");
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
                                void *dst, size_t count, sdfio_message *message)
{
    (void)state;
    const size_t size = sdfio_data_type_size(header->data_type);
    if (sdfio_source_read(source, dst, count * size) != count * size)
    {
        return sdfio_source_failure(source, message,
                                    "Unexpected end of file while reading the "
                                    "SDF data area");
    }
    sdfio_fix_endianness(dst, count, size);
    return SDFIO_OK;
}

static sdfio_status end_data(sdfio_source *source, const sdfio_header *header,
                             bool complete, size_t values_left, void *scratch,
                             size_t scratch_size, sdfio_message *message)
{
    if (complete)
    {
        return SDFIO_OK;
    }
    const size_t left = values_left * sdfio_data_type_size(header->data_type);
    size_t done = 0;
    while (done < left)
    {
        const size_t step =
            left - done < scratch_size ? left - done : scratch_size;
        if (sdfio_source_read(source, scratch, step) != step)
        {
            return sdfio_source_failure(source, message,
                                        "Unexpected end of file while reading "
                                        "the SDF data area");
        }
        done += step;
    }
    return SDFIO_OK;
}

static const sdfio_reader_format binary_format = {
    .state_size = sizeof(sdfio_trailer_state),
    .prefix = 'b',
    .version_numbers = version_numbers,
    .version_number_count = sizeof version_numbers / sizeof version_numbers[0],
    .read_header = read_header,
    .read_values = read_values,
    .end_data = end_data,
    .read_trailer = read_trailer,
};

sdfio_status sdfio_reader_add_binary(sdfio_reader *reader)
{
    return sdfio_reader_install_format(reader, &binary_format);
}
