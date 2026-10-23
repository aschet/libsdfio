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
#include "writer_state.h"

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
        size_t entries = 0;
        sdfio_data_entries(header, &entries);
        if (sdfio_is_floating(header->data_type) || entries == 0)
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

sdfio_status sdfio_write_data(sdfio_writer *writer, const void *src,
                              size_t rows)
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
    status = sdfio_writer_start_data(writer);
    if (status != SDFIO_OK)
    {
        return status == SDFIO_ERR_ARGUMENT ? status
                                            : sdfio_writer_fail(writer, status);
    }
    const size_t row_bytes =
        header->columns * sdfio_data_type_size(header->data_type);
    /* Rows without a column hold nothing, however many a file declares. */
    const size_t valued_rows = header->columns > 0 ? rows : 0;
    for (size_t i = 0; i < valued_rows; ++i)
    {
        status = sdfio_writer_put_values(
            writer, (const unsigned char *)src + i * row_bytes,
            header->columns);
        if (status != SDFIO_OK)
        {
            return status;
        }
        ++writer->next_row;
    }
    writer->next_row += rows - valued_rows;
    return SDFIO_OK;
}

/* The writing of rows from a buffer of the C type of the data type of the
 * file, which the type of the pointer has to be. */
static sdfio_status write_typed(sdfio_writer *writer, sdfio_data_type type,
                                const void *src, size_t rows)
{
    const sdfio_status status = sdfio_writer_require_writable(writer);
    if (status != SDFIO_OK)
    {
        return status;
    }
    if (writer->header.data_type != type)
    {
        return sdfio_fail(&writer->message, SDFIO_ERR_ARGUMENT,
                          "The data type of the file is not that of the "
                          "buffer");
    }
    return sdfio_write_data(writer, src, rows);
}

/** A function that writes rows from a buffer of a C type, for the data type. */
// A type cannot be parenthesized in the declaration of the function.
// NOLINTBEGIN(bugprone-macro-parentheses)
#define WRITE_TYPED(name, ctype, type)                                         \
    sdfio_status sdfio_write_data_##name(sdfio_writer *writer,                 \
                                         const ctype *src, size_t rows)        \
    {                                                                          \
        return write_typed(writer, type, src, rows);                           \
    }
// NOLINTEND(bugprone-macro-parentheses)

WRITE_TYPED(int8, int8_t, SDFIO_TYPE_INT8)
WRITE_TYPED(int16, int16_t, SDFIO_TYPE_INT16)
WRITE_TYPED(int32, int32_t, SDFIO_TYPE_INT32)
WRITE_TYPED(float, float, SDFIO_TYPE_FLOAT)
WRITE_TYPED(double, double, SDFIO_TYPE_DOUBLE)
