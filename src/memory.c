/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

#include "internal.h"

/* The room for `rows` rows of `columns` elements, or null if the size does not
 * fit in size_t. An empty matrix gets one byte, so that null always means a
 * failure. */
static void *allocate(size_t columns, size_t rows, size_t element_size)
{
    if (element_size == 0 || (columns != 0 && rows > SIZE_MAX / columns))
    {
        return NULL;
    }
    const size_t count = columns * rows;
    if (count > SIZE_MAX / element_size)
    {
        return NULL;
    }
    const size_t bytes = count * element_size;
    return calloc(bytes > 0 ? bytes : 1, 1);
}

void *sdf_alloc_stored(const sdf_info *info, size_t rows)
{
    if (info == NULL)
    {
        return NULL;
    }
    return allocate(info->columns, rows, sdf_data_type_size(info->data_type));
}

void *sdf_alloc_heights(const sdf_info *info, sdf_sample sample, size_t rows)
{
    if (info == NULL)
    {
        return NULL;
    }
    return allocate(info->columns, rows, sdf_sample_size(sample));
}

void sdf_free(void *memory)
{
    free(memory);
}
