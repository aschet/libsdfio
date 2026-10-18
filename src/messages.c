/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* The text that tells what went wrong, written into the handle. */

#include "internal.h"

sdfio_status sdfio_fail(sdfio_message *message, sdfio_status code,
                        const char *format, ...)
{
    /* The analyzer of clang-tidy 21 does not see that va_start initializes. */
    // NOLINTBEGIN(clang-analyzer-valist.Uninitialized)
    va_list arguments;
    va_start(arguments, format);
    if (message != NULL)
    {
        vsnprintf(message->text, sizeof message->text, format, arguments);
    }
    va_end(arguments);
    // NOLINTEND(clang-analyzer-valist.Uninitialized)
    return code;
}
