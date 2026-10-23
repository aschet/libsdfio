/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file ascii.h
 * @brief Spelling of the ASCII representation.
 */

#ifndef SDFIO_SRC_ASCII_H
#define SDFIO_SRC_ASCII_H

/** The end of a line, which a writer writes. */
#define SDFIO_ASCII_LINE_END "\r\n"
/** The character of a line that ends a record. */
#define SDFIO_ASCII_TERMINATOR '*'
/** The word of a point that is not measured. */
#define SDFIO_ASCII_BAD "BAD"

/** The name of the field of the manufacturer identifier in the header. */
#define SDFIO_ASCII_MANUFACTURER_ID "ManufacID"
/** The name of the field of the creation date in the header. */
#define SDFIO_ASCII_CREATE_DATE "CreateDate"
/** The name of the field of the modification date in the header. */
#define SDFIO_ASCII_MODIFICATION_DATE "ModDate"
/** The name of the field of the number of points of a profile, the columns in
 * the header. */
#define SDFIO_ASCII_POINTS "NumPoints"
/** The name of the field of the number of profiles, the rows in the header. */
#define SDFIO_ASCII_PROFILES "NumProfiles"
/** The name of the field of the scale of x in the header. */
#define SDFIO_ASCII_X_SCALE "Xscale"
/** The name of the field of the scale of y in the header. */
#define SDFIO_ASCII_Y_SCALE "Yscale"
/** The name of the field of the scale of z in the header. */
#define SDFIO_ASCII_Z_SCALE "Zscale"
/** The name of the field of the resolution of z in the header. */
#define SDFIO_ASCII_Z_RESOLUTION "Zresolution"
/** The name of the field of the compression type in the header. */
#define SDFIO_ASCII_COMPRESSION "Compression"
/** The name of the field of the data type in the header. */
#define SDFIO_ASCII_DATA_TYPE "DataType"
/** The name of the field of the checksum type in the header. */
#define SDFIO_ASCII_CHECK_TYPE "CheckType"

#endif /* SDFIO_SRC_ASCII_H */
