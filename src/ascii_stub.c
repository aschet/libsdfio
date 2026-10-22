/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* The build without text files: only binary files are read and written. */

#include "internal.h"

/* The functions have the signatures of the ones in ascii.c, which write
 * through their pointers. */
// NOLINTBEGIN(readability-non-const-parameter)

static sdfio_status unsupported(sdfio_message *message)
{
    return sdfio_fail(message, SDFIO_ERR_UNSUPPORTED,
                      "Text SDF files are not supported by this build");
}

sdfio_point sdfio_point_now(void)
{
    return (sdfio_point){{'.', '\0'}};
}

bool sdfio_has_text(void)
{
    return false;
}

sdfio_status sdfio_ascii_parse_header(sdfio_source *source,
                                      sdfio_header *header,
                                      const sdfio_point *point,
                                      const sdfio_time_zone *zone,
                                      sdfio_message *message)
{
    (void)zone;
    (void)source;
    (void)header;
    (void)point;
    return unsupported(message);
}

sdfio_status sdfio_ascii_value(sdfio_source *source, const sdfio_header *header,
                               const sdfio_point *point, void *dst,
                               size_t index, sdfio_message *message)
{
    (void)source;
    (void)header;
    (void)point;
    (void)dst;
    (void)index;
    return unsupported(message);
}

sdfio_status sdfio_ascii_finish(sdfio_source *source,
                                const sdfio_header *header,
                                sdfio_message *message)
{
    (void)source;
    (void)header;
    return unsupported(message);
}

sdfio_status sdfio_ascii_skip(sdfio_source *source, const sdfio_header *header,
                              sdfio_message *message)
{
    (void)source;
    (void)header;
    return unsupported(message);
}

sdfio_status sdfio_ascii_trailer(sdfio_source *source,
                                 const sdfio_header *header, sdfio_string *out,
                                 bool *present, sdfio_message *message)
{
    (void)source;
    (void)header;
    (void)out;
    (void)present;
    return unsupported(message);
}

sdfio_status sdfio_ascii_header(const sdfio_header *header,
                                const sdfio_point *point,
                                const sdfio_time_zone *zone, sdfio_string *out,
                                sdfio_message *message)
{
    (void)zone;
    (void)header;
    (void)point;
    (void)out;
    return unsupported(message);
}

size_t sdfio_ascii_format(const sdfio_header *header, const sdfio_point *point,
                          const void *coded, size_t index, char *out,
                          size_t capacity)
{
    (void)header;
    (void)point;
    (void)coded;
    (void)index;
    (void)out;
    (void)capacity;
    return 0;
}

sdfio_status sdfio_ascii_tail(const char *trailer, size_t size,
                              sdfio_string *out, sdfio_message *message)
{
    (void)trailer;
    (void)size;
    (void)out;
    return unsupported(message);
}
// NOLINTEND(readability-non-const-parameter)
