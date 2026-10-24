/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file header.c
 * @brief Creation, accessors and validation of the header.
 */

#include "header.h"
#include "allocation.h"
#include "data_type.h"
#include "datetime.h"
#include "defaults.h"
#include "message.h"
#include "revision.h"
#include "text.h"
#include <string.h>

sdfio_status sdfio_header_create(const sdfio_allocator *allocator,
                                 sdfio_header **header)
{
    if (header == NULL)
    {
        return SDFIO_ERR_ARGUMENT;
    }
    *header = NULL;
    sdfio_allocator used;
    if (!sdfio_allocator_take(allocator, &used))
    {
        return SDFIO_ERR_ARGUMENT;
    }
    sdfio_header *created = sdfio_allocate(&used, sizeof *created);
    if (created == NULL)
    {
        return SDFIO_ERR_MEMORY;
    }
    *created = (sdfio_header){.revision = SDFIO_ISO_2_0,
                              .format = SDFIO_BINARY,
                              .data_type = SDFIO_TYPE_FLOAT,
                              .x_scale = 1.0,
                              .y_scale = 1.0,
                              .z_scale = SDFIO_AUTO_Z_SCALE,
                              .z_resolution = SDFIO_NO_RESOLUTION,
                              .creation_date = SDFIO_NO_TIME,
                              .modification_date = SDFIO_NO_TIME,
                              .manufacturer_id = "libsdfio",
                              .allocator = used};
    *header = created;
    return SDFIO_OK;
}

sdfio_status sdfio_header_copy(sdfio_header *dst, const sdfio_header *src)
{
    if (dst == NULL || src == NULL)
    {
        return SDFIO_ERR_ARGUMENT;
    }
    const sdfio_allocator allocator = dst->allocator;
    *dst = *src;
    dst->allocator = allocator;
    return SDFIO_OK;
}

void sdfio_header_free(sdfio_header *header)
{
    if (header == NULL)
    {
        return;
    }
    const sdfio_allocator allocator = header->allocator;
    sdfio_release(&allocator, header);
}

sdfio_revision sdfio_header_revision(const sdfio_header *header)
{
    return header->revision;
}

sdfio_format sdfio_header_format(const sdfio_header *header)
{
    return header->format;
}

sdfio_data_type sdfio_header_data_type(const sdfio_header *header)
{
    return header->data_type;
}

uint32_t sdfio_header_columns(const sdfio_header *header)
{
    return header->columns;
}

uint32_t sdfio_header_rows(const sdfio_header *header)
{
    return header->rows;
}

double sdfio_header_x_scale(const sdfio_header *header)
{
    return header->x_scale;
}

double sdfio_header_y_scale(const sdfio_header *header)
{
    return header->y_scale;
}

double sdfio_header_z_scale(const sdfio_header *header)
{
    return header->z_scale;
}

double sdfio_header_z_resolution(const sdfio_header *header)
{
    return header->z_resolution;
}

time_t sdfio_header_creation_date(const sdfio_header *header)
{
    return header->creation_date;
}

time_t sdfio_header_modification_date(const sdfio_header *header)
{
    return header->modification_date;
}

const char *sdfio_header_manufacturer_id(const sdfio_header *header)
{
    return header->manufacturer_id;
}

/* Whether a number is greater than 0 and finite. */
static bool positive(double value)
{
    return value > 0.0 && isfinite(value);
}

/* Whether a date is not recorded or lies in the supported range of years. */
static bool date_valid(time_t date)
{
    return date == SDFIO_NO_TIME || sdfio_datetime_representable(date);
}

sdfio_status sdfio_header_set_revision(sdfio_header *header,
                                       sdfio_revision revision)
{
    if (header == NULL || !sdfio_revision_valid(revision))
    {
        return SDFIO_ERR_ARGUMENT;
    }
    header->revision = revision;
    return SDFIO_OK;
}

sdfio_status sdfio_header_set_format(sdfio_header *header, sdfio_format format)
{
    if (header == NULL || (format != SDFIO_BINARY && format != SDFIO_ASCII))
    {
        return SDFIO_ERR_ARGUMENT;
    }
    header->format = format;
    return SDFIO_OK;
}

sdfio_status sdfio_header_set_data_type(sdfio_header *header,
                                        sdfio_data_type data_type)
{
    if (header == NULL || !sdfio_valid_data_type(data_type))
    {
        return SDFIO_ERR_ARGUMENT;
    }
    header->data_type = data_type;
    return SDFIO_OK;
}

sdfio_status sdfio_header_set_columns(sdfio_header *header, uint32_t columns)
{
    if (header == NULL)
    {
        return SDFIO_ERR_ARGUMENT;
    }
    header->columns = columns;
    return SDFIO_OK;
}

sdfio_status sdfio_header_set_rows(sdfio_header *header, uint32_t rows)
{
    if (header == NULL)
    {
        return SDFIO_ERR_ARGUMENT;
    }
    header->rows = rows;
    return SDFIO_OK;
}

/* The standard puts no limit on the scales: the y-scale of a profile is
 * meaningless, so a scale is any finite number. */
sdfio_status sdfio_header_set_x_scale(sdfio_header *header, double x_scale)
{
    if (header == NULL || !isfinite(x_scale))
    {
        return SDFIO_ERR_ARGUMENT;
    }
    header->x_scale = x_scale;
    return SDFIO_OK;
}

sdfio_status sdfio_header_set_y_scale(sdfio_header *header, double y_scale)
{
    if (header == NULL || !isfinite(y_scale))
    {
        return SDFIO_ERR_ARGUMENT;
    }
    header->y_scale = y_scale;
    return SDFIO_OK;
}

sdfio_status sdfio_header_set_z_scale(sdfio_header *header, double z_scale)
{
    if (header == NULL || (z_scale != SDFIO_AUTO_Z_SCALE && !positive(z_scale)))
    {
        return SDFIO_ERR_ARGUMENT;
    }
    header->z_scale = z_scale;
    return SDFIO_OK;
}

/* A resolution that is not positive is not known. */
sdfio_status sdfio_header_set_z_resolution(sdfio_header *header,
                                           double z_resolution)
{
    if (header == NULL || !isfinite(z_resolution))
    {
        return SDFIO_ERR_ARGUMENT;
    }
    header->z_resolution = z_resolution;
    return SDFIO_OK;
}

sdfio_status sdfio_header_set_creation_date(sdfio_header *header,
                                            time_t creation_date)
{
    if (header == NULL || !date_valid(creation_date))
    {
        return SDFIO_ERR_ARGUMENT;
    }
    header->creation_date = creation_date;
    return SDFIO_OK;
}

sdfio_status sdfio_header_set_modification_date(sdfio_header *header,
                                                time_t modification_date)
{
    if (header == NULL || !date_valid(modification_date))
    {
        return SDFIO_ERR_ARGUMENT;
    }
    header->modification_date = modification_date;
    return SDFIO_OK;
}

sdfio_status sdfio_header_set_manufacturer_id(sdfio_header *header,
                                              const char *manufacturer_id)
{
    if (header == NULL || manufacturer_id == NULL)
    {
        return SDFIO_ERR_ARGUMENT;
    }
    const size_t length = strlen(manufacturer_id);
    if (length > SDFIO_MANUFACTURER_ID_MAX)
    {
        return SDFIO_ERR_ARGUMENT;
    }
    for (size_t i = 0; i < length; ++i)
    {
        if (!sdfio_is_ascii(manufacturer_id[i]))
        {
            return SDFIO_ERR_ARGUMENT;
        }
    }
    memcpy(header->manufacturer_id, manufacturer_id, length + 1);
    return SDFIO_OK;
}

bool sdfio_coded_values(const sdfio_header *header, size_t *values)
{
    if (header->columns != 0 && header->rows > SIZE_MAX / header->columns)
    {
        return false;
    }
    *values = (size_t)header->columns * header->rows;
    return true;
}

sdfio_status sdfio_header_validate_for_writing(const sdfio_header *header,
                                               sdfio_message *message)
{
    if (!sdfio_revision_allows(header->revision, header->data_type))
    {
        return sdfio_fail(message, SDFIO_ERR_ARGUMENT,
                          "The data type is not valid for the SDF revision");
    }
    if (header->creation_date == SDFIO_NO_TIME
        || header->modification_date == SDFIO_NO_TIME)
    {
        return sdfio_fail(message, SDFIO_ERR_ARGUMENT,
                          "A file needs a creation date and a modification "
                          "date: SDFIO_NO_TIME cannot be written");
    }
    size_t values = 0;
    if (!sdfio_coded_values(header, &values))
    {
        return sdfio_fail(message, SDFIO_ERR_ARGUMENT,
                          "columns times rows does not fit in size_t");
    }
    if (header->columns > sdfio_max_columns_and_rows(header->revision)
        || header->rows > sdfio_max_columns_and_rows(header->revision))
    {
        return sdfio_fail(message, SDFIO_ERR_ARGUMENT,
                          "The columns or the rows do not fit the header of "
                          "the SDF revision");
    }
    return SDFIO_OK;
}
