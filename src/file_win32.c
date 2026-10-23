/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file file_win32.c
 * @brief Files of Windows by a path of wide characters.
 *
 * The functions are those of file.c with the functions of the C library for
 * wide paths.
 */

#include <sdfio/file.h>

#include "internal.h"

#include <wchar.h>

sdfio_status sdfio_open_file_w(sdfio_reader *reader, const wchar_t *path)
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

sdfio_status sdfio_create_file_w(sdfio_writer *writer, const wchar_t *path,
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
