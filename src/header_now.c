/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file header_now.c
 * @brief Dates of a header from the clock; the only code that reads it.
 */

#include "datetime.h"
#include <sdfio/sdfio.h>
#include <time.h>

sdfio_status sdfio_header_set_now(sdfio_header *header)
{
    if (header == NULL)
    {
        return SDFIO_ERR_ARGUMENT;
    }
    const time_t now = time(NULL);
    if (now == (time_t)-1)
    {
        return SDFIO_ERR_UNSUPPORTED;
    }
    const time_t minute = sdfio_datetime_round(now);
    const sdfio_status status = sdfio_header_set_created(header, minute);
    return status != SDFIO_OK ? status
                              : sdfio_header_set_modified(header, minute);
}
