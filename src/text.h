/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file text.h
 * @brief The characters of text, whatever the locale.
 */

#ifndef SDFIO_SRC_TEXT_H
#define SDFIO_SRC_TEXT_H

#include "common.h"

/** The first number that is not a character of ASCII. */
enum
{
    SDFIO_FIRST_NON_ASCII = 0x80
};

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
 * Tells whether a character is a digit from 0 to 9, whatever the locale.
 *
 * @param c The character.
 */
static inline bool sdfio_is_digit(char c)
{
    return c >= '0' && c <= '9';
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
 * Compares a text with a word in lower case, without regard to case.
 *
 * @param text The text.
 * @param size The length of the text.
 * @param lower The word, in lower case and 0-terminated.
 */
bool sdfio_equals_nocase(const char *text, size_t size, const char *lower);

/**
 * Removes white space from both ends of a text.
 *
 * @param[in,out] text The start of the text.
 * @param[in,out] size The length of the text.
 */
void sdfio_trim(const char **text, size_t *size);

#endif /* SDFIO_SRC_TEXT_H */
