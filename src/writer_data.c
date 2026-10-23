/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file writer_data.c
 * @brief Writing of the data area as coded values.
 */

#include "data_type.h"
#include "header.h"
#include "heights.h"
#include "writer_state.h"
#include <string.h>

/* The header goes out with the first values, when the z-scale is known. */
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
            return sdfio_fail(
                &writer->message, SDFIO_ERR_ARGUMENT,
                "Stored integer values need a z-scale, the "
                "height in metres that a coded value of 1 stands for: use "
                "sdfio_write_heights() "
                "to have one chosen");
        }
    }
    const sdfio_status status = writer->format->write_header(
        &writer->sink, header, writer->format_state, &writer->zone,
        &writer->allocator, &writer->message);
    if (status == SDFIO_ERR_IO)
    {
        return sdfio_writer_fail(writer, status);
    }
    writer->header_written = true;
    return status;
}

/* Stored values of the machine into the file. */
sdfio_status sdfio_writer_put_values(sdfio_writer *writer, const void *values,
                                     size_t count)
{
    const sdfio_status status = writer->format->write_values(
        &writer->sink, &writer->header, writer->format_state, values, count,
        writer->scratch, writer->chunk_values, &writer->message);
    return status == SDFIO_OK ? status : sdfio_writer_fail(writer, status);
}

/* Writes the values of a row from a buffer whose elements are `element` bytes
 * apart, each of them `size` bytes. */
static sdfio_status write_row(sdfio_writer *writer, const unsigned char *row,
                              ptrdiff_t element, size_t size)
{
    const sdfio_header *header = &writer->header;
    if (element == (ptrdiff_t)size)
    {
        return sdfio_writer_put_values(writer, row, header->columns);
    }
    for (size_t first = 0; first < header->columns;
         first += writer->chunk_values)
    {
        const size_t count = header->columns - first < writer->chunk_values
                                 ? header->columns - first
                                 : writer->chunk_values;
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

sdfio_status sdfio_write_data(sdfio_writer *writer, sdfio_layout layout,
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
                          "More rows were given than are left to write");
    }
    if (rows > 0 && header->columns > 0 && src == NULL)
    {
        return sdfio_fail(&writer->message, SDFIO_ERR_ARGUMENT,
                          "Give the rows to write");
    }
    if (layout.type != header->data_type)
    {
        return sdfio_fail(&writer->message, SDFIO_ERR_ARGUMENT,
                          "The type of the layout is not the data type of the "
                          "file");
    }
    /* Rows without a column hold nothing, so the layout is not used. */
    status = header->columns > 0
                 ? sdfio_check_layout(&layout, rows, header->columns,
                                      &writer->message)
                 : SDFIO_OK;
    if (status != SDFIO_OK)
    {
        return status;
    }
    status = sdfio_writer_start_data(writer);
    if (status != SDFIO_OK)
    {
        return status == SDFIO_ERR_ARGUMENT ? status
                                            : sdfio_writer_fail(writer, status);
    }
    const ptrdiff_t element = sdfio_element_stride(layout);
    const size_t size = sdfio_data_type_size(header->data_type);
    /* Rows without a column hold nothing, however many a file declares. */
    const size_t valued_rows = header->columns > 0 ? rows : 0;
    for (size_t i = 0; i < valued_rows; ++i)
    {
        status = write_row(writer,
                           (const unsigned char *)src
                               + (ptrdiff_t)i * layout.row_stride,
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
