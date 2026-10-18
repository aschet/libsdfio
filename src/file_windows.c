/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* Files of Windows by a path of wide characters. The functions are those of
 * file.c with the functions of the C library for wide paths. */

#include <sdfio/file.h>

#include "internal.h"

#include <wchar.h>

typedef struct wide_reading
{
    FILE *file;
} wide_reading;

static void close_reading(void *user)
{
    wide_reading *state = user;
    fclose(state->file);
    free(state);
}

sdfio_status sdfio_open_file_w(sdfio_reader *reader, const wchar_t *path)
{
    if (reader == NULL || path == NULL)
    {
        return SDFIO_ERR_ARGUMENT;
    }
    if (sdfio_reader_is_open(reader))
    {
        return sdfio_fail(sdfio_reader_message(reader), SDFIO_ERR_STATE,
                          "The reader is open already");
    }
    FILE *file = _wfopen(path, L"rb");
    if (file == NULL)
    {
        return sdfio_fail(sdfio_reader_message(reader), SDFIO_ERR_IO,
                          "The file cannot be opened");
    }
    wide_reading *state = malloc(sizeof *state);
    if (state == NULL)
    {
        fclose(file);
        return sdfio_fail(sdfio_reader_message(reader), SDFIO_ERR_MEMORY,
                          "Out of memory");
    }
    state->file = file;
    const sdfio_stream stream = sdfio_stdio_stream(file);
    const sdfio_status status = sdfio_open_stream(reader, &stream);
    if (status != SDFIO_OK)
    {
        close_reading(state);
        return status;
    }
    sdfio_reader_own(reader, (sdfio_owner){NULL, close_reading, state});
    return SDFIO_OK;
}

/* A file is written beside its place and moved there when it is whole. */
typedef struct wide_writing
{
    FILE *file;
    wchar_t *path;
    wchar_t *temporary_path;
} wide_writing;

static wchar_t *copy_text(const wchar_t *text, const wchar_t *suffix)
{
    const size_t size = wcslen(text);
    const size_t suffix_size = wcslen(suffix);
    wchar_t *copy = malloc((size + suffix_size + 1) * sizeof *copy);
    if (copy != NULL)
    {
        wmemcpy(copy, text, size);
        wmemcpy(copy + size, suffix, suffix_size + 1);
    }
    return copy;
}

/* A file that was not moved to its place is removed. */
static void abandon_writing(void *user)
{
    wide_writing *state = user;
    if (state == NULL)
    {
        return;
    }
    if (state->file != NULL)
    {
        fclose(state->file);
        _wremove(state->temporary_path);
    }
    free(state->path);
    free(state->temporary_path);
    free(state);
}

static sdfio_status finish_writing(void *user, sdfio_message *message)
{
    wide_writing *state = user;
    const bool flushed = fflush(state->file) == 0;
    const bool closed = fclose(state->file) == 0;
    state->file = NULL;
    if (!flushed || !closed)
    {
        _wremove(state->temporary_path);
        return sdfio_fail(message, SDFIO_ERR_IO,
                          "The file could not be written");
    }
    if (_wrename(state->temporary_path, state->path) != 0)
    {
        /* Windows does not replace a file that exists. */
        _wremove(state->path);
        if (_wrename(state->temporary_path, state->path) != 0)
        {
            _wremove(state->temporary_path);
            return sdfio_fail(message, SDFIO_ERR_IO,
                              "The file could not be moved to its place");
        }
    }
    return SDFIO_OK;
}

sdfio_status sdfio_create_file_w(sdfio_writer *writer, const wchar_t *path,
                                 const sdfio_header *header)
{
    if (writer == NULL || path == NULL || header == NULL)
    {
        return SDFIO_ERR_ARGUMENT;
    }
    if (!sdfio_writer_is_new(writer))
    {
        return sdfio_fail(sdfio_writer_message(writer), SDFIO_ERR_STATE,
                          "The writer has created a file already");
    }
    wide_writing *state = calloc(1, sizeof *state);
    if (state != NULL)
    {
        state->path = copy_text(path, L"");
        state->temporary_path = copy_text(path, L".sdftmp");
    }
    if (state == NULL || state->path == NULL || state->temporary_path == NULL)
    {
        abandon_writing(state);
        return sdfio_fail(sdfio_writer_message(writer), SDFIO_ERR_MEMORY,
                          "Out of memory");
    }
    state->file = _wfopen(state->temporary_path, L"wb");
    if (state->file == NULL)
    {
        abandon_writing(state);
        return sdfio_fail(sdfio_writer_message(writer), SDFIO_ERR_IO,
                          "The file cannot be written");
    }
    const sdfio_stream stream = sdfio_stdio_stream(state->file);
    const sdfio_status status = sdfio_create_stream(writer, &stream, header);
    if (status != SDFIO_OK)
    {
        abandon_writing(state);
        return status;
    }
    sdfio_writer_own(writer,
                     (sdfio_owner){finish_writing, abandon_writing, state});
    return SDFIO_OK;
}
