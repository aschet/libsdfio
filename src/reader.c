/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file reader.c
 * @brief The reader: opening a stream, reading the header, the heights and
 * the trailer.
 */

#include "internal.h"

/** A stream over memory of the caller, for sdfio_open_memory(). */
typedef struct memory_stream
{
    /** The bytes. */
    const unsigned char *data;
    /** The number of bytes. */
    size_t size;
    /** The index of the next byte. */
    size_t position;
} memory_stream;

/** The state of a reader. */
struct sdfio_reader
{
    /** The text of the last failure. */
    sdfio_message message;
    /** The header of the open file. */
    sdfio_header header;
    /** The buffered stream. */
    sdfio_source source;
    /** The decimal point of the locale, measured when the file was opened. */
    sdfio_point point;
    /** The allocator of the configuration. */
    sdfio_allocator allocator;
    /** The time zone of the configuration. */
    sdfio_time_zone zone;
    /** What a file adds to the reader. */
    sdfio_owner owner;
    /** The stream over memory, if the file is in memory. */
    memory_stream memory;
    /** Whether a file is open. */
    bool opened;
    /** The number of values of the data area. */
    size_t value_count;
    /** The index of the next row to read. */
    size_t next_row;
    /** The index of the next value to read. */
    size_t next_value;
    /** Whether the end of the data area was passed. */
    bool data_done;
    /** Whether an error in the middle of the data lost the position. */
    bool failed;
    /** Whether the trailer was read. */
    bool trailer_read;
    /** Whether the file has a trailer. */
    bool trailer_present;
    /** The trailer. */
    sdfio_string trailer;
    /** The room for a chunk of values. */
    unsigned char *chunk;
    /** The number of values in a chunk. */
    size_t chunk_values;
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

sdfio_reader *sdfio_reader_new(const sdfio_config *config)
{
    sdfio_allocator allocator;
    sdfio_time_zone zone;
    size_t buffer = 0;
    size_t chunk = 0;
    if (!sdfio_config_resolve(config, &allocator, &buffer, &chunk, &zone))
    {
        return NULL;
    }
    sdfio_reader *reader = sdfio_allocate(&allocator, sizeof *reader);
    if (reader == NULL)
    {
        return NULL;
    }
    *reader = (sdfio_reader){
        .allocator = allocator, .zone = zone, .chunk_values = chunk};
    reader->trailer.allocator = &reader->allocator;
    reader->chunk =
        sdfio_allocate(&reader->allocator, chunk * SDFIO_LARGEST_SAMPLE);
    if (reader->chunk == NULL
        || !sdfio_source_create(&reader->source, buffer, &reader->allocator))
    {
        sdfio_reader_free(reader);
        return NULL;
    }
    return reader;
}

void sdfio_reader_free(sdfio_reader *reader)
{
    if (reader == NULL)
    {
        return;
    }
    if (reader->owner.close != NULL)
    {
        reader->owner.close(reader->owner.user);
    }
    sdfio_string_free(&reader->trailer);
    sdfio_source_destroy(&reader->source);
    const sdfio_allocator allocator = reader->allocator;
    sdfio_release(&allocator, reader->chunk);
    sdfio_release(&allocator, reader);
}

void sdfio_reader_own(sdfio_reader *reader, sdfio_owner owner)
{
    reader->owner = owner;
}

sdfio_message *sdfio_reader_message(sdfio_reader *reader)
{
    return &reader->message;
}

bool sdfio_reader_is_open(const sdfio_reader *reader)
{
    return reader->opened;
}

const char *sdfio_reader_error_message(const sdfio_reader *reader)
{
    return reader != NULL && reader->message.text != NULL ? reader->message.text
                                                          : "";
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

static sdfio_status open_source(sdfio_reader *reader,
                                const sdfio_stream *stream)
{
    if (reader->opened)
    {
        return sdfio_fail(&reader->message, SDFIO_ERR_STATE,
                          "The reader is open already");
    }
    sdfio_source_init(&reader->source, stream);
    reader->point = sdfio_point_now();
    sdfio_status status =
        sdfio_read_header(&reader->source, &reader->header, &reader->point,
                          &reader->zone, &reader->message);
    if (status != SDFIO_OK)
    {
        return status;
    }
    const sdfio_header *header = &reader->header;
    const size_t type_size = sdfio_data_type_size(header->data_type);
    if (!sdfio_count(header, &reader->value_count)
        || reader->value_count > SIZE_MAX / type_size)
    {
        return sdfio_fail(&reader->message, SDFIO_ERR_FORMAT,
                          "The SDF file has more points than can be counted");
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
    const sdfio_stream stream = {&reader->memory, memory_read, NULL};
    return open_source(reader, &stream);
}

static sdfio_status require_readable(sdfio_reader *reader)
{
    if (reader == NULL)
    {
        return SDFIO_ERR_ARGUMENT;
    }
    if (!reader->opened)
    {
        return sdfio_fail(&reader->message, SDFIO_ERR_STATE,
                          "Open a file first");
    }
    if (reader->failed)
    {
        return sdfio_fail(&reader->message, SDFIO_ERR_STATE,
                          "An error ended the reading");
    }
    return SDFIO_OK;
}

static sdfio_status stream_error(sdfio_reader *reader)
{
    return reader->source.failed
               ? sdfio_fail(&reader->message, SDFIO_ERR_IO,
                            "The stream could not be read")
               : sdfio_fail(&reader->message, SDFIO_ERR_FORMAT,
                            "Unexpected end of file while reading the SDF data "
                            "area");
}

/* The next `count` values of the data area, as the file stores them. */
static sdfio_status read_values(sdfio_reader *reader, void *dst, size_t count)
{
    const sdfio_header *header = &reader->header;
    if (header->representation == SDFIO_ASCII)
    {
        for (size_t i = 0; i < count; ++i)
        {
            const sdfio_status status =
                sdfio_ascii_value(&reader->source, header, &reader->point, dst,
                                  i, &reader->message);
            if (status != SDFIO_OK)
            {
                return status;
            }
        }
    }
    else
    {
        const size_t size = sdfio_data_type_size(header->data_type);
        if (sdfio_source_read(&reader->source, dst, count * size)
            != count * size)
        {
            return stream_error(reader);
        }
        sdfio_fix_endianness(dst, count, size);
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
    if (header->representation == SDFIO_ASCII)
    {
        const sdfio_status status =
            all_read
                ? sdfio_ascii_finish(&reader->source, header, &reader->message)
                : sdfio_ascii_skip(&reader->source, header, &reader->message);
        if (status != SDFIO_OK)
        {
            return status;
        }
    }
    else if (!all_read)
    {
        const size_t size = sdfio_data_type_size(header->data_type);
        const size_t left = (reader->value_count - reader->next_value) * size;
        const size_t piece = reader->chunk_values * SDFIO_LARGEST_SAMPLE;
        for (size_t done = 0; done < left;)
        {
            const size_t step = left - done < piece ? left - done : piece;
            if (sdfio_source_read(&reader->source, reader->chunk, step) != step)
            {
                return stream_error(reader);
            }
            done += step;
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
        return sdfio_fail(&reader->message, SDFIO_ERR_ARGUMENT,
                          "More rows were asked for than are left");
    }
    if (rows > 0 && header->columns > 0 && dst == NULL)
    {
        return sdfio_fail(&reader->message, SDFIO_ERR_ARGUMENT,
                          "Give the memory to read the rows into");
    }
    const size_t row_bytes =
        header->columns * sdfio_data_type_size(header->data_type);
    /* Rows without a column hold nothing, however many a file declares. */
    const size_t valued_rows = header->columns > 0 ? rows : 0;
    for (size_t i = 0; i < valued_rows; ++i)
    {
        status = read_values(reader, (unsigned char *)dst + i * row_bytes,
                             header->columns);
        if (status != SDFIO_OK)
        {
            return fail_reading(reader, status);
        }
        ++reader->next_row;
    }
    reader->next_row += rows - valued_rows;
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

sdfio_status sdfio_read_heights(sdfio_reader *reader, sdfio_sample sample,
                                void *dst)
{
    return sdfio_read_heights_strided(reader, sdfio_format_packed(sample), dst);
}

sdfio_status sdfio_read_heights_strided(sdfio_reader *reader,
                                        sdfio_format format, void *dst)
{
    sdfio_status status = require_readable(reader);
    if (status != SDFIO_OK)
    {
        return status;
    }
    const sdfio_header *header = &reader->header;
    if (reader->next_row != 0)
    {
        return sdfio_fail(&reader->message, SDFIO_ERR_STATE,
                          "Rows were read already");
    }
    status = sdfio_check_layout(&format, header->rows, header->columns,
                                &reader->message);
    if (status != SDFIO_OK)
    {
        return status;
    }
    const ptrdiff_t element = sdfio_element_stride(format);
    if (reader->value_count > 0 && dst == NULL)
    {
        return sdfio_fail(&reader->message, SDFIO_ERR_ARGUMENT,
                          "Give the memory to read the heights into");
    }
    const size_t valued_rows = header->columns > 0 ? header->rows : 0;
    for (size_t file_row = 0; file_row < valued_rows; ++file_row)
    {
        unsigned char *row =
            (unsigned char *)dst
            + (ptrdiff_t)sdfio_row_y(header, file_row) * format.row_stride;
        for (size_t first = 0; first < header->columns;
             first += reader->chunk_values)
        {
            const size_t count = header->columns - first < reader->chunk_values
                                     ? header->columns - first
                                     : reader->chunk_values;
            status = read_values(reader, reader->chunk, count);
            if (status == SDFIO_OK)
            {
                status =
                    sdfio_decode_heights(header, format, reader->chunk, count,
                                         row + (ptrdiff_t)first * element);
                if (status != SDFIO_OK)
                {
                    sdfio_fail(&reader->message, status,
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
    reader->next_row = header->rows;
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
        *trailer = reader->trailer_present ? sdfio_string_text(&reader->trailer)
                                           : NULL;
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
        status = reader->header.representation == SDFIO_ASCII
                     ? sdfio_ascii_trailer(
                           &reader->source, &reader->header, &reader->trailer,
                           &reader->trailer_present, &reader->message)
                     : sdfio_binary_trailer(&reader->source, &reader->trailer,
                                            &reader->trailer_present,
                                            &reader->message);
    }
    if (status != SDFIO_OK)
    {
        return fail_reading(reader, status);
    }
    reader->trailer_read = true;
    *trailer =
        reader->trailer_present ? sdfio_string_text(&reader->trailer) : NULL;
    *size = reader->trailer.size;
    return SDFIO_OK;
}
