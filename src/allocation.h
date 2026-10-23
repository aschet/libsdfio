/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file allocation.h
 * @brief Allocation through the allocator of a configuration.
 */

#ifndef SDFIO_SRC_ALLOCATION_H
#define SDFIO_SRC_ALLOCATION_H

#include <sdfio/sdfio.h>

/**
 * Allocates memory through the allocator of a configuration.
 *
 * @param allocator The allocator. If it is null or has no allocate function,
 * malloc is used.
 * @param size The size in bytes.
 * @return The memory, or null if there is none.
 */
void *sdfio_allocate(const sdfio_allocator *allocator, size_t size);

/**
 * Releases memory from sdfio_allocate().
 *
 * @param allocator The allocator that made the memory.
 * @param memory The memory. May be null.
 */
void sdfio_release(const sdfio_allocator *allocator, void *memory);

#endif /* SDFIO_SRC_ALLOCATION_H */
