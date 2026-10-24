/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file reader_data.c
 * @brief Reading of the data area as coded values.
 */

#include "chunk.h"
#include "data_type.h"
#include "header.h"
#include "heights.h"
#include "reader_state.h"
#include <string.h>

/* The next `count` values of the data area, as the file stores them. */
sdfio_status sdfio_reader_take_values(sdfio_reader *reader, void *dst,
                                      size_t count)
{
    const sdfio_status status = reader->format->read_values(
        &reader->source, &reader->header, reader->format_state, dst, count,
        &reader->message);
    if (status == SDFIO_OK)
    {
        reader->next_value += count;
    }
    return status;
}

/* Reads or skips to the end of the data area. */
sdfio_status sdfio_reader_finish_data(sdfio_reader *reader)
{
    if (reader->data_done)
    {
        return SDFIO_OK;
    }
    const sdfio_status status = reader->format->end_data(
        &reader->source, &reader->header,
        reader->next_row == reader->header.rows,
        reader->value_count - reader->next_value, reader->chunk,
        reader->options.chunk_values * SDFIO_LARGEST_CODED_VALUE,
        &reader->message);
    if (status != SDFIO_OK)
    {
        return status;
    }
    reader->data_done = true;
    return SDFIO_OK;
}

/* Reads the values of a row into a buffer whose elements are `element` bytes
 * apart, each of them `size` bytes. */
static sdfio_status read_row(sdfio_reader *reader, unsigned char *row,
                             ptrdiff_t element, size_t size)
{
    const sdfio_header *header = &reader->header;
    if (element == (ptrdiff_t)size)
    {
        return sdfio_reader_take_values(reader, row, header->columns);
    }
    for (size_t first = 0; first < header->columns;
         first += reader->options.chunk_values)
    {
        const size_t count = sdfio_chunk_count(header->columns, first,
                                               reader->options.chunk_values);
        const sdfio_status status =
            sdfio_reader_take_values(reader, reader->chunk, count);
        if (status != SDFIO_OK)
        {
            return status;
        }
        for (size_t i = 0; i < count; ++i)
        {
            memcpy(row + (ptrdiff_t)(first + i) * element,
                   reader->chunk + i * size, size);
        }
    }
    return SDFIO_OK;
}

sdfio_status sdfio_read_data(sdfio_reader *reader, const sdfio_layout *layout,
                             void *dst, size_t rows)
{
    sdfio_status status = sdfio_reader_require_readable(reader);
    if (status != SDFIO_OK)
    {
        return status;
    }
    const sdfio_header *header = &reader->header;
    if (reader->trailer_begun && rows > 0)
    {
        return sdfio_fail(
            &reader->message, SDFIO_ERR_STATE,
            "No row can be read after the reading of the trailer has begun");
    }
    if (rows > header->rows - reader->next_row)
    {
        return sdfio_fail(&reader->message, SDFIO_ERR_ARGUMENT,
                          "More rows are requested than the file has left");
    }
    if (rows > 0 && header->columns > 0 && dst == NULL)
    {
        return sdfio_fail(&reader->message, SDFIO_ERR_ARGUMENT,
                          "The memory to read the rows into is null");
    }
    if (layout == NULL)
    {
        return sdfio_fail(&reader->message, SDFIO_ERR_ARGUMENT,
                          "The layout of the buffer is null");
    }
    sdfio_layout resolved = *layout;
    status = sdfio_layout_check_data(&resolved, header, rows, &reader->message);
    if (status != SDFIO_OK)
    {
        return status;
    }
    const ptrdiff_t element = sdfio_element_stride(resolved);
    const size_t size = sdfio_data_type_size(header->data_type);
    /* Rows without a column hold nothing, however many a file declares. */
    const size_t valued_rows = header->columns > 0 ? rows : 0;
    for (size_t i = 0; i < valued_rows; ++i)
    {
        status = read_row(
            reader, (unsigned char *)dst + (ptrdiff_t)i * resolved.row_stride,
            element, size);
        if (status != SDFIO_OK)
        {
            return sdfio_reader_fail(reader, status);
        }
        ++reader->next_row;
    }
    reader->next_row += rows - valued_rows;
    if (reader->next_row == header->rows)
    {
        status = sdfio_reader_finish_data(reader);
        if (status != SDFIO_OK)
        {
            return sdfio_reader_fail(reader, status);
        }
    }
    return SDFIO_OK;
}
