/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* The bytes of a file: the stdio stream, a buffered source and a buffered
 * sink over the callbacks of sdf_stream. */

#include "internal.h"

static size_t stdio_read(void *user, void *data, size_t size)
{
    FILE *file = user;
    const size_t count = fread(data, 1, size, file);
    if (count < size && ferror(file) != 0)
    {
        return SDF_STREAM_ERROR;
    }
    return count;
}

static size_t stdio_write(void *user, const void *data, size_t size)
{
    return fwrite(data, 1, size, (FILE *)user);
}

static int64_t stdio_seek(void *user, int64_t offset, int whence)
{
    FILE *file = user;
#if defined(_WIN32)
    if (_fseeki64(file, offset, whence) != 0)
    {
        return -1;
    }
    return _ftelli64(file);
#else
    if (fseeko(file, (off_t)offset, whence) != 0)
    {
        return -1;
    }
    return (int64_t)ftello(file);
#endif
}

sdf_stream sdf_stdio_stream(FILE *file)
{
    sdf_stream stream = {0};
    stream.user = file;
    stream.read = stdio_read;
    stream.write = stdio_write;
    stream.seek = stdio_seek;
    return stream;
}

void sdfi_source_init(sdfi_source *source, const sdf_stream *io)
{
    source->io = *io;
    source->position = 0;
    source->end = 0;
    source->base = 0;
    source->at_end = false;
    source->failed = false;
    source->skip_lf = false;
    source->seekable = false;
    if (io->seek != NULL)
    {
        /* A stream that answers where it is can seek. */
        const int64_t here = io->seek(io->user, 0, SEEK_CUR);
        if (here >= 0)
        {
            source->seekable = true;
            source->base = here;
        }
    }
}

size_t sdfi_source_fill(sdfi_source *source, size_t want)
{
    if (want > SDFI_SOURCE_BUFFER)
    {
        want = SDFI_SOURCE_BUFFER;
    }
    if (source->end - source->position >= want)
    {
        return source->end - source->position;
    }
    /* What was consumed is gone from the buffer. */
    if (source->position > 0)
    {
        memmove(source->buffer, source->buffer + source->position,
                source->end - source->position);
        source->base += (int64_t)source->position;
        source->end -= source->position;
        source->position = 0;
    }
    while (source->end < want && !source->at_end && !source->failed)
    {
        const size_t count =
            source->io.read(source->io.user, source->buffer + source->end,
                            SDFI_SOURCE_BUFFER - source->end);
        if (count == SDF_STREAM_ERROR)
        {
            source->failed = true;
        }
        else if (count == 0)
        {
            source->at_end = true;
        }
        else
        {
            source->end += count;
        }
    }
    return source->end - source->position;
}

int sdfi_source_peek(sdfi_source *source)
{
    if (source->position == source->end && sdfi_source_fill(source, 1) == 0)
    {
        return -1;
    }
    return source->buffer[source->position];
}

int sdfi_source_getc(sdfi_source *source)
{
    const int c = sdfi_source_peek(source);
    if (c >= 0)
    {
        ++source->position;
    }
    return c;
}

size_t sdfi_source_read(sdfi_source *source, void *dst, size_t size)
{
    unsigned char *out = dst;
    size_t done = 0;
    while (done < size)
    {
        const size_t buffered = source->end - source->position;
        if (buffered > 0)
        {
            const size_t count =
                buffered < size - done ? buffered : size - done;
            memcpy(out + done, source->buffer + source->position, count);
            source->position += count;
            done += count;
            continue;
        }
        if (source->at_end || source->failed)
        {
            break;
        }
        if (size - done >= SDFI_SOURCE_BUFFER)
        {
            /* A large piece goes straight to its place. */
            source->base += (int64_t)source->end;
            source->position = 0;
            source->end = 0;
            const size_t count =
                source->io.read(source->io.user, out + done, size - done);
            if (count == SDF_STREAM_ERROR)
            {
                source->failed = true;
                break;
            }
            if (count == 0)
            {
                source->at_end = true;
                break;
            }
            source->base += (int64_t)count;
            done += count;
            continue;
        }
        if (sdfi_source_fill(source, 1) == 0)
        {
            break;
        }
    }
    return done;
}

int64_t sdfi_source_tell(const sdfi_source *source)
{
    return source->base + (int64_t)source->position;
}

bool sdfi_source_seek(sdfi_source *source, int64_t position)
{
    if (!source->seekable
        || source->io.seek(source->io.user, position, SEEK_SET) != position)
    {
        return false;
    }
    source->position = 0;
    source->end = 0;
    source->base = position;
    source->at_end = false;
    source->failed = false;
    source->skip_lf = false;
    return true;
}

int64_t sdfi_source_size(sdfi_source *source)
{
    if (!source->seekable)
    {
        return -1;
    }
    const int64_t here = sdfi_source_tell(source);
    const int64_t size = source->io.seek(source->io.user, 0, SEEK_END);
    if (size < 0 || !sdfi_source_seek(source, here))
    {
        return -1;
    }
    return size;
}

void sdfi_sink_init(sdfi_sink *sink, const sdf_stream *io)
{
    sink->io = *io;
    sink->used = 0;
    sink->failed = false;
}

bool sdfi_sink_flush(sdfi_sink *sink)
{
    if (sink->used > 0 && !sink->failed)
    {
        if (sink->io.write == NULL
            || sink->io.write(sink->io.user, sink->buffer, sink->used)
                   != sink->used)
        {
            sink->failed = true;
        }
    }
    sink->used = 0;
    return !sink->failed;
}

bool sdfi_sink_write(sdfi_sink *sink, const void *data, size_t size)
{
    if (sink->failed)
    {
        return false;
    }
    if (size >= SDFI_SINK_BUFFER)
    {
        if (!sdfi_sink_flush(sink))
        {
            return false;
        }
        if (sink->io.write == NULL
            || sink->io.write(sink->io.user, data, size) != size)
        {
            sink->failed = true;
        }
        return !sink->failed;
    }
    if (size > SDFI_SINK_BUFFER - sink->used && !sdfi_sink_flush(sink))
    {
        return false;
    }
    memcpy(sink->buffer + sink->used, data, size);
    sink->used += size;
    return true;
}
