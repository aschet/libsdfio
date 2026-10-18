/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* Numbers in memory: the samples of a buffer and the stored values, converted
 * through double, which holds every integer type of the format exactly. */

#include "internal.h"

#include <math.h>

bool sdfi_host_is_little_endian(void)
{
    const uint16_t one = 1;
    unsigned char first = 0;
    memcpy(&first, &one, 1);
    return first == 1;
}

void sdfi_fix_endianness(void *values, size_t count, size_t size)
{
    if (sdfi_host_is_little_endian() || size < 2)
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

#define LOAD_CASE(sample, data, name, type, bits, low, high, floating)         \
    case sample:                                                               \
        for (size_t i = 0; i < count; ++i)                                     \
        {                                                                      \
            type value;                                                        \
            memcpy(&value, (const unsigned char *)src + i * sizeof value,      \
                   sizeof value);                                              \
            out[i] = (double)value;                                            \
        }                                                                      \
        break;

void sdfi_load_samples(sdfio_sample sample, const void *src, size_t count,
                       double *out)
{
    switch (sample)
    {
        SDFI_SCALARS(LOAD_CASE)
    }
}

#define LOAD_STORED_CASE(sample, data, name, type, bits, low, high, floating)  \
    case data:                                                                 \
        sdfi_load_samples(sample, src, count, out);                            \
        break;

void sdfi_load_coded(sdfio_data_type type, const void *src, size_t count,
                     double *out)
{
    switch (type)
    {
        SDFI_SCALARS(LOAD_STORED_CASE)
    }
}

/* A number into the type of a sample or of a stored value: the integers round
 * to the nearest, and NaN is 0 where there is no NaN. */
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
            memcpy((unsigned char *)dst + i * sizeof converted, &converted,    \
                   sizeof converted);                                          \
        }                                                                      \
        break;

sdfio_status sdfi_store_samples(sdfio_sample sample, void *dst,
                                const double *values, size_t count)
{
    switch (sample)
    {
        SDFI_SCALARS(STORE_CASE)
    }
    return SDFIO_OK;
}

#define STORE_STORED_CASE(sample, data, name, type, bits, low, high, floating) \
    case data:                                                                 \
        return sdfi_store_samples(sample, dst, values, count);

sdfio_status sdfi_store_coded(sdfio_data_type type, void *dst,
                              const double *values, size_t count)
{
    switch (type)
    {
        SDFI_SCALARS(STORE_STORED_CASE)
    }
    return SDFIO_ERR_ARGUMENT;
}

sdfio_status sdfi_put_real(sdfio_data_type type, void *dst, size_t index,
                           double value)
{
    const size_t size = sdfio_data_type_size(type);
    return sdfi_store_coded(type, (unsigned char *)dst + index * size, &value,
                            1);
}

sdfio_status sdfi_put_integer(sdfio_data_type type, void *dst, size_t index,
                              long long value)
{
    return sdfi_put_real(type, dst, index, (double)value);
}

double sdfi_get_real(sdfio_data_type type, const void *src, size_t index)
{
    double value = 0.0;
    const size_t size = sdfio_data_type_size(type);
    sdfi_load_coded(type, (const unsigned char *)src + index * size, 1, &value);
    return value;
}
