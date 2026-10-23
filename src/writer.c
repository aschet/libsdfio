/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file writer.c
 * @brief The writer: the header, the heights and the trailer of a file
 * written to a stream.
 */

#include "internal.h"

/** Where a writer is in the life of a file. */
typedef enum writer_state
{
    /** No file is created. */
    STATE_NEW,
    /** A file is created and not ended. */
    STATE_CREATED,
    /** The file is ended. */
    STATE_ENDED,
    /** An error ended the writing. */
    STATE_FAILED
} writer_state;

/** The state of a writer. */
struct sdfio_writer
{
    /** The text of the last failure. */
    sdfio_message message;
    /** The header of the file. */
    sdfio_header header;
    /** The buffered stream. */
    sdfio_sink sink;
    /** The decimal point of the locale, measured when the file was created. */
    sdfio_point point;
    /** Where the writer is in the life of a file. */
    writer_state state;
    /** Whether the header went to the sink. */
    bool header_written;
    /** The index of the next row to write. */
    size_t next_row;
    /** The column of the next value in an ASCII file. */
    size_t column;
    /** The allocator of the configuration. */
    sdfio_allocator allocator;
    /** The time zone of the configuration. */
    sdfio_time_zone zone;
    /** What a file adds to the writer. */
    sdfio_owner owner;
    /** The number of values converted at a time. */
    size_t chunk_values;
    /** The room for a chunk of values in the data type of the file, made when
     * the file is created. */
    unsigned char *coded;
    /** The room for the same values in file byte order, if the host differs. */
    unsigned char *swapped;
    /** The room for the text of a chunk of values in an ASCII file. */
    char *text;
};

sdfio_writer *sdfio_writer_new(const sdfio_config *config)
{
    sdfio_allocator allocator;
    sdfio_time_zone zone;
    size_t buffer = 0;
    size_t chunk = 0;
    if (!sdfio_config_resolve(config, &allocator, &buffer, &chunk, &zone))
    {
        return NULL;
    }
    sdfio_writer *writer = sdfio_allocate(&allocator, sizeof *writer);
    if (writer == NULL)
    {
        return NULL;
    }
    *writer = (sdfio_writer){
        .allocator = allocator, .zone = zone, .chunk_values = chunk};
    writer->coded =
        sdfio_allocate(&writer->allocator, chunk * SDFIO_LARGEST_SAMPLE);
    if (writer->coded == NULL
        || !sdfio_sink_create(&writer->sink, buffer, &writer->allocator))
    {
        sdfio_writer_free(writer);
        return NULL;
    }
    return writer;
}

void sdfio_writer_free(sdfio_writer *writer)
{
    if (writer == NULL)
    {
        return;
    }
    if (writer->owner.close != NULL)
    {
        writer->owner.close(writer->owner.user);
    }
    sdfio_sink_destroy(&writer->sink);
    const sdfio_allocator allocator = writer->allocator;
    sdfio_release(&allocator, writer->coded);
    sdfio_release(&allocator, writer->swapped);
    sdfio_release(&allocator, writer->text);
    sdfio_release(&allocator, writer);
}

void sdfio_writer_own(sdfio_writer *writer, sdfio_owner owner)
{
    writer->owner = owner;
}

sdfio_message *sdfio_writer_message(sdfio_writer *writer)
{
    return &writer->message;
}

bool sdfio_writer_is_new(const sdfio_writer *writer)
{
    return writer->state == STATE_NEW;
}

const char *sdfio_writer_error_message(const sdfio_writer *writer)
{
    return writer != NULL && writer->message.text != NULL ? writer->message.text
                                                          : "";
}

static sdfio_status fail_writing(sdfio_writer *writer, sdfio_status status)
{
    writer->state = STATE_FAILED;
    return status;
}

static sdfio_status start_stream(sdfio_writer *writer,
                                 const sdfio_stream *stream,
                                 const sdfio_header *header)
{
    if (writer->state != STATE_NEW)
    {
        return sdfio_fail(&writer->message, SDFIO_ERR_STATE,
                          "The writer has created a file already");
    }
    const sdfio_status status = sdfio_validate(header, &writer->message);
    if (status != SDFIO_OK)
    {
        return status;
    }
    if (header->representation == SDFIO_ASCII)
    {
        writer->text = sdfio_allocate(
            &writer->allocator, writer->chunk_values * SDFIO_TEXT_PER_VALUE);
    }
    else if (!sdfio_host_is_little_endian())
    {
        writer->swapped = sdfio_allocate(
            &writer->allocator, writer->chunk_values * SDFIO_LARGEST_SAMPLE);
    }
    if ((header->representation == SDFIO_ASCII && writer->text == NULL)
        || (header->representation == SDFIO_BINARY
            && !sdfio_host_is_little_endian() && writer->swapped == NULL))
    {
        return sdfio_fail(&writer->message, SDFIO_ERR_MEMORY, "Out of memory");
    }
    writer->header = *header;
    writer->point = sdfio_point_now();
    sdfio_sink_init(&writer->sink, stream);
    writer->state = STATE_CREATED;
    return SDFIO_OK;
}

sdfio_status sdfio_create_stream(sdfio_writer *writer,
                                 const sdfio_stream *stream,
                                 const sdfio_header *header)
{
    if (writer == NULL || stream == NULL || stream->write == NULL
        || header == NULL)
    {
        return SDFIO_ERR_ARGUMENT;
    }
    return start_stream(writer, stream, header);
}

static sdfio_status write_failed(sdfio_writer *writer)
{
    return fail_writing(writer, sdfio_fail(&writer->message, SDFIO_ERR_IO,
                                           "The stream could not be written"));
}

/* The header goes out with the first values, when the z-scale is known. */
static sdfio_status start_data(sdfio_writer *writer)
{
    if (writer->header_written)
    {
        return SDFIO_OK;
    }
    sdfio_header *header = &writer->header;
    if (header->z_scale == SDFIO_AUTO_Z_SCALE)
    {
        size_t count = 0;
        sdfio_count(header, &count);
        if (sdfio_is_floating(header->data_type) || count == 0)
        {
            header->z_scale = 1.0;
        }
        else
        {
            return sdfio_fail(&writer->message, SDFIO_ERR_ARGUMENT,
                              "Stored integer values need a z-scale, the "
                              "height of one count: use sdfio_write_heights() "
                              "to have one chosen");
        }
    }
    sdfio_status status = SDFIO_OK;
    if (header->representation == SDFIO_BINARY)
    {
        unsigned char bytes[SDFIO_BINARY_HEADER_MAX];
        size_t size = 0;
        status = sdfio_binary_header(header, &writer->zone, bytes, &size,
                                     &writer->message);
        if (status == SDFIO_OK && !sdfio_sink_write(&writer->sink, bytes, size))
        {
            return write_failed(writer);
        }
    }
    else
    {
        sdfio_string text = {.allocator = &writer->allocator};
        status = sdfio_ascii_header(header, &writer->point, &writer->zone,
                                    &text, &writer->message);
        if (status == SDFIO_OK
            && !sdfio_sink_write(&writer->sink, sdfio_string_text(&text),
                                 text.size))
        {
            sdfio_string_free(&text);
            return write_failed(writer);
        }
        sdfio_string_free(&text);
    }
    writer->header_written = true;
    return status;
}

/* Stored values of the machine into the file. */
static sdfio_status put_values(sdfio_writer *writer, const void *values,
                               size_t count)
{
    const sdfio_header *header = &writer->header;
    const size_t size = sdfio_data_type_size(header->data_type);
    if (header->representation == SDFIO_BINARY)
    {
        if (sdfio_host_is_little_endian())
        {
            return sdfio_sink_write(&writer->sink, values, count * size)
                       ? SDFIO_OK
                       : write_failed(writer);
        }
        for (size_t first = 0; first < count; first += writer->chunk_values)
        {
            const size_t n = count - first < writer->chunk_values
                                 ? count - first
                                 : writer->chunk_values;
            memcpy(writer->swapped,
                   (const unsigned char *)values + first * size, n * size);
            sdfio_fix_endianness(writer->swapped, n, size);
            if (!sdfio_sink_write(&writer->sink, writer->swapped, n * size))
            {
                return write_failed(writer);
            }
        }
        return SDFIO_OK;
    }
    static const char crlf[] = "\r\n";
    for (size_t first = 0; first < count; first += writer->chunk_values)
    {
        const size_t n = count - first < writer->chunk_values
                             ? count - first
                             : writer->chunk_values;
        size_t used = 0;
        for (size_t i = 0; i < n; ++i)
        {
            if (writer->column > 0)
            {
                writer->text[used++] = ' ';
            }
            used += sdfio_ascii_format(
                header, &writer->point,
                (const unsigned char *)values + first * size, i,
                writer->text + used, SDFIO_TEXT_PER_VALUE);
            if (++writer->column == header->columns)
            {
                memcpy(writer->text + used, crlf, sizeof crlf - 1);
                used += sizeof crlf - 1;
                writer->column = 0;
            }
        }
        if (!sdfio_sink_write(&writer->sink, writer->text, used))
        {
            return write_failed(writer);
        }
    }
    return SDFIO_OK;
}

static sdfio_status require_writable(sdfio_writer *writer)
{
    if (writer == NULL)
    {
        return SDFIO_ERR_ARGUMENT;
    }
    if (writer->state == STATE_NEW)
    {
        return sdfio_fail(&writer->message, SDFIO_ERR_STATE,
                          "Create a file first");
    }
    if (writer->state != STATE_CREATED)
    {
        return sdfio_fail(&writer->message, SDFIO_ERR_STATE,
                          "The file is ended, or an error ended the writing");
    }
    return SDFIO_OK;
}

sdfio_status sdfio_write_data(sdfio_writer *writer, const void *src,
                              size_t rows)
{
    sdfio_status status = require_writable(writer);
    if (status != SDFIO_OK)
    {
        return status;
    }
    const sdfio_header *header = &writer->header;
    if (rows > header->rows - writer->next_row)
    {
        return sdfio_fail(&writer->message, SDFIO_ERR_ARGUMENT,
                          "More rows were given than are left to write");
    }
    if (rows > 0 && header->columns > 0 && src == NULL)
    {
        return sdfio_fail(&writer->message, SDFIO_ERR_ARGUMENT,
                          "Give the rows to write");
    }
    status = start_data(writer);
    if (status != SDFIO_OK)
    {
        return status == SDFIO_ERR_ARGUMENT ? status
                                            : fail_writing(writer, status);
    }
    const size_t row_bytes =
        header->columns * sdfio_data_type_size(header->data_type);
    /* Rows without a column hold nothing, however many a file declares. */
    const size_t valued_rows = header->columns > 0 ? rows : 0;
    for (size_t i = 0; i < valued_rows; ++i)
    {
        status = put_values(writer, (const unsigned char *)src + i * row_bytes,
                            header->columns);
        if (status != SDFIO_OK)
        {
            return status;
        }
        ++writer->next_row;
    }
    writer->next_row += rows - valued_rows;
    return SDFIO_OK;
}

/* The writing of rows from a buffer of the C type of the data type of the
 * file, which the type of the pointer has to be. */
static sdfio_status write_typed(sdfio_writer *writer, sdfio_data_type type,
                                const void *src, size_t rows)
{
    const sdfio_status status = require_writable(writer);
    if (status != SDFIO_OK)
    {
        return status;
    }
    if (writer->header.data_type != type)
    {
        return sdfio_fail(&writer->message, SDFIO_ERR_ARGUMENT,
                          "The data type of the file is not that of the "
                          "buffer");
    }
    return sdfio_write_data(writer, src, rows);
}

/** A function that writes rows from a buffer of a C type, for the data type. */
// A type cannot be parenthesized in the declaration of the function.
// NOLINTBEGIN(bugprone-macro-parentheses)
#define WRITE_TYPED(name, ctype, type)                                         \
    sdfio_status sdfio_write_data_##name(sdfio_writer *writer,                 \
                                         const ctype *src, size_t rows)        \
    {                                                                          \
        return write_typed(writer, type, src, rows);                           \
    }
// NOLINTEND(bugprone-macro-parentheses)

WRITE_TYPED(int8, int8_t, SDFIO_TYPE_INT8)
WRITE_TYPED(int16, int16_t, SDFIO_TYPE_INT16)
WRITE_TYPED(int32, int32_t, SDFIO_TYPE_INT32)
WRITE_TYPED(float, float, SDFIO_TYPE_FLOAT)
WRITE_TYPED(double, double, SDFIO_TYPE_DOUBLE)

sdfio_status sdfio_write_heights(sdfio_writer *writer, sdfio_data_type type,
                                 const void *src)
{
    return sdfio_write_heights_strided(writer, sdfio_format_packed(type), src);
}

sdfio_status sdfio_write_heights_strided(sdfio_writer *writer,
                                         sdfio_format format, const void *src)
{
    sdfio_status status = require_writable(writer);
    if (status != SDFIO_OK)
    {
        return status;
    }
    sdfio_header *header = &writer->header;
    if (writer->next_row != 0 || writer->header_written)
    {
        return sdfio_fail(&writer->message, SDFIO_ERR_STATE,
                          "Rows were written already");
    }
    status = sdfio_check_layout(&format, header->rows, header->columns,
                                &writer->message);
    if (status != SDFIO_OK)
    {
        return status;
    }
    const ptrdiff_t element = sdfio_element_stride(format);
    if (header->rows > 0 && header->columns > 0 && src == NULL)
    {
        return sdfio_fail(&writer->message, SDFIO_ERR_ARGUMENT,
                          "Give the heights to write");
    }
    if (header->z_scale == SDFIO_AUTO_Z_SCALE
        && !sdfio_is_floating(header->data_type))
    {
        status =
            sdfio_suggest_z_scale(header->data_type, format, src, header->rows,
                                  header->columns, &header->z_scale);
        if (status != SDFIO_OK)
        {
            return status;
        }
    }
    status = start_data(writer);
    if (status != SDFIO_OK)
    {
        return status;
    }
    const size_t valued_rows = header->columns > 0 ? header->rows : 0;
    for (size_t file_row = 0; file_row < valued_rows; ++file_row)
    {
        const unsigned char *row = (const unsigned char *)src
                                   + (ptrdiff_t)file_row * format.row_stride;
        for (size_t first = 0; first < header->columns;
             first += writer->chunk_values)
        {
            const size_t count = header->columns - first < writer->chunk_values
                                     ? header->columns - first
                                     : writer->chunk_values;
            status = sdfio_encode_heights(header, format,
                                          row + (ptrdiff_t)first * element,
                                          count, writer->coded);
            if (status == SDFIO_ERR_RANGE || status == SDFIO_ERR_BAD_VALUE)
            {
                sdfio_fail(&writer->message, status,
                           status == SDFIO_ERR_RANGE
                               ? "A height is out of range for the data type"
                               : "A height equals the bad value of the data "
                                 "type: rescale the data");
            }
            if (status == SDFIO_OK)
            {
                status = put_values(writer, writer->coded, count);
            }
            if (status != SDFIO_OK)
            {
                return fail_writing(writer, status);
            }
        }
        ++writer->next_row;
    }
    writer->next_row = header->rows;
    return SDFIO_OK;
}

sdfio_status sdfio_write_trailer(sdfio_writer *writer, const char *trailer,
                                 size_t size)
{
    sdfio_status status = require_writable(writer);
    if (status != SDFIO_OK)
    {
        return status;
    }
    const sdfio_header *header = &writer->header;
    if (writer->next_row != header->rows)
    {
        return sdfio_fail(&writer->message, SDFIO_ERR_STATE,
                          "Not all rows of the data area were written");
    }
    if (trailer == NULL && size > 0)
    {
        return sdfio_fail(&writer->message, SDFIO_ERR_ARGUMENT,
                          "A trailer with a size needs its text");
    }
    status = start_data(writer);
    if (status != SDFIO_OK)
    {
        return status;
    }
    if (header->representation == SDFIO_BINARY)
    {
        if (size > 0 && !sdfio_write_crlf_lines(&writer->sink, trailer, size))
        {
            return write_failed(writer);
        }
    }
    else
    {
        sdfio_string tail = {.allocator = &writer->allocator};
        status = sdfio_ascii_tail(trailer != NULL ? trailer : "", size, &tail,
                                  &writer->message);
        if (status == SDFIO_OK
            && !sdfio_sink_write(&writer->sink, sdfio_string_text(&tail),
                                 tail.size))
        {
            status = write_failed(writer);
        }
        sdfio_string_free(&tail);
        if (status != SDFIO_OK)
        {
            return fail_writing(writer, status);
        }
    }
    if (!sdfio_sink_flush(&writer->sink))
    {
        return write_failed(writer);
    }
    status = SDFIO_OK;
    if (writer->owner.finish != NULL)
    {
        status = writer->owner.finish(writer->owner.user, &writer->message);
        /* The file is ended, whatever the result. */
        writer->owner.close = NULL;
    }
    if (status != SDFIO_OK)
    {
        return fail_writing(writer, status);
    }
    writer->state = STATE_ENDED;
    return SDFIO_OK;
}
