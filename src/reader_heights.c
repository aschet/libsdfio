/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file reader_heights.c
 * @brief Reading of the data area as heights.
 */

#include "chunk.h"
#include "data_type.h"
#include "header.h"
#include "heights.h"
#include "reader_state.h"

sdfio_status sdfio_read_heights(sdfio_reader *reader,
                                const sdfio_layout *layout_of_buffer,
                                double not_measured, void *dst)
{
    sdfio_status status = sdfio_reader_require_readable(reader);
    if (status != SDFIO_OK)
    {
        return status;
    }
    const sdfio_header *header = &reader->header;
    if (reader->next_row != 0 || reader->trailer_begun)
    {
        return sdfio_fail(&reader->message, SDFIO_ERR_STATE,
                          "Rows have been read already, or the reading of the "
                          "trailer has begun");
    }
    if (layout_of_buffer == NULL)
    {
        return sdfio_fail(&reader->message, SDFIO_ERR_ARGUMENT,
                          "The layout of the buffer is null");
    }
    sdfio_layout layout = *layout_of_buffer;
    status = sdfio_layout_check_heights(&layout, header, &reader->message);
    if (status != SDFIO_OK)
    {
        return status;
    }
    const ptrdiff_t element = sdfio_element_stride(layout);
    if (reader->value_count > 0 && dst == NULL)
    {
        return sdfio_fail(&reader->message, SDFIO_ERR_ARGUMENT,
                          "The memory to read the heights into is null");
    }
    const size_t valued_rows = header->columns > 0 ? header->rows : 0;
    for (size_t file_row = 0; file_row < valued_rows; ++file_row)
    {
        unsigned char *row =
            (unsigned char *)dst + (ptrdiff_t)file_row * layout.row_stride;
        for (size_t first = 0; first < header->columns;
             first += reader->options.chunk_values)
        {
            const size_t count = sdfio_chunk_count(
                header->columns, first, reader->options.chunk_values);
            status = sdfio_reader_take_values(reader, reader->chunk, count);
            if (status == SDFIO_OK)
            {
                status = sdfio_decode_heights(header, &layout, not_measured,
                                              reader->chunk, count,
                                              row + (ptrdiff_t)first * element);
                if (status != SDFIO_OK)
                {
                    sdfio_fail(&reader->message, status,
                               "A height does not fit the type of the buffer");
                }
            }
            if (status != SDFIO_OK)
            {
                return sdfio_reader_fail(reader, status);
            }
        }
        ++reader->next_row;
    }
    reader->next_row = header->rows;
    status = sdfio_reader_finish_data(reader);
    return status == SDFIO_OK ? status : sdfio_reader_fail(reader, status);
}
