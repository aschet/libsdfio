/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file file.c
 * @brief File access through the C library.
 */

#include <sdfio/file.h>

#include "message.h"
#include "reader_file.h"
#include "writer_file.h"
#include <sdfio/sdfio.h>

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

sdfio_stream sdfio_stdio_stream(FILE *file)
{
    sdfio_stream stream = {0};
    stream.user = file;
    stream.read = stdio_read;
    stream.write = stdio_write;
    return stream;
}

static void stdio_close(void *user)
{
    fclose(user);
}

/* Flushes and closes the file that a writer wrote. */
static sdfio_status stdio_finish(void *user, sdfio_message *message)
{
    FILE *file = user;
    const bool flushed = fflush(file) == 0;
    const bool closed = fclose(file) == 0;
    if (!flushed || !closed)
    {
        return sdfio_fail(message, SDFIO_ERR_IO,
                          "The file could not be written");
    }
    return SDFIO_OK;
}

sdfio_status sdfio_file_check_reader(sdfio_reader *reader)
{
    if (sdfio_reader_is_open(reader))
    {
        return sdfio_fail(sdfio_reader_message(reader), SDFIO_ERR_STATE,
                          "The reader is open already");
    }
    return SDFIO_OK;
}

sdfio_status sdfio_file_adopt_reader(sdfio_reader *reader, FILE *file)
{
    if (file == NULL)
    {
        return sdfio_fail(sdfio_reader_message(reader), SDFIO_ERR_IO,
                          "The file cannot be opened");
    }
    const sdfio_stream stream = sdfio_stdio_stream(file);
    const sdfio_status status = sdfio_open_stream(reader, &stream);
    if (status != SDFIO_OK)
    {
        fclose(file);
        return status;
    }
    sdfio_reader_own(reader, (sdfio_owner){NULL, stdio_close, file});
    return SDFIO_OK;
}

sdfio_status sdfio_file_adopt_writer(sdfio_writer *writer, FILE *file,
                                     const sdfio_header *header)
{
    if (file == NULL)
    {
        return sdfio_fail(sdfio_writer_message(writer), SDFIO_ERR_IO,
                          "The file cannot be written");
    }
    const sdfio_stream stream = sdfio_stdio_stream(file);
    const sdfio_status status = sdfio_create_stream(writer, &stream, header);
    if (status != SDFIO_OK)
    {
        fclose(file);
        return status;
    }
    sdfio_writer_own(writer, (sdfio_owner){stdio_finish, stdio_close, file});
    return SDFIO_OK;
}

sdfio_status sdfio_open_file(sdfio_reader *reader, const char *path)
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
    return sdfio_file_adopt_reader(reader, fopen(path, "rb"));
}

sdfio_status sdfio_create_file(sdfio_writer *writer, const char *path,
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
    return sdfio_file_adopt_writer(writer, fopen(path, "wb"), header);
}
