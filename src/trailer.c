/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* Assembling a trailer: the lines `field name = value` in memory of the caller.
 */

#include "internal.h"

/* " = " between the name and the value. */
static const char separator[] = " = ";

void sdfio_trailer_init(sdfio_trailer *trailer, char *memory, size_t capacity)
{
    if (trailer == NULL)
    {
        return;
    }
    trailer->text = memory;
    trailer->size = 0;
    trailer->capacity = memory != NULL ? capacity : 0;
    if (trailer->capacity > 0)
    {
        memory[0] = '\0';
    }
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

sdfio_status sdfio_trailer_add_field(sdfio_trailer *trailer, const char *field,
                                     const char *value)
{
    if (trailer == NULL || field == NULL || value == NULL
        || (trailer->text == NULL && trailer->capacity > 0)
        || trailer->size > trailer->capacity || !valid_field_name(field)
        || !valid_value(value))
    {
        return SDFIO_ERR_ARGUMENT;
    }
    /* The line and the 0 after it. */
    const size_t line =
        strlen(field) + sizeof separator - 1 + strlen(value) + 1;
    const size_t room = trailer->capacity - trailer->size;
    if (line > room || room - line < 1)
    {
        return SDFIO_ERR_RANGE;
    }
    char *at = trailer->text + trailer->size;
    memcpy(at, field, strlen(field));
    at += strlen(field);
    memcpy(at, separator, sizeof separator - 1);
    at += sizeof separator - 1;
    memcpy(at, value, strlen(value));
    at += strlen(value);
    *at++ = '\n';
    *at = '\0';
    trailer->size += line;
    return SDFIO_OK;
}
