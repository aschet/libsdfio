/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file text.h
 * @brief Locale-independent ASCII text helpers.
 */

#ifndef SDFIO_SRC_TEXT_H
#define SDFIO_SRC_TEXT_H

#include <sdfio/sdfio.h>

/**
 * Tells whether a character belongs to 7-bit ASCII, whatever the signedness of
 * `char`.
 *
 * @param c The character.
 */
static inline bool sdfio_is_ascii(char c)
{
    return (unsigned char)c < 0x80;
}

/**
 * Tells whether a character is a printable 7-bit ASCII character, from the
 * space to the tilde, whatever the signedness of `char` and the locale.
 *
 * @param c The character.
 */
static inline bool sdfio_is_printable(char c)
{
    return (unsigned char)c >= (unsigned char)' '
           && (unsigned char)c <= (unsigned char)'~';
}

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
