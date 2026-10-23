/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file common.h
 * @brief Shared facilities: platform checks, errors, allocation and strings.
 */

#ifndef SDFIO_SRC_COMMON_H
#define SDFIO_SRC_COMMON_H

#include <sdfio/sdfio.h>

#include <float.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

/* What the format needs of the platform, checked here and nowhere else: bytes
 * of 8 bits, and IEEE 754 binary32 and binary64 for float and double. The
 * integer types of the data area have exact widths, which the standard
 * guarantees for the types of <stdint.h>. */
_Static_assert(CHAR_BIT == 8, "the format counts in 8-bit bytes");
_Static_assert(sizeof(float) == 4 && FLT_RADIX == 2 && FLT_MANT_DIG == 24,
               "float has to be IEEE 754 binary32");
_Static_assert(sizeof(double) == 8 && DBL_MANT_DIG == 53,
               "double has to be IEEE 754 binary64");

/** The size of the largest sample of the data area, a binary64. */
enum
{
    SDFIO_LARGEST_SAMPLE = 8
};
_Static_assert(SDFIO_LARGEST_SAMPLE == sizeof(double),
               "the largest sample has to be a binary64");

/**
 * The text of the last failure of a handle.
 *
 * The text is a string of the library, which stays valid, so a handle holds no
 * memory for it.
 */
typedef struct sdfio_message
{
    /** The text, or null if there was no failure. */
    const char *text;
} sdfio_message;

/**
 * Records the text of a failure and returns its code.
 *
 * @param[out] message Receives the text. May be null.
 * @param code The status to return.
 * @param text The text, a string that stays valid.
 * @return `code`.
 */
static inline sdfio_status sdfio_fail(sdfio_message *message, sdfio_status code,
                                      const char *text)
{
    if (message != NULL)
    {
        message->text = text;
    }
    return code;
}

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
 * Allocates memory through the allocator of a configuration.
 *
 * @param allocator The allocator. If it is null or has no allocate function,
 * malloc is used.
 * @param size The size in bytes.
 * @return The memory, or null if there is none.
 */
void *sdfio_allocate(const sdfio_allocator *allocator, size_t size);

/**
 * Releases memory from sdfio_allocate().
 *
 * @param allocator The allocator that made the memory.
 * @param memory The memory. May be null.
 */
void sdfio_release(const sdfio_allocator *allocator, void *memory);

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

#endif /* SDFIO_SRC_COMMON_H */
