/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file allocator.c
 * @brief Allocation and release through the allocator of a handle.
 */

#include "allocator.h"

void *sdfio_allocate(const sdfio_allocator *allocator, size_t size)
{
    return allocator->allocate(allocator->user, size);
}

void sdfio_release(const sdfio_allocator *allocator, void *memory)
{
    if (memory != NULL)
    {
        allocator->release(allocator->user, memory);
    }
}
