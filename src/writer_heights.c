/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file writer_heights.c
 * @brief Writing of the data area from heights.
 */

#include "chunk.h"
#include "data_type.h"
#include "header.h"
#include "heights.h"
#include "writer_state.h"

sdfio_status sdfio_write_heights(sdfio_writer *writer, sdfio_layout layout,
                                 const void *src)
{
    sdfio_status status = sdfio_writer_require_writable(writer);
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
    status = sdfio_check_heights_layout(&layout, header, &writer->message);
    if (status != SDFIO_OK)
    {
        return status;
    }
    const ptrdiff_t element = sdfio_element_stride(layout);
    if (header->rows > 0 && header->columns > 0 && src == NULL)
    {
        return sdfio_fail(&writer->message, SDFIO_ERR_ARGUMENT,
                          "Give the heights to write");
    }
    if (header->z_scale == SDFIO_AUTO_Z_SCALE
        && !sdfio_is_floating(header->data_type))
    {
        status =
            sdfio_suggest_z_scale(header->data_type, layout, src, header->rows,
                                  header->columns, &header->z_scale);
        if (status != SDFIO_OK)
        {
            return status;
        }
    }
    status = sdfio_writer_start_data(writer);
    if (status != SDFIO_OK)
    {
        return status;
    }
    const size_t valued_rows = header->columns > 0 ? header->rows : 0;
    for (size_t file_row = 0; file_row < valued_rows; ++file_row)
    {
        const unsigned char *row = (const unsigned char *)src
                                   + (ptrdiff_t)file_row * layout.row_stride;
        for (size_t first = 0; first < header->columns;
             first += writer->chunk_values)
        {
            const size_t count =
                sdfio_chunk_count(header->columns, first, writer->chunk_values);
            status = sdfio_encode_heights(header, layout,
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
                status = sdfio_writer_put_values(writer, writer->coded, count);
            }
            if (status != SDFIO_OK)
            {
                return sdfio_writer_fail(writer, status);
            }
        }
        ++writer->next_row;
    }
    writer->next_row = header->rows;
    return SDFIO_OK;
}
