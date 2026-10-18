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
static double marker_of(sdf_format format)
{
    if (isnan(format.missing))
    {
        return NAN;
    }
    switch (format.sample)
    {
    case SDF_FLOAT32:
        return fabs(format.missing) > (double)FLT_MAX
                   ? format.missing
                   : (double)(float)format.missing;
    case SDF_FLOAT64:
        return format.missing;
    default:
        return nearbyint(format.missing);
    }
}

static bool usable_description(const sdf_info *info, sdf_format format)
{
    return info != NULL && sdfi_valid_data_type(info->data_type)
           && sdfi_valid_sample(format.sample);
}

sdf_status sdf_decode_heights(const sdf_info *info, sdf_format format,
                              const void *stored, size_t count, void *heights)
{
    if (!usable_description(info, format)
        || (count > 0 && (stored == NULL || heights == NULL)))
    {
        return SDF_ERR_ARGUMENT;
    }
    const sdf_data_type type = info->data_type;
    const sdfi_sentinel_test is_missing =
        sdfi_sentinel_test_for(type, info->revision);
    const size_t stored_size = sdf_data_type_size(type);
    const size_t sample_size = sdf_sample_size(format.sample);
    const double marker = marker_of(format);
    double values[SDFI_CHUNK];
    for (size_t first = 0; first < count; first += SDFI_CHUNK)
    {
        const size_t n =
            count - first < SDFI_CHUNK ? count - first : SDFI_CHUNK;
        sdfi_load_stored(type,
                         (const unsigned char *)stored + first * stored_size, n,
                         values);
        for (size_t i = 0; i < n; ++i)
        {
            /* NaN before scaling: a huge marker could overflow when scaled. */
            double height = sdfi_matches(is_missing, values[i])
                                ? NAN
                                : values[i] * info->z_scale;
            if (isnan(height) && !isnan(marker))
            {
                height = marker;
            }
            values[i] = height;
        }
        const sdf_status status = sdfi_store_samples(
            format.sample, (unsigned char *)heights + first * sample_size,
            values, n);
        if (status != SDF_OK)
        {
            return status;
        }
    }
    return SDF_OK;
}

/* A height as it is stored, checked against the range of the type and against
 * the number that marks a point as not measured. */
static sdf_status checked_stored(sdf_data_type type, sdf_revision revision,
                                 double stored, double *out)
{
    double candidate = stored;
    if (sdfi_is_floating(type))
    {
        if (!in_range(stored, sdfi_lowest(type), sdfi_highest(type)))
        {
            return SDF_ERR_RANGE;
        }
        /* A number is compared as it is stored: one that rounds to the marker
         * is the marker. */
        if (type == SDF_DATA_BINARY32)
        {
            candidate = (double)(float)stored;
        }
    }
    else
    {
        candidate = nearbyint(stored);
        if (!in_range(candidate, sdfi_lowest(type), sdfi_highest(type)))
        {
            return SDF_ERR_RANGE;
        }
    }
    if (sdf_is_sentinel(type, revision, candidate))
    {
        return SDF_ERR_SENTINEL;
    }
    *out = sdfi_is_floating(type) ? stored : candidate;
    return SDF_OK;
}

sdf_status sdf_encode_heights(const sdf_info *info, sdf_format format,
                              const void *heights, size_t count, void *stored)
{
    if (!usable_description(info, format)
        || (count > 0 && (stored == NULL || heights == NULL))
        || !(info->z_scale > 0.0 && isfinite(info->z_scale)))
    {
        return SDF_ERR_ARGUMENT;
    }
    const sdf_data_type type = info->data_type;
    const double marker = sdf_sentinel(type, info->revision);
    const size_t stored_size = sdf_data_type_size(type);
    const size_t sample_size = sdf_sample_size(format.sample);
    const double marker_height = marker_of(format);
    const bool has_marker = !isnan(marker_height);
    double values[SDFI_CHUNK];
    for (size_t first = 0; first < count; first += SDFI_CHUNK)
    {
        const size_t n =
            count - first < SDFI_CHUNK ? count - first : SDFI_CHUNK;
        sdfi_load_samples(format.sample,
                          (const unsigned char *)heights + first * sample_size,
                          n, values);
        for (size_t i = 0; i < n; ++i)
        {
            const double height = values[i];
            if (!isfinite(height) || (has_marker && height == marker_height))
            {
                values[i] = marker;
                continue;
            }
            const sdf_status status = checked_stored(
                type, info->revision, height / info->z_scale, &values[i]);
            if (status != SDF_OK)
            {
                return status;
            }
        }
        const sdf_status status = sdfi_store_stored(
            type, (unsigned char *)stored + first * stored_size, values, n);
        if (status != SDF_OK)
        {
            return status;
        }
    }
    return SDF_OK;
}

/* The step from the extreme of a type that a sentinel takes away: one count. */
static const double one_count = 1.0;

double sdf_z_scale_for_range(sdf_data_type type, sdf_revision revision,
                             double lowest, double highest)
{
    if (!sdfi_valid_data_type(type) || sdfi_is_floating(type)
        || !(lowest <= highest))
    {
        return 1.0;
    }
    /* The bound that the revision keeps for the sentinel is one step towards
     * zero, as that exact value is not usable for a height. */
    const bool max_sentinel = sdfi_uses_max_sentinel(revision);
    const double usable_max =
        sdfi_highest(type) - (max_sentinel ? one_count : 0.0);
    const double usable_min =
        sdfi_lowest(type) + (max_sentinel ? 0.0 : one_count);
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

static sdf_status find_range(sdf_format format, const void *heights,
                             size_t rows, size_t columns, ptrdiff_t row_stride,
                             double *lowest, double *highest)
{
    if (!sdfi_valid_sample(format.sample) || lowest == NULL || highest == NULL
        || (rows > 0 && columns > 0 && heights == NULL))
    {
        return SDF_ERR_ARGUMENT;
    }
    const size_t sample_size = sdf_sample_size(format.sample);
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
            sdfi_load_samples(format.sample, at + first * sample_size, n,
                              values);
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
    return SDF_OK;
}

sdf_status sdf_suggest_z_scale(sdf_data_type type, sdf_revision revision,
                               sdf_format format, const void *heights,
                               size_t rows, size_t columns,
                               ptrdiff_t row_stride, double *z_scale)
{
    if (!sdfi_valid_data_type(type) || z_scale == NULL)
    {
        return SDF_ERR_ARGUMENT;
    }
    double lowest = 0.0;
    double highest = 0.0;
    const sdf_status status = find_range(format, heights, rows, columns,
                                         row_stride, &lowest, &highest);
    if (status == SDF_OK)
    {
        *z_scale = sdf_z_scale_for_range(type, revision, lowest, highest);
    }
    return status;
}
