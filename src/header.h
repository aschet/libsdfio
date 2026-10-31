/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief Header of a file.
 */

#ifndef SDFIO_SRC_HEADER_H
#define SDFIO_SRC_HEADER_H

#include "fields.h"
#include <sdfio/sdfio.h>

/** Header of a file. */
struct sdfio_header
{
    /** Revision of the format. */
    sdfio_revision revision;
    /** Format of the file. */
    sdfio_format format;
    /** Data type of the coded values in the data area. */
    sdfio_data_type data_type;
    /** Number of columns M of the data matrix. */
    uint32_t columns;
    /** Number of rows N of the data matrix. */
    uint32_t rows;
    /** Sampling interval along x in metres. */
    double x_scale;
    /** Sampling interval along y in metres. */
    double y_scale;
    /** Height in metres that a coded value of 1 stands for. */
    double z_scale;
    /** Quantization step of the instrument in metres. */
    double z_resolution;
    /** Original creation date and time. */
    time_t creation_date;
    /** Last modification date and time. */
    time_t modification_date;
    /** Manufacturer's identifier, terminated by a null character. */
    char manufacturer_id[SDFIO_MANUFACTURER_ID_MAX + 1];
    /** The allocator that created the header; empty for a header inside a
     * reader or a writer. */
    sdfio_allocator allocator;
};

/**
 * Computes the total number of coded values of the data area: the number of
 * columns times the number of rows of the data matrix that the header
 * describes. The function reports an overflow of size_t instead of wrapping
 * around.
 *
 * @param header The header, with the numbers of columns and rows.
 * @param[out] values Receives the number of coded values.
 * @return False if the number does not fit in size_t.
 */
bool sdfio_coded_values(const sdfio_header *header, size_t *values);

/**
 * Validates a header for the creation of a file.
 *
 * The setters have checked every field alone. The function verifies what
 * depends on several fields: the revision permits the data type, the
 * creation date is set, the numbers of columns and rows do not exceed the limit
 * of the revision, and their product fits in size_t.
 *
 * @param header The header to validate.
 * @param[out] message Receives the reason of a failure. May be null.
 * @retval SDFIO_OK The header can be written.
 * @retval SDFIO_ERR_ARGUMENT A field of the header is invalid.
 */
sdfio_status sdfio_header_check(const sdfio_header *header,
                                const char **message);

/**
 * Formats the creation date and the modification date of a header as the date
 * fields of a file, before anything of the file is written.
 *
 * @param header The header.
 * @param zone The time zone of the caller. May be null.
 * @param[out] created Receives the creation date, without a terminator.
 * @param[out] modified Receives the modification date, without a terminator.
 * @param[out] message Receives the reason of a failure.
 * @retval SDFIO_OK Both dates are formatted.
 * @retval SDFIO_ERR_RANGE The time zone cannot convert a date.
 */
sdfio_status sdfio_header_format_dates(const sdfio_header *header,
                                       const sdfio_time_zone *zone,
                                       char created[SDFIO_DATETIME_SIZE],
                                       char modified[SDFIO_DATETIME_SIZE],
                                       const char **message);

/**
 * Converts the data type code of a file header to a data type that the
 * revision of the file permits.
 *
 * @param code The code of the data type.
 * @param revision The revision of the file.
 * @param[out] type Receives the data type.
 * @param[out] message Receives the reason of a failure.
 * @retval SDFIO_OK The code names a data type that the revision permits.
 * @retval SDFIO_ERR_FORMAT The code is unknown, or the revision does not permit
 * the data type.
 */
sdfio_status sdfio_header_data_type_from_code(long long code,
                                              sdfio_revision revision,
                                              sdfio_data_type *type,
                                              const char **message);

#endif /* SDFIO_SRC_HEADER_H */
