/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file text.c
 * @brief Locale-independent ASCII text helpers.
 */

#include "text.h"
#include <string.h>

void sdfio_trim(const char **text, size_t *size)
{
    while (*size > 0 && sdfio_is_space((*text)[0]))
    {
        ++*text;
        --*size;
    }
    while (*size > 0 && sdfio_is_space((*text)[*size - 1]))
    {
        --*size;
    }
}
