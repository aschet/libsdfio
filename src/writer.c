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

struct sdf_writer
{
    sdfi_message message;
    sdf_info info;
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
    unsigned char stored[SDFI_CHUNK * SDFI_LARGEST_SAMPLE];
    unsigned char swapped[SDFI_CHUNK * SDFI_LARGEST_SAMPLE];
    char text[SDFI_CHUNK * SDFI_TEXT_PER_VALUE];
};

sdf_writer *sdf_writer_new(void)
{
    return calloc(1, sizeof(sdf_writer));
}

const char *sdf_writer_message(const sdf_writer *writer)
{
    return writer != NULL ? writer->message.text : "";
}

static void abandon(sdf_writer *writer)
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

void sdf_writer_free(sdf_writer *writer)
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

static sdf_status fail_writing(sdf_writer *writer, sdf_status status)
{
    writer->state = STATE_FAILED;
    return status;
}

static sdf_status start_stream(sdf_writer *writer, const sdf_stream *stream,
                               const sdf_info *info)
{
    if (writer->state != STATE_NEW)
    {
        return sdfi_fail(&writer->message, SDF_ERR_STATE,
                         "The writer has created a file already");
    }
    const sdf_status status = sdfi_validate(info, &writer->message);
    if (status != SDF_OK)
    {
        return status;
    }
    writer->info = *info;
    writer->point = sdfi_point_now();
    sdfi_sink_init(&writer->sink, stream);
    writer->state = STATE_CREATED;
    return SDF_OK;
}

sdf_status sdf_create_stream(sdf_writer *writer, const sdf_stream *stream,
                             const sdf_info *info)
{
    if (writer == NULL || stream == NULL || stream->write == NULL
        || info == NULL)
    {
        return SDF_ERR_ARGUMENT;
    }
    return start_stream(writer, stream, info);
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

sdf_status sdf_create_file(sdf_writer *writer, const char *path,
                           const sdf_info *info)
{
    if (writer == NULL || path == NULL || info == NULL)
    {
        return SDF_ERR_ARGUMENT;
    }
    if (writer->state != STATE_NEW)
    {
        return sdfi_fail(&writer->message, SDF_ERR_STATE,
                         "The writer has created a file already");
    }
    /* The file is written beside its place and moved there when it is whole,
     * so that an error leaves the old file as it was. */
    writer->path = copy_text(path, "");
    writer->temporary_path = copy_text(path, ".sdftmp");
    if (writer->path == NULL || writer->temporary_path == NULL)
    {
        return sdfi_fail(&writer->message, SDF_ERR_MEMORY, "Out of memory");
    }
    writer->file = fopen(writer->temporary_path, "wb");
    if (writer->file == NULL)
    {
        return sdfi_fail(&writer->message, SDF_ERR_IO,
                         "The file cannot be written: %s", path);
    }
    const sdf_stream stream = sdf_stdio_stream(writer->file);
    const sdf_status status = start_stream(writer, &stream, info);
    if (status != SDF_OK)
    {
        abandon(writer);
    }
    return status;
}

static sdf_status write_failed(sdf_writer *writer)
{
    return fail_writing(writer, sdfi_fail(&writer->message, SDF_ERR_IO,
                                          "The stream could not be written"));
}

/* The header goes out with the first values, when the z-scale is known. */
static sdf_status start_data(sdf_writer *writer)
{
    if (writer->header_written)
    {
        return SDF_OK;
    }
    sdf_info *info = &writer->info;
    if (info->z_scale == SDF_AUTO_Z_SCALE)
    {
        size_t count = 0;
        sdfi_count(info, &count);
        if (sdfi_is_floating(info->data_type) || count == 0)
        {
            info->z_scale = 1.0;
        }
        else
        {
            return sdfi_fail(&writer->message, SDF_ERR_ARGUMENT,
                             "Stored integer values need a z-scale, the "
                             "height of one count: use sdf_write_heights() "
                             "to have one chosen");
        }
    }
    sdf_status status = SDF_OK;
    if (info->storage == SDF_BINARY)
    {
        unsigned char header[SDFI_BINARY_HEADER_MAX];
        size_t size = 0;
        status = sdfi_binary_header(info, header, &size, &writer->message);
        if (status == SDF_OK && !sdfi_sink_write(&writer->sink, header, size))
        {
            return write_failed(writer);
        }
    }
    else
    {
        sdfi_string text = {0};
        status =
            sdfi_ascii_header(info, &writer->point, &text, &writer->message);
        if (status == SDF_OK
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
static sdf_status put_values(sdf_writer *writer, const void *values,
                             size_t count)
{
    const sdf_info *info = &writer->info;
    const size_t size = sdf_data_type_size(info->data_type);
    if (info->storage == SDF_BINARY)
    {
        if (sdfi_host_is_little_endian())
        {
            return sdfi_sink_write(&writer->sink, values, count * size)
                       ? SDF_OK
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
        return SDF_OK;
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
                sdfi_ascii_format(info, &writer->point,
                                  (const unsigned char *)values + first * size,
                                  i, writer->text + used, SDFI_TEXT_PER_VALUE);
            if (++writer->column == info->columns)
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
    return SDF_OK;
}

static sdf_status require_writable(sdf_writer *writer)
{
    if (writer == NULL)
    {
        return SDF_ERR_ARGUMENT;
    }
    if (writer->state == STATE_NEW)
    {
        return sdfi_fail(&writer->message, SDF_ERR_STATE,
                         "Create a file first");
    }
    if (writer->state != STATE_CREATED)
    {
        return sdfi_fail(&writer->message, SDF_ERR_STATE,
                         "The file is ended, or an error ended the writing");
    }
    return SDF_OK;
}

sdf_status sdf_write_rows(sdf_writer *writer, const void *src, size_t rows)
{
    sdf_status status = require_writable(writer);
    if (status != SDF_OK)
    {
        return status;
    }
    const sdf_info *info = &writer->info;
    if (rows > info->rows - writer->next_row)
    {
        return sdfi_fail(&writer->message, SDF_ERR_ARGUMENT,
                         "%zu rows were given, %zu are left to write", rows,
                         info->rows - writer->next_row);
    }
    if (rows > 0 && info->columns > 0 && src == NULL)
    {
        return sdfi_fail(&writer->message, SDF_ERR_ARGUMENT,
                         "Give the rows to write");
    }
    status = start_data(writer);
    if (status != SDF_OK)
    {
        return status == SDF_ERR_ARGUMENT ? status
                                          : fail_writing(writer, status);
    }
    const size_t row_bytes =
        info->columns * sdf_data_type_size(info->data_type);
    for (size_t i = 0; i < rows; ++i)
    {
        status = put_values(writer, (const unsigned char *)src + i * row_bytes,
                            info->columns);
        if (status != SDF_OK)
        {
            return status;
        }
        ++writer->next_row;
    }
    return SDF_OK;
}

sdf_status sdf_write_heights(sdf_writer *writer, sdf_format format,
                             const void *src, ptrdiff_t row_stride)
{
    sdf_status status = require_writable(writer);
    if (status != SDF_OK)
    {
        return status;
    }
    sdf_info *info = &writer->info;
    if (writer->next_row != 0 || writer->header_written)
    {
        return sdfi_fail(&writer->message, SDF_ERR_STATE,
                         "Rows were written already");
    }
    if (!sdfi_valid_sample(format.sample))
    {
        return sdfi_fail(&writer->message, SDF_ERR_ARGUMENT,
                         "The sample type is not one of the enumeration");
    }
    const size_t sample_size = sdf_sample_size(format.sample);
    if (info->columns > SIZE_MAX / sample_size)
    {
        return sdfi_fail(&writer->message, SDF_ERR_ARGUMENT,
                         "A row is larger than memory");
    }
    const size_t row_bytes = info->columns * sample_size;
    const size_t stride_size =
        row_stride < 0 ? (size_t)(-(row_stride + 1)) + 1 : (size_t)row_stride;
    if (info->rows > 1 && stride_size < row_bytes)
    {
        return sdfi_fail(&writer->message, SDF_ERR_ARGUMENT,
                         "The row stride is smaller than a row");
    }
    if (info->rows > 0 && info->columns > 0 && src == NULL)
    {
        return sdfi_fail(&writer->message, SDF_ERR_ARGUMENT,
                         "Give the heights to write");
    }
    if (info->z_scale == SDF_AUTO_Z_SCALE && !sdfi_is_floating(info->data_type))
    {
        status = sdf_suggest_z_scale(info->data_type, info->revision, format,
                                     src, info->rows, info->columns, row_stride,
                                     &info->z_scale);
        if (status != SDF_OK)
        {
            return status;
        }
    }
    status = start_data(writer);
    if (status != SDF_OK)
    {
        return status;
    }
    for (size_t file_row = 0; file_row < info->rows; ++file_row)
    {
        const unsigned char *row =
            (const unsigned char *)src
            + (ptrdiff_t)sdf_row_y(info, file_row) * row_stride;
        for (size_t first = 0; first < info->columns; first += SDFI_CHUNK)
        {
            const size_t count = info->columns - first < SDFI_CHUNK
                                     ? info->columns - first
                                     : SDFI_CHUNK;
            status = sdf_encode_heights(info, format, row + first * sample_size,
                                        count, writer->stored);
            if (status == SDF_ERR_RANGE || status == SDF_ERR_SENTINEL)
            {
                sdfi_fail(&writer->message, status,
                          status == SDF_ERR_RANGE
                              ? "Data value out of range for SDF data type %s "
                                "at row %zu"
                              : "A data value collides with the %s "
                                "invalid-point sentinel at row %zu; rescale "
                                "the data to avoid this value",
                          sdf_data_type_name(info->data_type),
                          sdf_row_y(info, file_row));
            }
            if (status == SDF_OK)
            {
                status = put_values(writer, writer->stored, count);
            }
            if (status != SDF_OK)
            {
                return fail_writing(writer, status);
            }
        }
        ++writer->next_row;
    }
    return SDF_OK;
}

static sdf_status finish_file(sdf_writer *writer)
{
    if (writer->file == NULL)
    {
        return SDF_OK;
    }
    const bool flushed = fflush(writer->file) == 0;
    const bool closed = fclose(writer->file) == 0;
    writer->file = NULL;
    if (!flushed || !closed)
    {
        remove(writer->temporary_path);
        return sdfi_fail(&writer->message, SDF_ERR_IO,
                         "The file could not be written");
    }
    if (rename(writer->temporary_path, writer->path) != 0)
    {
        /* Some systems do not replace a file that exists. */
        remove(writer->path);
        if (rename(writer->temporary_path, writer->path) != 0)
        {
            remove(writer->temporary_path);
            return sdfi_fail(&writer->message, SDF_ERR_IO,
                             "The file could not be moved to its place");
        }
    }
    return SDF_OK;
}

static bool is_ascii_text(const char *text, size_t size)
{
    for (size_t i = 0; i < size; ++i)
    {
        if ((unsigned char)text[i] >= SDFI_FIRST_NON_ASCII)
        {
            return false;
        }
    }
    return true;
}

sdf_status sdf_write_end(sdf_writer *writer, const char *trailer, size_t size)
{
    sdf_status status = require_writable(writer);
    if (status != SDF_OK)
    {
        return status;
    }
    const sdf_info *info = &writer->info;
    if (writer->next_row != info->rows)
    {
        return sdfi_fail(&writer->message, SDF_ERR_STATE,
                         "%zu of %zu rows were written", writer->next_row,
                         info->rows);
    }
    if (trailer == NULL && size > 0)
    {
        return SDF_ERR_ARGUMENT;
    }
    if (!is_ascii_text(trailer != NULL ? trailer : "",
                       trailer != NULL ? size : 0))
    {
        return sdfi_fail(&writer->message, SDF_ERR_ARGUMENT,
                         "The SDF trailer must be 7-bit ASCII");
    }
    if (size > 0 && sdfi_requires_tagged_trailer(info->revision)
        && !sdfi_tagged_valid(trailer, size))
    {
        return sdfi_fail(&writer->message, SDF_ERR_ARGUMENT,
                         "The trailer of SDF %s must use the tagged "
                         "'Name = Value' form",
                         sdf_revision_name(info->revision));
    }
    status = start_data(writer);
    if (status != SDF_OK)
    {
        return status;
    }
    if (info->storage == SDF_BINARY)
    {
        if (size > 0 && !sdfi_sink_write(&writer->sink, trailer, size))
        {
            return write_failed(writer);
        }
    }
    else
    {
        sdfi_string tail = {0};
        status = sdfi_ascii_tail(trailer != NULL ? trailer : "", size, &tail,
                                 &writer->message);
        if (status == SDF_OK
            && !sdfi_sink_write(&writer->sink, sdfi_string_text(&tail),
                                tail.size))
        {
            status = write_failed(writer);
        }
        sdfi_string_free(&tail);
        if (status != SDF_OK)
        {
            return fail_writing(writer, status);
        }
    }
    if (!sdfi_sink_flush(&writer->sink))
    {
        return write_failed(writer);
    }
    status = finish_file(writer);
    if (status != SDF_OK)
    {
        return fail_writing(writer, status);
    }
    writer->state = STATE_ENDED;
    return SDF_OK;
}
