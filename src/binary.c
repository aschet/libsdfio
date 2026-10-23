/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file binary.c
 * @brief Codecs of the binary representation.
 *
 * The files store values in little-endian byte order, regardless of the byte
 * order of the host.
 */

#include "binary.h"
#include "byte_order.h"
#include "common.h"
#include "data_type.h"
#include "dates.h"
#include "fields.h"
#include "reader_codec.h"
#include "revision.h"
#include "sink.h"
#include "source.h"
#include "tagged.h"
#include "text.h"
#include "writer_codec.h"

enum
{
    /** The number of bytes of a trailer that are read at a time. */
    SDFIO_TRAILER_PIECE = 256
};

static uint64_t get_le(const unsigned char *bytes, size_t size)
{
    uint64_t value = 0;
    for (size_t i = size; i > 0; --i)
    {
        value = (value << CHAR_BIT) | bytes[i - 1];
    }
    return value;
}

static void put_le(unsigned char *bytes, size_t size, uint64_t value)
{
    for (size_t i = 0; i < size; ++i)
    {
        bytes[i] = (unsigned char)(value & UCHAR_MAX);
        value >>= CHAR_BIT;
    }
}

static double get_double(const unsigned char *bytes)
{
    const uint64_t bits = get_le(bytes, sizeof(double));
    double value = 0.0;
    memcpy(&value, &bits, sizeof value);
    return value;
}

static void put_double(unsigned char *bytes, double value)
{
    uint64_t bits = 0;
    memcpy(&bits, &value, sizeof bits);
    put_le(bytes, sizeof bits, bits);
}

size_t sdfio_binary_header_size(sdfio_revision revision)
{
    /* The magic, the identifier and the two dates, the counts, four binary64
     * numbers and the bytes of the compression, the data type and the
     * checksum. */
    const size_t fixed =
        SDFIO_MAGIC_SIZE + SDFIO_ID_SIZE + SDFIO_HEADER_DATES * SDFIO_DATE_SIZE
        + SDFIO_HEADER_DOUBLES * sizeof(double) + SDFIO_HEADER_CODES;
    const size_t count_size =
        revision == SDFIO_ISO_2_0 ? sizeof(uint32_t) : sizeof(uint16_t);
    return fixed + SDFIO_HEADER_COUNTS * count_size;
}

sdfio_status sdfio_binary_revision(const unsigned char *magic,
                                   sdfio_revision *revision,
                                   sdfio_message *message)
{
    const char *text = (const char *)magic;
    const char prefix = text[0];
    if ((prefix != 'a' && prefix != 'b') || text[SDFIO_MAGIC_DASH_AT] != '-')
    {
        return sdfio_fail(message, SDFIO_ERR_FORMAT,
                          "Not an SDF file: unexpected magic");
    }
    if (!sdfio_revision_from_text(text + SDFIO_MAGIC_NAME_AT,
                                  SDFIO_MAGIC_SIZE - SDFIO_MAGIC_NAME_AT,
                                  revision))
    {
        return sdfio_fail(message, SDFIO_ERR_FORMAT,
                          "Unknown or unsupported SDF revision in magic");
    }
    if (prefix != 'b')
    {
        return sdfio_fail(message, SDFIO_ERR_FORMAT,
                          "ASCII magic found while parsing a binary SDF file");
    }
    return SDFIO_OK;
}

/* The identifier up to the first 0, without the white space at its end. The
 * standard fills a string that is shorter than its field with spaces; the
 * writer of MountainsMap ends it in a 0 instead, and the bytes after that are
 * whatever was in memory. */
static void copy_identifier(sdfio_header *header, const unsigned char *field)
{
    size_t size = 0;
    while (size < SDFIO_ID_SIZE && field[size] != '\0')
    {
        ++size;
    }
    while (size > 0 && sdfio_is_space((char)field[size - 1]))
    {
        --size;
    }
    memcpy(header->manufacturer_id, field, size);
    header->manufacturer_id[size] = '\0';
}

sdfio_status sdfio_binary_parse(const unsigned char *bytes,
                                sdfio_header *header,
                                const sdfio_time_zone *zone,
                                sdfio_message *message)
{
    sdfio_revision revision = SDFIO_ISO_2_0;
    sdfio_status status = sdfio_binary_revision(bytes, &revision, message);
    if (status != SDFIO_OK)
    {
        return status;
    }
    const unsigned char *at = bytes + SDFIO_MAGIC_SIZE;
    copy_identifier(header, at);
    at += SDFIO_ID_SIZE;
    if (!sdfio_date_is_valid((const char *)at, SDFIO_DATE_SIZE)
        || !sdfio_date_is_valid((const char *)at + SDFIO_DATE_SIZE,
                                SDFIO_DATE_SIZE))
    {
        return sdfio_fail(message, SDFIO_ERR_FORMAT,
                          "A date of the SDF header is not valid");
    }
    const time_t created =
        sdfio_date_parse((const char *)at, SDFIO_DATE_SIZE, revision, zone);
    at += SDFIO_DATE_SIZE;
    const time_t modified =
        sdfio_date_parse((const char *)at, SDFIO_DATE_SIZE, revision, zone);
    at += SDFIO_DATE_SIZE;
    const size_t count_size =
        revision == SDFIO_ISO_2_0 ? sizeof(uint32_t) : sizeof(uint16_t);
    header->columns = (size_t)get_le(at, count_size);
    at += count_size;
    header->rows = (size_t)get_le(at, count_size);
    at += count_size;
    header->x_scale = get_double(at);
    at += sizeof(double);
    header->y_scale = get_double(at);
    at += sizeof(double);
    header->z_scale = get_double(at);
    at += sizeof(double);
    const double resolution = get_double(at);
    at += sizeof(double);
    const unsigned compression = *at++;
    const unsigned code = *at++;
    const unsigned check_type = *at;
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
    if (code < (unsigned)SDFIO_TYPE_FLOAT || code > (unsigned)SDFIO_TYPE_DOUBLE)
    {
        return sdfio_fail(message, SDFIO_ERR_FORMAT,
                          "Unknown SDF data type code");
    }
    const sdfio_data_type type = (sdfio_data_type)code;
    if (!sdfio_revision_allows(revision, type))
    {
        return sdfio_fail(message, SDFIO_ERR_FORMAT,
                          "The data type is not valid for the SDF revision");
    }
    header->revision = revision;
    header->representation = SDFIO_BINARY;
    header->data_type = type;
    header->z_resolution = resolution > 0.0 && isfinite(resolution)
                               ? resolution
                               : SDFIO_NO_RESOLUTION;
    header->creation_date = created;
    header->modification_date = modified;
    return SDFIO_OK;
}

static unsigned char *put_text(unsigned char *out, const char *text,
                               size_t width)
{
    /* A text that is too long is cut, and one that is too short is filled
     * with spaces. */
    const size_t size = strlen(text);
    for (size_t i = 0; i < width; ++i)
    {
        out[i] = (unsigned char)(i < size ? text[i] : ' ');
    }
    return out + width;
}

sdfio_status sdfio_binary_header(const sdfio_header *header,
                                 const sdfio_time_zone *zone,
                                 unsigned char *out, size_t *size,
                                 sdfio_message *message)
{
    unsigned char *at = out;
    char magic[SDFIO_MAGIC_SIZE + 1] = {'b'};
    const char *revision_name = sdfio_revision_text(header->revision);
    const size_t revision_size = strlen(revision_name);
    memcpy(magic + 1, revision_name,
           revision_size < SDFIO_MAGIC_SIZE ? revision_size
                                            : SDFIO_MAGIC_SIZE - 1);
    at = put_text(at, magic, SDFIO_MAGIC_SIZE);
    at = put_text(at, header->manufacturer_id, SDFIO_ID_SIZE);
    char date[SDFIO_DATE_SIZE + 1] = {0};
    if (!sdfio_date_format(header->creation_date, header->revision, zone, date))
    {
        return sdfio_fail(message, SDFIO_ERR_RANGE,
                          "The time zone cannot convert the creation date");
    }
    at = put_text(at, date, SDFIO_DATE_SIZE);
    if (!sdfio_date_format(header->modification_date, header->revision, zone,
                           date))
    {
        return sdfio_fail(message, SDFIO_ERR_RANGE,
                          "The time zone cannot convert the modification date");
    }
    at = put_text(at, date, SDFIO_DATE_SIZE);
    const size_t count_size =
        header->revision == SDFIO_ISO_2_0 ? sizeof(uint32_t) : sizeof(uint16_t);
    put_le(at, count_size, header->columns);
    at += count_size;
    put_le(at, count_size, header->rows);
    at += count_size;
    put_double(at, header->x_scale);
    at += sizeof(double);
    put_double(at, header->y_scale);
    at += sizeof(double);
    put_double(at, header->z_scale);
    at += sizeof(double);
    put_double(at, header->z_resolution > 0.0 ? header->z_resolution
                                              : SDFIO_NO_RESOLUTION);
    at += sizeof(double);
    *at++ = SDFIO_NONE;
    *at++ = (unsigned char)header->data_type;
    *at++ = SDFIO_NONE;
    *size = (size_t)(at - out);
    return SDFIO_OK;
}

static sdfio_status read_trailer(sdfio_source *source, sdfio_string *out,
                                 bool *present, sdfio_message *message)
{
    char piece[SDFIO_TRAILER_PIECE];
    size_t count = 0;
    while ((count = sdfio_source_read(source, piece, sizeof piece)) > 0)
    {
        if (out->size + count > SDFIO_MAX_TRAILER)
        {
            return sdfio_fail(message, SDFIO_ERR_FORMAT,
                              "The SDF trailer is too long");
        }
        if (!sdfio_string_append(out, piece, count))
        {
            return sdfio_fail(message, SDFIO_ERR_MEMORY, "Out of memory");
        }
    }
    if (source->failed)
    {
        return sdfio_source_failure(source, message, "");
    }
    *present = out->size > 0; /* the file ends with the data */
    sdfio_lines_to_lf(out);
    return SDFIO_OK;
}

static sdfio_status read_header(sdfio_source *source, sdfio_header *header,
                                void *state, const sdfio_time_zone *zone,
                                sdfio_message *message)
{
    (void)state;
    const size_t available = sdfio_source_fill(source, SDFIO_MAGIC_SIZE);
    if (available < SDFIO_MAGIC_SIZE)
    {
        return sdfio_source_failure(source, message,
                                    "Not an SDF file, unexpected magic");
    }
    sdfio_revision revision = SDFIO_ISO_2_0;
    const sdfio_status status = sdfio_binary_revision(
        source->buffer + source->position, &revision, message);
    if (status != SDFIO_OK)
    {
        return status;
    }
    unsigned char bytes[SDFIO_BINARY_HEADER_MAX];
    const size_t size = sdfio_binary_header_size(revision);
    if (sdfio_source_read(source, bytes, size) != size)
    {
        return sdfio_source_failure(source, message,
                                    "File is too short to contain an SDF "
                                    "header");
    }
    return sdfio_binary_parse(bytes, header, zone, message);
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
    for (size_t done = 0; done < left;)
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

static size_t scratch_size(size_t chunk_values)
{
    return sdfio_host_is_little_endian() ? 0
                                         : chunk_values * SDFIO_LARGEST_SAMPLE;
}

static sdfio_status write_header(sdfio_sink *sink, const sdfio_header *header,
                                 void *state, const sdfio_time_zone *zone,
                                 const sdfio_allocator *allocator,
                                 sdfio_message *message)
{
    (void)state;
    (void)allocator;
    unsigned char bytes[SDFIO_BINARY_HEADER_MAX];
    size_t size = 0;
    const sdfio_status status =
        sdfio_binary_header(header, zone, bytes, &size, message);
    if (status == SDFIO_OK && !sdfio_sink_write(sink, bytes, size))
    {
        return sdfio_sink_failure(message);
    }
    return status;
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

const sdfio_reader_codec sdfio_binary_reader_codec = {
    .prefix = 'b',
    .magics = {"bISO-1.0", "bISO-2.0"},
    .read_header = read_header,
    .read_values = read_values,
    .end_data = end_data,
    .read_trailer = read_trailer,
};

const sdfio_writer_codec sdfio_binary_writer_codec = {
    .representation = SDFIO_BINARY,
    .scratch_size = scratch_size,
    .write_header = write_header,
    .write_values = write_values,
    .begin_trailer = begin_trailer,
    .end_trailer = end_trailer,
};
