/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief Layout of a buffer in memory.
 */

#include "layout.h"
#include "data_type.h"
#include "header.h"
#include "message.h"
#include <sdfio/sdfio.h>

static const sdfio_layout packed_layouts[] = {
    [SDFIO_TYPE_FLOAT] = {SDFIO_TYPE_FLOAT, 0, 0},
    [SDFIO_TYPE_INT8] = {SDFIO_TYPE_INT8, 0, 0},
    [SDFIO_TYPE_INT16] = {SDFIO_TYPE_INT16, 0, 0},
    [SDFIO_TYPE_INT32] = {SDFIO_TYPE_INT32, 0, 0},
    [SDFIO_TYPE_DOUBLE] = {SDFIO_TYPE_DOUBLE, 0, 0}};

void sdfio_layout_init(sdfio_layout *layout)
{
    *layout = packed_layouts[SDFIO_TYPE_FLOAT];
}

const sdfio_layout *sdfio_layout_packed(sdfio_data_type type)
{
    return sdfio_data_type_valid(type) ? &packed_layouts[type] : NULL;
}

ptrdiff_t sdfio_element_stride(sdfio_layout layout)
{
    return layout.element_stride != 0
               ? layout.element_stride
               : (ptrdiff_t)sdfio_data_type_size(layout.type);
}

/* The absolute value of a stride, also if it is the lowest ptrdiff_t. */
static size_t magnitude(ptrdiff_t stride)
{
    return stride < 0 ? (size_t)(-(stride + 1)) + 1 : (size_t)stride;
}

/* The bytes from the first byte of the first element of `count` elements that
 * are `stride` apart to the end of the last, or 0 if that is not countable. */
static size_t extent_of(size_t count, size_t stride, size_t size)
{
    if (count == 0)
    {
        return 0;
    }
    if (stride != 0 && count - 1 > (SIZE_MAX - size) / stride)
    {
        return 0;
    }
    return (count - 1) * stride + size;
}

sdfio_status sdfio_layout_check(sdfio_layout *layout, size_t rows,
                                size_t columns, sdfio_message *message)
{
    if (!sdfio_data_type_valid(layout->type))
    {
        return sdfio_fail(message, SDFIO_ERR_ARGUMENT,
                          "The type of the layout is not a data type");
    }
    const size_t size = sdfio_data_type_size(layout->type);
    const size_t element = magnitude(sdfio_element_stride(*layout));
    if (layout->row_stride == 0)
    {
        /* Rows that follow each other. */
        if (columns != 0 && element > (size_t)PTRDIFF_MAX / columns)
        {
            return sdfio_fail(message, SDFIO_ERR_ARGUMENT,
                              "The matrix exceeds the address space");
        }
        layout->row_stride = (ptrdiff_t)(columns * element);
    }
    const size_t row = magnitude(layout->row_stride);
    if (columns > 1 && element < size)
    {
        return sdfio_fail(message, SDFIO_ERR_ARGUMENT,
                          "The element stride is smaller than an element");
    }
    if (rows > 1 && row < size)
    {
        return sdfio_fail(message, SDFIO_ERR_ARGUMENT,
                          "The row stride is smaller than an element");
    }
    const size_t row_extent = extent_of(columns, element, size);
    const size_t column_extent = extent_of(rows, row, size);
    if ((columns > 1 && row_extent == 0) || (rows > 1 && column_extent == 0))
    {
        return sdfio_fail(message, SDFIO_ERR_ARGUMENT,
                          "The matrix exceeds the address space");
    }
    if (rows > 1 && columns > 1 && row < row_extent && element < column_extent)
    {
        return sdfio_fail(message, SDFIO_ERR_ARGUMENT,
                          "The strides let elements overlap: either the rows "
                          "or the columns have to lie side by side");
    }
    return SDFIO_OK;
}

sdfio_status sdfio_layout_check_heights(sdfio_layout *layout,
                                        const sdfio_header *header,
                                        sdfio_message *message)
{
    if (!sdfio_heights_type_valid(layout->type))
    {
        return sdfio_fail(message, SDFIO_ERR_ARGUMENT,
                          "The type of the heights is not SDFIO_TYPE_FLOAT or "
                          "SDFIO_TYPE_DOUBLE");
    }
    return sdfio_layout_check(layout, header->rows, header->columns, message);
}

sdfio_status sdfio_layout_check_data(sdfio_layout *layout,
                                     const sdfio_header *header, size_t rows,
                                     sdfio_message *message)
{
    if (layout->type != header->data_type)
    {
        return sdfio_fail(message, SDFIO_ERR_ARGUMENT,
                          "The type of the layout is not the data type of the "
                          "file");
    }
    return header->columns > 0
               ? sdfio_layout_check(layout, rows, header->columns, message)
               : SDFIO_OK;
}
