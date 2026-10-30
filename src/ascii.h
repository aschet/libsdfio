/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief Names and characters of the ASCII format.
 */

#ifndef SDFIO_SRC_ASCII_H
#define SDFIO_SRC_ASCII_H

/** Line terminator of the ASCII format: a carriage return followed by a
 * line feed. */
#define SDFIO_ASCII_LINE_END "\r\n"
/** Character of the line that terminates a record of the ASCII
 * format. */
#define SDFIO_ASCII_TERMINATOR '*'
/** String that labels a non-measured or spurious point in the data area of the
 * ASCII format. */
#define SDFIO_ASCII_BAD "BAD"
/** Separator of the name and the value of a field in the ASCII format. */
#define SDFIO_ASCII_ASSIGNMENT " = "
/** The room for the text of one value in the data area of the ASCII format,
 * with its separator. */
#define SDFIO_TEXT_PER_VALUE 32

/** ASCII field name of the manufacturer's ID. */
#define SDFIO_ASCII_MANUFACTURER_ID "ManufacID"
/** ASCII field name of the creation date and time. */
#define SDFIO_ASCII_CREATE_DATE "CreateDate"
/** ASCII field name of the last modification date and time. */
#define SDFIO_ASCII_MODIFICATION_DATE "ModDate"
/** ASCII field name of the number of columns in the data matrix. The name
 * originates from version 1.0 of the SDF format, in which the field held the
 * number of points per profile. */
#define SDFIO_ASCII_POINTS "NumPoints"
/** ASCII field name of the number of rows in the data matrix. The name
 * originates from version 1.0 of the SDF format, in which the field held the
 * number of profiles. */
#define SDFIO_ASCII_PROFILES "NumProfiles"
/** ASCII field name of the X-scale factor. */
#define SDFIO_ASCII_X_SCALE "Xscale"
/** ASCII field name of the Y-scale factor. */
#define SDFIO_ASCII_Y_SCALE "Yscale"
/** ASCII field name of the Z-scale factor. */
#define SDFIO_ASCII_Z_SCALE "Zscale"
/** ASCII field name of the Z-resolution. */
#define SDFIO_ASCII_Z_RESOLUTION "Zresolution"
/** ASCII field name of the compression type. */
#define SDFIO_ASCII_COMPRESSION "Compression"
/** ASCII field name of the data type. */
#define SDFIO_ASCII_DATA_TYPE "DataType"
/** ASCII field name of the checksum type. */
#define SDFIO_ASCII_CHECK_TYPE "CheckType"

#endif /* SDFIO_SRC_ASCII_H */
