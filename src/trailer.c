/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* The trailer as fields: reading them, composing them and checking them. */

#include "internal.h"

#include <stdio.h>

enum
{
    /* A number as text, with its sign and exponent. */
    INTEGER_CAPACITY = 32,
    /* The digits after the point that read a binary64 back exactly: 17
     * significant digits. */
    EXACT_DIGITS = 16
};

/* " = " between the name and the value, and the end of the line. */
static const char separator[] = " = ";

bool sdfio_trailer_find(const char *trailer, size_t size, const char *name,
                        const char **value, size_t *value_size)
{
    if (trailer == NULL || name == NULL || value == NULL || value_size == NULL)
    {
        return false;
    }
    return sdfi_tagged_find(trailer, size, name, value, value_size);
}

bool sdfio_trailer_next(const char *trailer, size_t size, size_t *position,
                        sdfio_field *field)
{
    if (trailer == NULL || position == NULL || field == NULL)
    {
        return false;
    }
    return sdfi_tagged_next(trailer, size, position, &field->name,
                            &field->name_size, &field->value,
                            &field->value_size);
}

static sdfio_status find_field(const char *trailer, size_t size,
                               const char *field, const char **value,
                               size_t *value_size)
{
    if (trailer == NULL || field == NULL)
    {
        return SDFIO_ERR_ARGUMENT;
    }
    return sdfi_tagged_find(trailer, size, field, value, value_size)
               ? SDFIO_OK
               : SDFIO_ERR_NOT_FOUND;
}

sdfio_status sdfio_trailer_get_integer(const char *trailer, size_t size,
                                       const char *field, long long *value)
{
    const char *text = NULL;
    size_t text_size = 0;
    const sdfio_status status =
        value == NULL ? SDFIO_ERR_ARGUMENT
                      : find_field(trailer, size, field, &text, &text_size);
    if (status != SDFIO_OK)
    {
        return status;
    }
    return sdfi_parse_integer(text, text_size, value) ? SDFIO_OK
                                                      : SDFIO_ERR_FORMAT;
}

sdfio_status sdfio_trailer_get_real(const char *trailer, size_t size,
                                    const char *field, double *value)
{
    const char *text = NULL;
    size_t text_size = 0;
    const sdfio_status status =
        value == NULL ? SDFIO_ERR_ARGUMENT
                      : find_field(trailer, size, field, &text, &text_size);
    if (status != SDFIO_OK)
    {
        return status;
    }
    const sdfi_point point = sdfi_point_now();
    return sdfi_parse_number(&point, text, text_size, value) ? SDFIO_OK
                                                             : SDFIO_ERR_FORMAT;
}

static bool valid_field_name(const char *name)
{
    if (name[0] == '\0')
    {
        return false;
    }
    for (size_t i = 0; name[i] != '\0'; ++i)
    {
        const char c = sdfi_lower(name[i]);
        if (!sdfi_is_digit(c) && !(c >= 'a' && c <= 'z') && c != '_')
        {
            return false;
        }
    }
    return true;
}

/* A value is one line of 7-bit ASCII. */
static bool valid_value(const char *value)
{
    for (size_t i = 0; value[i] != '\0'; ++i)
    {
        const unsigned char c = (unsigned char)value[i];
        if (c == '\r' || c == '\n' || c >= SDFI_FIRST_NON_ASCII)
        {
            return false;
        }
    }
    return true;
}

sdfio_status sdfio_trailer_put_text(char *buffer, size_t capacity, size_t *size,
                                    const char *field, const char *value)
{
    if (size == NULL || field == NULL || value == NULL
        || (buffer == NULL && capacity > 0) || *size > capacity
        || !valid_field_name(field) || !valid_value(value))
    {
        return SDFIO_ERR_ARGUMENT;
    }
    /* The line and the 0 after it. */
    const size_t line =
        strlen(field) + sizeof separator - 1 + strlen(value) + 1;
    if (line > capacity - *size || capacity - *size - line < 1)
    {
        return SDFIO_ERR_RANGE;
    }
    char *at = buffer + *size;
    memcpy(at, field, strlen(field));
    at += strlen(field);
    memcpy(at, separator, sizeof separator - 1);
    at += sizeof separator - 1;
    memcpy(at, value, strlen(value));
    at += strlen(value);
    *at++ = '\n';
    *at = '\0';
    *size += line;
    return SDFIO_OK;
}

sdfio_status sdfio_trailer_put_integer(char *buffer, size_t capacity,
                                       size_t *size, const char *field,
                                       long long value)
{
    char text[INTEGER_CAPACITY];
    snprintf(text, sizeof text, "%lld", value);
    return sdfio_trailer_put_text(buffer, capacity, size, field, text);
}

sdfio_status sdfio_trailer_put_real(char *buffer, size_t capacity, size_t *size,
                                    const char *field, double value)
{
    if (!isfinite(value))
    {
        return SDFIO_ERR_ARGUMENT;
    }
    const sdfi_point point = sdfi_point_now();
    char text[SDFI_NUMBER_CAPACITY];
    sdfi_format_scientific(&point, text, sizeof text, value, EXACT_DIGITS,
                           SDFI_DOUBLE_EXPONENT);
    return sdfio_trailer_put_text(buffer, capacity, size, field, text);
}

sdfio_status sdfi_check_trailer(sdfio_revision revision, const char *trailer,
                                size_t size, sdfi_message *message)
{
    if (trailer == NULL && size > 0)
    {
        return sdfi_fail(message, SDFIO_ERR_ARGUMENT,
                         "A trailer of %zu bytes needs its text", size);
    }
    for (size_t i = 0; i < size; ++i)
    {
        if ((unsigned char)trailer[i] >= SDFI_FIRST_NON_ASCII)
        {
            return sdfi_fail(message, SDFIO_ERR_ARGUMENT,
                             "The SDF trailer must be 7-bit ASCII");
        }
    }
    if (size > 0 && sdfi_requires_tagged_trailer(revision)
        && !sdfi_tagged_valid(trailer, size))
    {
        return sdfi_fail(message, SDFIO_ERR_ARGUMENT,
                         "The trailer of SDF %s must use the tagged "
                         "'field name = value' form",
                         sdfio_revision_name(revision));
    }
    return SDFIO_OK;
}

sdfio_status sdfio_check_trailer(sdfio_revision revision, const char *trailer,
                                 size_t size)
{
    if ((int)revision < SDFIO_ISO_1_0 || (int)revision > SDFIO_BCR_1_0)
    {
        return SDFIO_ERR_ARGUMENT;
    }
    return sdfi_check_trailer(revision, trailer, size, NULL);
}
