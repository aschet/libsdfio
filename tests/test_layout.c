/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

#include "check.h"
#include "layout.h"
#include <stdint.h>

/* Checks a layout with a given row stride. */
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

static void check_missing_layout(void)
{
    membuf file = {0};
    const sdfio_stream stream = membuf_stream(&file);
    sdfio_header *header = new_header();
    CHECK_OK(sdfio_header_set_columns(header, 1));
    CHECK_OK(sdfio_header_set_rows(header, 1));
    sdfio_writer *writer = new_writer(all_formats());
    CHECK_OK(sdfio_create_stream(writer, &stream, header));
    const float height = 1e-6F;
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_write_data(writer, NULL, &height, 1));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_write_heights(writer, NULL, &height));
    CHECK_OK(sdfio_write_heights(writer, sdfio_layout_packed(SDFIO_TYPE_FLOAT),
                                 &height));
    CHECK_OK(sdfio_end_file(writer));
    sdfio_writer_free(writer);

    sdfio_reader *reader = new_reader(all_formats());
    CHECK_OK(sdfio_open_memory(reader, file.data, file.size));
    float back = 0.0F;
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_read_data(reader, NULL, &back, 1));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_read_heights(reader, NULL, &back));
    CHECK_OK(sdfio_read_heights(reader, sdfio_layout_packed(SDFIO_TYPE_FLOAT),
                                &back));
    sdfio_reader_free(reader);
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_decode_heights(header, NULL, &back, 1, &back));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_encode_heights(header, NULL, &back, 1, &back));
    double z_scale = 0.0;
    CHECK_STATUS(
        SDFIO_ERR_ARGUMENT,
        sdfio_suggest_z_scale(SDFIO_TYPE_INT16, NULL, &back, 1, 1, &z_scale));
    free(file.data);
}

int main(void)
{
    check_layout();
    check_missing_layout();
    return finish_tests();
}
