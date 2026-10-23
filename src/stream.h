/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file stream.h
 * @brief The bytes of a file: a buffered source and a buffered sink over the
 * callbacks of sdfio_stream.
 *
 * Not part of the interface: the library is built with hidden symbols. The
 * tests link the objects and call the functions directly.
 */

#ifndef SDFIO_SRC_STREAM_H
#define SDFIO_SRC_STREAM_H

#include "common.h"

/**
 * A buffered stream for reading.
 *
 * The functions of the source work with the buffer, so a stream that gives a
 * byte at a time costs one call for the buffer.
 */
typedef struct sdfio_source
{
    /** The stream. */
    sdfio_stream io;
    /** The buffer. */
    unsigned char *buffer;
    /** The size of the buffer. */
    size_t capacity;
    /** The allocator of the buffer. */
    const sdfio_allocator *allocator;
    /** The index of the next byte in the buffer. */
    size_t position;
    /** The index after the last byte in the buffer. */
    size_t end;
    /** Whether the stream gave no more bytes. */
    bool at_end;
    /** Whether the stream failed. */
    bool failed;
    /** Whether a line did not fit in memory. */
    bool no_memory;
} sdfio_source;

/**
 * Makes the buffer of a source, once, with the reader.
 *
 * @param[out] source The source.
 * @param capacity The size of the buffer.
 * @param allocator The allocator. May be null.
 * @return False if there is no memory.
 */
bool sdfio_source_create(sdfio_source *source, size_t capacity,
                         const sdfio_allocator *allocator);

/**
 * Releases the buffer of a source.
 *
 * @param source The source.
 */
void sdfio_source_destroy(sdfio_source *source);

/**
 * Attaches a stream to a source, and starts at its current position.
 *
 * @param source The source, with a buffer.
 * @param io The stream.
 */
void sdfio_source_init(sdfio_source *source, const sdfio_stream *io);

/**
 * Makes sure that `want` bytes are buffered, if the stream has them.
 *
 * @param source The source.
 * @param want The number of bytes. A number above the size of the buffer is
 * taken as the size of the buffer.
 * @return The number of bytes that are buffered.
 */
size_t sdfio_source_fill(sdfio_source *source, size_t want);

/**
 * Returns the next byte without taking it.
 *
 * The function is inline because a text is read byte by byte.
 *
 * @param source The source.
 * @return The byte, or -1 at the end or after a failure.
 */
static inline int sdfio_source_peek(sdfio_source *source)
{
    if (source->position == source->end && sdfio_source_fill(source, 1) == 0)
    {
        return -1;
    }
    return source->buffer[source->position];
}

/**
 * Takes the next byte.
 *
 * @param source The source.
 * @return The byte, or -1 at the end or after a failure.
 */
static inline int sdfio_source_getc(sdfio_source *source)
{
    const int c = sdfio_source_peek(source);
    if (c >= 0)
    {
        ++source->position;
    }
    return c;
}

/**
 * Reads bytes.
 *
 * @param source The source.
 * @param[out] dst Receives the bytes.
 * @param size The number of bytes to read.
 * @return The number of bytes read, less than `size` at the end or after a
 * failure.
 */
size_t sdfio_source_read(sdfio_source *source, void *dst, size_t size);

/** A buffered stream for writing. */
typedef struct sdfio_sink
{
    /** The stream. */
    sdfio_stream io;
    /** The buffer. */
    unsigned char *buffer;
    /** The size of the buffer. */
    size_t capacity;
    /** The allocator of the buffer. */
    const sdfio_allocator *allocator;
    /** The number of bytes in the buffer. */
    size_t used;
    /** Whether the stream failed. */
    bool failed;
} sdfio_sink;

/**
 * Makes the buffer of a sink, once, with the writer.
 *
 * @param[out] sink The sink.
 * @param capacity The size of the buffer.
 * @param allocator The allocator. May be null.
 * @return False if there is no memory.
 */
bool sdfio_sink_create(sdfio_sink *sink, size_t capacity,
                       const sdfio_allocator *allocator);

/**
 * Releases the buffer of a sink.
 *
 * @param sink The sink.
 */
void sdfio_sink_destroy(sdfio_sink *sink);

/**
 * Attaches a stream to a sink.
 *
 * @param sink The sink, with a buffer.
 * @param io The stream.
 */
void sdfio_sink_init(sdfio_sink *sink, const sdfio_stream *io);

/**
 * Writes bytes.
 *
 * @param sink The sink.
 * @param data The bytes.
 * @param size The number of bytes.
 * @return False if the stream failed, now or before.
 */
bool sdfio_sink_write(sdfio_sink *sink, const void *data, size_t size);

/**
 * Writes the bytes in the buffer to the stream.
 *
 * @param sink The sink.
 * @return False if the stream failed, now or before.
 */
bool sdfio_sink_flush(sdfio_sink *sink);

/**
 * Why a read of a source failed: the memory, the stream, or the damaged file,
 * whose text is `format_text`.
 *
 * @param source The source.
 * @param[out] message Receives the reason.
 * @param format_text The reason if the file is damaged.
 * @return The status.
 */
sdfio_status sdfio_source_failure(const sdfio_source *source,
                                  sdfio_message *message,
                                  const char *format_text);

/**
 * Records that the stream of a sink failed.
 *
 * @param[out] message Receives the reason.
 * @return SDFIO_ERR_IO.
 */
sdfio_status sdfio_sink_failure(sdfio_message *message);

#endif /* SDFIO_SRC_STREAM_H */
