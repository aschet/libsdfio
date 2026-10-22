/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file file.c
 * @brief Files of the C library: the stdio stream, and a reader and a writer
 * that own a file.
 */

#include <sdfio/file.h>

#include "internal.h"

static size_t stdio_read(void *user, void *data, size_t size)
{
    FILE *file = user;
    const size_t count = fread(data, 1, size, file);
    if (count < size && ferror(file) != 0)
    {
        return SDFIO_STREAM_ERROR;
    }
    return count;
}

static size_t stdio_write(void *user, const void *data, size_t size)
{
    return fwrite(data, 1, size, (FILE *)user);
}

static uint64_t stdio_seek(void *user, uint64_t offset, int whence)
{
    FILE *file = user;
#if defined(_WIN32)
    if (offset > INT64_MAX || _fseeki64(file, (int64_t)offset, whence) != 0)
    {
        return SDFIO_SEEK_ERROR;
    }
    const int64_t position = _ftelli64(file);
#else
    /* Standard C seeks by a long: a file or an offset beyond what a long
     * holds cannot be addressed, which is the case above 2 GiB where long has
     * 32 bits. */
    if (offset > LONG_MAX || fseek(file, (long)offset, whence) != 0)
    {
        return SDFIO_SEEK_ERROR;
    }
    const long position = ftell(file);
#endif
    return position < 0 ? SDFIO_SEEK_ERROR : (uint64_t)position;
}

sdfio_stream sdfio_stdio_stream(FILE *file)
{
    sdfio_stream stream = {0};
    stream.user = file;
    stream.read = stdio_read;
    stream.write = stdio_write;
    stream.seek = stdio_seek;
    return stream;
}

/** The file that a reader opened and closes. */
typedef struct reading_file
{
    /** The file. */
    FILE *file;
} reading_file;

static void close_reading(void *user)
{
    reading_file *state = user;
    fclose(state->file);
    free(state);
}

sdfio_status sdfio_open_file(sdfio_reader *reader, const char *path)
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
    FILE *file = fopen(path, "rb");
    if (file == NULL)
    {
        return sdfio_fail(sdfio_reader_message(reader), SDFIO_ERR_IO,
                          "The file cannot be opened");
    }
    reading_file *state = malloc(sizeof *state);
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

/** The file that a writer created and closes. */
typedef struct writing_file
{
    /** The file. */
    FILE *file;
} writing_file;

/* A file that was not ended is closed as it is. */
static void close_writing(void *user)
{
    writing_file *state = user;
    if (state->file != NULL)
    {
        fclose(state->file);
    }
    free(state);
}

static sdfio_status finish_writing(void *user, sdfio_message *message)
{
    writing_file *state = user;
    const bool flushed = fflush(state->file) == 0;
    const bool closed = fclose(state->file) == 0;
    state->file = NULL;
    if (!flushed || !closed)
    {
        return sdfio_fail(message, SDFIO_ERR_IO,
                          "The file could not be written");
    }
    return SDFIO_OK;
}

sdfio_status sdfio_create_file(sdfio_writer *writer, const char *path,
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
    /* A header that cannot be written does not create a file. */
    const sdfio_status valid =
        sdfio_validate(header, sdfio_writer_message(writer));
    if (valid != SDFIO_OK)
    {
        return valid;
    }
    writing_file *state = malloc(sizeof *state);
    if (state == NULL)
    {
        return sdfio_fail(sdfio_writer_message(writer), SDFIO_ERR_MEMORY,
                          "Out of memory");
    }
    state->file = fopen(path, "wb");
    if (state->file == NULL)
    {
        free(state);
        return sdfio_fail(sdfio_writer_message(writer), SDFIO_ERR_IO,
                          "The file cannot be written");
    }
    const sdfio_stream stream = sdfio_stdio_stream(state->file);
    const sdfio_status status = sdfio_create_stream(writer, &stream, header);
    if (status != SDFIO_OK)
    {
        close_writing(state);
        return status;
    }
    sdfio_writer_own(writer,
                     (sdfio_owner){finish_writing, close_writing, state});
    return SDFIO_OK;
}
