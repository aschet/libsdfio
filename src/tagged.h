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

#include "dynstring.h"
#include "sink.h"

/**
 * Converts a trailer from the file: every line end becomes LF, and the end of
 * the last line is dropped.
 *
 * @param[in,out] text The trailer.
 */
void sdfio_lines_to_lf(sdfio_string *text);

/**
 * Tells whether every line of a text is blank or of the form `Name = Value`.
 *
 * @param text The text.
 * @param size The length of the text.
 */
bool sdfio_tagged_valid(const char *text, size_t size);

/**
 * Finds the field of a name.
 *
 * @param text The text.
 * @param size The length of the text.
 * @param name The name, 0-terminated.
 * @param[out] value Receives the value, trimmed.
 * @param[out] value_size Receives the length of the value.
 * @return False if there is no such field.
 */
bool sdfio_tagged_find(const char *text, size_t size, const char *name,
                       const char **value, size_t *value_size);

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
