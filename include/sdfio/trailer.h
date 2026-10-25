/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * Fields of a trailer text.
 */

#ifndef SDFIO_TRAILER_H
#define SDFIO_TRAILER_H

#include <sdfio/sdfio_export.h>

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * Finds the next field of a trailer text, a line of the form `Name = Value`,
 * which ISO-2.0 expects. The text is the text that sdfio_read_trailer()
 * returned, or any text with LF or CRLF as line ends. The lines that are not
 * fields are skipped.
 *
 * The name consists of letters, digits and `_`. White space around the name,
 * the separator and the value is not part of them. A value ends at the end of
 * the line, so it can contain `=`, and it can be empty. The name and the value
 * are given as offsets from the start of `text` and as lengths, so the bytes
 * are `text + name_at` and `text + value_at`. The text is bytes, not
 * terminated, and its encoding is not checked.
 *
 * @param[in] text The text of the trailer.
 * @param size The length of the text in bytes.
 * @param[in,out] position The offset of the line to start with: 0 for the
 * start of the text. The function sets it behind the line that it found, so the
 * next call continues there.
 * @param[out] name_at Receives the offset of the name.
 * @param[out] name_size Receives the length of the name.
 * @param[out] value_at Receives the offset of the value.
 * @param[out] value_size Receives the length of the value.
 * @return True if a field was found. False at the end of the text, or if a
 * pointer is null.
 */
SDFIO_API bool sdfio_trailer_next_field(const char *text, size_t size,
                                        size_t *position, size_t *name_at,
                                        size_t *name_size, size_t *value_at,
                                        size_t *value_size);

/**
 * Finds the value of the field of a name in a trailer text. The name is
 * compared without regard to case, as the standard has it for field names. If
 * the text has several fields of the name, the first one counts. The rules of
 * sdfio_trailer_next_field() apply.
 *
 * @param[in] text The text of the trailer.
 * @param size The length of the text in bytes.
 * @param[in] name The name of the field.
 * @param[out] value_at Receives the offset of the value from the start of
 * `text`, or 0 if there is no such field.
 * @param[out] value_size Receives the length of the value, or 0.
 * @return True if the field was found. False if there is none, or if a pointer
 * is null.
 */
SDFIO_API bool sdfio_trailer_find_field(const char *text, size_t size,
                                        const char *name, size_t *value_at,
                                        size_t *value_size);

#ifdef __cplusplus
}
#endif

#endif /* SDFIO_TRAILER_H */
