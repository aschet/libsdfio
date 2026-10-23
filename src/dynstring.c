/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file dynstring.c
 * @brief Growable text.
 */

#include "dynstring.h"
#include "allocation.h"
#include <stdint.h>
#include <string.h>

enum
{
    /* The room that a string starts with. */
    SDFIO_STRING_MINIMUM = 64
};

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
