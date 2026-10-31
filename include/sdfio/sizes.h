/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * Sizes of buffers for the data area and for heights.
 */

#ifndef SDFIO_SIZES_H
#define SDFIO_SIZES_H

#include <sdfio/header.h>
#include <sdfio/sdfio_export.h>
#include <sdfio/types.h>

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

#ifdef __cplusplus
}
#endif

#endif /* SDFIO_SIZES_H */
