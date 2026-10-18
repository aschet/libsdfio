/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* The local clock of the C library. Only localtime_r (localtime_s on Windows)
 * is used, which is safe to call from several threads. */

#include "internal.h"

bool sdfio_local_time(time_t time, struct tm *out)
{
#if defined(_WIN32)
    return localtime_s(out, &time) == 0;
#else
    return localtime_r(&time, out) != NULL;
#endif
}
