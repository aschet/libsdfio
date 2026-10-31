/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief Version of the library.
 */

#include <sdfio/sdfio.h>

const char *sdfio_version(void)
{
    return SDFIO_VERSION_STRING;
}
