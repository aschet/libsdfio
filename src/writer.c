/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

#include "internal.h"

typedef enum writer_state
{
    STATE_NEW,
    STATE_CREATED,
    STATE_ENDED,
    STATE_FAILED
} writer_state;

struct sdfio_writer
{
    sdfi_message message;
    sdfio_header header;
    sdfi_sink sink;
    sdfi_point point;
    writer_state state;
    bool header_written;
    size_t next_row;
    /* The column of the next value in a text file. */
    size_t column;
    FILE *file;
    char *path;
    char *temporary_path;
    unsigned char coded[SDFI_CHUNK * SDFI_LARGEST_SAMPLE];
    unsigned char swapped[SDFI_CHUNK * SDFI_LARGEST_SAMPLE];
    char text[SDFI_CHUNK * SDFI_TEXT_PER_VALUE];
};

sdfio_writer *sdfio_writer_new(void)
{
    return calloc(1, sizeof(sdfio_writer));
}

const char *sdfio_writer_error_message(const sdfio_writer *writer)
{
    return writer != NULL ? writer->message.text : "";
}

static void abandon(sdfio_writer *writer)
{
    if (writer->file != NULL)
    {
        fclose(writer->file);
        writer->file = NULL;
        if (writer->state != STATE_ENDED && writer->temporary_path != NULL)
        {
            remove(writer->temporary_path);
        }
    }
}

void sdfio_writer_free(sdfio_writer *writer)
{
    if (writer == NULL)
    {
        return;
    }
    abandon(writer);
    free(writer->path);
    free(writer->temporary_path);
    free(writer);
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
        return sdfi_fail(&writer->message, SDFIO_ERR_STATE,
                         "The writer has created a file already");
    }
    const sdfio_status status = sdfi_validate(header, &writer->message);
    if (status != SDFIO_OK)
    {
        return status;
    }
    writer->header = *header;
    writer->point = sdfi_point_now();
    sdfi_sink_init(&writer->sink, stream);
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

static char *copy_text(const char *text, const char *suffix)
{
    const size_t size = strlen(text);
    const size_t suffix_size = strlen(suffix);
    char *copy = malloc(size + suffix_size + 1);
    if (copy != NULL)
    {
        snprintf(copy, size + suffix_size + 1, "%s%s", text, suffix);
    }
    return copy;
}

sdfio_status sdfio_create_file(sdfio_writer *writer, const char *path,
                               const sdfio_header *header)
{
    if (writer == NULL || path == NULL || header == NULL)
    {
        return SDFIO_ERR_ARGUMENT;
    }
    if (writer->state != STATE_NEW)
    {
        return sdfi_fail(&writer->message, SDFIO_ERR_STATE,
                         "The writer has created a file already");
    }
    /* The file is written beside its place and moved there when it is whole,
     * so that an error leaves the old file as it was. */
    writer->path = copy_text(path, "");
    writer->temporary_path = copy_text(path, ".sdftmp");
    if (writer->path == NULL || writer->temporary_path == NULL)
    {
        return sdfi_fail(&writer->message, SDFIO_ERR_MEMORY, "Out of memory");
    }
    writer->file = fopen(writer->temporary_path, "wb");
    if (writer->file == NULL)
    {
        return sdfi_fail(&writer->message, SDFIO_ERR_IO,
                         "The file cannot be written: %s", path);
    }
    const sdfio_stream stream = sdfio_stdio_stream(writer->file);
    const sdfio_status status = start_stream(writer, &stream, header);
    if (status != SDFIO_OK)
    {
        abandon(writer);
    }
    return status;
}

static sdfio_status write_failed(sdfio_writer *writer)
{
    return fail_writing(writer, sdfi_fail(&writer->message, SDFIO_ERR_IO,
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
        sdfi_count(header, &count);
        if (sdfi_is_floating(header->data_type) || count == 0)
        {
            header->z_scale = 1.0;
        }
        else
        {
            return sdfi_fail(&writer->message, SDFIO_ERR_ARGUMENT,
                             "Stored integer values need a z-scale, the "
                             "height of one count: use sdfio_write_heights() "
                             "to have one chosen");
        }
    }
    sdfio_status status = SDFIO_OK;
    if (header->representation == SDFIO_BINARY)
    {
        unsigned char bytes[SDFI_BINARY_HEADER_MAX];
        size_t size = 0;
        status = sdfi_binary_header(header, bytes, &size, &writer->message);
        if (status == SDFIO_OK && !sdfi_sink_write(&writer->sink, bytes, size))
        {
            return write_failed(writer);
        }
    }
    else
    {
        sdfi_string text = {0};
        status =
            sdfi_ascii_header(header, &writer->point, &text, &writer->message);
        if (status == SDFIO_OK
            && !sdfi_sink_write(&writer->sink, sdfi_string_text(&text),
                                text.size))
        {
            sdfi_string_free(&text);
            return write_failed(writer);
        }
        sdfi_string_free(&text);
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
        if (sdfi_host_is_little_endian())
        {
            return sdfi_sink_write(&writer->sink, values, count * size)
                       ? SDFIO_OK
                       : write_failed(writer);
        }
        for (size_t first = 0; first < count; first += SDFI_CHUNK)
        {
            const size_t n =
                count - first < SDFI_CHUNK ? count - first : SDFI_CHUNK;
            memcpy(writer->swapped,
                   (const unsigned char *)values + first * size, n * size);
            sdfi_fix_endianness(writer->swapped, n, size);
            if (!sdfi_sink_write(&writer->sink, writer->swapped, n * size))
            {
                return write_failed(writer);
            }
        }
        return SDFIO_OK;
    }
    static const char crlf[] = "\r\n";
    for (size_t first = 0; first < count; first += SDFI_CHUNK)
    {
        const size_t n =
            count - first < SDFI_CHUNK ? count - first : SDFI_CHUNK;
        size_t used = 0;
        for (size_t i = 0; i < n; ++i)
        {
            if (writer->column > 0)
            {
                writer->text[used++] = ' ';
            }
            used +=
                sdfi_ascii_format(header, &writer->point,
                                  (const unsigned char *)values + first * size,
                                  i, writer->text + used, SDFI_TEXT_PER_VALUE);
            if (++writer->column == header->columns)
            {
                memcpy(writer->text + used, crlf, sizeof crlf - 1);
                used += sizeof crlf - 1;
                writer->column = 0;
            }
        }
        if (!sdfi_sink_write(&writer->sink, writer->text, used))
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
        return sdfi_fail(&writer->message, SDFIO_ERR_STATE,
                         "Create a file first");
    }
    if (writer->state != STATE_CREATED)
    {
        return sdfi_fail(&writer->message, SDFIO_ERR_STATE,
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
        return sdfi_fail(&writer->message, SDFIO_ERR_ARGUMENT,
                         "%zu rows were given, %zu are left to write", rows,
                         header->rows - writer->next_row);
    }
    if (rows > 0 && header->columns > 0 && src == NULL)
    {
        return sdfi_fail(&writer->message, SDFIO_ERR_ARGUMENT,
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
    for (size_t i = 0; i < rows; ++i)
    {
        status = put_values(writer, (const unsigned char *)src + i * row_bytes,
                            header->columns);
        if (status != SDFIO_OK)
        {
            return status;
        }
        ++writer->next_row;
    }
    return SDFIO_OK;
}

sdfio_status sdfio_write_heights(sdfio_writer *writer, sdfio_format format,
                                 const void *src, ptrdiff_t row_stride)
{
    sdfio_status status = require_writable(writer);
    if (status != SDFIO_OK)
    {
        return status;
    }
    sdfio_header *header = &writer->header;
    if (writer->next_row != 0 || writer->header_written)
    {
        return sdfi_fail(&writer->message, SDFIO_ERR_STATE,
                         "Rows were written already");
    }
    status = sdfi_check_layout(format, header->rows, header->columns,
                               row_stride, &writer->message);
    if (status != SDFIO_OK)
    {
        return status;
    }
    const ptrdiff_t element = sdfi_element_stride(format);
    if (header->rows > 0 && header->columns > 0 && src == NULL)
    {
        return sdfi_fail(&writer->message, SDFIO_ERR_ARGUMENT,
                         "Give the heights to write");
    }
    if (header->z_scale == SDFIO_AUTO_Z_SCALE
        && !sdfi_is_floating(header->data_type))
    {
        status = sdfio_suggest_z_scale(
            header->data_type, header->revision, format, src, header->rows,
            header->columns, row_stride, &header->z_scale);
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
    for (size_t file_row = 0; file_row < header->rows; ++file_row)
    {
        const unsigned char *row =
            (const unsigned char *)src
            + (ptrdiff_t)sdfio_row_y(header, file_row) * row_stride;
        for (size_t first = 0; first < header->columns; first += SDFI_CHUNK)
        {
            const size_t count = header->columns - first < SDFI_CHUNK
                                     ? header->columns - first
                                     : SDFI_CHUNK;
            status = sdfio_encode_heights(header, format,
                                          row + (ptrdiff_t)first * element,
                                          count, writer->coded);
            if (status == SDFIO_ERR_RANGE || status == SDFIO_ERR_BAD_VALUE)
            {
                sdfi_fail(&writer->message, status,
                          status == SDFIO_ERR_RANGE
                              ? "Data value out of range for SDF data type %s "
                                "at row %zu"
                              : "A data value collides with the %s "
                                "bad value at row %zu; rescale "
                                "the data to avoid this value",
                          sdfio_data_type_name(header->data_type),
                          sdfio_row_y(header, file_row));
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
    return SDFIO_OK;
}

static sdfio_status finish_file(sdfio_writer *writer)
{
    if (writer->file == NULL)
    {
        return SDFIO_OK;
    }
    const bool flushed = fflush(writer->file) == 0;
    const bool closed = fclose(writer->file) == 0;
    writer->file = NULL;
    if (!flushed || !closed)
    {
        remove(writer->temporary_path);
        return sdfi_fail(&writer->message, SDFIO_ERR_IO,
                         "The file could not be written");
    }
    if (rename(writer->temporary_path, writer->path) != 0)
    {
        /* Some systems do not replace a file that exists. */
        remove(writer->path);
        if (rename(writer->temporary_path, writer->path) != 0)
        {
            remove(writer->temporary_path);
            return sdfi_fail(&writer->message, SDFIO_ERR_IO,
                             "The file could not be moved to its place");
        }
    }
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
        return sdfi_fail(&writer->message, SDFIO_ERR_STATE,
                         "%zu of %zu rows were written", writer->next_row,
                         header->rows);
    }
    if (trailer == NULL && size > 0)
    {
        return sdfi_fail(&writer->message, SDFIO_ERR_ARGUMENT,
                         "A trailer of %zu bytes needs its text", size);
    }
    status = start_data(writer);
    if (status != SDFIO_OK)
    {
        return status;
    }
    if (header->representation == SDFIO_BINARY)
    {
        if (size > 0 && !sdfi_write_crlf_lines(&writer->sink, trailer, size))
        {
            return write_failed(writer);
        }
    }
    else
    {
        sdfi_string tail = {0};
        status = sdfi_ascii_tail(trailer != NULL ? trailer : "", size, &tail,
                                 &writer->message);
        if (status == SDFIO_OK
            && !sdfi_sink_write(&writer->sink, sdfi_string_text(&tail),
                                tail.size))
        {
            status = write_failed(writer);
        }
        sdfi_string_free(&tail);
        if (status != SDFIO_OK)
        {
            return fail_writing(writer, status);
        }
    }
    if (!sdfi_sink_flush(&writer->sink))
    {
        return write_failed(writer);
    }
    status = finish_file(writer);
    if (status != SDFIO_OK)
    {
        return fail_writing(writer, status);
    }
    writer->state = STATE_ENDED;
    return SDFIO_OK;
}
