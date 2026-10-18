/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* The build without messages: a call fails with its status code and the
 * handle keeps an empty text, so that no formatting is linked in. */

#include "internal.h"

sdfio_status sdfio_fail(sdfio_message *message, sdfio_status code,
                        const char *format, ...)
{
    (void)message;
    (void)format;
    return code;
}
