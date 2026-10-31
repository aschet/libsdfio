/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief Locale-independent tests and conversions of ASCII characters.
 */

#ifndef SDFIO_SRC_TEXT_H
#define SDFIO_SRC_TEXT_H

#include <sdfio/sdfio.h>

/**
 * Tells whether a character is white space, whatever the locale: a space, a
 * tab, a line feed, and the carriage return of a CRLF.
 *
 * The function is inline because the parsers call it for every character.
 *
 * @param c The character.
 */
static inline bool sdfio_is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

/**
 * Returns a character in lower case, whatever the locale. Only the letters A to
 * Z change.
 *
 * @param c The character.
 */
static inline char sdfio_lower(char c)
{
    return c >= 'A' && c <= 'Z' ? (char)(c + ('a' - 'A')) : c;
}

/**
 * Removes white space from both ends of a text.
 *
 * @param[in,out] text The start of the text.
 * @param[in,out] size The length of the text.
 */
void sdfio_trim(const char **text, size_t *size);

#endif /* SDFIO_SRC_TEXT_H */
