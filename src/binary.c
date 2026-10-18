/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* The binary representation: the fixed header, and the trailer that follows
 * the data. Little endian, whatever the machine is. */

#include "internal.h"

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

sdfio_status sdfi_binary_revision(const unsigned char *magic,
                                  sdfio_revision *revision,
                                  sdfi_message *message)
{
    const char *text = (const char *)magic;
    const char prefix = sdfi_lower(text[0]);
    if ((prefix != 'a' && prefix != 'b') || text[SDFI_MAGIC_DASH_AT] != '-')
    {
        return sdfi_fail(message, SDFIO_ERR_FORMAT,
                         "Not an SDF file, unexpected magic: '%.*s'",
                         SDFI_MAGIC_SIZE, text);
    }
    if (!sdfi_revision_from_text(text + SDFI_MAGIC_NAME_AT,
                                 SDFI_MAGIC_SIZE - SDFI_MAGIC_NAME_AT,
                                 revision))
    {
        return sdfi_fail(message, SDFIO_ERR_FORMAT,
                         "Unknown or unsupported SDF revision in magic '%.*s'",
                         SDFI_MAGIC_SIZE, text);
    }
    if (prefix != 'b')
    {
        return sdfi_fail(message, SDFIO_ERR_FORMAT,
                         "ASCII magic found while parsing a binary SDF file");
    }
    return SDFIO_OK;
}

/* The identifier up to the first 0, without the white space at its end:
 * BCR-1.0 lets a string that is shorter than its field end in a 0, and some
 * ISO writers do the same instead of filling the field with spaces. */
static void copy_identifier(sdfio_header *header, const unsigned char *field)
{
    size_t size = 0;
    while (size < SDFI_ID_SIZE && field[size] != '\0')
    {
        ++size;
    }
    while (size > 0 && sdfi_is_space((char)field[size - 1]))
    {
        --size;
    }
    memcpy(header->manufacturer_id, field, size);
    header->manufacturer_id[size] = '\0';
}

sdfio_status sdfi_binary_parse(const unsigned char *bytes, sdfio_header *header,
                               sdfi_message *message)
{
    sdfio_revision revision = SDFIO_ISO_2_0;
    sdfio_status status = sdfi_binary_revision(bytes, &revision, message);
    if (status != SDFIO_OK)
    {
        return status;
    }
    const unsigned char *at = bytes + SDFI_MAGIC_SIZE;
    copy_identifier(header, at);
    at += SDFI_ID_SIZE;
    const time_t created =
        sdfi_date_parse((const char *)at, SDFI_DATE_SIZE, revision);
    at += SDFI_DATE_SIZE;
    const time_t modified =
        sdfi_date_parse((const char *)at, SDFI_DATE_SIZE, revision);
    at += SDFI_DATE_SIZE;
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
    if (compression != SDFI_NONE)
    {
        return sdfi_fail(message, SDFIO_ERR_UNSUPPORTED,
                         "Compressed SDF data areas are not supported");
    }
    if (check_type != SDFI_NONE)
    {
        return sdfi_fail(message, SDFIO_ERR_UNSUPPORTED,
                         "Checksummed SDF data areas are not supported");
    }
    if (code > (unsigned)SDFIO_DATA_BINARY64)
    {
        return sdfi_fail(message, SDFIO_ERR_FORMAT,
                         "Unknown SDF data type code %u", code);
    }
    const sdfio_data_type type = (sdfio_data_type)code;
    if (!sdfio_revision_allows(revision, type))
    {
        return sdfi_fail(message, SDFIO_ERR_FORMAT,
                         "Data type %s is not valid for SDF revision %s",
                         sdfio_data_type_name(type),
                         sdfio_revision_name(revision));
    }
    header->revision = revision;
    header->storage = SDFIO_BINARY;
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

sdfio_status sdfi_binary_header(const sdfio_header *header, unsigned char *out,
                                size_t *size, sdfi_message *message)
{
    (void)message;
    unsigned char *at = out;
    char magic[SDFI_MAGIC_SIZE + 1];
    snprintf(magic, sizeof magic, "b%s", sdfio_revision_name(header->revision));
    at = put_text(at, magic, SDFI_MAGIC_SIZE);
    at = put_text(at, header->manufacturer_id, SDFI_ID_SIZE);
    char date[SDFI_DATE_SIZE + 1] = {0};
    sdfi_date_format(header->creation_date, header->revision, date);
    at = put_text(at, date, SDFI_DATE_SIZE);
    sdfi_date_format(header->modification_date, header->revision, date);
    at = put_text(at, date, SDFI_DATE_SIZE);
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
    *at++ = SDFI_NONE;
    *at++ = (unsigned char)header->data_type;
    *at++ = SDFI_NONE;
    *size = (size_t)(at - out);
    return SDFIO_OK;
}

sdfio_status sdfi_binary_trailer(sdfi_source *source, sdfi_string *out,
                                 bool *present, sdfi_message *message)
{
    char piece[SDFI_SOURCE_BUFFER];
    size_t count = 0;
    while ((count = sdfi_source_read(source, piece, sizeof piece)) > 0)
    {
        if (out->size + count > SDFI_MAX_TRAILER)
        {
            return sdfi_fail(message, SDFIO_ERR_FORMAT,
                             "The SDF trailer is too long");
        }
        if (!sdfi_string_append(out, piece, count))
        {
            return sdfi_fail(message, SDFIO_ERR_MEMORY, "Out of memory");
        }
    }
    if (source->failed)
    {
        return sdfi_fail(message, SDFIO_ERR_IO, "The stream could not be read");
    }
    *present = out->size > 0; /* the file ends with the data */
    sdfi_lines_to_lf(out);
    return SDFIO_OK;
}
