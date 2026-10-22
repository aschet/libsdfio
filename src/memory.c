/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file memory.c
 * @brief The memory for the data and the heights that the library allocates
 * for the caller.
 */

#include "internal.h"

/* The room for `rows` rows of `columns` elements, or null if the size does not
 * fit in size_t. An empty matrix gets one byte, so that null always means a
 * failure. */
static void *allocate(const sdfio_allocator *allocator, size_t columns,
                      size_t rows, size_t element_size)
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
    void *memory = sdfio_allocate(allocator, bytes > 0 ? bytes : 1);
    if (memory != NULL)
    {
        memset(memory, 0, bytes > 0 ? bytes : 1);
    }
    return memory;
}

void *sdfio_alloc_data(const sdfio_allocator *allocator,
                       const sdfio_header *header, size_t rows)
{
    if (header == NULL)
    {
        return NULL;
    }
    return allocate(allocator, header->columns, rows,
                    sdfio_data_type_size(header->data_type));
}

void *sdfio_alloc_heights(const sdfio_allocator *allocator,
                          const sdfio_header *header, sdfio_sample sample,
                          size_t rows)
{
    if (header == NULL)
    {
        return NULL;
    }
    return allocate(allocator, header->columns, rows,
                    sdfio_sample_size(sample));
}

void sdfio_free(const sdfio_allocator *allocator, void *memory)
{
    sdfio_release(allocator, memory);
}
