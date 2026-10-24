/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file allocation.c
 * @brief Allocation through the allocator of a handle.
 */

#include "allocation.h"
#include <stdlib.h>

void *sdfio_allocate(const sdfio_allocator *allocator, size_t size)
{
    if (allocator != NULL && allocator->allocate != NULL)
    {
        return allocator->allocate(allocator->user, size);
    }
    return malloc(size);
}

void sdfio_release(const sdfio_allocator *allocator, void *memory)
{
    if (memory == NULL)
    {
        return;
    }
    if (allocator != NULL && allocator->release != NULL)
    {
        allocator->release(allocator->user, memory);
    }
    else
    {
        free(memory);
    }
}
