/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file ascii_writer.c
 * @brief Writing of the ASCII format.
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
#include "writer_format.h"
#include "writer_state.h"
#include <sdfio/sdfio.h>
#include <stdio.h>
#include <string.h>

static const char terminator = SDFIO_ASCII_TERMINATOR;
static const char crlf[] = SDFIO_ASCII_LINE_END;

/* What the writer of this format keeps for a file. */
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
        type stored;                                                           \
        memcpy(&stored, (const unsigned char *)src + index * sizeof stored,    \
               sizeof stored);                                                 \
        return stored == (type)(low);                                          \
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
        type stored;                                                           \
        memcpy(&stored, (const unsigned char *)src + index * sizeof stored,    \
               sizeof stored);                                                 \
        return (floating) ? 0 : (int32_t)stored;                               \
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
        type stored;                                                           \
        memcpy(&stored, (const unsigned char *)src + index * sizeof stored,    \
               sizeof stored);                                                 \
        return (floating) ? (double)stored : 0.0;                              \
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

static const char *const version_numbers[] = {
    [SDFIO_ISO_1_0] = "aISO-1.0", [SDFIO_ISO_2_0] = "aISO-2.0"};

static bool append_text(sdfio_sink *sink, const char *text)
{
    return sdfio_sink_write(sink, text, strlen(text));
}

/* Appends the header line `name = value`. */
static bool append_field(sdfio_sink *sink, const char *name, const char *value)
{
    return append_text(sink, name) && append_text(sink, " = ")
           && append_text(sink, value) && append_text(sink, crlf);
}

/* Appends a header line whose value is a scale in the format of binary64. */
static bool append_scale(sdfio_sink *sink,
                         const sdfio_decimal_point *decimal_point,
                         const char *name, double value)
{
    char text[SDFIO_NUMBER_CAPACITY];
    const size_t length =
        sdfio_format_scientific(decimal_point, text, sizeof text - 1, value,
                                SDFIO_DOUBLE_DIGITS, SDFIO_DOUBLE_EXPONENT);
    text[length < sizeof text - 1 ? length : sizeof text - 1] = '\0';
    return append_field(sink, name, text);
}

/* Writes the header: the version number and a line for each field. The dates
 * are formatted first, so that nothing is written if one cannot be. */
static sdfio_status write_header(sdfio_sink *sink, const sdfio_header *header,
                                 void *state, const sdfio_time_zone *zone,
                                 sdfio_message *message)
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
    char identifier[SDFIO_ID_SIZE + 1] = {0};
    const char *end = memchr(header->manufacturer_id, '\0', SDFIO_ID_SIZE);
    memcpy(identifier, header->manufacturer_id,
           end != NULL ? (size_t)(end - header->manufacturer_id)
                       : (size_t)SDFIO_ID_SIZE);
    char text[SDFIO_NUMBER_CAPACITY];
    const sdfio_decimal_point *point = &writing->decimal_point;
    bool ok = append_text(sink, version_numbers[header->revision])
              && append_text(sink, crlf)
              && append_field(sink, SDFIO_ASCII_MANUFACTURER_ID, identifier)
              && append_field(sink, SDFIO_ASCII_CREATE_DATE, created)
              && append_field(sink, SDFIO_ASCII_MODIFICATION_DATE, modified);
    snprintf(text, sizeof text, "%lu", (unsigned long)header->columns);
    ok = ok && append_field(sink, SDFIO_ASCII_POINTS, text);
    snprintf(text, sizeof text, "%lu", (unsigned long)header->rows);
    ok = ok && append_field(sink, SDFIO_ASCII_PROFILES, text);
    ok = ok && append_scale(sink, point, SDFIO_ASCII_X_SCALE, header->x_scale)
         && append_scale(sink, point, SDFIO_ASCII_Y_SCALE, header->y_scale)
         && append_scale(sink, point, SDFIO_ASCII_Z_SCALE, header->z_scale)
         && append_scale(sink, point, SDFIO_ASCII_Z_RESOLUTION,
                         header->z_resolution > 0.0 ? header->z_resolution
                                                    : SDFIO_NO_RESOLUTION);
    snprintf(text, sizeof text, "%d", SDFIO_NONE);
    ok = ok && append_field(sink, SDFIO_ASCII_COMPRESSION, text);
    snprintf(text, sizeof text, "%d", (int)header->data_type);
    ok = ok && append_field(sink, SDFIO_ASCII_DATA_TYPE, text);
    snprintf(text, sizeof text, "%d", SDFIO_NONE);
    ok = ok && append_field(sink, SDFIO_ASCII_CHECK_TYPE, text);
    ok =
        ok && sdfio_sink_write(sink, &terminator, 1) && append_text(sink, crlf);
    return ok ? SDFIO_OK : sdfio_sink_failure(message);
}

enum
{
    /* The digits of the largest integer of a data type, int32. */
    SDFIO_INTEGER_DIGITS = 10
};

/* Writes an integer in decimal digits, without a terminator. */
static size_t put_integer(char *out, int32_t value)
{
    char digits[SDFIO_INTEGER_DIGITS];
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

static size_t scratch_size(size_t chunk_values)
{
    return chunk_values * SDFIO_TEXT_PER_VALUE;
}

static sdfio_status write_values(sdfio_sink *sink, const sdfio_header *header,
                                 void *state, const void *values, size_t count,
                                 void *scratch, size_t chunk_values,
                                 sdfio_message *message)
{
    struct writing *writing = state;
    char *text = scratch;
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
static sdfio_status write_terminator(sdfio_sink *sink, sdfio_message *message)
{
    if (!sdfio_sink_write(sink, &terminator, 1)
        || !sdfio_sink_write(sink, crlf, sizeof crlf - 1))
    {
        return sdfio_sink_failure(message);
    }
    return SDFIO_OK;
}

static const sdfio_writer_format ascii_format = {
    .state_size = sizeof(struct writing),
    .writes = SDFIO_ASCII,
    .scratch_size = scratch_size,
    .write_header = write_header,
    .write_values = write_values,
    .begin_trailer = write_terminator,
    .end_trailer = write_terminator,
};

sdfio_status sdfio_writer_add_ascii_format(sdfio_writer *writer)
{
    return sdfio_writer_install_format(writer, &ascii_format);
}
