/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file heights.h
 * @brief The layout of a matrix of heights in memory of the caller.
 */

#ifndef SDFIO_SRC_HEIGHTS_H
#define SDFIO_SRC_HEIGHTS_H

#include "common.h"

/**
 * Returns the distance in bytes between the samples of a row of the heights of
 * the caller.
 *
 * @param format The format of the buffer.
 * @return The element stride of the format, or the size of a sample if that is
 * 0.
 */
ptrdiff_t sdfio_element_stride(sdfio_format format);

/**
 * Checks that the strides of a matrix of heights keep its elements apart.
 *
 * The rows lie beside each other, or the columns do (a column-major matrix),
 * and the samples do not overlap. A row stride of 0 becomes the stride of rows
 * that follow each other: the number of columns times the distance between the
 * elements.
 *
 * @param[in,out] format The format of the buffer, whose row stride is
 * resolved.
 * @param rows The number of rows.
 * @param columns The number of columns.
 * @param[out] message Receives the reason. May be null.
 * @retval SDFIO_ERR_ARGUMENT The strides do not keep the elements apart, or
 * the matrix is larger than memory.
 */
sdfio_status sdfio_check_layout(sdfio_format *format, size_t rows,
                                size_t columns, sdfio_message *message);

#endif /* SDFIO_SRC_HEIGHTS_H */
