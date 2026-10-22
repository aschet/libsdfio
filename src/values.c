/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file values.c
 * @brief Numbers in memory: the samples of a buffer and the stored values,
 * converted through double, which holds every integer type of the format
 * exactly.
 */

#include "internal.h"

#include <math.h>

bool sdfio_host_is_little_endian(void)
{
    const uint16_t one = 1;
    unsigned char first = 0;
    memcpy(&first, &one, 1);
    return first == 1;
}

void sdfio_fix_endianness(void *values, size_t count, size_t size)
{
    if (sdfio_host_is_little_endian() || size < 2)
    {
        return;
    }
    unsigned char *bytes = values;
    for (size_t i = 0; i < count; ++i)
    {
        unsigned char *value = bytes + i * size;
        for (size_t low = 0, high = size - 1; low < high; ++low, --high)
        {
            const unsigned char swap = value[low];
            value[low] = value[high];
            value[high] = swap;
        }
    }
}

static bool in_range(double value, double low, double high)
{
    return value >= low && value <= high;
}

/** A case of the conversion of a sample to double, for SDFIO_SCALARS(). */
#define LOAD_CASE(sample, data, name, type, bits, low, high, floating)         \
    case sample:                                                               \
        for (size_t i = 0; i < count; ++i)                                     \
        {                                                                      \
            type value;                                                        \
            memcpy(&value, (const unsigned char *)src + (ptrdiff_t)i * stride, \
                   sizeof value);                                              \
            out[i] = (double)value;                                            \
        }                                                                      \
        break;

void sdfio_load_strided(sdfio_sample sample, const void *src, ptrdiff_t stride,
                        size_t count, double *out)
{
    switch (sample)
    {
        SDFIO_SCALARS(LOAD_CASE)
    }
}

void sdfio_load_samples(sdfio_sample sample, const void *src, size_t count,
                        double *out)
{
    sdfio_load_strided(sample, src, (ptrdiff_t)sdfio_sample_size(sample), count,
                       out);
}

/** A case of the conversion of a stored value to double, for SDFIO_SCALARS().
 */
#define LOAD_STORED_CASE(sample, data, name, type, bits, low, high, floating)  \
    case data:                                                                 \
        sdfio_load_samples(sample, src, count, out);                           \
        break;

void sdfio_load_coded(sdfio_data_type type, const void *src, size_t count,
                      double *out)
{
    switch (type)
    {
        SDFIO_SCALARS(LOAD_STORED_CASE)
    }
}

/**
 * A case of the conversion of a double to a sample, for SDFIO_SCALARS(): the
 * integers round to the nearest, and NaN is 0 where there is no NaN.
 */
#define STORE_CASE(sample, data, name, type, bits, low, high, floating)        \
    case sample:                                                               \
        for (size_t i = 0; i < count; ++i)                                     \
        {                                                                      \
            type converted;                                                    \
            if (floating)                                                      \
            {                                                                  \
                if (isfinite(values[i])                                        \
                    && !in_range(values[i], (double)(low), (double)(high)))    \
                {                                                              \
                    return SDFIO_ERR_RANGE;                                    \
                }                                                              \
                converted = (type)values[i];                                   \
            }                                                                  \
            else                                                               \
            {                                                                  \
                const double rounded =                                         \
                    isnan(values[i]) ? 0.0 : nearbyint(values[i]);             \
                if (!in_range(rounded, (double)(low), (double)(high)))         \
                {                                                              \
                    return SDFIO_ERR_RANGE;                                    \
                }                                                              \
                converted = (type)rounded;                                     \
            }                                                                  \
            memcpy((unsigned char *)dst + (ptrdiff_t)i * stride, &converted,   \
                   sizeof converted);                                          \
        }                                                                      \
        break;

sdfio_status sdfio_store_strided(sdfio_sample sample, void *dst,
                                 ptrdiff_t stride, const double *values,
                                 size_t count)
{
    switch (sample)
    {
        SDFIO_SCALARS(STORE_CASE)
    }
    return SDFIO_OK;
}

sdfio_status sdfio_store_samples(sdfio_sample sample, void *dst,
                                 const double *values, size_t count)
{
    return sdfio_store_strided(
        sample, dst, (ptrdiff_t)sdfio_sample_size(sample), values, count);
}

/** A case of the conversion of double to a stored value, for SDFIO_SCALARS().
 */
#define STORE_STORED_CASE(sample, data, name, type, bits, low, high, floating) \
    case data:                                                                 \
        return sdfio_store_samples(sample, dst, values, count);

sdfio_status sdfio_store_coded(sdfio_data_type type, void *dst,
                               const double *values, size_t count)
{
    switch (type)
    {
        SDFIO_SCALARS(STORE_STORED_CASE)
    }
    return SDFIO_ERR_ARGUMENT;
}

sdfio_status sdfio_put_real(sdfio_data_type type, void *dst, size_t index,
                            double value)
{
    const size_t size = sdfio_data_type_size(type);
    return sdfio_store_coded(type, (unsigned char *)dst + index * size, &value,
                             1);
}

sdfio_status sdfio_put_integer(sdfio_data_type type, void *dst, size_t index,
                               long long value)
{
    return sdfio_put_real(type, dst, index, (double)value);
}

double sdfio_get_real(sdfio_data_type type, const void *src, size_t index)
{
    double value = 0.0;
    const size_t size = sdfio_data_type_size(type);
    sdfio_load_coded(type, (const unsigned char *)src + index * size, 1,
                     &value);
    return value;
}
