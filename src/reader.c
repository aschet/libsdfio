/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

#include "internal.h"

typedef struct memory_stream
{
    const unsigned char *data;
    size_t size;
    size_t position;
} memory_stream;

struct sdf_reader
{
    sdfi_message message;
    sdf_info info;
    sdfi_source source;
    sdfi_point point;
    FILE *file;
    memory_stream memory;
    bool opened;
    /* Where the data area begins, if the source can seek. */
    int64_t data_start;
    size_t value_count;
    /* How much of the data area is read. */
    size_t next_row;
    size_t next_value;
    bool data_done;
    /* After an error in the middle of the data the position is lost. */
    bool failed;
    bool trailer_read;
    bool trailer_present;
    sdfi_string trailer;
    unsigned char chunk[SDFI_CHUNK * SDFI_LARGEST_SAMPLE];
};

static size_t memory_read(void *user, void *data, size_t size)
{
    memory_stream *memory = user;
    const size_t left = memory->size - memory->position;
    const size_t count = size < left ? size : left;
    if (count > 0)
    {
        memcpy(data, memory->data + memory->position, count);
    }
    memory->position += count;
    return count;
}

static int64_t memory_seek(void *user, int64_t offset, int whence)
{
    memory_stream *memory = user;
    int64_t origin = 0;
    if (whence == SEEK_CUR)
    {
        origin = (int64_t)memory->position;
    }
    else if (whence == SEEK_END)
    {
        origin = (int64_t)memory->size;
    }
    else if (whence != SEEK_SET)
    {
        return -1;
    }
    const int64_t target = origin + offset;
    if (target < 0 || (uint64_t)target > memory->size)
    {
        return -1;
    }
    memory->position = (size_t)target;
    return target;
}

sdf_reader *sdf_reader_new(void)
{
    return calloc(1, sizeof(sdf_reader));
}

void sdf_reader_free(sdf_reader *reader)
{
    if (reader == NULL)
    {
        return;
    }
    if (reader->file != NULL)
    {
        fclose(reader->file);
    }
    sdfi_string_free(&reader->trailer);
    free(reader);
}

const char *sdf_reader_message(const sdf_reader *reader)
{
    return reader != NULL ? reader->message.text : "";
}

const sdf_info *sdf_get_info(const sdf_reader *reader)
{
    return reader != NULL && reader->opened ? &reader->info : NULL;
}

static sdf_status fail_reading(sdf_reader *reader, sdf_status status)
{
    reader->failed = true;
    return status;
}

/* The size of a file that has been opened, against what its header claims. */
static sdf_status check_size(sdf_reader *reader)
{
    const sdf_info *info = &reader->info;
    const int64_t size = sdfi_source_size(&reader->source);
    if (size < 0)
    {
        return sdfi_fail(&reader->message, SDF_ERR_IO,
                         "The stream cannot seek to its end");
    }
    const uint64_t room = (uint64_t)size > (uint64_t)reader->data_start
                              ? (uint64_t)size - (uint64_t)reader->data_start
                              : 0;
    if (info->storage == SDF_BINARY)
    {
        if (reader->value_count > room / sdf_data_type_size(info->data_type))
        {
            return sdfi_fail(&reader->message, SDF_ERR_FORMAT,
                             "Unexpected end of file while reading the SDF "
                             "data area");
        }
    }
    else if (reader->value_count > room)
    {
        return sdfi_fail(&reader->message, SDF_ERR_FORMAT,
                         "Expected %zu data values, found fewer",
                         reader->value_count);
    }
    return SDF_OK;
}

static sdf_status open_source(sdf_reader *reader, const sdf_stream *stream)
{
    if (reader->opened)
    {
        return sdfi_fail(&reader->message, SDF_ERR_STATE,
                         "The reader is open already");
    }
    sdfi_source_init(&reader->source, stream);
    reader->point = sdfi_point_now();
    sdf_status status = sdfi_read_header(&reader->source, &reader->info,
                                         &reader->point, &reader->message);
    if (status != SDF_OK)
    {
        return status;
    }
    const sdf_info *info = &reader->info;
    const size_t type_size = sdf_data_type_size(info->data_type);
    if (!sdfi_count(info, &reader->value_count)
        || reader->value_count > SIZE_MAX / type_size)
    {
        return sdfi_fail(&reader->message, SDF_ERR_FORMAT,
                         "The SDF file has more points than can be counted");
    }
    reader->data_start = -1;
    if (reader->source.seekable)
    {
        reader->data_start = sdfi_source_tell(&reader->source);
        status = check_size(reader);
        if (status != SDF_OK)
        {
            return status;
        }
    }
    reader->next_row = 0;
    reader->next_value = 0;
    reader->data_done = false;
    reader->failed = false;
    reader->opened = true;
    return SDF_OK;
}

sdf_status sdf_open_stream(sdf_reader *reader, const sdf_stream *stream)
{
    if (reader == NULL || stream == NULL || stream->read == NULL)
    {
        return SDF_ERR_ARGUMENT;
    }
    return open_source(reader, stream);
}

sdf_status sdf_open_memory(sdf_reader *reader, const void *data, size_t size)
{
    if (reader == NULL || (data == NULL && size > 0))
    {
        return SDF_ERR_ARGUMENT;
    }
    reader->memory = (memory_stream){data, size, 0};
    const sdf_stream stream = {&reader->memory, memory_read, NULL, memory_seek};
    return open_source(reader, &stream);
}

sdf_status sdf_open_file(sdf_reader *reader, const char *path)
{
    if (reader == NULL || path == NULL)
    {
        return SDF_ERR_ARGUMENT;
    }
    if (reader->opened)
    {
        return sdfi_fail(&reader->message, SDF_ERR_STATE,
                         "The reader is open already");
    }
    FILE *file = fopen(path, "rb");
    if (file == NULL)
    {
        return sdfi_fail(&reader->message, SDF_ERR_IO,
                         "The file cannot be opened: %s", path);
    }
    const sdf_stream stream = sdf_stdio_stream(file);
    const sdf_status status = open_source(reader, &stream);
    if (status != SDF_OK)
    {
        fclose(file);
        return status;
    }
    reader->file = file;
    return SDF_OK;
}

static sdf_status require_readable(sdf_reader *reader)
{
    if (reader == NULL)
    {
        return SDF_ERR_ARGUMENT;
    }
    if (!reader->opened)
    {
        return sdfi_fail(&reader->message, SDF_ERR_STATE, "Open a file first");
    }
    if (reader->failed)
    {
        return sdfi_fail(&reader->message, SDF_ERR_STATE,
                         "An error ended the reading: rewind the reader to "
                         "start again");
    }
    return SDF_OK;
}

static sdf_status stream_error(sdf_reader *reader)
{
    return reader->source.failed
               ? sdfi_fail(&reader->message, SDF_ERR_IO,
                           "The stream could not be read")
               : sdfi_fail(&reader->message, SDF_ERR_FORMAT,
                           "Unexpected end of file while reading the SDF data "
                           "area");
}

/* The next `count` values of the data area, as the file stores them. */
static sdf_status read_values(sdf_reader *reader, void *dst, size_t count)
{
    const sdf_info *info = &reader->info;
    if (info->storage == SDF_ASCII)
    {
        for (size_t i = 0; i < count; ++i)
        {
            const sdf_status status =
                sdfi_ascii_value(&reader->source, info, &reader->point, dst, i,
                                 reader->next_value + i, &reader->message);
            if (status != SDF_OK)
            {
                return status;
            }
        }
    }
    else
    {
        const size_t size = sdf_data_type_size(info->data_type);
        if (sdfi_source_read(&reader->source, dst, count * size)
            != count * size)
        {
            return stream_error(reader);
        }
        sdfi_fix_endianness(dst, count, size);
    }
    reader->next_value += count;
    return SDF_OK;
}

/* Reads or skips to the end of the data area. */
static sdf_status finish_data(sdf_reader *reader)
{
    if (reader->data_done)
    {
        return SDF_OK;
    }
    const sdf_info *info = &reader->info;
    const bool all_read = reader->next_row == info->rows;
    if (info->storage == SDF_ASCII)
    {
        const sdf_status status =
            all_read
                ? sdfi_ascii_finish(&reader->source, info, &reader->message)
                : sdfi_ascii_skip(&reader->source, info, &reader->message);
        if (status != SDF_OK)
        {
            return status;
        }
    }
    else if (!all_read)
    {
        const size_t size = sdf_data_type_size(info->data_type);
        const size_t left = (reader->value_count - reader->next_value) * size;
        if (reader->source.seekable)
        {
            const int64_t end = reader->data_start
                                + (int64_t)reader->value_count * (int64_t)size;
            if (!sdfi_source_seek(&reader->source, end))
            {
                return sdfi_fail(&reader->message, SDF_ERR_IO,
                                 "The stream cannot seek to the trailer");
            }
        }
        else
        {
            /* A stream that cannot seek is read through. */
            unsigned char piece[SDFI_CHUNK];
            for (size_t done = 0; done < left;)
            {
                const size_t step =
                    left - done < sizeof piece ? left - done : sizeof piece;
                if (sdfi_source_read(&reader->source, piece, step) != step)
                {
                    return stream_error(reader);
                }
                done += step;
            }
        }
    }
    reader->data_done = true;
    return SDF_OK;
}

sdf_status sdf_read_rows(sdf_reader *reader, void *dst, size_t rows)
{
    sdf_status status = require_readable(reader);
    if (status != SDF_OK)
    {
        return status;
    }
    const sdf_info *info = &reader->info;
    if (rows > info->rows - reader->next_row)
    {
        return sdfi_fail(&reader->message, SDF_ERR_ARGUMENT,
                         "%zu rows were asked for, %zu are left", rows,
                         info->rows - reader->next_row);
    }
    if (rows > 0 && info->columns > 0 && dst == NULL)
    {
        return sdfi_fail(&reader->message, SDF_ERR_ARGUMENT,
                         "Give the memory to read the rows into");
    }
    const size_t row_bytes =
        info->columns * sdf_data_type_size(info->data_type);
    for (size_t i = 0; i < rows; ++i)
    {
        status = read_values(reader, (unsigned char *)dst + i * row_bytes,
                             info->columns);
        if (status != SDF_OK)
        {
            return fail_reading(reader, status);
        }
        ++reader->next_row;
    }
    if (reader->next_row == info->rows)
    {
        status = finish_data(reader);
        if (status != SDF_OK)
        {
            return fail_reading(reader, status);
        }
    }
    return SDF_OK;
}

sdf_status sdf_read_heights(sdf_reader *reader, sdf_format format, void *dst,
                            ptrdiff_t row_stride)
{
    sdf_status status = require_readable(reader);
    if (status != SDF_OK)
    {
        return status;
    }
    const sdf_info *info = &reader->info;
    if (reader->next_row != 0)
    {
        return sdfi_fail(&reader->message, SDF_ERR_STATE,
                         "Rows were read already: rewind the reader to read "
                         "the heights");
    }
    if (!sdfi_valid_sample(format.sample))
    {
        return sdfi_fail(&reader->message, SDF_ERR_ARGUMENT,
                         "The sample type is not one of the enumeration");
    }
    const size_t sample_size = sdf_sample_size(format.sample);
    if (info->columns > SIZE_MAX / sample_size)
    {
        return sdfi_fail(&reader->message, SDF_ERR_ARGUMENT,
                         "A row is larger than memory");
    }
    const size_t row_bytes = info->columns * sample_size;
    const size_t stride_size =
        row_stride < 0 ? (size_t)(-(row_stride + 1)) + 1 : (size_t)row_stride;
    if (info->rows > 1 && stride_size < row_bytes)
    {
        return sdfi_fail(&reader->message, SDF_ERR_ARGUMENT,
                         "The row stride is smaller than a row");
    }
    if (reader->value_count > 0 && dst == NULL)
    {
        return sdfi_fail(&reader->message, SDF_ERR_ARGUMENT,
                         "Give the memory to read the heights into");
    }
    for (size_t file_row = 0; file_row < info->rows; ++file_row)
    {
        unsigned char *row =
            (unsigned char *)dst
            + (ptrdiff_t)sdf_row_y(info, file_row) * row_stride;
        for (size_t first = 0; first < info->columns; first += SDFI_CHUNK)
        {
            const size_t count = info->columns - first < SDFI_CHUNK
                                     ? info->columns - first
                                     : SDFI_CHUNK;
            status = read_values(reader, reader->chunk, count);
            if (status == SDF_OK)
            {
                status = sdf_decode_heights(info, format, reader->chunk, count,
                                            row + first * sample_size);
                if (status != SDF_OK)
                {
                    sdfi_fail(&reader->message, status,
                              "A height does not fit the sample type");
                }
            }
            if (status != SDF_OK)
            {
                return fail_reading(reader, status);
            }
        }
        ++reader->next_row;
    }
    status = finish_data(reader);
    return status == SDF_OK ? status : fail_reading(reader, status);
}

sdf_status sdf_read_trailer(sdf_reader *reader, const char **trailer,
                            size_t *size)
{
    if (trailer == NULL || size == NULL)
    {
        return SDF_ERR_ARGUMENT;
    }
    if (reader != NULL && reader->trailer_read && !reader->failed)
    {
        *trailer =
            reader->trailer_present ? sdfi_string_text(&reader->trailer) : NULL;
        *size = reader->trailer.size;
        return SDF_OK;
    }
    sdf_status status = require_readable(reader);
    if (status != SDF_OK)
    {
        return status;
    }
    status = finish_data(reader);
    if (status == SDF_OK)
    {
        status = reader->info.storage == SDF_ASCII
                     ? sdfi_ascii_trailer(
                           &reader->source, &reader->info, &reader->trailer,
                           &reader->trailer_present, &reader->message)
                     : sdfi_binary_trailer(&reader->source, &reader->trailer,
                                           &reader->trailer_present,
                                           &reader->message);
    }
    if (status != SDF_OK)
    {
        return fail_reading(reader, status);
    }
    reader->trailer_read = true;
    *trailer =
        reader->trailer_present ? sdfi_string_text(&reader->trailer) : NULL;
    *size = reader->trailer.size;
    return SDF_OK;
}

sdf_status sdf_rewind(sdf_reader *reader)
{
    if (reader == NULL)
    {
        return SDF_ERR_ARGUMENT;
    }
    if (!reader->opened)
    {
        return sdfi_fail(&reader->message, SDF_ERR_STATE, "Open a file first");
    }
    if (!sdfi_source_seek(&reader->source, reader->data_start))
    {
        return sdfi_fail(&reader->message, SDF_ERR_STATE,
                         "A source that cannot seek cannot be rewound");
    }
    reader->next_row = 0;
    reader->next_value = 0;
    reader->data_done = false;
    reader->failed = false;
    return SDF_OK;
}
