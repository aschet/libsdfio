/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file common.h
 * @brief What every part of the library uses: the checks at compile time, the
 * text of a failure, the allocation through the allocator of a handle, and the
 * growable string.
 *
 * Not part of the interface: the library is built with hidden symbols. The
 * tests link the objects and call the functions directly.
 */

#ifndef SDFIO_SRC_COMMON_H
#define SDFIO_SRC_COMMON_H

#include <sdfio/sdfio.h>

#include <float.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

/**
 * A check at compile time, where the compiler has C11: without it the check is
 * not made. The expansion without C11 is a declaration that can repeat, so that
 * the use ends with a semicolon in both cases.
 *
 * @param condition The condition that has to hold.
 * @param message The text of the error if it does not.
 */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define SDFIO_STATIC_ASSERT(condition, message)                                \
    _Static_assert(condition, message)
#else
#define SDFIO_STATIC_ASSERT(condition, message) extern int sdfio_not_checked
#endif

/* What the format needs of the platform, checked here and nowhere else: bytes
 * of 8 bits, and IEEE 754 binary32 and binary64 for float and double. */
SDFIO_STATIC_ASSERT(CHAR_BIT == 8, "the format counts in 8-bit bytes");
SDFIO_STATIC_ASSERT(sizeof(float) == 4 && FLT_RADIX == 2 && FLT_MANT_DIG == 24,
                    "float has to be IEEE 754 binary32");
SDFIO_STATIC_ASSERT(sizeof(double) == 8 && DBL_MANT_DIG == 53,
                    "double has to be IEEE 754 binary64");

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
