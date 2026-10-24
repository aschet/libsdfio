/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file allocator.h
 * @brief The allocator of a handle.
 */

#ifndef SDFIO_SRC_ALLOCATOR_H
#define SDFIO_SRC_ALLOCATOR_H

#include <sdfio/sdfio.h>
#include <stdbool.h>

/**
 * Allocates memory through the allocator of a handle.
 *
 * @param allocator The allocator, with both callbacks set.
 * @param size The size in bytes.
 * @return The memory, or null if there is none.
 */
void *sdfio_allocate(const sdfio_allocator *allocator, size_t size);

/**
 * Releases memory from sdfio_allocate().
 *
 * @param allocator The allocator that allocated the memory.
 * @param memory The memory. May be null.
 */
void sdfio_release(const sdfio_allocator *allocator, void *memory);

/**
 * Tells whether an allocator can be used: both callbacks are set.
 *
 * @param allocator The allocator.
 */
static inline bool sdfio_allocator_valid(const sdfio_allocator *allocator)
{
    return allocator->allocate != NULL && allocator->release != NULL;
}

/**
 * Takes the allocator argument of a create function.
 *
 * @param given The argument.
 * @param[out] used Receives a copy of the allocator.
 * @return False if the argument is null or lacks a callback.
 */
static inline bool sdfio_allocator_take(const sdfio_allocator *given,
                                        sdfio_allocator *used)
{
    if (given == NULL || !sdfio_allocator_valid(given))
    {
        return false;
    }
    *used = *given;
    return true;
}

#endif /* SDFIO_SRC_ALLOCATOR_H */
