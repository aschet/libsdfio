/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file sizes.c
 * @brief The size of the buffers that the caller allocates for the data and
 * the heights.
 */

#include "internal.h"

/* The bytes of `rows` rows of `columns` elements, or false if the size does
 * not fit in size_t. */
static sdfio_status size_of(size_t columns, size_t rows, size_t element_size,
                            size_t *bytes)
{
    if (columns != 0 && rows > SIZE_MAX / columns)
    {
        return SDFIO_ERR_RANGE;
    }
    const size_t count = columns * rows;
    if (count > SIZE_MAX / element_size)
    {
        return SDFIO_ERR_RANGE;
    }
    *bytes = count * element_size;
    return SDFIO_OK;
}

static sdfio_status data_bytes(const sdfio_header *header, size_t rows,
                               size_t *bytes)
{
    if (header == NULL || bytes == NULL
        || !sdfio_valid_data_type(header->data_type))
    {
        return SDFIO_ERR_ARGUMENT;
    }
    return size_of(header->columns, rows,
                   sdfio_data_type_size(header->data_type), bytes);
}

static sdfio_status heights_bytes(const sdfio_header *header,
                                  sdfio_sample sample, size_t rows,
                                  size_t *bytes)
{
    if (header == NULL || bytes == NULL || !sdfio_valid_sample(sample))
    {
        return SDFIO_ERR_ARGUMENT;
    }
    return size_of(header->columns, rows, sdfio_sample_size(sample), bytes);
}

sdfio_status sdfio_data_size(const sdfio_header *header, size_t *bytes)
{
    return header == NULL ? SDFIO_ERR_ARGUMENT
                          : data_bytes(header, header->rows, bytes);
}

sdfio_status sdfio_data_row_size(const sdfio_header *header, size_t *bytes)
{
    return data_bytes(header, 1, bytes);
}

sdfio_status sdfio_heights_size(const sdfio_header *header, sdfio_sample sample,
                                size_t *bytes)
{
    return header == NULL ? SDFIO_ERR_ARGUMENT
                          : heights_bytes(header, sample, header->rows, bytes);
}

sdfio_status sdfio_heights_row_size(const sdfio_header *header,
                                    sdfio_sample sample, size_t *bytes)
{
    return heights_bytes(header, sample, 1, bytes);
}
