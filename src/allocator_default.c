/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file allocator_default.c
 * @brief The default allocator on malloc() and free().
 */

#include <sdfio/sdfio.h>
#include <stdlib.h>

static void *default_allocate(void *user, size_t size)
{
    (void)user;
    return malloc(size);
}

static void default_release(void *user, void *memory)
{
    (void)user;
    free(memory);
}

const sdfio_allocator *sdfio_default_allocator(void)
{
    static const sdfio_allocator allocator = {NULL, default_allocate,
                                              default_release};
    return &allocator;
}
