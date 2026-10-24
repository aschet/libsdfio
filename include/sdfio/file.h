/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * Files of the C library.
 *
 * The functions use `<stdio.h>`. `<sdfio/sdfio.h>` does not include this
 * header.
 */

#ifndef SDFIO_FILE_H
#define SDFIO_FILE_H

#include <stdio.h>
#include <wchar.h>

#include "sdfio/sdfio.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * Creates a stream over a C file. The file remains owned by the caller and is
 * not closed.
 *
 * The file must be open in binary mode. On Windows `stdin` and `stdout` are in
 * text mode, which translates line ends and treats the byte 0x1A as the end of
 * the input. Switch them before the first use with
 * `_setmode(_fileno(stdin), _O_BINARY)`. The files of sdfio_open_file() and
 * sdfio_create_file() are always binary.
 *
 * @param file The C file.
 * @return A stream whose `user` is the file.
 */
SDFIO_API sdfio_stream sdfio_stdio_stream(FILE *file);

/**
 * Opens the file at a path as the source and reads its header, as
 * sdfio_open_stream() does for a stream. The reader closes the file when it is
 * freed or when the open fails.
 *
 * @param reader The reader.
 * @param[in] path The path of the file.
 * @retval SDFIO_ERR_ARGUMENT A pointer is null.
 * @retval SDFIO_ERR_IO The file cannot be opened.
 * @return Otherwise the result of sdfio_open_stream().
 */
SDFIO_API sdfio_status sdfio_open_file(sdfio_reader *reader, const char *path);

/**
 * Creates the file at a path and starts it as sdfio_create_stream() does. An
 * existing file is truncated. The data is written in place, and
 * sdfio_end_file() closes the file. If the writing fails, the data written
 * so far remains. A header that cannot be written creates no file.
 *
 * @param writer The writer.
 * @param[in] path The path of the file.
 * @param[in] header The description of the file.
 * @retval SDFIO_ERR_ARGUMENT A pointer is null or the header cannot be
 * written.
 * @retval SDFIO_ERR_STATE The writer has created a file already.
 * @retval SDFIO_ERR_IO The file cannot be created.
 * @return Otherwise the result of sdfio_create_stream().
 */
SDFIO_API sdfio_status sdfio_create_file(sdfio_writer *writer, const char *path,
                                         const sdfio_header *header);

#if defined(_WIN32)
/**
 * Variant of sdfio_open_file() with a path of wide characters.
 *
 * @param reader The reader.
 * @param[in] path The path of the file.
 */
SDFIO_API sdfio_status sdfio_open_wfile(sdfio_reader *reader,
                                        const wchar_t *path);

/**
 * Variant of sdfio_create_file() with a path of wide characters.
 *
 * @param writer The writer.
 * @param[in] path The path of the file.
 * @param[in] header The description of the file.
 */
SDFIO_API sdfio_status sdfio_create_wfile(sdfio_writer *writer,
                                          const wchar_t *path,
                                          const sdfio_header *header);
#endif

#ifdef __cplusplus
}
#endif

#endif
