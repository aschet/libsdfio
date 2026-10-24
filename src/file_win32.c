/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file file_win32.c
 * @brief File access with wide-character paths on Windows.
 *
 * The functions correspond to those of file.c and open files with the functions
 * of the C library that take wide-character paths.
 */

#include <sdfio/file.h>

#include "reader_file.h"
#include "writer_file.h"

#include <wchar.h>

sdfio_status sdfio_open_wfile(sdfio_reader *reader, const wchar_t *path)
{
    if (reader == NULL || path == NULL)
    {
        return SDFIO_ERR_ARGUMENT;
    }
    const sdfio_status ready = sdfio_file_check_reader(reader);
    if (ready != SDFIO_OK)
    {
        return ready;
    }
    return sdfio_file_adopt_reader(reader, _wfopen(path, L"rb"));
}

sdfio_status sdfio_create_wfile(sdfio_writer *writer, const wchar_t *path,
                                const sdfio_header *header)
{
    if (writer == NULL || path == NULL || header == NULL)
    {
        return SDFIO_ERR_ARGUMENT;
    }
    const sdfio_status ready = sdfio_writer_check(writer, header);
    if (ready != SDFIO_OK)
    {
        return ready;
    }
    return sdfio_file_adopt_writer(writer, _wfopen(path, L"wb"), header);
}
