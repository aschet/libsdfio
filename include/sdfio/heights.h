/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * The sizes of buffers, the layout of a buffer and the conversion between coded
 * values and heights.
 */

#ifndef SDFIO_HEIGHTS_H
#define SDFIO_HEIGHTS_H

#include <sdfio/header.h>
#include <sdfio/sdfio_export.h>
#include <sdfio/types.h>

#include <math.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * Returns the size in bytes of a buffer for the whole data area: `header->rows`
 * times `header->columns` coded values of `header->data_type`. The size is
 * checked for arithmetic overflow. A buffer of this size is suitable for
 * sdfio_read_data() and sdfio_decode_heights(). The caller allocates it.
 *
 * @param[in] header The header of the file.
 * @param[out] bytes The size, which is 0 for an empty matrix.
 * @retval SDFIO_ERR_ARGUMENT A pointer is null or the data type is invalid.
 * @retval SDFIO_ERR_RANGE The size cannot be represented in a size_t.
 */
SDFIO_API sdfio_status sdfio_data_size(const sdfio_header *header,
                                       size_t *bytes);

/**
 * Returns the size in bytes of a buffer for one row of the data area, to read
 * or write the data row by row with sdfio_read_data() and sdfio_write_data().
 *
 * @param[in] header The header of the file.
 * @param[out] bytes The size, which is 0 for a row without columns.
 * @retval SDFIO_ERR_ARGUMENT A pointer is null or the data type is invalid.
 * @retval SDFIO_ERR_RANGE The size cannot be represented in a size_t.
 */
SDFIO_API sdfio_status sdfio_data_row_size(const sdfio_header *header,
                                           size_t *bytes);

/**
 * Returns the size in bytes of a buffer for the whole matrix of heights in a
 * type, for sdfio_read_heights() and sdfio_write_heights(). The caller
 * allocates it.
 *
 * @param[in] header The header of the file.
 * @param type The type of the heights: SDFIO_TYPE_FLOAT or SDFIO_TYPE_DOUBLE.
 * @param[out] bytes The size, which is 0 for an empty matrix.
 * @retval SDFIO_ERR_ARGUMENT A pointer is null or the type is not
 * SDFIO_TYPE_FLOAT or SDFIO_TYPE_DOUBLE.
 * @retval SDFIO_ERR_RANGE The size cannot be represented in a size_t.
 */
SDFIO_API sdfio_status sdfio_heights_size(const sdfio_header *header,
                                          sdfio_data_type type, size_t *bytes);

/**
 * Returns the size in bytes of a buffer for one row of heights in a
 * type, for sdfio_decode_heights() and sdfio_encode_heights() of one row of
 * the data area.
 *
 * @param[in] header The header of the file.
 * @param type The type of the heights: SDFIO_TYPE_FLOAT or SDFIO_TYPE_DOUBLE.
 * @param[out] bytes The size, which is 0 for a row without columns.
 * @retval SDFIO_ERR_ARGUMENT A pointer is null or the type is not
 * SDFIO_TYPE_FLOAT or SDFIO_TYPE_DOUBLE.
 * @retval SDFIO_ERR_RANGE The size cannot be represented in a size_t.
 */
SDFIO_API sdfio_status sdfio_heights_row_size(const sdfio_header *header,
                                              sdfio_data_type type,
                                              size_t *bytes);

/**
 * Description of a buffer of the caller for heights or for the values of the
 * data area: the type of its elements, how a point that is not measured is
 * marked in heights, and how the matrix lies in memory.
 *
 * The element (y, x) is at `data + y * row_stride + x * element_stride`. Both
 * strides can be negative. They must keep the elements apart: either the rows
 * or the columns must not overlap. This allows heights and values to be read
 * or written in place where they are not contiguous, for example the z member
 * of an array of points, one channel of an interleaved image or a column-major
 * matrix. sdfio_layout_packed() describes a plain array. A program that does
 * not use an initializer calls sdfio_layout_init() before it assigns fields:
 * the default of `not_measured` is NaN, not zero.
 */
typedef struct sdfio_layout
{
    /** Type of the elements of the buffer. For heights it is SDFIO_TYPE_FLOAT
     * or SDFIO_TYPE_DOUBLE, and for the values of the data area it is the data
     * type of the file. */
    sdfio_data_type type;
    /**
     * Marker for a point that is not measured in heights. The values of the
     * data area do not use it. NaN selects no marker: such a point is NaN. The
     * marker is compared in the precision of the type of the buffer, so -9999.1
     * marks the points of a float buffer that hold the float nearest to
     * -9999.1.
     */
    double not_measured;
    /**
     * Distance in bytes between consecutive elements within a row. 0 selects
     * elements that follow each other directly.
     */
    ptrdiff_t element_stride;
    /**
     * Distance in bytes between consecutive rows. 0 selects rows that follow
     * each other, each holding the columns of the matrix at `element_stride`.
     * A column-major matrix needs an explicit value. sdfio_decode_heights() and
     * sdfio_encode_heights() convert one row and ignore it.
     */
    ptrdiff_t row_stride;
} sdfio_layout;

/**
 * Initializes a layout of a plain array of `float` elements: the elements
 * follow each other, the rows follow each other, and NaN marks a point of
 * heights that is not measured. The caller sets the type and every other field
 * that differs.
 *
 * @param[out] layout The layout to initialize.
 */
SDFIO_API void sdfio_layout_init(sdfio_layout *layout);

/**
 * Returns the layout of a plain array: the elements follow each other, the rows
 * follow each other, and NaN marks a point of heights that is not measured.
 *
 * @param type The type of the elements: SDFIO_TYPE_FLOAT or SDFIO_TYPE_DOUBLE
 * for heights, and the data type of the file for the values of the data area.
 */
static inline sdfio_layout sdfio_layout_packed(sdfio_data_type type)
{
    sdfio_layout layout = {type, NAN, 0, 0};
    return layout;
}

/**
 * Converts values of the data area into heights in metres.
 *
 * `count` values of `header->data_type` are converted with `header->z_scale`.
 * The height i is stored at `heights + i * layout.element_stride`. A point that
 * is not measured becomes `layout.not_measured`. The function needs no handle,
 * so it works on data from any source.
 *
 * @param[in] header The header that describes the values.
 * @param layout The layout of the output buffer.
 * @param[in] data The values of the data area.
 * @param count The number of values.
 * @param[out] heights The output buffer.
 * @retval SDFIO_ERR_ARGUMENT A pointer is null or the description is invalid.
 * @retval SDFIO_ERR_RANGE A height does not fit the type of the buffer.
 */
SDFIO_API sdfio_status sdfio_decode_heights(const sdfio_header *header,
                                            sdfio_layout layout,
                                            const void *data, size_t count,
                                            void *heights);

/**
 * Converts heights in metres into values of the data area. It is the inverse of
 * sdfio_decode_heights().
 *
 * The heights are divided by `header->z_scale`. NaN, infinity and
 * `layout.not_measured` become the bad value. Integer results are rounded to
 * the nearest value.
 *
 * @param[in] header The header that describes the values.
 * @param layout The layout of the input buffer.
 * @param[in] heights The heights.
 * @param count The number of heights.
 * @param[out] data The values of the data area.
 * @retval SDFIO_ERR_ARGUMENT A pointer is null, the z-scale is not set, or the
 * description is invalid.
 * @retval SDFIO_ERR_RANGE A height does not fit the data type.
 * @retval SDFIO_ERR_BAD_VALUE A height would be read as a point that is not
 * measured.
 */
SDFIO_API sdfio_status sdfio_encode_heights(const sdfio_header *header,
                                            sdfio_layout layout,
                                            const void *heights, size_t count,
                                            void *data);

/**
 * Returns the z-scale that uses the range of an integer data type best for
 * heights between `lowest` and `highest` metres, or 1 for a floating point
 * data type.
 *
 * @param type The data type.
 * @param lowest The lowest height in metres.
 * @param highest The highest height in metres.
 */
SDFIO_API double sdfio_z_scale_for_range(sdfio_data_type type, double lowest,
                                         double highest);

/**
 * Determines the z-scale for a matrix of heights in metres, as
 * sdfio_write_heights() does: the range of the finite heights, excluding the
 * points that `layout.not_measured` marks, is determined and passed to
 * sdfio_z_scale_for_range(). It is intended for sdfio_write_data() with an
 * integer data type.
 *
 * @param type The data type.
 * @param layout The layout of the buffer.
 * @param[in] heights The heights.
 * @param rows The number of rows.
 * @param columns The number of columns.
 * @param[out] z_scale The z-scale.
 * @retval SDFIO_ERR_ARGUMENT A pointer is null or the strides are invalid.
 */
SDFIO_API sdfio_status sdfio_suggest_z_scale(sdfio_data_type type,
                                             sdfio_layout layout,
                                             const void *heights, size_t rows,
                                             size_t columns, double *z_scale);

#ifdef __cplusplus
}
#endif

#endif /* SDFIO_HEIGHTS_H */
