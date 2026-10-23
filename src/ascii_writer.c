/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file ascii_writer.c
 * @brief Writer codec of the ASCII representation.
 */

#include "ascii.h"
#include "data_type.h"
#include "datetime.h"
#include "dynstring.h"
#include "fields.h"
#include "message.h"
#include "number_text.h"
#include "sink.h"
#include "writer_codec.h"
#include <sdfio/sdfio.h>
#include <stdio.h>
#include <string.h>

static const char terminator = SDFIO_ASCII_TERMINATOR;
static const char crlf[] = SDFIO_ASCII_LINE_END;

/** What the writer of this codec keeps for a file. */
struct writing
{
    /** The decimal point of the locale, measured when the file was created. */
    sdfio_point point;
    /** The column of the next value to write. */
    size_t column;
};

/** A case of the test for the bad value, for SDFIO_SCALARS(). */
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

/** A case of the loading of a number, for SDFIO_SCALARS(). */
#define LOAD_CASE(data, type, low, high, floating)                             \
    case data:                                                                 \
    {                                                                          \
        type stored;                                                           \
        memcpy(&stored, (const unsigned char *)src + index * sizeof stored,    \
               sizeof stored);                                                 \
        return (double)stored;                                                 \
    }

/* The number at an index as a double, which holds every type of the data area
 * exactly. */
static double load_number(sdfio_data_type type, const void *src, size_t index)
{
    switch (type)
    {
        SDFIO_SCALARS(LOAD_CASE)
    }
    return 0.0;
}

static bool append_text(sdfio_string *out, const char *text)
{
    return sdfio_string_append(out, text, strlen(text));
}

static bool append_field(sdfio_string *out, const char *name, const char *value)
{
    return append_text(out, name) && append_text(out, " = ")
           && append_text(out, value) && append_text(out, crlf);
}

static bool append_scale(sdfio_string *out, const sdfio_point *point,
                         const char *name, double value)
{
    char text[SDFIO_NUMBER_CAPACITY];
    sdfio_format_scientific(point, text, sizeof text, value,
                            SDFIO_DOUBLE_DIGITS, SDFIO_DOUBLE_EXPONENT);
    return append_field(out, name, text);
}

static sdfio_status format_header(const sdfio_header *header,
                                  const sdfio_point *point,
                                  const sdfio_time_zone *zone,
                                  sdfio_string *out, sdfio_message *message)
{
    const char *version_number =
        sdfio_ascii_writer_codec.version_numbers[header->revision];
    char text[SDFIO_NUMBER_CAPACITY];
    char date[SDFIO_DATETIME_SIZE + 1] = {0};
    bool ok = append_text(out, version_number) && append_text(out, crlf)
              && append_field(out, SDFIO_ASCII_MANUFACTURER_ID,
                              header->manufacturer_id);
    if (!sdfio_datetime_format(header->creation_date, header->revision, zone,
                               date))
    {
        return sdfio_fail(message, SDFIO_ERR_RANGE,
                          "The time zone cannot convert the creation date");
    }
    ok = ok && append_field(out, SDFIO_ASCII_CREATE_DATE, date);
    if (!sdfio_datetime_format(header->modification_date, header->revision,
                               zone, date))
    {
        return sdfio_fail(message, SDFIO_ERR_RANGE,
                          "The time zone cannot convert the modification date");
    }
    ok = ok && append_field(out, SDFIO_ASCII_MODIFICATION_DATE, date);
    snprintf(text, sizeof text, "%zu", header->columns);
    ok = ok && append_field(out, SDFIO_ASCII_POINTS, text);
    snprintf(text, sizeof text, "%zu", header->rows);
    ok = ok && append_field(out, SDFIO_ASCII_PROFILES, text);
    ok = ok && append_scale(out, point, SDFIO_ASCII_X_SCALE, header->x_scale)
         && append_scale(out, point, SDFIO_ASCII_Y_SCALE, header->y_scale)
         && append_scale(out, point, SDFIO_ASCII_Z_SCALE, header->z_scale)
         && append_scale(out, point, SDFIO_ASCII_Z_RESOLUTION,
                         header->z_resolution > 0.0 ? header->z_resolution
                                                    : SDFIO_NO_RESOLUTION);
    snprintf(text, sizeof text, "%d", SDFIO_NONE);
    ok = ok && append_field(out, SDFIO_ASCII_COMPRESSION, text);
    snprintf(text, sizeof text, "%d", (int)header->data_type);
    ok = ok && append_field(out, SDFIO_ASCII_DATA_TYPE, text);
    snprintf(text, sizeof text, "%d", SDFIO_NONE);
    ok = ok && append_field(out, SDFIO_ASCII_CHECK_TYPE, text);
    ok = ok && sdfio_string_append(out, &terminator, 1)
         && append_text(out, crlf);
    if (!ok)
    {
        return sdfio_fail(message, SDFIO_ERR_MEMORY, "Out of memory");
    }
    return SDFIO_OK;
}

/* The decimal digits of an integer, without a terminator, which the C library
 * would format by a call per value. */
static size_t format_integer(long long value, char *out)
{
    unsigned long long magnitude = value < 0 ? 0ULL - (unsigned long long)value
                                             : (unsigned long long)value;
    char reversed[SDFIO_NUMBER_CAPACITY];
    size_t digits = 0;
    do
    {
        reversed[digits++] = (char)('0' + magnitude % 10);
        magnitude /= 10;
    } while (magnitude != 0);
    size_t used = 0;
    if (value < 0)
    {
        out[used++] = '-';
    }
    while (digits > 0)
    {
        out[used++] = reversed[--digits];
    }
    out[used] = '\0';
    return used;
}

static size_t format_value(const sdfio_header *header, const sdfio_point *point,
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
    const double value = load_number(type, coded, index);
    if (!sdfio_is_floating(type))
    {
        return format_integer((long long)value, out);
    }
    if (type == SDFIO_TYPE_FLOAT)
    {
        return sdfio_format_scientific(point, out, capacity, value,
                                       SDFIO_FLOAT_DIGITS,
                                       SDFIO_FLOAT_EXPONENT);
    }
    return sdfio_format_scientific(point, out, capacity, value,
                                   SDFIO_DOUBLE_DIGITS, SDFIO_DOUBLE_EXPONENT);
}

static size_t scratch_size(size_t chunk_values)
{
    return chunk_values * SDFIO_TEXT_PER_VALUE;
}

static sdfio_status write_header(sdfio_sink *sink, const sdfio_header *header,
                                 void *state, const sdfio_time_zone *zone,
                                 const sdfio_allocator *allocator,
                                 sdfio_message *message)
{
    struct writing *writing = state;
    writing->point = sdfio_point_now();
    writing->column = 0;
    sdfio_string text = {.allocator = allocator};
    sdfio_status status =
        format_header(header, &writing->point, zone, &text, message);
    if (status == SDFIO_OK
        && !sdfio_sink_write(sink, sdfio_string_text(&text), text.size))
    {
        status = sdfio_sink_failure(message);
    }
    sdfio_string_free(&text);
    return status;
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
        const size_t n =
            count - first < chunk_values ? count - first : chunk_values;
        size_t used = 0;
        for (size_t i = 0; i < n; ++i)
        {
            if (writing->column > 0)
            {
                text[used++] = ' ';
            }
            used += format_value(header, &writing->point,
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

const sdfio_writer_codec sdfio_ascii_writer_codec = {
    .version_numbers = {"aISO-1.0", "aISO-2.0"},
    .state_size = sizeof(struct writing),
    .representation = SDFIO_ASCII,
    .scratch_size = scratch_size,
    .write_header = write_header,
    .write_values = write_values,
    .begin_trailer = write_terminator,
    .end_trailer = write_terminator,
};
