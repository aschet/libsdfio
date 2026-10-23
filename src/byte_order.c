/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file byte_order.c
 * @brief Byte order handling.
 */

#include "byte_order.h"

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
