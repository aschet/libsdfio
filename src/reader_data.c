/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file reader_data.c
 * @brief Reading of the data area as coded values.
 */

#include "data_type.h"
#include "reader_state.h"

/* The next `count` values of the data area, as the file stores them. */
sdfio_status sdfio_reader_take_values(sdfio_reader *reader, void *dst,
                                      size_t count)
{
    const sdfio_status status = reader->codec->read_values(
        &reader->source, &reader->header, reader->codec_state, dst, count,
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
    const sdfio_status status = reader->codec->end_data(
        &reader->source, &reader->header,
        reader->next_row == reader->header.rows,
        reader->value_count - reader->next_value, reader->chunk,
        reader->chunk_values * SDFIO_LARGEST_DATA_ENTRY, &reader->message);
    if (status != SDFIO_OK)
    {
        return status;
    }
    reader->data_done = true;
    return SDFIO_OK;
}

sdfio_status sdfio_read_data(sdfio_reader *reader, void *dst, size_t rows)
{
    sdfio_status status = sdfio_reader_require_readable(reader);
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
        status = sdfio_reader_take_values(
            reader, (unsigned char *)dst + i * row_bytes, header->columns);
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

/* The reading of rows into a buffer of the C type of the data type of the
 * file, which the type of the pointer has to be. */
static sdfio_status read_typed(sdfio_reader *reader, sdfio_data_type type,
                               void *dst, size_t rows)
{
    const sdfio_status status = sdfio_reader_require_readable(reader);
    if (status != SDFIO_OK)
    {
        return status;
    }
    if (reader->header.data_type != type)
    {
        return sdfio_fail(&reader->message, SDFIO_ERR_ARGUMENT,
                          "The data type of the file is not that of the "
                          "buffer");
    }
    return sdfio_read_data(reader, dst, rows);
}

/** A function that reads rows into a buffer of a C type, for the data type. */
// A type cannot be parenthesized in the declaration of the function.
// NOLINTBEGIN(bugprone-macro-parentheses)
#define READ_TYPED(name, ctype, type)                                          \
    sdfio_status sdfio_read_data_##name(sdfio_reader *reader, ctype *dst,      \
                                        size_t rows)                           \
    {                                                                          \
        return read_typed(reader, type, dst, rows);                            \
    }
// NOLINTEND(bugprone-macro-parentheses)

READ_TYPED(int8, int8_t, SDFIO_TYPE_INT8)
READ_TYPED(int16, int16_t, SDFIO_TYPE_INT16)
READ_TYPED(int32, int32_t, SDFIO_TYPE_INT32)
READ_TYPED(float, float, SDFIO_TYPE_FLOAT)
READ_TYPED(double, double, SDFIO_TYPE_DOUBLE)
