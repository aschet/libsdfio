/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief Writing of rows as coded values.
 */

#include "chunk.h"
#include "data_type.h"
#include "header.h"
#include "layout.h"
#include "message.h"
#include "writer_state.h"
#include <string.h>

sdfio_status sdfio_writer_start_data(sdfio_writer *writer)
{
    if (writer->header_written)
    {
        return SDFIO_OK;
    }
    sdfio_header *header = &writer->header;
    if (header->z_scale == SDFIO_AUTO_Z_SCALE)
    {
        size_t values = 0;
        sdfio_coded_values(header, &values);
        if (sdfio_is_floating(header->data_type) || values == 0)
        {
            header->z_scale = 1.0;
        }
        else
        {
            return sdfio_fail(&writer->message, SDFIO_ERR_ARGUMENT,
                              "Coded integer values need a z-scale, the height "
                              "in metres that a coded value of 1 stands for: "
                              "sdfio_write_heights() chooses one");
        }
    }
    const sdfio_status status = writer->format->write_header(
        &writer->sink, header, writer->format_state, &writer->options.zone,
        &writer->message);
    if (status != SDFIO_OK)
    {
        return sdfio_writer_fail(writer, status);
    }
    writer->header_written = true;
    return SDFIO_OK;
}

sdfio_status sdfio_writer_put_values(sdfio_writer *writer, const void *values,
                                     size_t count)
{
    const sdfio_status status = writer->format->write_values(
        &writer->sink, &writer->header, writer->format_state, values, count,
        writer->file_chunk, writer->options.chunk_values, &writer->message);
    return status == SDFIO_OK ? status : sdfio_writer_fail(writer, status);
}

static sdfio_status write_row(sdfio_writer *writer, const unsigned char *row,
                              ptrdiff_t element, size_t size)
{
    const sdfio_header *header = &writer->header;
    if (element == (ptrdiff_t)size)
    {
        return sdfio_writer_put_values(writer, row, header->columns);
    }
    for (size_t first = 0; first < header->columns;
         first += writer->options.chunk_values)
    {
        const size_t count = sdfio_chunk_count(header->columns, first,
                                               writer->options.chunk_values);
        for (size_t i = 0; i < count; ++i)
        {
            memcpy(writer->coded + i * size,
                   row + (ptrdiff_t)(first + i) * element, size);
        }
        const sdfio_status status =
            sdfio_writer_put_values(writer, writer->coded, count);
        if (status != SDFIO_OK)
        {
            return status;
        }
    }
    return SDFIO_OK;
}

sdfio_status sdfio_write_data(sdfio_writer *writer, const sdfio_layout *layout,
                              const void *src, size_t rows)
{
    sdfio_status status = sdfio_writer_require_writable(writer);
    if (status != SDFIO_OK)
    {
        return status;
    }
    const sdfio_header *header = &writer->header;
    if (rows > header->rows - writer->next_row)
    {
        return sdfio_fail(&writer->message, SDFIO_ERR_ARGUMENT,
                          "More rows are given than the file has left");
    }
    if (rows > 0 && header->columns > 0 && src == NULL)
    {
        return sdfio_fail(&writer->message, SDFIO_ERR_ARGUMENT,
                          "The rows to write are null");
    }
    if (layout == NULL)
    {
        return sdfio_fail(&writer->message, SDFIO_ERR_ARGUMENT,
                          "The layout of the buffer is null");
    }
    sdfio_layout resolved = *layout;
    status = sdfio_layout_check_data(&resolved, header, rows, &writer->message);
    if (status != SDFIO_OK)
    {
        return status;
    }
    status = sdfio_writer_start_data(writer);
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
        status = write_row(writer,
                           (const unsigned char *)src
                               + (ptrdiff_t)i * resolved.row_stride,
                           element, size);
        if (status != SDFIO_OK)
        {
            return status;
        }
        ++writer->next_row;
    }
    writer->next_row += rows - valued_rows;
    return SDFIO_OK;
}
