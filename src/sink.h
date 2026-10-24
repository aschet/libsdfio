/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file sink.h
 * @brief The buffer in front of the stream of a writer.
 */

#ifndef SDFIO_SRC_SINK_H
#define SDFIO_SRC_SINK_H

#include "message.h"
#include <sdfio/sdfio.h>

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
 * Creates the buffer of a sink, once, with the writer.
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
 * Records that the stream of a sink failed.
 *
 * @param[out] message Receives the reason.
 * @return SDFIO_ERR_IO.
 */
sdfio_status sdfio_sink_failure(sdfio_message *message);

#endif /* SDFIO_SRC_SINK_H */
