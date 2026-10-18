/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

#include "internal.h"

#include <stdio.h>

const char *sdfio_version(void)
{
    return SDFIO_VERSION_STRING;
}

const char *sdfio_status_string(sdfio_status status)
{
    switch (status)
    {
    case SDFIO_OK:
        return "no error";
    case SDFIO_ERR_ARGUMENT:
        return "invalid argument";
    case SDFIO_ERR_STATE:
        return "the call does not fit the state of the handle";
    case SDFIO_ERR_IO:
        return "input or output error";
    case SDFIO_ERR_FORMAT:
        return "not an SDF file, or a damaged one";
    case SDFIO_ERR_UNSUPPORTED:
        return "a feature of the file is not supported";
    case SDFIO_ERR_RANGE:
        return "a number does not fit its type";
    case SDFIO_ERR_BAD_VALUE:
        return "a height equals the marker of a point that is not measured";
    case SDFIO_ERR_NOT_FOUND:
        return "a field was not found";
    case SDFIO_ERR_MEMORY:
        return "out of memory";
    }
    return "unknown status";
}

sdfio_status sdfi_fail(sdfi_message *message, sdfio_status code,
                       const char *format, ...)
{
    /* The analyzer of clang-tidy 21 does not see that va_start initializes. */
    // NOLINTBEGIN(clang-analyzer-valist.Uninitialized)
    va_list arguments;
    va_start(arguments, format);
    if (message != NULL)
    {
        vsnprintf(message->text, sizeof message->text, format, arguments);
    }
    va_end(arguments);
    // NOLINTEND(clang-analyzer-valist.Uninitialized)
    return code;
}

bool sdfi_string_append(sdfi_string *string, const char *text, size_t size)
{
    if (size > SIZE_MAX - string->size - 1)
    {
        return false;
    }
    const size_t needed = string->size + size + 1;
    if (needed > string->capacity)
    {
        size_t capacity =
            string->capacity == 0 ? SDFI_STRING_MINIMUM : string->capacity;
        while (capacity < needed)
        {
            if (capacity > SIZE_MAX / 2)
            {
                capacity = needed;
                break;
            }
            capacity *= 2;
        }
        char *grown = realloc(string->data, capacity);
        if (grown == NULL)
        {
            return false;
        }
        string->data = grown;
        string->capacity = capacity;
    }
    if (size > 0)
    {
        memcpy(string->data + string->size, text, size);
    }
    string->size += size;
    string->data[string->size] = '\0';
    return true;
}

void sdfi_string_clear(sdfi_string *string)
{
    string->size = 0;
    if (string->data != NULL)
    {
        string->data[0] = '\0';
    }
}

void sdfi_string_free(sdfi_string *string)
{
    free(string->data);
    *string = (sdfi_string){0};
}

const char *sdfi_string_text(const sdfi_string *string)
{
    return string->data != NULL ? string->data : "";
}
