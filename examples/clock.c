/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* The clock of the scanner for the firmware image of the allocators example,
 * which sdfio_header_create() reads for the dates. The C library of a
 * bare-metal target has none. */

#include <time.h>

time_t time(time_t *timer)
{
    static const time_t scan_time = 1700000000;
    if (timer != NULL)
    {
        *timer = scan_time;
    }
    return scan_time;
}
