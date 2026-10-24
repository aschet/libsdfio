/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file allocator_default.c
 * @brief The allocator that uses malloc() and free().
 */

#include <sdfio/sdfio.h>
#include <stdlib.h>

static void *allocate(void *user, size_t size)
{
    (void)user;
    return malloc(size);
}

static void release(void *user, void *memory)
{
    (void)user;
    free(memory);
}

const sdfio_allocator *sdfio_default_allocator(void)
{
    static const sdfio_allocator allocator = {NULL, allocate, release};
    return &allocator;
}
