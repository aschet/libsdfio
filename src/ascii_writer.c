/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief Writing of the ASCII format: header lines, numbers in text, closing
 * `*` lines.
 */

#include "ascii.h"
#include "chunk.h"
#include "data_type.h"
#include "datetime.h"
#include "fields.h"
#include "header.h"
#include "message.h"
#include "number_text.h"
#include "sink.h"
#include "size_math.h"
#include "writer_format.h"
#include <sdfio/sdfio.h>
#include <stdio.h>
#include <string.h>

static const char terminator = SDFIO_ASCII_TERMINATOR;
static const char crlf[] = SDFIO_ASCII_LINE_END;

/* State of the ASCII writer for a file. */
struct writing
{
    /* The decimal point of the locale, measured when the file was created. */
    sdfio_decimal_point decimal_point;
    /* The column of the next value to write. */
    size_t column;
};

/* A case of the test for the bad value, for SDFIO_SCALARS(). */
#define IS_BAD_CASE(data, type, low, high, floating)                           \
    case data:                                                                 \
    {                                                                          \
        type coded_value;                                                      \
        memcpy(&coded_value,                                                   \
               (const unsigned char *)src + index * sizeof coded_value,        \
               sizeof coded_value);                                            \
        return coded_value == (type)(low);                                     \
    }

static bool is_bad(sdfio_data_type type, const void *src, size_t index)
{
    switch (type)
    {
        SDFIO_SCALARS(IS_BAD_CASE)
    }
    return false;
}

/* A case of the loading of an integer, for SDFIO_SCALARS(). */
#define LOAD_INTEGER_CASE(data, type, low, high, floating)                     \
    case data:                                                                 \
    {                                                                          \
        type coded_value;                                                      \
        memcpy(&coded_value,                                                   \
               (const unsigned char *)src + index * sizeof coded_value,        \
               sizeof coded_value);                                            \
        return (floating) ? 0 : (int32_t)coded_value;                          \
    }

/* The integer at an index, for a data type that is an integer type. */
static int32_t load_integer(sdfio_data_type type, const void *src, size_t index)
{
    switch (type)
    {
        SDFIO_SCALARS(LOAD_INTEGER_CASE)
    }
    return 0;
}

/* A case of the loading of a real number, for SDFIO_SCALARS(). */
#define LOAD_REAL_CASE(data, type, low, high, floating)                        \
    case data:                                                                 \
    {                                                                          \
        type coded_value;                                                      \
        memcpy(&coded_value,                                                   \
               (const unsigned char *)src + index * sizeof coded_value,        \
               sizeof coded_value);                                            \
        return (floating) ? (double)coded_value : 0.0;                         \
    }

/* The number at an index as a double, which holds a float exactly, for a data
 * type that is a floating point type. */
static double load_real(sdfio_data_type type, const void *src, size_t index)
{
    switch (type)
    {
        SDFIO_SCALARS(LOAD_REAL_CASE)
    }
    return 0.0;
}

static const char *const magics[] = {
    [SDFIO_ISO_1_0] = "aISO-1.0", [SDFIO_ISO_2_0] = "aISO-2.0"};

static bool put_text(sdfio_sink *sink, const char *text)
{
    return sdfio_sink_write(sink, text, strlen(text));
}

/* Appends the header line `name = value`. */
static bool put_field(sdfio_sink *sink, const char *name, const char *value)
{
    return put_text(sink, name) && put_text(sink, SDFIO_ASCII_ASSIGNMENT)
           && put_text(sink, value) && put_text(sink, crlf);
}

/* Appends a header line whose value is a scale in the format of binary64. */
static bool put_scale(sdfio_sink *sink,
                      const sdfio_decimal_point *decimal_point,
                      const char *name, double value)
{
    char text[SDFIO_NUMBER_CAPACITY];
    const size_t length =
        sdfio_format_scientific(decimal_point, text, sizeof text - 1, value,
                                SDFIO_DOUBLE_DIGITS, SDFIO_DOUBLE_EXPONENT);
    text[length < sizeof text - 1 ? length : sizeof text - 1] = '\0';
    return put_field(sink, name, text);
}

/* Writes the header: the magic and a line for each field. The dates
 * are formatted first, so that nothing is written if one cannot be. */
static sdfio_status write_header(sdfio_sink *sink, const sdfio_header *header,
                                 void *state, const sdfio_time_zone *zone,
                                 const char **message)
{
    struct writing *writing = state;
    writing->decimal_point = sdfio_decimal_point_now();
    writing->column = 0;
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
    /* The field has 10 characters, as in a binary file. */
    char identifier[SDFIO_MANUFACTURER_ID_MAX + 1] = {0};
    const char *end =
        memchr(header->manufacturer_id, '\0', SDFIO_MANUFACTURER_ID_MAX);
    memcpy(identifier, header->manufacturer_id,
           end != NULL ? (size_t)(end - header->manufacturer_id)
                       : (size_t)SDFIO_MANUFACTURER_ID_MAX);
    char text[SDFIO_NUMBER_CAPACITY];
    const sdfio_decimal_point *point = &writing->decimal_point;
    bool ok = put_text(sink, magics[header->revision]) && put_text(sink, crlf)
              && put_field(sink, SDFIO_ASCII_MANUFACTURER_ID, identifier)
              && put_field(sink, SDFIO_ASCII_CREATE_DATE, created)
              && put_field(sink, SDFIO_ASCII_MODIFICATION_DATE, modified);
    snprintf(text, sizeof text, "%lu", (unsigned long)header->columns);
    ok = ok && put_field(sink, SDFIO_ASCII_POINTS, text);
    snprintf(text, sizeof text, "%lu", (unsigned long)header->rows);
    ok = ok && put_field(sink, SDFIO_ASCII_PROFILES, text);
    ok = ok && put_scale(sink, point, SDFIO_ASCII_X_SCALE, header->x_scale)
         && put_scale(sink, point, SDFIO_ASCII_Y_SCALE, header->y_scale)
         && put_scale(sink, point, SDFIO_ASCII_Z_SCALE, header->z_scale)
         && put_scale(sink, point, SDFIO_ASCII_Z_RESOLUTION,
                      header->z_resolution);
    snprintf(text, sizeof text, "%d", SDFIO_NONE);
    ok = ok && put_field(sink, SDFIO_ASCII_COMPRESSION, text);
    snprintf(text, sizeof text, "%d", (int)header->data_type);
    ok = ok && put_field(sink, SDFIO_ASCII_DATA_TYPE, text);
    snprintf(text, sizeof text, "%d", SDFIO_NONE);
    ok = ok && put_field(sink, SDFIO_ASCII_CHECK_TYPE, text);
    ok = ok && sdfio_sink_write(sink, &terminator, 1) && put_text(sink, crlf);
    return ok ? SDFIO_OK : sdfio_sink_failure(message);
}

/* The digits of the largest integer of a data type, int32. */
#define INTEGER_DIGITS 10

/* Writes an integer in decimal digits, without a terminator. */
static size_t put_integer(char *out, int32_t value)
{
    char digits[INTEGER_DIGITS];
    size_t count = 0;
    uint32_t magnitude =
        value < 0 ? UINT32_C(0) - (uint32_t)value : (uint32_t)value;
    do
    {
        digits[count] = (char)('0' + magnitude % 10U);
        ++count;
        magnitude /= 10U;
    } while (magnitude != 0);
    size_t used = 0;
    if (value < 0)
    {
        out[used] = '-';
        ++used;
    }
    while (count > 0)
    {
        --count;
        out[used] = digits[count];
        ++used;
    }
    return used;
}

/* Writes the text of the value at an index, and returns its length. */
static size_t format_value(const sdfio_header *header,
                           const sdfio_decimal_point *decimal_point,
                           const void *coded, size_t index, char *out,
                           size_t capacity)
{
    const sdfio_data_type type = header->data_type;
    if (is_bad(type, coded, index))
    {
        static const char bad[] = SDFIO_ASCII_BAD;
        memcpy(out, bad, sizeof bad);
        return sizeof bad - 1;
    }
    if (!sdfio_is_floating(type))
    {
        return put_integer(out, load_integer(type, coded, index));
    }
    const double value = load_real(type, coded, index);
    if (type == SDFIO_TYPE_FLOAT)
    {
        return sdfio_format_scientific(decimal_point, out, capacity, value,
                                       SDFIO_FLOAT_DIGITS,
                                       SDFIO_FLOAT_EXPONENT);
    }
    return sdfio_format_scientific(decimal_point, out, capacity, value,
                                   SDFIO_DOUBLE_DIGITS, SDFIO_DOUBLE_EXPONENT);
}

static size_t file_chunk_size(size_t chunk_values)
{
    return chunk_values * SDFIO_TEXT_PER_VALUE;
}

static sdfio_status write_values(sdfio_sink *sink, const sdfio_header *header,
                                 void *state, const void *values, size_t count,
                                 void *file_chunk, size_t chunk_values,
                                 const char **message)
{
    struct writing *writing = state;
    char *text = file_chunk;
    const size_t size = sdfio_data_type_size(header->data_type);
    for (size_t first = 0; first < count; first += chunk_values)
    {
        const size_t n = sdfio_chunk_count(count, first, chunk_values);
        size_t used = 0;
        for (size_t i = 0; i < n; ++i)
        {
            if (writing->column > 0)
            {
                text[used] = ' ';
                ++used;
            }
            used += format_value(header, &writing->decimal_point,
                                 (const unsigned char *)values + first * size,
                                 i, text + used, SDFIO_TEXT_PER_VALUE);
            if (++writing->column == header->columns)
            {
                memcpy(text + used, crlf, sizeof crlf - 1);
                used += sizeof crlf - 1;
                writing->column = 0;
            }
        }
        if (!sdfio_sink_write(sink, text, used))
        {
            return sdfio_sink_failure(message);
        }
    }
    return SDFIO_OK;
}

/* The line that closes a record: the data area, and the trailer. */
static sdfio_status write_terminator(sdfio_sink *sink, const char **message)
{
    if (!sdfio_sink_write(sink, &terminator, 1)
        || !sdfio_sink_write(sink, crlf, sizeof crlf - 1))
    {
        return sdfio_sink_failure(message);
    }
    return SDFIO_OK;
}

/* The digits of the largest number of columns or rows, a uint32_t. */
#define COUNT_DIGITS 10
/* The digits of the code of the compression, the data type and the checksum. */
#define CODE_DIGITS 1

/* The length of the line `name = value` for a value of at most `value_size`
 * characters. */
static size_t field_size(const char *name, size_t value_size)
{
    return strlen(name) + strlen(SDFIO_ASCII_ASSIGNMENT) + value_size
           + strlen(crlf);
}

/* The length of a number in the form of sdfio_format_scientific(): the sign,
 * a digit, the decimal point, the digits, the `e`, the sign of the exponent and
 * the digits of the exponent. */
static size_t scientific_size(size_t digits, size_t exponent_digits)
{
    return 1 + 1 + 1 + digits + 1 + 1 + exponent_digits;
}

/* The length of the longest text of a value of a data type. The text of the
 * bad value is shorter. */
static size_t value_size(sdfio_data_type type)
{
    switch (type)
    {
    case SDFIO_TYPE_INT8:
        return sizeof "-128" - 1;
    case SDFIO_TYPE_INT16:
        return sizeof "-32768" - 1;
    case SDFIO_TYPE_INT32:
        return sizeof "-2147483648" - 1;
    case SDFIO_TYPE_FLOAT:
        return scientific_size(SDFIO_FLOAT_DIGITS, SDFIO_FLOAT_EXPONENT);
    case SDFIO_TYPE_DOUBLE:
        return scientific_size(SDFIO_DOUBLE_DIGITS, SDFIO_DOUBLE_EXPONENT);
    }
    return 0;
}

/* The longest header that write_header() writes: every field has its longest
 * text, and the dates and the identifier are fields of a fixed width. */
static size_t header_size(sdfio_revision revision)
{
    const size_t scale =
        scientific_size(SDFIO_DOUBLE_DIGITS, SDFIO_DOUBLE_EXPONENT);
    return strlen(magics[revision]) + strlen(crlf)
           + field_size(SDFIO_ASCII_MANUFACTURER_ID, SDFIO_MANUFACTURER_ID_MAX)
           + field_size(SDFIO_ASCII_CREATE_DATE, SDFIO_DATETIME_SIZE)
           + field_size(SDFIO_ASCII_MODIFICATION_DATE, SDFIO_DATETIME_SIZE)
           + field_size(SDFIO_ASCII_POINTS, COUNT_DIGITS)
           + field_size(SDFIO_ASCII_PROFILES, COUNT_DIGITS)
           + field_size(SDFIO_ASCII_X_SCALE, scale)
           + field_size(SDFIO_ASCII_Y_SCALE, scale)
           + field_size(SDFIO_ASCII_Z_SCALE, scale)
           + field_size(SDFIO_ASCII_Z_RESOLUTION, scale)
           + field_size(SDFIO_ASCII_COMPRESSION, CODE_DIGITS)
           + field_size(SDFIO_ASCII_DATA_TYPE, CODE_DIGITS)
           + field_size(SDFIO_ASCII_CHECK_TYPE, CODE_DIGITS) + 1 + strlen(crlf);
}

/* The size of a file without the trailer: the header, the data area with a
 * separator or a line end after every value, and the two lines that close the
 * data area and the trailer. */
static sdfio_status estimate_size(const sdfio_header *header, size_t *size)
{
    size_t values = 0;
    size_t text = 0;
    size_t line_ends = 0;
    if (!sdfio_size_multiply(header->columns, header->rows, &values)
        || !sdfio_size_multiply(values, value_size(header->data_type) + 1,
                                &text)
        || !sdfio_size_multiply(header->rows, strlen(crlf), &line_ends))
    {
        return SDFIO_ERR_RANGE;
    }
    *size = header_size(header->revision);
    return sdfio_size_add(size, text) && sdfio_size_add(size, line_ends)
                   && sdfio_size_add(size, 2 * (1 + strlen(crlf)))
               ? SDFIO_OK
               : SDFIO_ERR_RANGE;
}

static const sdfio_writer_format ascii_format = {
    .state_size = sizeof(struct writing),
    .writes = SDFIO_ASCII,
    .trailer_ends_line = true,
    .file_chunk_size = file_chunk_size,
    .estimate_size = estimate_size,
    .write_header = write_header,
    .write_values = write_values,
    .begin_trailer = write_terminator,
    .end_trailer = write_terminator,
};

sdfio_status sdfio_writer_add_ascii(sdfio_writer *writer)
{
    return sdfio_writer_install_format(writer, &ascii_format);
}
