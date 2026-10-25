/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * Conversion between coded values and heights.
 *
 * @par Coded Values and Heights
 * The data area holds coded values in the data type of the file. A coded value
 * multiplied by the z-scale of the header is the height of a point in
 * metres. A program uses the highest of three layers that suffices:
 * - sdfio_read_data() and sdfio_write_data() transfer the coded values as the
 *   file has them.
 * - sdfio_decode_heights() and sdfio_encode_heights() convert between coded
 *   values and heights in metres. They need a header and no reader or writer.
 * - sdfio_read_heights() and sdfio_write_heights() transfer the whole matrix as
 *   heights in metres.
 *
 * @par Points That Are Not Measured
 * A point that is not measured has the bad value of the data type as a coded
 * value, and NaN as a height. The encoding of heights takes NaN and infinity
 * as such a point.
 *
 * @par Z-Scale
 * A file of an integer type trades range against resolution by its z-scale.
 * sdfio_suggest_z_scale_for_range() and sdfio_suggest_z_scale() give the
 * smallest z-scale that fits the heights, and sdfio_write_heights() uses it if
 * the header has SDFIO_AUTO_Z_SCALE.
 */

#ifndef SDFIO_HEIGHTS_H
#define SDFIO_HEIGHTS_H

#include <sdfio/header.h>
#include <sdfio/layout.h>
#include <sdfio/sdfio_export.h>
#include <sdfio/types.h>

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * Converts values of the data area into heights in metres.
 *
 * `count` values of `header->data_type` are converted with `header->z_scale`.
 * The height i is stored at `heights + i * layout->element_stride`. A point
 * that is not measured becomes NaN. The function needs no handle, so it works
 * on data from any source.
 *
 * @param[in] header The header that describes the values.
 * @param[in] layout The layout of the output buffer.
 * @param[in] data The values of the data area.
 * @param count The number of values.
 * @param[out] heights The output buffer.
 * @retval SDFIO_ERR_ARGUMENT A pointer is null or the description is invalid.
 * @retval SDFIO_ERR_RANGE A height does not fit the type of the buffer.
 */
SDFIO_API sdfio_status sdfio_decode_heights(const sdfio_header *header,
                                            const sdfio_layout *layout,
                                            const void *data, size_t count,
                                            void *heights);

/**
 * Converts heights in metres into values of the data area. It is the inverse of
 * sdfio_decode_heights().
 *
 * The heights are divided by `header->z_scale`. NaN and infinity become the bad
 * value. Integer results are rounded to the nearest value.
 *
 * @param[in] header The header that describes the values.
 * @param[in] layout The layout of the input buffer.
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
                                            const sdfio_layout *layout,
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
SDFIO_API double sdfio_suggest_z_scale_for_range(sdfio_data_type type,
                                                 double lowest, double highest);

/**
 * Determines the z-scale for a matrix of heights in metres, as
 * sdfio_write_heights() does: the range of the finite heights is determined and
 * passed to sdfio_suggest_z_scale_for_range(). It is intended for
 * sdfio_write_data() with an integer data type.
 *
 * @param type The data type.
 * @param[in] layout The layout of the buffer.
 * @param[in] heights The heights.
 * @param rows The number of rows.
 * @param columns The number of columns.
 * @param[out] z_scale The z-scale.
 * @retval SDFIO_ERR_ARGUMENT A pointer is null or the strides are invalid.
 */
SDFIO_API sdfio_status sdfio_suggest_z_scale(sdfio_data_type type,
                                             const sdfio_layout *layout,
                                             const void *heights, size_t rows,
                                             size_t columns, double *z_scale);

#ifdef __cplusplus
}
#endif

#endif /* SDFIO_HEIGHTS_H */
