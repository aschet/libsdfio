/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file text.c
 * @brief Locale-independent tests and conversions of ASCII characters.
 */

#include "text.h"

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
