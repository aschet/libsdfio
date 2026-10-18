/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* Stored values to heights in metres and back. These functions need no handle:
 * they take the description of a file and memory. */

#include "internal.h"

static bool in_range(double value, double low, double high)
{
    return value >= low && value <= high;
}

/* The marker of the caller as the buffer holds it: rounded to the sample type,
 * so that it equals the points that were set to it. NaN is no marker. */
static double marker_of(sdfio_format format)
{
    if (isnan(format.missing))
    {
        return NAN;
    }
    switch (format.sample)
    {
    case SDFIO_FLOAT32:
        return fabs(format.missing) > (double)FLT_MAX
                   ? format.missing
                   : (double)(float)format.missing;
    case SDFIO_FLOAT64:
        return format.missing;
    default:
        return nearbyint(format.missing);
    }
}

ptrdiff_t sdfi_element_stride(sdfio_format format)
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

sdfio_status sdfi_check_layout(sdfio_format format, size_t rows, size_t columns,
                               ptrdiff_t row_stride, sdfi_message *message)
{
    if (!sdfi_valid_sample(format.sample))
    {
        return sdfi_fail(message, SDFIO_ERR_ARGUMENT,
                         "The sample type is not one of the enumeration");
    }
    const size_t size = sdfio_sample_size(format.sample);
    const size_t element = magnitude(sdfi_element_stride(format));
    const size_t row = magnitude(row_stride);
    if (columns > 1 && element < size)
    {
        return sdfi_fail(message, SDFIO_ERR_ARGUMENT,
                         "The element stride is smaller than a sample");
    }
    if (rows > 1 && row < size)
    {
        return sdfi_fail(message, SDFIO_ERR_ARGUMENT,
                         "The row stride is smaller than a sample");
    }
    const size_t row_extent = extent_of(columns, element, size);
    const size_t column_extent = extent_of(rows, row, size);
    if ((columns > 1 && row_extent == 0) || (rows > 1 && column_extent == 0))
    {
        return sdfi_fail(message, SDFIO_ERR_ARGUMENT,
                         "The matrix is larger than memory");
    }
    if (rows > 1 && columns > 1 && row < row_extent && element < column_extent)
    {
        return sdfi_fail(message, SDFIO_ERR_ARGUMENT,
                         "The strides make elements overlap: the rows have to "
                         "lie beside each other, or the columns do");
    }
    return SDFIO_OK;
}

static bool usable_description(const sdfio_header *header, sdfio_format format)
{
    return header != NULL && sdfi_valid_data_type(header->data_type)
           && sdfi_valid_sample(format.sample);
}

sdfio_status sdfio_decode_heights(const sdfio_header *header,
                                  sdfio_format format, const void *data,
                                  size_t count, void *heights)
{
    if (!usable_description(header, format)
        || (count > 0 && (data == NULL || heights == NULL))
        || sdfi_check_layout(format, 1, count, 0, NULL) != SDFIO_OK)
    {
        return SDFIO_ERR_ARGUMENT;
    }
    const sdfio_data_type type = header->data_type;
    const sdfi_bad_test is_missing = sdfi_bad_test_for(type, header->revision);
    const size_t coded_size = sdfio_data_type_size(type);
    const ptrdiff_t element = sdfi_element_stride(format);
    const double marker = marker_of(format);
    double values[SDFI_CHUNK];
    for (size_t first = 0; first < count; first += SDFI_CHUNK)
    {
        const size_t n =
            count - first < SDFI_CHUNK ? count - first : SDFI_CHUNK;
        sdfi_load_coded(type, (const unsigned char *)data + first * coded_size,
                        n, values);
        for (size_t i = 0; i < n; ++i)
        {
            /* NaN before scaling: a huge marker could overflow when scaled. */
            double height = sdfi_matches(is_missing, values[i])
                                ? NAN
                                : values[i] * header->z_scale;
            if (isnan(height) && !isnan(marker))
            {
                height = marker;
            }
            values[i] = height;
        }
        const sdfio_status status = sdfi_store_strided(
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

/* A height as the data area codes it, checked against the range of the type and
 * against the number that marks a point as not measured. */
static sdfio_status checked_coded(sdfio_data_type type, sdfio_revision revision,
                                  double value, double *out)
{
    double candidate = value;
    if (sdfi_is_floating(type))
    {
        if (!in_range(value, sdfi_lowest(type), sdfi_highest(type)))
        {
            return SDFIO_ERR_RANGE;
        }
        /* A number is compared as it is stored: one that rounds to the marker
         * is the marker. */
        if (type == SDFIO_DATA_BINARY32)
        {
            candidate = (double)(float)value;
        }
    }
    else
    {
        candidate = nearbyint(value);
        if (!in_range(candidate, sdfi_lowest(type), sdfi_highest(type)))
        {
            return SDFIO_ERR_RANGE;
        }
    }
    if (sdfio_is_bad_value(type, revision, candidate))
    {
        return SDFIO_ERR_BAD_VALUE;
    }
    *out = sdfi_is_floating(type) ? value : candidate;
    return SDFIO_OK;
}

sdfio_status sdfio_encode_heights(const sdfio_header *header,
                                  sdfio_format format, const void *heights,
                                  size_t count, void *data)
{
    if (!usable_description(header, format)
        || (count > 0 && (data == NULL || heights == NULL))
        || sdfi_check_layout(format, 1, count, 0, NULL) != SDFIO_OK
        || !(header->z_scale > 0.0 && isfinite(header->z_scale)))
    {
        return SDFIO_ERR_ARGUMENT;
    }
    const sdfio_data_type type = header->data_type;
    const double marker = sdfio_bad_value(type, header->revision);
    const size_t coded_size = sdfio_data_type_size(type);
    const ptrdiff_t element = sdfi_element_stride(format);
    const double marker_height = marker_of(format);
    const bool has_marker = !isnan(marker_height);
    double values[SDFI_CHUNK];
    for (size_t first = 0; first < count; first += SDFI_CHUNK)
    {
        const size_t n =
            count - first < SDFI_CHUNK ? count - first : SDFI_CHUNK;
        sdfi_load_strided(format.sample,
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
            const sdfio_status status = checked_coded(
                type, header->revision, height / header->z_scale, &values[i]);
            if (status != SDFIO_OK)
            {
                return status;
            }
        }
        const sdfio_status status = sdfi_store_coded(
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
    if (!sdfi_valid_data_type(type) || sdfi_is_floating(type)
        || !(lowest <= highest))
    {
        return 1.0;
    }
    /* The bound that the revision keeps for the bad value is one step towards
     * zero, as that exact value is not usable for a height. */
    const bool bad_is_maximum = sdfi_bad_is_maximum(revision);
    const double usable_max =
        sdfi_highest(type) - (bad_is_maximum ? one_count : 0.0);
    const double usable_min =
        sdfi_lowest(type) + (bad_is_maximum ? 0.0 : one_count);
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
                               size_t rows, size_t columns,
                               ptrdiff_t row_stride, double *lowest,
                               double *highest)
{
    if (!sdfi_valid_sample(format.sample) || lowest == NULL || highest == NULL
        || (rows > 0 && columns > 0 && heights == NULL)
        || sdfi_check_layout(format, rows, columns, row_stride, NULL)
               != SDFIO_OK)
    {
        return SDFIO_ERR_ARGUMENT;
    }
    const ptrdiff_t element = sdfi_element_stride(format);
    const double marker_height = marker_of(format);
    const bool has_marker = !isnan(marker_height);
    double low = INFINITY;
    double high = -INFINITY;
    double values[SDFI_CHUNK];
    for (size_t row = 0; row < rows; ++row)
    {
        const unsigned char *at =
            (const unsigned char *)heights + (ptrdiff_t)row * row_stride;
        for (size_t first = 0; first < columns; first += SDFI_CHUNK)
        {
            const size_t n =
                columns - first < SDFI_CHUNK ? columns - first : SDFI_CHUNK;
            sdfi_load_strided(format.sample, at + (ptrdiff_t)first * element,
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
                                   size_t columns, ptrdiff_t row_stride,
                                   double *z_scale)
{
    if (!sdfi_valid_data_type(type) || z_scale == NULL)
    {
        return SDFIO_ERR_ARGUMENT;
    }
    double lowest = 0.0;
    double highest = 0.0;
    const sdfio_status status = find_range(format, heights, rows, columns,
                                           row_stride, &lowest, &highest);
    if (status == SDFIO_OK)
    {
        *z_scale = sdfio_z_scale_for_range(type, revision, lowest, highest);
    }
    return status;
}
