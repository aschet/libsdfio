/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file header.c
 * @brief Operations on the header structure.
 */

#include "header.h"
#include "data_type.h"
#include "dates.h"
#include "message.h"
#include "revision.h"
#include "text.h"
#include <string.h>

void sdfio_header_init(sdfio_header *header)
{
    *header = (sdfio_header){0};
    header->revision = SDFIO_ISO_2_0;
    header->representation = SDFIO_BINARY;
    header->data_type = SDFIO_TYPE_FLOAT;
    header->x_scale = 1.0;
    header->y_scale = 1.0;
    header->z_scale = SDFIO_AUTO_Z_SCALE;
    header->z_resolution = SDFIO_NO_RESOLUTION;
    header->creation_date = sdfio_now();
    header->modification_date = header->creation_date;
    static const char producer[] = "libsdfio";
    memcpy(header->manufacturer_id, producer, sizeof producer);
}

bool sdfio_count(const sdfio_header *header, size_t *count)
{
    if (header->columns != 0 && header->rows > SIZE_MAX / header->columns)
    {
        return false;
    }
    *count = header->columns * header->rows;
    return true;
}

static bool is_ascii_text(const char *text, size_t size)
{
    for (size_t i = 0; i < size; ++i)
    {
        if ((unsigned char)text[i] >= SDFIO_FIRST_NON_ASCII)
        {
            return false;
        }
    }
    return true;
}

static bool positive(double value)
{
    return value > 0.0 && isfinite(value);
}

sdfio_status sdfio_validate(const sdfio_header *header, sdfio_message *message)
{
    if ((unsigned)header->revision > (unsigned)SDFIO_ISO_2_0
        || (header->representation != SDFIO_BINARY
            && header->representation != SDFIO_ASCII)
        || !sdfio_valid_data_type(header->data_type))
    {
        return sdfio_fail(
            message, SDFIO_ERR_ARGUMENT,
            "The revision, the representation or the data type is not "
            "one of the enumeration");
    }
    if (!sdfio_revision_allows(header->revision, header->data_type))
    {
        return sdfio_fail(message, SDFIO_ERR_ARGUMENT,
                          "The data type is not valid for the SDF revision");
    }
    if (header->z_scale != SDFIO_AUTO_Z_SCALE && !positive(header->z_scale))
    {
        return sdfio_fail(message, SDFIO_ERR_ARGUMENT,
                          "z_scale must be a positive number");
    }
    /* A resolution that is not positive is not known. */
    if (!isfinite(header->z_resolution))
    {
        return sdfio_fail(message, SDFIO_ERR_ARGUMENT,
                          "z_resolution must be a finite number");
    }
    /* The standard puts no limit on the scales: the y_scale of a profile is
     * meaningless. */
    if (!isfinite(header->x_scale) || !isfinite(header->y_scale))
    {
        return sdfio_fail(message, SDFIO_ERR_ARGUMENT,
                          "x_scale and y_scale must be finite numbers");
    }
    const char *terminator =
        memchr(header->manufacturer_id, '\0', sizeof header->manufacturer_id);
    if (terminator == NULL)
    {
        return sdfio_fail(message, SDFIO_ERR_ARGUMENT,
                          "manufacturer_id has to end with a 0");
    }
    if (!is_ascii_text(header->manufacturer_id,
                       (size_t)(terminator - header->manufacturer_id)))
    {
        return sdfio_fail(message, SDFIO_ERR_ARGUMENT,
                          "manufacturer_id must be 7-bit ASCII");
    }
    if (header->creation_date == SDFIO_NO_TIME
        || header->modification_date == SDFIO_NO_TIME)
    {
        return sdfio_fail(message, SDFIO_ERR_ARGUMENT,
                          "A file needs a creation date and a modification "
                          "date: SDFIO_NO_TIME cannot be written");
    }
    if (!sdfio_date_representable(header->creation_date)
        || !sdfio_date_representable(header->modification_date))
    {
        return sdfio_fail(message, SDFIO_ERR_ARGUMENT,
                          "The year of a date is out of the range of the "
                          "SDF files");
    }
    size_t count = 0;
    if (!sdfio_count(header, &count))
    {
        return sdfio_fail(message, SDFIO_ERR_ARGUMENT,
                          "columns times rows does not fit in size_t");
    }
    if (header->columns > sdfio_max_extent(header->revision)
        || header->rows > sdfio_max_extent(header->revision))
    {
        return sdfio_fail(message, SDFIO_ERR_ARGUMENT,
                          "The columns or the rows do not fit the header of "
                          "the SDF revision");
    }
    return SDFIO_OK;
}
