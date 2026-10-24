/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file tagged.h
 * @brief Handling of Name = Value fields.
 */

#ifndef SDFIO_SRC_TAGGED_H
#define SDFIO_SRC_TAGGED_H

#include "sink.h"

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
 * Writes a trailer into the file.
 *
 * Every line end of the text, LF, CR or CRLF, is written as CRLF as the
 * standard has it, and the last line is closed by one.
 *
 * @param sink The sink.
 * @param text The trailer.
 * @param size The length of the trailer.
 * @return False if the stream failed.
 */
bool sdfio_write_crlf_lines(sdfio_sink *sink, const char *text, size_t size);

/**
 * Tells whether a text is the name of a field: letters, digits and `_`.
 *
 * @param name The name, 0-terminated.
 */
bool sdfio_tagged_name_valid(const char *name);

/**
 * Tells whether a text is the value of a field: one line of 7-bit ASCII.
 *
 * @param value The value, 0-terminated.
 */
bool sdfio_tagged_value_valid(const char *value);

#endif /* SDFIO_SRC_TAGGED_H */
