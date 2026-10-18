/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * Files of the C library. They are apart from <sdfio/sdfio.h> so that a
 * program without a file system, a scanner that sends its data over a
 * link, does not need <stdio.h>.
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
 * A stream over a C file, which stays yours and is not closed. The file has to
 * be open in binary mode. On Windows `stdin` and `stdout` are in text mode,
 * which changes the line ends of what passes and ends the input at a byte
 * 0x1A: switch them before the first use with
 * `_setmode(_fileno(stdin), _O_BINARY)`. The files of sdfio_open_file() and
 * sdfio_create_file() are always binary.
 */
SDFIO_API sdfio_stream sdfio_stdio_stream(FILE *file);

/**
 * Opens the file at `path` and reads its header, as sdfio_open_stream() does
 * for a stream. The reader closes the file when it is freed or when the open
 * fails.
 *
 * @return SDFIO_ERR_IO if the file cannot be opened, and what
 * sdfio_open_stream() returns. On Windows the path is in the code page of the
 * C library.
 */
SDFIO_API sdfio_status sdfio_open_file(sdfio_reader *reader, const char *path);

/**
 * Starts a file at `path` as sdfio_create_stream() does, and writes it there.
 * sdfio_write_trailer() closes it. If the writing fails, what was written
 * stays. A header that cannot be written creates no file.
 */
SDFIO_API sdfio_status sdfio_create_file(sdfio_writer *writer, const char *path,
                                         const sdfio_header *header);

#if defined(_WIN32)
/**
 * The same with a path of wide characters, for the file names of Windows that
 * the code page of the C library cannot hold. Only on Windows.
 */
SDFIO_API sdfio_status sdfio_open_file_w(sdfio_reader *reader,
                                         const wchar_t *path);
SDFIO_API sdfio_status sdfio_create_file_w(sdfio_writer *writer,
                                           const wchar_t *path,
                                           const sdfio_header *header);
#endif

#ifdef __cplusplus
}
#endif

#endif
