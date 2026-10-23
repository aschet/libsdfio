/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file heights.c
 * @brief Stored values to heights in metres and back.
 *
 * These functions need no handle: they take the description of a file and
 * memory. Heights of a type are calculated in that type: the conversion is
 * written once in heights_real.h and compiled for float and for double.
 */

#include "heights.h"
#include "common.h"
#include "types.h"

ptrdiff_t sdfio_element_stride(sdfio_format format)
{
    return format.element_stride != 0
               ? format.element_stride
               : (ptrdiff_t)sdfio_data_type_size(format.type);
}

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

sdfio_status sdfio_check_layout(sdfio_format *format, size_t rows,
                                size_t columns, sdfio_message *message)
{
    if (!sdfio_valid_height_type(format->type))
    {
        return sdfio_fail(message, SDFIO_ERR_ARGUMENT,
                          "The type of the heights is not SDFIO_TYPE_FLOAT or "
                          "SDFIO_TYPE_DOUBLE");
    }
    const size_t size = sdfio_data_type_size(format->type);
    const size_t element = magnitude(sdfio_element_stride(*format));
    if (format->row_stride == 0)
    {
        /* Rows that follow each other. */
        if (columns != 0 && element > (size_t)PTRDIFF_MAX / columns)
        {
            return sdfio_fail(message, SDFIO_ERR_ARGUMENT,
                              "The matrix is larger than memory");
        }
        format->row_stride = (ptrdiff_t)(columns * element);
    }
    const size_t row = magnitude(format->row_stride);
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
                          "The matrix is larger than memory");
    }
    if (rows > 1 && columns > 1 && row < row_extent && element < column_extent)
    {
        return sdfio_fail(message, SDFIO_ERR_ARGUMENT,
                          "The strides make elements overlap: the rows have to "
                          "lie beside each other, or the columns do");
    }
    return SDFIO_OK;
}

static bool usable_description(const sdfio_header *header, sdfio_format format)
{
    return header != NULL && sdfio_valid_data_type(header->data_type)
           && sdfio_valid_height_type(format.type);
}

/**
 * The scale for heights of type float, as two floats that add up to the z-scale
 * more closely than one does, so that a calculation in float rounds as the one
 * in double would. A scale far from 1 is calculated in double instead.
 */
typedef struct scale_float
{
    /** The scale as a float. */
    float head;
    /** What the float lacks of the scale. */
    float tail;
    /** The scale itself. */
    double exact;
    /** Whether the scale is too far from 1 for the pair of floats. */
    bool far;
} scale_float;

static void make_scale_float(double scale, scale_float *out)
{
    out->exact = scale;
    out->far = !(scale >= 1e-30 && scale <= 1e30);
    out->head = out->far ? 0.0F : (float)scale;
    out->tail = out->far ? 0.0F : (float)(scale - (double)out->head);
}

/* The height of a stored number, or false if it does not fit a float. */
static bool scale_multiply_float(const scale_float *scale, float number,
                                 float *height)
{
    if (scale->far)
    {
        const double product = (double)number * scale->exact;
        if (!(fabs(product) <= (double)FLT_MAX))
        {
            return false;
        }
        *height = (float)product;
        return true;
    }
    *height = fmaf(number, scale->head, number * scale->tail);
    return fabsf(*height) <= FLT_MAX;
}

/* The quotient of a height and the scale as a float and the correction that
 * the rounding of the quotient needs, or false if it does not fit a float. */
static bool scale_divide_float(const scale_float *scale, float height,
                               float *quotient, float *correction)
{
    *correction = 0.0F;
    if (scale->far)
    {
        const double divided = (double)height / scale->exact;
        if (!(fabs(divided) <= (double)FLT_MAX))
        {
            return false;
        }
        *quotient = (float)divided;
        *correction = (float)(divided - (double)*quotient);
        return true;
    }
    *quotient = height / scale->head;
    if (!(fabsf(*quotient) <= FLT_MAX))
    {
        return false;
    }
    const float rest = fmaf(-*quotient, scale->head, height);
    *correction = fmaf(-*quotient, scale->tail, rest) / scale->head;
    return true;
}

#define REAL float
#define REAL_SUFFIX float
#define REAL_MAX FLT_MAX
#define REAL_ABS fabsf
#define REAL_NEARBY nearbyintf
#define scale_type_float scale_float
#include "heights_real.h"
#undef scale_type_float
#undef REAL_NEARBY
#undef REAL_ABS
#undef REAL_MAX
#undef REAL_SUFFIX
#undef REAL

/** The scale for heights of type double. */
typedef struct scale_double
{
    /** The scale. */
    double value;
} scale_double;

static void make_scale_double(double scale, scale_double *out)
{
    out->value = scale;
}

static bool scale_multiply_double(const scale_double *scale, double number,
                                  double *height)
{
    *height = number * scale->value;
    return fabs(*height) <= DBL_MAX;
}

static bool scale_divide_double(const scale_double *scale, double height,
                                double *quotient, double *correction)
{
    *correction = 0.0;
    *quotient = height / scale->value;
    return fabs(*quotient) <= DBL_MAX;
}

#define REAL double
#define REAL_SUFFIX double
#define REAL_MAX DBL_MAX
#define REAL_ABS fabs
#define REAL_NEARBY nearbyint
#define scale_type_double scale_double
#include "heights_real.h"
#undef scale_type_double
#undef REAL_NEARBY
#undef REAL_ABS
#undef REAL_MAX
#undef REAL_SUFFIX
#undef REAL

sdfio_status sdfio_decode_heights(const sdfio_header *header,
                                  sdfio_format format, const void *data,
                                  size_t count, void *heights)
{
    sdfio_format layout = format;
    if (!usable_description(header, format)
        || (count > 0 && (data == NULL || heights == NULL))
        || sdfio_check_layout(&layout, 1, count, NULL) != SDFIO_OK)
    {
        return SDFIO_ERR_ARGUMENT;
    }
    const ptrdiff_t element = sdfio_element_stride(format);
    return format.type == SDFIO_TYPE_FLOAT
               ? decode_float(header, data, count, heights, element,
                              format.missing)
               : decode_double(header, data, count, heights, element,
                               format.missing);
}

sdfio_status sdfio_encode_heights(const sdfio_header *header,
                                  sdfio_format format, const void *heights,
                                  size_t count, void *data)
{
    sdfio_format layout = format;
    if (!usable_description(header, format)
        || (count > 0 && (data == NULL || heights == NULL))
        || sdfio_check_layout(&layout, 1, count, NULL) != SDFIO_OK
        || !(header->z_scale > 0.0 && isfinite(header->z_scale)))
    {
        return SDFIO_ERR_ARGUMENT;
    }
    const ptrdiff_t element = sdfio_element_stride(format);
    return format.type == SDFIO_TYPE_FLOAT
               ? encode_float(header, heights, element, count, data,
                              format.missing)
               : encode_double(header, heights, element, count, data,
                               format.missing);
}

double sdfio_z_scale_for_range(sdfio_data_type type, double lowest,
                               double highest)
{
    /* The bad value is the lowest number of the type, so the lowest height is
     * one count above it. */
    double usable_min = 0.0;
    double usable_max = 0.0;
    switch (type)
    {
    case SDFIO_TYPE_INT8:
        usable_min = INT8_MIN + 1.0;
        usable_max = INT8_MAX;
        break;
    case SDFIO_TYPE_INT16:
        usable_min = INT16_MIN + 1.0;
        usable_max = INT16_MAX;
        break;
    case SDFIO_TYPE_INT32:
        usable_min = INT32_MIN + 1.0;
        usable_max = INT32_MAX;
        break;
    default:
        return 1.0;
    }
    if (!(lowest <= highest))
    {
        return 1.0;
    }
    double z_scale = 0.0;
    if (highest > 0.0)
    {
        z_scale = fmax(z_scale, highest / usable_max);
    }
    if (lowest < 0.0)
    {
        z_scale = fmax(z_scale, lowest / usable_min);
    }
    return z_scale > 0.0 ? z_scale : 1.0;
}

static sdfio_status find_range(sdfio_format format, const void *heights,
                               size_t rows, size_t columns, double *lowest,
                               double *highest)
{
    if (!sdfio_valid_height_type(format.type) || lowest == NULL
        || highest == NULL || (rows > 0 && columns > 0 && heights == NULL)
        || sdfio_check_layout(&format, rows, columns, NULL) != SDFIO_OK)
    {
        return SDFIO_ERR_ARGUMENT;
    }
    const ptrdiff_t element = sdfio_element_stride(format);
    bool has_marker = false;
    if (format.type == SDFIO_TYPE_FLOAT)
    {
        float marker = 0.0F;
        if (!marker_of_float(format.missing, &marker, &has_marker))
        {
            return SDFIO_ERR_ARGUMENT;
        }
        find_range_float(heights, rows, columns, format.row_stride, element,
                         marker, has_marker, lowest, highest);
    }
    else
    {
        double marker = 0.0;
        if (!marker_of_double(format.missing, &marker, &has_marker))
        {
            return SDFIO_ERR_ARGUMENT;
        }
        find_range_double(heights, rows, columns, format.row_stride, element,
                          marker, has_marker, lowest, highest);
    }
    return SDFIO_OK;
}

sdfio_status sdfio_suggest_z_scale(sdfio_data_type type, sdfio_format format,
                                   const void *heights, size_t rows,
                                   size_t columns, double *z_scale)
{
    if (!sdfio_valid_data_type(type) || z_scale == NULL)
    {
        return SDFIO_ERR_ARGUMENT;
    }
    double lowest = 0.0;
    double highest = 0.0;
    const sdfio_status status =
        find_range(format, heights, rows, columns, &lowest, &highest);
    if (status == SDFIO_OK)
    {
        *z_scale = sdfio_z_scale_for_range(type, lowest, highest);
    }
    return status;
}
