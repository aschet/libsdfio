/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * Layout of a buffer in memory.
 *
 * A layout lets the functions transfer a matrix in place when it is part of
 * something else: a member of a structure, a column-major matrix or a matrix
 * upside down.
 */

#ifndef SDFIO_LAYOUT_H
#define SDFIO_LAYOUT_H

#include <sdfio/sdfio_export.h>
#include <sdfio/types.h>

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * Description of a buffer of the caller for heights or for the values of the
 * data area: the type of its elements, and how the matrix lies in memory.
 *
 * The element (y, x) is at `data + y * row_stride + x * element_stride`. Both
 * strides can be negative. They must keep the elements apart: either the rows
 * or the columns must not overlap. This allows heights and values to be read
 * or written in place where they are not contiguous, for example the z member
 * of an array of points, one channel of an interleaved image or a column-major
 * matrix. sdfio_layout_packed() describes a plain array. A program that does
 * not use an initializer calls sdfio_layout_init() before it assigns fields.
 *
 * The structure is frozen for the major version: it has these three fields and
 * a new property of a buffer is a new function.
 */
typedef struct sdfio_layout
{
    /** Type of the elements of the buffer. For heights it is SDFIO_TYPE_FLOAT
     * or SDFIO_TYPE_DOUBLE, and for the values of the data area it is the data
     * type of the file. */
    sdfio_data_type type;
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
 * follow each other and the rows follow each other. The caller sets the type
 * and every other field that differs.
 *
 * @param[out] layout The layout to initialize.
 */
SDFIO_API void sdfio_layout_init(sdfio_layout *layout);

/**
 * Returns the layout of a plain array: the elements follow each other and the
 * rows follow each other. The layout is a constant of the library, valid for
 * the life of the program, and the same for every thread. A program that needs
 * other strides copies it, or uses sdfio_layout_init().
 *
 * @param type The type of the elements: SDFIO_TYPE_FLOAT or SDFIO_TYPE_DOUBLE
 * for heights, and the data type of the file for the values of the data area.
 * @return The layout, or null if `type` is not a data type.
 */
SDFIO_API const sdfio_layout *sdfio_layout_packed(sdfio_data_type type);

#ifdef __cplusplus
}
#endif

#endif /* SDFIO_LAYOUT_H */
