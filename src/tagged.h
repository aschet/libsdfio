/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief Name = Value lines: splitting and checks.
 */

#ifndef SDFIO_SRC_TAGGED_H
#define SDFIO_SRC_TAGGED_H

#include <stdbool.h>
#include <stddef.h>

/**
 * Splits a line of the form `Name = Value` into its name and its value. The
 * name consists of letters, digits and underscores, and white space around the
 * name, the separator and the value is removed.
 *
 * @param line The line, without its line end.
 * @param size The length of the line.
 * @param[out] name Receives the name.
 * @param[out] name_size Receives the length of the name.
 * @param[out] value Receives the value.
 * @param[out] value_size Receives the length of the value.
 * @return False if the line is not of that form.
 */
bool sdfio_tagged_split(const char *line, size_t size, const char **name,
                        size_t *name_size, const char **value,
                        size_t *value_size);

/**
 * Tells whether a text is the name of a field: letters, digits and `_`.
 *
 * @param name The name, 0-terminated.
 */
bool sdfio_tagged_name_valid(const char *name);

/**
 * Tells whether two names of fields are the same, whatever the case.
 *
 * @param a The first name.
 * @param a_size The number of characters of `a`.
 * @param b The second name.
 * @param b_size The number of characters of `b`.
 */
bool sdfio_tagged_name_equal(const char *a, size_t a_size, const char *b,
                             size_t b_size);

/**
 * Tells whether a text is the value of a field: one line. The encoding is not
 * checked.
 *
 * @param value The value, 0-terminated.
 */
bool sdfio_tagged_value_valid(const char *value);

#endif /* SDFIO_SRC_TAGGED_H */
