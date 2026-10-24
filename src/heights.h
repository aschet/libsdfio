/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file heights.h
 * @brief Checks of a buffer layout against the matrix of a file.
 */

#ifndef SDFIO_SRC_HEIGHTS_H
#define SDFIO_SRC_HEIGHTS_H

#include "message.h"
#include <sdfio/sdfio.h>

/**
 * Returns the distance in bytes between the elements of a row of the heights of
 * the caller.
 *
 * @param layout The layout of the buffer.
 * @return The element stride of the layout, or the size of an element if that
 * is 0.
 */
ptrdiff_t sdfio_element_stride(sdfio_layout layout);

/**
 * Checks that the strides of a matrix of heights keep its elements apart.
 *
 * The rows lie beside each other, or the columns do (a column-major matrix),
 * and the elements do not overlap. A row stride of 0 becomes the stride of rows
 * that follow each other: the number of columns times the distance between the
 * elements.
 *
 * @param[in,out] layout The layout of the buffer, whose row stride is
 * resolved.
 * @param rows The number of rows.
 * @param columns The number of columns.
 * @param[out] message Receives the reason. May be null.
 * @retval SDFIO_ERR_ARGUMENT The strides do not keep the elements apart, or
 * the matrix is larger than memory.
 */
sdfio_status sdfio_layout_check(sdfio_layout *layout, size_t rows,
                                size_t columns, sdfio_message *message);

/**
 * Checks the layout of a buffer of heights for the matrix of a header: the
 * type of the heights, and the strides (see sdfio_layout_check()).
 *
 * @param[in,out] layout The layout of the buffer, whose row stride is
 * resolved.
 * @param header The header of the file.
 * @param[out] message Receives the reason. May be null.
 * @retval SDFIO_ERR_ARGUMENT The type is not SDFIO_TYPE_FLOAT or
 * SDFIO_TYPE_DOUBLE, or the strides are invalid.
 */
sdfio_status sdfio_layout_check_heights(sdfio_layout *layout,
                                        const sdfio_header *header,
                                        sdfio_message *message);

/**
 * Checks the layout of a buffer for rows of the data area: the type has to be
 * the data type of the file, and the strides have to be valid. Rows without a
 * column hold nothing, so their layout is not looked at.
 *
 * @param[in,out] layout The layout of the buffer, whose row stride is
 * resolved.
 * @param header The header of the file.
 * @param rows The number of rows that the buffer holds.
 * @param[out] message Receives the reason. May be null.
 * @retval SDFIO_ERR_ARGUMENT The type is not the data type of the file, or the
 * strides are invalid.
 */
sdfio_status sdfio_layout_check_data(sdfio_layout *layout,
                                     const sdfio_header *header, size_t rows,
                                     sdfio_message *message);

#endif /* SDFIO_SRC_HEIGHTS_H */
