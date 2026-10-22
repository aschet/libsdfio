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
 * memory.
 */

#include "internal.h"

static bool in_range(double value, double low, double high)
{
    return value >= low && value <= high;
}

/* The marker of the caller as the buffer holds it: rounded to the sample type,
 * so that it equals the points that were set to it. NaN is no marker. */
static double marker_of(sdfio_format format)
{
    if (isnan(format.missing) || format.sample == SDFIO_FLOAT64)
    {
        return format.missing;
    }
    return fabs(format.missing) > (double)FLT_MAX
               ? format.missing
               : (double)(float)format.missing;
}

ptrdiff_t sdfio_element_stride(sdfio_format format)
{
    return format.element_stride != 0
               ? format.element_stride
               : (ptrdiff_t)sdfio_sample_size(format.sample);
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
    if (!sdfio_valid_sample(format->sample))
    {
        return sdfio_fail(message, SDFIO_ERR_ARGUMENT,
                          "The sample type is not one of the enumeration");
    }
    const size_t size = sdfio_sample_size(format->sample);
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
                          "The element stride is smaller than a sample");
    }
    if (rows > 1 && row < size)
    {
        return sdfio_fail(message, SDFIO_ERR_ARGUMENT,
                          "The row stride is smaller than a sample");
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
           && sdfio_valid_sample(format.sample);
}

/* The tests of a float that stay in float: the generic tests of the C library
 * may convert to double, which a chip without a double FPU does in software. */
static bool float_is_nan(float value)
{
    return value != value; // NOLINT(misc-redundant-expression)
}

static bool float_is_infinite(float value)
{
    return fabsf(value) > FLT_MAX;
}

static bool float_is_finite(float value)
{
    return fabsf(value) <= FLT_MAX;
}

/** Whether a float holds every value of a type exactly. */
#define EXACT_IN_FLOAT(type, floating)                                         \
    (sizeof(type) <= 2 || ((floating) && sizeof(type) == 4))

/**
 * The z-scale as two floats, which add up to it more closely than one does, so
 * that a float calculation rounds as the one in double would. The scales that
 * are far from 1 are left to the calculation in double.
 */
typedef struct scale_pair
{
    /** The scale as a float. */
    float head;
    /** What the float lacks of the scale. */
    float tail;
} scale_pair;

static bool split_scale(double scale, scale_pair *pair)
{
    pair->head = (float)scale;
    pair->tail = (float)(scale - (double)pair->head);
    return pair->head >= 1e-30F && pair->head <= 1e30F;
}

/** The result of the decoding or encoding in float. */
typedef enum float_result
{
    /** Done. */
    FLOAT_DONE,
    /** Not for this type or scale: the calculation in double does it. */
    FLOAT_UNHANDLED,
    /** A number does not fit the type. */
    FLOAT_RANGE,
    /** A height would be read as a point that is not measured. */
    FLOAT_BAD_VALUE
} float_result;

/**
 * A case of the decoding into floats, for SDFIO_SCALARS(): the types that a
 * float holds exactly are converted and scaled in float, with no double.
 */
#define DECODE_FLOAT_CASE(data, name, type, bits, low, high, floating)         \
    case data:                                                                 \
        if (!EXACT_IN_FLOAT(type, floating))                                   \
        {                                                                      \
            return FLOAT_UNHANDLED;                                            \
        }                                                                      \
        for (size_t i = 0; i < count; ++i)                                     \
        {                                                                      \
            type stored;                                                       \
            memcpy(&stored, (const unsigned char *)coded + i * sizeof stored,  \
                   sizeof stored);                                             \
            const float number = (float)stored;                                \
            float height =                                                     \
                (is_missing.at_least ? number >= bad : number == bad)          \
                    ? NAN                                                      \
                    : fmaf(number, scale.head, number * scale.tail);           \
            if (float_is_infinite(height) && !float_is_infinite(number))       \
            {                                                                  \
                return FLOAT_RANGE;                                            \
            }                                                                  \
            if (float_is_nan(height) && has_marker)                            \
            {                                                                  \
                height = marker_float;                                         \
            }                                                                  \
            memcpy((unsigned char *)heights + (ptrdiff_t)i * element, &height, \
                   sizeof height);                                             \
        }                                                                      \
        return FLOAT_DONE;

/* The decoding into a buffer of floats without a double, for the data types
 * that a float holds exactly and the scales that two floats hold. */
static float_result decode_float(const sdfio_header *header, const void *coded,
                                 size_t count, void *heights, ptrdiff_t element,
                                 double marker)
{
    scale_pair scale;
    if (!split_scale(header->z_scale, &scale))
    {
        return FLOAT_UNHANDLED;
    }
    const sdfio_bad_test is_missing =
        sdfio_bad_test_for(header->data_type, header->revision);
    const float bad = (float)is_missing.value;
    if (!isnan(marker) && fabs(marker) > (double)FLT_MAX)
    {
        return FLOAT_UNHANDLED;
    }
    const float marker_float = (float)marker;
    const bool has_marker = !float_is_nan(marker_float);
    switch (header->data_type)
    {
        SDFIO_SCALARS(DECODE_FLOAT_CASE)
    }
    return FLOAT_UNHANDLED;
}

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
    const sdfio_data_type type = header->data_type;
    const sdfio_bad_test is_missing =
        sdfio_bad_test_for(type, header->revision);
    const size_t coded_size = sdfio_data_type_size(type);
    const ptrdiff_t element = sdfio_element_stride(format);
    const double marker = marker_of(format);
    if (format.sample == SDFIO_FLOAT32)
    {
        const float_result result =
            decode_float(header, data, count, heights, element, marker);
        if (result == FLOAT_RANGE)
        {
            return SDFIO_ERR_RANGE;
        }
        if (result == FLOAT_DONE)
        {
            return SDFIO_OK;
        }
    }
    double values[SDFIO_STACK_CHUNK];
    for (size_t first = 0; first < count; first += SDFIO_STACK_CHUNK)
    {
        const size_t n = count - first < SDFIO_STACK_CHUNK ? count - first
                                                           : SDFIO_STACK_CHUNK;
        sdfio_load_coded(type, (const unsigned char *)data + first * coded_size,
                         n, values);
        for (size_t i = 0; i < n; ++i)
        {
            /* NaN before scaling: a huge marker could overflow when scaled. */
            double height = sdfio_matches(is_missing, values[i])
                                ? NAN
                                : values[i] * header->z_scale;
            if (isnan(height) && !isnan(marker))
            {
                height = marker;
            }
            values[i] = height;
        }
        const sdfio_status status = sdfio_store_strided(
            format.sample,
            (unsigned char *)heights + (ptrdiff_t)first * element, element,
            values, n);
        if (status != SDFIO_OK)
        {
            return status;
        }
    }
    return SDFIO_OK;
}

/**
 * What the check of a height needs of the data area, found once for all
 * heights.
 */
typedef struct coder
{
    /** Whether the data type is a floating point type. */
    bool floating;
    /** Whether the data type is a binary32. */
    bool binary32;
    /** The lowest value of the data type. */
    double lowest;
    /** The highest value of the data type. */
    double highest;
    /** The test for a point that is not measured. */
    sdfio_bad_test bad;
} coder;

static coder coder_for(sdfio_data_type type, sdfio_revision revision)
{
    coder result;
    result.floating = sdfio_is_floating(type);
    result.binary32 = type == SDFIO_DATA_BINARY32;
    result.lowest = sdfio_lowest(type);
    result.highest = sdfio_highest(type);
    result.bad = sdfio_bad_test_for(type, revision);
    return result;
}

/* A height as the data area codes it, checked against the range of the type and
 * against the number that marks a point as not measured. */
static sdfio_status checked_coded(const coder *checks, double value,
                                  double *out)
{
    double candidate = value;
    if (checks->floating)
    {
        if (!in_range(value, checks->lowest, checks->highest))
        {
            return SDFIO_ERR_RANGE;
        }
        /* A number is compared as it is stored: one that rounds to the marker
         * is the marker. */
        if (checks->binary32)
        {
            candidate = (double)(float)value;
        }
    }
    else
    {
        candidate = nearbyint(value);
        if (!in_range(candidate, checks->lowest, checks->highest))
        {
            return SDFIO_ERR_RANGE;
        }
    }
    if (sdfio_matches(checks->bad, candidate))
    {
        return SDFIO_ERR_BAD_VALUE;
    }
    *out = checks->floating ? value : candidate;
    return SDFIO_OK;
}

/**
 * A case of the encoding from floats, for SDFIO_SCALARS(): the types that a
 * float holds exactly are scaled and rounded in float, with no double. The
 * quotient is corrected with the rest of the division, so that the rounding to
 * a whole number is that of the calculation in double.
 */
#define ENCODE_FLOAT_CASE(data, name, type, bits, low, high, floating)         \
    case data:                                                                 \
        if (!EXACT_IN_FLOAT(type, floating))                                   \
        {                                                                      \
            return FLOAT_UNHANDLED;                                            \
        }                                                                      \
        for (size_t i = 0; i < count; ++i)                                     \
        {                                                                      \
            float height;                                                      \
            memcpy(&height,                                                    \
                   (const unsigned char *)heights + (ptrdiff_t)i * element,    \
                   sizeof height);                                             \
            type stored;                                                       \
            if (!float_is_finite(height)                                       \
                || (has_marker && height == marker_float))                     \
            {                                                                  \
                stored = (type)not_measured;                                   \
            }                                                                  \
            else                                                               \
            {                                                                  \
                const float quotient = height / scale.head;                    \
                if (!float_is_finite(quotient))                                \
                {                                                              \
                    return FLOAT_RANGE;                                        \
                }                                                              \
                const float rest = fmaf(-quotient, scale.head, height);        \
                const float correction =                                       \
                    fmaf(-quotient, scale.tail, rest) / scale.head;            \
                float number = quotient + correction;                          \
                if (!(floating))                                               \
                {                                                              \
                    number = nearbyintf(quotient);                             \
                    const float difference = (quotient - number) + correction; \
                    number += difference > 0.5F    ? 1.0F                      \
                              : difference < -0.5F ? -1.0F                     \
                                                   : 0.0F;                     \
                    const double lowest_value = (double)(low);                 \
                    const double highest_value = (double)(high);               \
                    if (number < (float)lowest_value                           \
                        || number > (float)highest_value)                      \
                    {                                                          \
                        return FLOAT_RANGE;                                    \
                    }                                                          \
                }                                                              \
                else if (!float_is_finite(number))                             \
                {                                                              \
                    return FLOAT_RANGE;                                        \
                }                                                              \
                if (is_bad.at_least ? number >= bad : number == bad)           \
                {                                                              \
                    return FLOAT_BAD_VALUE;                                    \
                }                                                              \
                stored = (type)number;                                         \
            }                                                                  \
            memcpy((unsigned char *)coded + i * sizeof stored, &stored,        \
                   sizeof stored);                                             \
        }                                                                      \
        return FLOAT_DONE;

/* The encoding from a buffer of floats without a double, for the data types
 * that a float holds exactly and the scales that two floats hold. */
static float_result encode_float(const sdfio_header *header,
                                 const void *heights, ptrdiff_t element,
                                 size_t count, void *coded, double marker)
{
    scale_pair scale;
    if (!split_scale(header->z_scale, &scale))
    {
        return FLOAT_UNHANDLED;
    }
    const sdfio_bad_test is_bad =
        sdfio_bad_test_for(header->data_type, header->revision);
    const float bad = (float)is_bad.value;
    const float not_measured =
        (float)sdfio_bad_value(header->data_type, header->revision);
    if (!isnan(marker) && fabs(marker) > (double)FLT_MAX)
    {
        return FLOAT_UNHANDLED;
    }
    const float marker_float = (float)marker;
    const bool has_marker = !float_is_nan(marker_float);
    switch (header->data_type)
    {
        SDFIO_SCALARS(ENCODE_FLOAT_CASE)
    }
    return FLOAT_UNHANDLED;
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
    const sdfio_data_type type = header->data_type;
    const double marker = sdfio_bad_value(type, header->revision);
    const size_t coded_size = sdfio_data_type_size(type);
    const ptrdiff_t element = sdfio_element_stride(format);
    const double marker_height = marker_of(format);
    const bool has_marker = !isnan(marker_height);
    if (format.sample == SDFIO_FLOAT32)
    {
        const float_result result =
            encode_float(header, heights, element, count, data, marker_height);
        if (result == FLOAT_RANGE)
        {
            return SDFIO_ERR_RANGE;
        }
        if (result == FLOAT_BAD_VALUE)
        {
            return SDFIO_ERR_BAD_VALUE;
        }
        if (result == FLOAT_DONE)
        {
            return SDFIO_OK;
        }
    }
    const coder checks = coder_for(type, header->revision);
    double values[SDFIO_STACK_CHUNK];
    for (size_t first = 0; first < count; first += SDFIO_STACK_CHUNK)
    {
        const size_t n = count - first < SDFIO_STACK_CHUNK ? count - first
                                                           : SDFIO_STACK_CHUNK;
        sdfio_load_strided(format.sample,
                           (const unsigned char *)heights
                               + (ptrdiff_t)first * element,
                           element, n, values);
        for (size_t i = 0; i < n; ++i)
        {
            const double height = values[i];
            if (!isfinite(height) || (has_marker && height == marker_height))
            {
                values[i] = marker;
                continue;
            }
            const sdfio_status status =
                checked_coded(&checks, height / header->z_scale, &values[i]);
            if (status != SDFIO_OK)
            {
                return status;
            }
        }
        const sdfio_status status = sdfio_store_coded(
            type, (unsigned char *)data + first * coded_size, values, n);
        if (status != SDFIO_OK)
        {
            return status;
        }
    }
    return SDFIO_OK;
}

/* The step from the extreme of a type that the bad value takes away: one
 * count. */
static const double one_count = 1.0;

double sdfio_z_scale_for_range(sdfio_data_type type, sdfio_revision revision,
                               double lowest, double highest)
{
    if (!sdfio_valid_data_type(type) || sdfio_is_floating(type)
        || !(lowest <= highest))
    {
        return 1.0;
    }
    /* The bound that the revision keeps for the bad value is one step towards
     * zero, as that exact value is not usable for a height. */
    const bool bad_is_maximum = sdfio_bad_is_maximum(revision);
    const double usable_max =
        sdfio_highest(type) - (bad_is_maximum ? one_count : 0.0);
    const double usable_min =
        sdfio_lowest(type) + (bad_is_maximum ? 0.0 : one_count);
    double z_scale = 0.0;
    if (highest > 0.0)
    {
        z_scale = fmax(z_scale, highest / usable_max);
    }
    /* The minimum of an unsigned type is 0, which no negative height fits at
     * any scale: leave that to the range check of the write. */
    if (lowest < 0.0 && usable_min != 0.0)
    {
        z_scale = fmax(z_scale, lowest / usable_min);
    }
    return z_scale > 0.0 ? z_scale : 1.0;
}

static sdfio_status find_range(sdfio_format format, const void *heights,
                               size_t rows, size_t columns, double *lowest,
                               double *highest)
{
    if (!sdfio_valid_sample(format.sample) || lowest == NULL || highest == NULL
        || (rows > 0 && columns > 0 && heights == NULL)
        || sdfio_check_layout(&format, rows, columns, NULL) != SDFIO_OK)
    {
        return SDFIO_ERR_ARGUMENT;
    }
    const ptrdiff_t element = sdfio_element_stride(format);
    const double marker_height = marker_of(format);
    const bool has_marker = !isnan(marker_height);
    double low = INFINITY;
    double high = -INFINITY;
    double values[SDFIO_STACK_CHUNK];
    for (size_t row = 0; row < rows; ++row)
    {
        const unsigned char *at =
            (const unsigned char *)heights + (ptrdiff_t)row * format.row_stride;
        for (size_t first = 0; first < columns; first += SDFIO_STACK_CHUNK)
        {
            const size_t n = columns - first < SDFIO_STACK_CHUNK
                                 ? columns - first
                                 : SDFIO_STACK_CHUNK;
            sdfio_load_strided(format.sample, at + (ptrdiff_t)first * element,
                               element, n, values);
            for (size_t i = 0; i < n; ++i)
            {
                if (isfinite(values[i])
                    && !(has_marker && values[i] == marker_height))
                {
                    low = fmin(low, values[i]);
                    high = fmax(high, values[i]);
                }
            }
        }
    }
    *lowest = low <= high ? low : NAN;
    *highest = low <= high ? high : NAN;
    return SDFIO_OK;
}

sdfio_status sdfio_suggest_z_scale(sdfio_data_type type,
                                   sdfio_revision revision, sdfio_format format,
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
        *z_scale = sdfio_z_scale_for_range(type, revision, lowest, highest);
    }
    return status;
}
