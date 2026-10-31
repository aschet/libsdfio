/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief Initial values of the stream and the time zone.
 */

#include <sdfio/sdfio.h>

void sdfio_stream_init(sdfio_stream *stream)
{
    *stream = (sdfio_stream){0};
}

void sdfio_time_zone_init(sdfio_time_zone *time_zone)
{
    *time_zone = (sdfio_time_zone){0};
}
