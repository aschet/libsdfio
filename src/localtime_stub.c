/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* The build without a local clock: the files of ISO-1.0 and BCR-1.0 hold
 * the dates as UTC then, as those of ISO-2.0 do. */

#include "internal.h"

bool sdfio_local_time(time_t time, struct tm *out)
{
    (void)time;
    (void)out;
    return false;
}
