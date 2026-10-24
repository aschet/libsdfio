/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * The header of a file (record 1).
 */

#ifndef SDFIO_HEADER_H
#define SDFIO_HEADER_H

#include <sdfio/sdfio_export.h>
#include <sdfio/types.h>

#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * Header of a file (record 1), with the field names of the standard.
 *
 * The points of the matrix lie in a right-handed coordinate system: x grows
 * with the column, y with the row, and z points to the viewer who looks from
 * above. The row y = 0 is the first row of the file.
 *
 * A header is opaque. sdfio_header_create() creates one with defaults, the
 * setters fill it, sdfio_header_copy() copies it and sdfio_header_free()
 * releases it. A reader describes the file that it opened by a header that
 * sdfio_reader_header() returns. A writer takes a header as the description of
 * the file to create and keeps a copy. The compression type and the checksum
 * type are not members: the standard allows neither.
 *
 * Setters check what depends on their field alone. sdfio_create_file() and
 * sdfio_create_stream() check the rest: the data type against the revision,
 * the numbers of columns and rows against the limit of the revision, and the
 * dates.
 */
typedef struct sdfio_header sdfio_header;

/**
 * Creates a header with default values.
 *
 * The defaults are: revision ISO-2.0, binary format, data type
 * SDFIO_TYPE_FLOAT, x_scale and y_scale of 1, z_scale SDFIO_AUTO_Z_SCALE,
 * z_resolution SDFIO_NO_RESOLUTION, both dates SDFIO_NO_TIME, manufacturer_id
 * "libsdfio", and zero columns and rows. The caller sets the number of columns
 * and rows, the dates, which a file that is written requires, and every other
 * field that differs. The function does not read the clock, so a program
 * without one needs none.
 *
 * @param[in] allocator The allocator of the header, for example
 * sdfio_default_allocator(). The structure is copied, so it need not outlive
 * the call. Both callbacks must be set.
 * @param[out] header Receives the header, or null if the call fails. The
 * caller releases it with sdfio_header_free().
 * @retval SDFIO_ERR_ARGUMENT A pointer is null, or the allocator lacks a
 * callback.
 * @retval SDFIO_ERR_MEMORY The allocation failed.
 */
SDFIO_API sdfio_status sdfio_header_create(const sdfio_allocator *allocator,
                                           sdfio_header **header);

/**
 * Copies every field of a header into another header, which keeps its
 * allocator. A copy of the header of a reader is a header that the caller can
 * change and keep after the reader is freed.
 *
 * @param[out] dst The header that receives the fields.
 * @param[in] src The header to copy.
 * @retval SDFIO_ERR_ARGUMENT A pointer is null.
 */
SDFIO_API sdfio_status sdfio_header_copy(sdfio_header *dst,
                                         const sdfio_header *src);

/**
 * Sets the creation date and the modification date to the time now, rounded
 * down to the minute. The function is the only one of the library that reads
 * the clock.
 *
 * @param[in,out] header The header.
 * @retval SDFIO_ERR_ARGUMENT `header` is null, or the year of the clock is out
 * of the range of the SDF files.
 * @retval SDFIO_ERR_UNSUPPORTED The clock is not available.
 */
SDFIO_API sdfio_status sdfio_header_set_now(sdfio_header *header);

/**
 * Releases a header that sdfio_header_create() created. A null pointer is
 * ignored. The header that sdfio_reader_header() returns is not released.
 *
 * @param[in] header The header.
 */
SDFIO_API void sdfio_header_free(sdfio_header *header);

/**
 * Returns the revision of the format.
 *
 * @param[in] header The header.
 */
SDFIO_API sdfio_revision sdfio_header_revision(const sdfio_header *header);

/**
 * Sets the revision of the format.
 *
 * @param[in,out] header The header.
 * @param revision The revision.
 * @retval SDFIO_ERR_ARGUMENT A pointer is null, or the value is not an
 * enumerator.
 */
SDFIO_API sdfio_status sdfio_header_set_revision(sdfio_header *header,
                                                 sdfio_revision revision);

/**
 * Returns the format of the file.
 *
 * @param[in] header The header.
 */
SDFIO_API sdfio_format sdfio_header_format(const sdfio_header *header);

/**
 * Sets the format of the file.
 *
 * @param[in,out] header The header.
 * @param format The format.
 * @retval SDFIO_ERR_ARGUMENT A pointer is null, or the value is not an
 * enumerator.
 */
SDFIO_API sdfio_status sdfio_header_set_format(sdfio_header *header,
                                               sdfio_format format);

/**
 * Returns the data type of the coded values in the data area.
 *
 * @param[in] header The header.
 */
SDFIO_API sdfio_data_type sdfio_header_data_type(const sdfio_header *header);

/**
 * Sets the data type of the coded values in the data area.
 *
 * @param[in,out] header The header.
 * @param data_type The data type.
 * @retval SDFIO_ERR_ARGUMENT A pointer is null, or the value is not an
 * enumerator.
 */
SDFIO_API sdfio_status sdfio_header_set_data_type(sdfio_header *header,
                                                  sdfio_data_type data_type);

/**
 * Returns the number of columns M of the data matrix: the points along x.
 *
 * @param[in] header The header.
 */
SDFIO_API uint32_t sdfio_header_columns(const sdfio_header *header);

/**
 * Sets the number of columns M of the data matrix. ISO-1.0 stores 16 bits, so
 * the limit of the revision is checked when the file is created.
 *
 * @param[in,out] header The header.
 * @param columns The number of columns.
 * @retval SDFIO_ERR_ARGUMENT `header` is null.
 */
SDFIO_API sdfio_status sdfio_header_set_columns(sdfio_header *header,
                                                uint32_t columns);

/**
 * Returns the number of rows N of the data matrix: 1 for a profile.
 *
 * @param[in] header The header.
 */
SDFIO_API uint32_t sdfio_header_rows(const sdfio_header *header);

/**
 * Sets the number of rows N of the data matrix. ISO-1.0 stores 16 bits, so the
 * limit of the revision is checked when the file is created.
 *
 * @param[in,out] header The header.
 * @param rows The number of rows.
 * @retval SDFIO_ERR_ARGUMENT `header` is null.
 */
SDFIO_API sdfio_status sdfio_header_set_rows(sdfio_header *header,
                                             uint32_t rows);

/**
 * Returns the sampling interval along x in metres (X-scale factor).
 *
 * @param[in] header The header.
 */
SDFIO_API double sdfio_header_x_scale(const sdfio_header *header);

/**
 * Sets the sampling interval along x in metres.
 *
 * @param[in,out] header The header.
 * @param x_scale The interval, a finite number.
 * @retval SDFIO_ERR_ARGUMENT `header` is null, or the value is not finite.
 */
SDFIO_API sdfio_status sdfio_header_set_x_scale(sdfio_header *header,
                                                double x_scale);

/**
 * Returns the sampling interval along y in metres (Y-scale factor). It is
 * meaningless for a profile.
 *
 * @param[in] header The header.
 */
SDFIO_API double sdfio_header_y_scale(const sdfio_header *header);

/**
 * Sets the sampling interval along y in metres. Any finite value is written
 * for a profile.
 *
 * @param[in,out] header The header.
 * @param y_scale The interval, a finite number.
 * @retval SDFIO_ERR_ARGUMENT `header` is null, or the value is not finite.
 */
SDFIO_API sdfio_status sdfio_header_set_y_scale(sdfio_header *header,
                                                double y_scale);

/**
 * Returns the z-scale factor: the height in metres that a coded value of 1 in
 * the data area stands for. A reader always sets it.
 *
 * @param[in] header The header.
 */
SDFIO_API double sdfio_header_z_scale(const sdfio_header *header);

/**
 * Sets the z-scale factor.
 *
 * A writer treats SDFIO_AUTO_Z_SCALE as 1 for the floating point data types.
 * For the integer data types, sdfio_write_heights() selects the z-scale from
 * the heights, whereas sdfio_write_data() requires it to be set.
 *
 * @param[in,out] header The header.
 * @param z_scale A positive finite number, or SDFIO_AUTO_Z_SCALE.
 * @retval SDFIO_ERR_ARGUMENT `header` is null, or the value is neither.
 */
SDFIO_API sdfio_status sdfio_header_set_z_scale(sdfio_header *header,
                                                double z_scale);

/**
 * Returns the z-resolution: the quantization step of the instrument in metres,
 * or SDFIO_NO_RESOLUTION. It is independent of the z-scale, because data can
 * be rescaled after the measurement.
 *
 * @param[in] header The header.
 */
SDFIO_API double sdfio_header_z_resolution(const sdfio_header *header);

/**
 * Sets the z-resolution.
 *
 * @param[in,out] header The header.
 * @param z_resolution A finite number. A number that is not positive means
 * SDFIO_NO_RESOLUTION.
 * @retval SDFIO_ERR_ARGUMENT `header` is null, or the value is not finite.
 */
SDFIO_API sdfio_status sdfio_header_set_z_resolution(sdfio_header *header,
                                                     double z_resolution);

/**
 * Returns the original creation date and time, with a resolution of one
 * minute.
 *
 * A file that is read can have none (SDFIO_NO_TIME). ISO-2.0 files store UTC.
 * ISO-1.0 files store the local clock of the writer, without a time zone. The
 * library does not know the zone, so it keeps such a clock reading as it is,
 * as if it were UTC, unless the program sets an sdfio_time_zone.
 *
 * @param[in] header The header.
 */
SDFIO_API time_t sdfio_header_created(const sdfio_header *header);

/**
 * Sets the original creation date and time. A file that is written requires
 * one, so sdfio_create_file() and sdfio_create_stream() reject
 * SDFIO_NO_TIME.
 *
 * @param[in,out] header The header.
 * @param creation_date The date, or SDFIO_NO_TIME.
 * @retval SDFIO_ERR_ARGUMENT `header` is null, or the year is out of the range
 * of the SDF files.
 */
SDFIO_API sdfio_status sdfio_header_set_created(sdfio_header *header,
                                                time_t creation_date);

/**
 * Returns the last modification date and time. The rules of the creation date
 * apply.
 *
 * @param[in] header The header.
 */
SDFIO_API time_t sdfio_header_modified(const sdfio_header *header);

/**
 * Sets the last modification date and time. The rules of the creation date
 * apply.
 *
 * @param[in,out] header The header.
 * @param modification_date The date, or SDFIO_NO_TIME.
 * @retval SDFIO_ERR_ARGUMENT `header` is null, or the year is out of the range
 * of the SDF files.
 */
SDFIO_API sdfio_status sdfio_header_set_modified(sdfio_header *header,
                                                 time_t modification_date);

/**
 * Returns the manufacturer's identifier: 7-bit ASCII, at most
 * SDFIO_MANUFACTURER_ID_MAX characters, terminated by a null character. The
 * pointer is valid as long as the header.
 *
 * @param[in] header The header.
 */
SDFIO_API const char *sdfio_header_manufacturer(const sdfio_header *header);

/**
 * Sets the manufacturer's identifier.
 *
 * @param[in,out] header The header.
 * @param[in] manufacturer_id The identifier, terminated by a null character.
 * @retval SDFIO_ERR_ARGUMENT A pointer is null, the identifier is longer than
 * SDFIO_MANUFACTURER_ID_MAX characters, or it has a character that is not a
 * printable 7-bit ASCII character.
 */
SDFIO_API sdfio_status sdfio_header_set_manufacturer(
    sdfio_header *header, const char *manufacturer_id);

#ifdef __cplusplus
}
#endif

#endif /* SDFIO_HEADER_H */
