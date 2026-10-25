/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* The strides of a layout. */

#include "check.h"
#include "layout.h"
#include <stdint.h>

/* The check of a layout whose row stride is given separately. */
static sdfio_status layout_with(sdfio_layout layout, size_t rows,
                                size_t columns, ptrdiff_t row_stride)
{
    layout.row_stride = row_stride;
    return sdfio_layout_check(&layout, rows, columns, NULL);
}

static void check_layout(void)
{
    sdfio_layout layout = *sdfio_layout_packed(SDFIO_TYPE_DOUBLE);
    const ptrdiff_t size = sizeof(double);
    /* Row-major, with padding at the end of the rows. */
    CHECK_OK(layout_with(layout, 3, 4, 5 * size));
    /* Column-major: the columns lie beside each other. */
    layout.element_stride = 3 * size;
    CHECK_OK(layout_with(layout, 3, 4, size));
    /* Interleaved: one of three channels. */
    layout.element_stride = 3 * size;
    CHECK_OK(layout_with(layout, 3, 4, 12 * size));
    /* Mirrored. */
    layout.element_stride = -size;
    CHECK_OK(layout_with(layout, 3, 4, -4 * size));
    /* Overlapping: both strides a element. */
    layout.element_stride = 0;
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, layout_with(layout, 3, 4, size));
    /* Rows that interleave but do not fit: a stride between. */
    layout.element_stride = 2 * size;
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, layout_with(layout, 3, 4, 3 * size));
    /* A matrix of one row or column needs only one of the strides. */
    layout.element_stride = 0;
    CHECK_OK(layout_with(layout, 1, 4, 0));
    layout = *sdfio_layout_packed(SDFIO_TYPE_DOUBLE);
    layout.element_stride = 0;
    CHECK_OK(layout_with(layout, 4, 1, size));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, layout_with(layout, 4, 1, 3));
    /* Not countable. */
    layout.element_stride = PTRDIFF_MAX;
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 layout_with(layout, 2, SIZE_MAX / 2, PTRDIFF_MAX));
}

int main(void)
{
    check_layout();
    return finish_tests();
}
