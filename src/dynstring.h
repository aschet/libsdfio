/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file dynstring.h
 * @brief Growable text.
 */

#ifndef SDFIO_SRC_DYNSTRING_H
#define SDFIO_SRC_DYNSTRING_H

#include <sdfio/sdfio.h>

/** A growable text, always 0-terminated once something is in it. */
typedef struct sdfio_string
{
    /** The characters, or null while the string is empty. */
    char *data;
    /** The length without the terminator. */
    size_t size;
    /** The bytes that `data` holds. */
    size_t capacity;
    /** Where the memory comes from: null is malloc and free. */
    const sdfio_allocator *allocator;
} sdfio_string;

/**
 * Appends text to a string.
 *
 * @param string The string.
 * @param text The characters to append.
 * @param size The number of characters.
 * @return False if there is no memory. The string is not changed then.
 */
bool sdfio_string_append(sdfio_string *string, const char *text, size_t size);

/**
 * Empties a string and keeps its memory.
 *
 * @param string The string.
 */
void sdfio_string_clear(sdfio_string *string);

/**
 * Releases the memory of a string.
 *
 * @param string The string.
 */
void sdfio_string_free(sdfio_string *string);

/**
 * Returns the characters of a string.
 *
 * @param string The string.
 * @return A 0-terminated text, also for an empty string.
 */
const char *sdfio_string_text(const sdfio_string *string);

#endif /* SDFIO_SRC_DYNSTRING_H */
