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

struct sdfio_reader
{
    sdfi_message message;
    sdfio_header header;
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

sdfio_reader *sdfio_reader_new(void)
{
    return calloc(1, sizeof(sdfio_reader));
}

void sdfio_reader_free(sdfio_reader *reader)
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

const char *sdfio_reader_error_message(const sdfio_reader *reader)
{
    return reader != NULL ? reader->message.text : "";
}

const sdfio_header *sdfio_get_header(const sdfio_reader *reader)
{
    return reader != NULL && reader->opened ? &reader->header : NULL;
}

static sdfio_status fail_reading(sdfio_reader *reader, sdfio_status status)
{
    reader->failed = true;
    return status;
}

/* The size of a file that has been opened, against what its header claims. */
static sdfio_status check_size(sdfio_reader *reader)
{
    const sdfio_header *header = &reader->header;
    const int64_t size = sdfi_source_size(&reader->source);
    if (size < 0)
    {
        return sdfi_fail(&reader->message, SDFIO_ERR_IO,
                         "The stream cannot seek to its end");
    }
    const uint64_t room = (uint64_t)size > (uint64_t)reader->data_start
                              ? (uint64_t)size - (uint64_t)reader->data_start
                              : 0;
    if (header->storage == SDFIO_BINARY)
    {
        if (reader->value_count
            > room / sdfio_data_type_size(header->data_type))
        {
            return sdfi_fail(&reader->message, SDFIO_ERR_FORMAT,
                             "Unexpected end of file while reading the SDF "
                             "data area");
        }
    }
    else if (reader->value_count > room)
    {
        return sdfi_fail(&reader->message, SDFIO_ERR_FORMAT,
                         "Expected %zu data values, found fewer",
                         reader->value_count);
    }
    return SDFIO_OK;
}

static sdfio_status open_source(sdfio_reader *reader,
                                const sdfio_stream *stream)
{
    if (reader->opened)
    {
        return sdfi_fail(&reader->message, SDFIO_ERR_STATE,
                         "The reader is open already");
    }
    sdfi_source_init(&reader->source, stream);
    reader->point = sdfi_point_now();
    sdfio_status status = sdfi_read_header(&reader->source, &reader->header,
                                           &reader->point, &reader->message);
    if (status != SDFIO_OK)
    {
        return status;
    }
    const sdfio_header *header = &reader->header;
    const size_t type_size = sdfio_data_type_size(header->data_type);
    if (!sdfi_count(header, &reader->value_count)
        || reader->value_count > SIZE_MAX / type_size)
    {
        return sdfi_fail(&reader->message, SDFIO_ERR_FORMAT,
                         "The SDF file has more points than can be counted");
    }
    reader->data_start = -1;
    if (reader->source.seekable)
    {
        reader->data_start = sdfi_source_tell(&reader->source);
        status = check_size(reader);
        if (status != SDFIO_OK)
        {
            return status;
        }
    }
    reader->next_row = 0;
    reader->next_value = 0;
    reader->data_done = false;
    reader->failed = false;
    reader->opened = true;
    return SDFIO_OK;
}

sdfio_status sdfio_open_stream(sdfio_reader *reader, const sdfio_stream *stream)
{
    if (reader == NULL || stream == NULL || stream->read == NULL)
    {
        return SDFIO_ERR_ARGUMENT;
    }
    return open_source(reader, stream);
}

sdfio_status sdfio_open_memory(sdfio_reader *reader, const void *data,
                               size_t size)
{
    if (reader == NULL || (data == NULL && size > 0))
    {
        return SDFIO_ERR_ARGUMENT;
    }
    reader->memory = (memory_stream){data, size, 0};
    const sdfio_stream stream = {&reader->memory, memory_read, NULL,
                                 memory_seek};
    return open_source(reader, &stream);
}

sdfio_status sdfio_open_file(sdfio_reader *reader, const char *path)
{
    if (reader == NULL || path == NULL)
    {
        return SDFIO_ERR_ARGUMENT;
    }
    if (reader->opened)
    {
        return sdfi_fail(&reader->message, SDFIO_ERR_STATE,
                         "The reader is open already");
    }
    FILE *file = fopen(path, "rb");
    if (file == NULL)
    {
        return sdfi_fail(&reader->message, SDFIO_ERR_IO,
                         "The file cannot be opened: %s", path);
    }
    const sdfio_stream stream = sdfio_stdio_stream(file);
    const sdfio_status status = open_source(reader, &stream);
    if (status != SDFIO_OK)
    {
        fclose(file);
        return status;
    }
    reader->file = file;
    return SDFIO_OK;
}

static sdfio_status require_readable(sdfio_reader *reader)
{
    if (reader == NULL)
    {
        return SDFIO_ERR_ARGUMENT;
    }
    if (!reader->opened)
    {
        return sdfi_fail(&reader->message, SDFIO_ERR_STATE,
                         "Open a file first");
    }
    if (reader->failed)
    {
        return sdfi_fail(&reader->message, SDFIO_ERR_STATE,
                         "An error ended the reading: rewind the reader to "
                         "start again");
    }
    return SDFIO_OK;
}

static sdfio_status stream_error(sdfio_reader *reader)
{
    return reader->source.failed
               ? sdfi_fail(&reader->message, SDFIO_ERR_IO,
                           "The stream could not be read")
               : sdfi_fail(&reader->message, SDFIO_ERR_FORMAT,
                           "Unexpected end of file while reading the SDF data "
                           "area");
}

/* The next `count` values of the data area, as the file stores them. */
static sdfio_status read_values(sdfio_reader *reader, void *dst, size_t count)
{
    const sdfio_header *header = &reader->header;
    if (header->storage == SDFIO_ASCII)
    {
        for (size_t i = 0; i < count; ++i)
        {
            const sdfio_status status =
                sdfi_ascii_value(&reader->source, header, &reader->point, dst,
                                 i, reader->next_value + i, &reader->message);
            if (status != SDFIO_OK)
            {
                return status;
            }
        }
    }
    else
    {
        const size_t size = sdfio_data_type_size(header->data_type);
        if (sdfi_source_read(&reader->source, dst, count * size)
            != count * size)
        {
            return stream_error(reader);
        }
        sdfi_fix_endianness(dst, count, size);
    }
    reader->next_value += count;
    return SDFIO_OK;
}

/* Reads or skips to the end of the data area. */
static sdfio_status finish_data(sdfio_reader *reader)
{
    if (reader->data_done)
    {
        return SDFIO_OK;
    }
    const sdfio_header *header = &reader->header;
    const bool all_read = reader->next_row == header->rows;
    if (header->storage == SDFIO_ASCII)
    {
        const sdfio_status status =
            all_read
                ? sdfi_ascii_finish(&reader->source, header, &reader->message)
                : sdfi_ascii_skip(&reader->source, header, &reader->message);
        if (status != SDFIO_OK)
        {
            return status;
        }
    }
    else if (!all_read)
    {
        const size_t size = sdfio_data_type_size(header->data_type);
        const size_t left = (reader->value_count - reader->next_value) * size;
        if (reader->source.seekable)
        {
            const int64_t end = reader->data_start
                                + (int64_t)reader->value_count * (int64_t)size;
            if (!sdfi_source_seek(&reader->source, end))
            {
                return sdfi_fail(&reader->message, SDFIO_ERR_IO,
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
    return SDFIO_OK;
}

sdfio_status sdfio_read_data(sdfio_reader *reader, void *dst, size_t rows)
{
    sdfio_status status = require_readable(reader);
    if (status != SDFIO_OK)
    {
        return status;
    }
    const sdfio_header *header = &reader->header;
    if (rows > header->rows - reader->next_row)
    {
        return sdfi_fail(&reader->message, SDFIO_ERR_ARGUMENT,
                         "%zu rows were asked for, %zu are left", rows,
                         header->rows - reader->next_row);
    }
    if (rows > 0 && header->columns > 0 && dst == NULL)
    {
        return sdfi_fail(&reader->message, SDFIO_ERR_ARGUMENT,
                         "Give the memory to read the rows into");
    }
    const size_t row_bytes =
        header->columns * sdfio_data_type_size(header->data_type);
    for (size_t i = 0; i < rows; ++i)
    {
        status = read_values(reader, (unsigned char *)dst + i * row_bytes,
                             header->columns);
        if (status != SDFIO_OK)
        {
            return fail_reading(reader, status);
        }
        ++reader->next_row;
    }
    if (reader->next_row == header->rows)
    {
        status = finish_data(reader);
        if (status != SDFIO_OK)
        {
            return fail_reading(reader, status);
        }
    }
    return SDFIO_OK;
}

sdfio_status sdfio_read_heights(sdfio_reader *reader, sdfio_format format,
                                void *dst, ptrdiff_t row_stride)
{
    sdfio_status status = require_readable(reader);
    if (status != SDFIO_OK)
    {
        return status;
    }
    const sdfio_header *header = &reader->header;
    if (reader->next_row != 0)
    {
        return sdfi_fail(&reader->message, SDFIO_ERR_STATE,
                         "Rows were read already: rewind the reader to read "
                         "the heights");
    }
    status = sdfi_check_layout(format, header->rows, header->columns,
                               row_stride, &reader->message);
    if (status != SDFIO_OK)
    {
        return status;
    }
    const ptrdiff_t element = sdfi_element_stride(format);
    if (reader->value_count > 0 && dst == NULL)
    {
        return sdfi_fail(&reader->message, SDFIO_ERR_ARGUMENT,
                         "Give the memory to read the heights into");
    }
    for (size_t file_row = 0; file_row < header->rows; ++file_row)
    {
        unsigned char *row =
            (unsigned char *)dst
            + (ptrdiff_t)sdfio_row_y(header, file_row) * row_stride;
        for (size_t first = 0; first < header->columns; first += SDFI_CHUNK)
        {
            const size_t count = header->columns - first < SDFI_CHUNK
                                     ? header->columns - first
                                     : SDFI_CHUNK;
            status = read_values(reader, reader->chunk, count);
            if (status == SDFIO_OK)
            {
                status =
                    sdfio_decode_heights(header, format, reader->chunk, count,
                                         row + (ptrdiff_t)first * element);
                if (status != SDFIO_OK)
                {
                    sdfi_fail(&reader->message, status,
                              "A height does not fit the sample type");
                }
            }
            if (status != SDFIO_OK)
            {
                return fail_reading(reader, status);
            }
        }
        ++reader->next_row;
    }
    status = finish_data(reader);
    return status == SDFIO_OK ? status : fail_reading(reader, status);
}

sdfio_status sdfio_read_trailer(sdfio_reader *reader, const char **trailer,
                                size_t *size)
{
    if (trailer == NULL || size == NULL)
    {
        return SDFIO_ERR_ARGUMENT;
    }
    if (reader != NULL && reader->trailer_read && !reader->failed)
    {
        *trailer =
            reader->trailer_present ? sdfi_string_text(&reader->trailer) : NULL;
        *size = reader->trailer.size;
        return SDFIO_OK;
    }
    sdfio_status status = require_readable(reader);
    if (status != SDFIO_OK)
    {
        return status;
    }
    status = finish_data(reader);
    if (status == SDFIO_OK)
    {
        status = reader->header.storage == SDFIO_ASCII
                     ? sdfi_ascii_trailer(
                           &reader->source, &reader->header, &reader->trailer,
                           &reader->trailer_present, &reader->message)
                     : sdfi_binary_trailer(&reader->source, &reader->trailer,
                                           &reader->trailer_present,
                                           &reader->message);
    }
    if (status != SDFIO_OK)
    {
        return fail_reading(reader, status);
    }
    reader->trailer_read = true;
    *trailer =
        reader->trailer_present ? sdfi_string_text(&reader->trailer) : NULL;
    *size = reader->trailer.size;
    return SDFIO_OK;
}

sdfio_status sdfio_rewind(sdfio_reader *reader)
{
    if (reader == NULL)
    {
        return SDFIO_ERR_ARGUMENT;
    }
    if (!reader->opened)
    {
        return sdfi_fail(&reader->message, SDFIO_ERR_STATE,
                         "Open a file first");
    }
    if (!sdfi_source_seek(&reader->source, reader->data_start))
    {
        return sdfi_fail(&reader->message, SDFIO_ERR_STATE,
                         "A source that cannot seek cannot be rewound");
    }
    reader->next_row = 0;
    reader->next_value = 0;
    reader->data_done = false;
    reader->failed = false;
    return SDFIO_OK;
}
