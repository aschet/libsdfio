/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file heights.c
 * @brief Conversion between coded values and heights.
 *
 * These functions need no handle: they take the description of a file and
 * memory. Heights of a type are calculated in that type: the conversion is
 * written once in heights_real.h and compiled for float and for double.
 */

#include "heights.h"
#include "data_type.h"
#include "header.h"
#include "message.h"
#include <float.h>
#include <math.h>
#include <sdfio/sdfio.h>
#include <string.h>

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

/* Whether the header and the layout describe types that heights can use. */
static bool usable_description(const sdfio_header *header, sdfio_layout layout)
{
    return header != NULL && sdfio_data_type_valid(header->data_type)
           && sdfio_heights_type_valid(layout.type);
}

/*
 * @def SDFIO_FLOAT_HEIGHTS_CHOSEN
 * Whether the processor favours calculating heights of type float in double.
 * A processor with hardware for single precision only, or with none, favours
 * float, where double precision arithmetic is a slow software routine. Every
 * other processor favours double, where float arithmetic with the correction
 * that keeps its accuracy is the slower one.
 */
#if (defined(__ARM_ARCH) && !(defined(__ARM_FP) && (__ARM_FP & 8)))            \
    || (defined(__riscv) && (!defined(__riscv_flen) || __riscv_flen < 64))
#define SDFIO_FLOAT_HEIGHTS_CHOSEN 0
#else
#define SDFIO_FLOAT_HEIGHTS_CHOSEN 1
#endif

/*
 * @def SDFIO_FLOAT_HEIGHTS_IN_DOUBLE
 * Whether heights of type float are calculated in double: what the processor
 * favours. The tests define SDFIO_FLOAT_HEIGHTS_OTHER to run the other
 * variant.
 */
#ifdef SDFIO_FLOAT_HEIGHTS_OTHER
#define SDFIO_FLOAT_HEIGHTS_IN_DOUBLE (!SDFIO_FLOAT_HEIGHTS_CHOSEN)
#else
#define SDFIO_FLOAT_HEIGHTS_IN_DOUBLE SDFIO_FLOAT_HEIGHTS_CHOSEN
#endif

/* The height of a coded value calculated in double, or false if it does not
 * fit a float. */
static bool multiply_in_double(double exact, float number, float *height)
{
    const double product = (double)number * exact;
    if (!(fabs(product) <= (double)FLT_MAX))
    {
        return false;
    }
    *height = (float)product;
    return true;
}

/* The quotient of a height and a scale calculated in double, as a float and
 * the correction that the rounding to a float needs, or false if it does not
 * fit a float. */
static bool divide_in_double(double exact, float height, float *quotient,
                             float *correction)
{
    const double divided = (double)height / exact;
    if (!(fabs(divided) <= (double)FLT_MAX))
    {
        return false;
    }
    *quotient = (float)divided;
    *correction = (float)(divided - (double)*quotient);
    return true;
}

#if SDFIO_FLOAT_HEIGHTS_IN_DOUBLE

/* The scale for heights of type float, which are calculated in double. */
typedef struct scale_float
{
    /* The scale. */
    double exact;
} scale_float;

static void scale_init_float(double scale, scale_float *out)
{
    out->exact = scale;
}

/* The height of a coded value, or false if it does not fit a float. */
static bool scale_multiply_float(const scale_float *scale, float number,
                                 float *height)
{
    return multiply_in_double(scale->exact, number, height);
}

/* The quotient of a height and the scale as a float and the correction that
 * the rounding of the quotient needs, or false if it does not fit a float. */
static bool scale_divide_float(const scale_float *scale, float height,
                               float *quotient, float *correction)
{
    return divide_in_double(scale->exact, height, quotient, correction);
}

#else

/*
 * The scale for heights of type float, as two floats that add up to the z-scale
 * more closely than one does, so that a calculation in float rounds as the one
 * in double would. A scale far from 1 is calculated in double instead.
 */
typedef struct scale_float
{
    /* The scale as a float. */
    float head;
    /* What the float lacks of the scale. */
    float tail;
    /* The reciprocal of head, which the correction of a quotient needs. */
    float inverse;
    /* The scale itself. */
    double exact;
    /* Whether the scale is too far from 1 for the pair of floats. */
    bool far;
} scale_float;

static void scale_init_float(double scale, scale_float *out)
{
    out->exact = scale;
    out->far = !(scale >= 1e-30 && scale <= 1e30);
    out->head = out->far ? 0.0F : (float)scale;
    out->tail = out->far ? 0.0F : (float)(scale - (double)out->head);
    out->inverse = out->far ? 0.0F : 1.0F / out->head;
}

/* The height of a coded value, or false if it does not fit a float. */
static bool scale_multiply_float(const scale_float *scale, float number,
                                 float *height)
{
    if (scale->far)
    {
        return multiply_in_double(scale->exact, number, height);
    }
    *height = fmaf(number, scale->head, number * scale->tail);
    return fabsf(*height) <= FLT_MAX;
}

/* The quotient of a height and the scale as a float and the correction that
 * the rounding of the quotient needs, or false if it does not fit a float. The
 * correction is less than the last digit of the quotient, so the reciprocal of
 * the scale gives it as accurately as a division. */
static bool scale_divide_float(const scale_float *scale, float height,
                               float *quotient, float *correction)
{
    *correction = 0.0F;
    if (scale->far)
    {
        return divide_in_double(scale->exact, height, quotient, correction);
    }
    *quotient = height / scale->head;
    if (!(fabsf(*quotient) <= FLT_MAX))
    {
        return false;
    }
    const float rest = fmaf(-*quotient, scale->head, height);
    *correction = fmaf(-*quotient, scale->tail, rest) * scale->inverse;
    return true;
}

#endif

/* Rounds to an integer in the current rounding mode. Where the arithmetic is
 * evaluated in the type of the operands, a number from 2^23 on has no fraction,
 * and below it adding and subtracting 2^23 rounds, which a processor without a
 * rounding instruction does faster than the C library. Where an operation can
 * have a wider type, as on the x87 unit, the sum would keep the fraction, so
 * the C library rounds. */
#if FLT_EVAL_METHOD == 0
static float nearby_float(float x)
{
    const float magic = 8388608.0F;
    if (!(fabsf(x) < magic))
    {
        return x;
    }
    const float offset = copysignf(magic, x);
    return (x + offset) - offset;
}

/* The same for double, from 2^52 on. */
static double nearby_double(double x)
{
    const double magic = 4503599627370496.0;
    if (!(fabs(x) < magic))
    {
        return x;
    }
    const double offset = copysign(magic, x);
    return (x + offset) - offset;
}
#else
static float nearby_float(float x)
{
    return nearbyintf(x);
}

static double nearby_double(double x)
{
    return nearbyint(x);
}
#endif

#define REAL float
#define REAL_SUFFIX float
#define REAL_MAX FLT_MAX
#define REAL_ABS fabsf
#define REAL_NEARBY nearby_float
typedef scale_float scale_type_float;
#include "heights_real.h"
#undef REAL_NEARBY
#undef REAL_ABS
#undef REAL_MAX
#undef REAL_SUFFIX
#undef REAL

/* The scale for heights of type double. */
typedef struct scale_double
{
    /* The scale. */
    double value;
} scale_double;

static void scale_init_double(double scale, scale_double *out)
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
#define REAL_NEARBY nearby_double
typedef scale_double scale_type_double;
#include "heights_real.h"
#undef REAL_NEARBY
#undef REAL_ABS
#undef REAL_MAX
#undef REAL_SUFFIX
#undef REAL

sdfio_status sdfio_decode_heights(const sdfio_header *header,
                                  const sdfio_layout *layout_of_heights,
                                  double not_measured, const void *data,
                                  size_t count, void *heights)
{
    if (layout_of_heights == NULL)
    {
        return SDFIO_ERR_ARGUMENT;
    }
    sdfio_layout layout = *layout_of_heights;
    if (!usable_description(header, layout)
        || (count > 0 && (data == NULL || heights == NULL))
        || sdfio_layout_check(&layout, 1, count, NULL) != SDFIO_OK)
    {
        return SDFIO_ERR_ARGUMENT;
    }
    const ptrdiff_t element = sdfio_element_stride(layout);
    return layout.type == SDFIO_TYPE_FLOAT
               ? decode_float(header, data, count, heights, element,
                              not_measured)
               : decode_double(header, data, count, heights, element,
                               not_measured);
}

sdfio_status sdfio_encode_heights(const sdfio_header *header,
                                  const sdfio_layout *layout_of_heights,
                                  double not_measured, const void *heights,
                                  size_t count, void *data)
{
    if (layout_of_heights == NULL)
    {
        return SDFIO_ERR_ARGUMENT;
    }
    sdfio_layout layout = *layout_of_heights;
    if (!usable_description(header, layout)
        || (count > 0 && (data == NULL || heights == NULL))
        || sdfio_layout_check(&layout, 1, count, NULL) != SDFIO_OK
        || !(header->z_scale > 0.0 && isfinite(header->z_scale)))
    {
        return SDFIO_ERR_ARGUMENT;
    }
    const ptrdiff_t element = sdfio_element_stride(layout);
    return layout.type == SDFIO_TYPE_FLOAT
               ? encode_float(header, heights, element, count, data,
                              not_measured)
               : encode_double(header, heights, element, count, data,
                               not_measured);
}

double sdfio_suggest_z_scale_for_range(sdfio_data_type type, double lowest,
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

/* The lowest and the highest height of a matrix that a layout describes. */
static sdfio_status find_range(sdfio_layout layout, double not_measured,
                               const void *heights, size_t rows, size_t columns,
                               double *lowest, double *highest)
{
    if (!sdfio_heights_type_valid(layout.type) || lowest == NULL
        || highest == NULL || (rows > 0 && columns > 0 && heights == NULL)
        || sdfio_layout_check(&layout, rows, columns, NULL) != SDFIO_OK)
    {
        return SDFIO_ERR_ARGUMENT;
    }
    const ptrdiff_t element = sdfio_element_stride(layout);
    bool has_marker = false;
    if (layout.type == SDFIO_TYPE_FLOAT)
    {
        float marker = 0.0F;
        if (!marker_of_float(not_measured, &marker, &has_marker))
        {
            return SDFIO_ERR_ARGUMENT;
        }
        find_range_float(heights, rows, columns, layout.row_stride, element,
                         marker, has_marker, lowest, highest);
    }
    else
    {
        double marker = 0.0;
        if (!marker_of_double(not_measured, &marker, &has_marker))
        {
            return SDFIO_ERR_ARGUMENT;
        }
        find_range_double(heights, rows, columns, layout.row_stride, element,
                          marker, has_marker, lowest, highest);
    }
    return SDFIO_OK;
}

sdfio_status sdfio_suggest_z_scale(sdfio_data_type type,
                                   const sdfio_layout *layout_of_heights,
                                   double not_measured, const void *heights,
                                   size_t rows, size_t columns, double *z_scale)
{
    if (!sdfio_data_type_valid(type) || layout_of_heights == NULL
        || z_scale == NULL)
    {
        return SDFIO_ERR_ARGUMENT;
    }
    double lowest = 0.0;
    double highest = 0.0;
    const sdfio_status status =
        find_range(*layout_of_heights, not_measured, heights, rows, columns,
                   &lowest, &highest);
    if (status == SDFIO_OK)
    {
        *z_scale = sdfio_suggest_z_scale_for_range(type, lowest, highest);
    }
    return status;
}
