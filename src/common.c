/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file common.c
 * @brief The version, the names of the status codes, the allocation through
 * a configuration, and the growable string.
 */

#include "common.h"

#include <stdio.h>

enum
{
    /** The room that a string starts with. */
    SDFIO_STRING_MINIMUM = 64
};

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
    case SDFIO_ERR_MEMORY:
        return "out of memory";
    }
    return "unknown status";
}

void *sdfio_allocate(const sdfio_allocator *allocator, size_t size)
{
    if (allocator != NULL && allocator->allocate != NULL)
    {
        return allocator->allocate(allocator->user, size);
    }
    return malloc(size);
}

void sdfio_release(const sdfio_allocator *allocator, void *memory)
{
    if (memory == NULL)
    {
        return;
    }
    if (allocator != NULL && allocator->release != NULL)
    {
        allocator->release(allocator->user, memory);
    }
    else
    {
        free(memory);
    }
}

bool sdfio_string_append(sdfio_string *string, const char *text, size_t size)
{
    if (size > SIZE_MAX - string->size - 1)
    {
        return false;
    }
    const size_t needed = string->size + size + 1;
    if (needed > string->capacity)
    {
        size_t capacity =
            string->capacity == 0 ? SDFIO_STRING_MINIMUM : string->capacity;
        while (capacity < needed)
        {
            if (capacity > SIZE_MAX / 2)
            {
                capacity = needed;
                break;
            }
            capacity *= 2;
        }
        char *grown = sdfio_allocate(string->allocator, capacity);
        if (grown == NULL)
        {
            return false;
        }
        if (string->size > 0)
        {
            memcpy(grown, string->data, string->size);
        }
        sdfio_release(string->allocator, string->data);
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

void sdfio_string_clear(sdfio_string *string)
{
    string->size = 0;
    if (string->data != NULL)
    {
        string->data[0] = '\0';
    }
}

void sdfio_string_free(sdfio_string *string)
{
    sdfio_release(string->allocator, string->data);
    *string = (sdfio_string){.allocator = string->allocator};
}

const char *sdfio_string_text(const sdfio_string *string)
{
    return string->data != NULL ? string->data : "";
}
