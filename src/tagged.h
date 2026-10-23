/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file tagged.h
 * @brief The lines of a trailer and the fields `Name = Value` in them.
 *
 * Not part of the interface: the library is built with hidden symbols. The
 * tests link the objects and call the functions directly.
 */

#ifndef SDFIO_SRC_TAGGED_H
#define SDFIO_SRC_TAGGED_H

#include "stream.h"

/**
 * Converts a trailer from the file: every line end becomes LF, and the end of
 * the last line is dropped.
 *
 * @param[in,out] text The trailer.
 */
void sdfio_lines_to_lf(sdfio_string *text);

/**
 * Finds the next line of a text, ended by LF, CR or CRLF.
 *
 * @param text The text.
 * @param size The length of the text.
 * @param[in,out] position Where to start, and where the next line starts.
 * @param[out] line Receives the line without its end.
 * @param[out] line_size Receives the length of the line.
 * @return False if there is no line left.
 */
bool sdfio_next_line(const char *text, size_t size, size_t *position,
                     const char **line, size_t *line_size);

/**
 * Tells whether every line of a text is blank or of the form `Name = Value`.
 *
 * @param text The text.
 * @param size The length of the text.
 */
bool sdfio_tagged_valid(const char *text, size_t size);

/**
 * Finds the next line that is a field.
 *
 * @param text The text.
 * @param size The length of the text.
 * @param[in,out] position Where to start, and where the search goes on.
 * @param[out] name Receives the name, trimmed.
 * @param[out] name_size Receives the length of the name.
 * @param[out] value Receives the value, trimmed.
 * @param[out] value_size Receives the length of the value.
 * @return False if there is no field left.
 */
bool sdfio_tagged_next(const char *text, size_t size, size_t *position,
                       const char **name, size_t *name_size, const char **value,
                       size_t *value_size);

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
 * Appends a trailer to a string, with the line ends of
 * sdfio_write_crlf_lines().
 *
 * @param[in,out] out The string.
 * @param text The trailer.
 * @param size The length of the trailer.
 * @return False if there is no memory.
 */
bool sdfio_append_crlf_lines(sdfio_string *out, const char *text, size_t size);

#endif /* SDFIO_SRC_TAGGED_H */
