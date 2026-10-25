/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * Reading and writing of ISO 25178-71 SDF surface data files.
 *
 * @par Standard
 * The library follows ISO 25178-71:2026. A reader accepts these deviations,
 * some of which occur in files of commercial applications:
 * - Numbers in an ASCII file can be in any decimal or scientific form, where
 *   the standard has the normalized form with a signed exponent.
 * - A manufacturer identifier in a binary file ends at its first 0 byte, where
 *   the standard fills it with spaces.
 * - A date of the character 0 only, or of the byte 0 only, reads as
 *   SDFIO_NO_TIME. In an ASCII file such a date can have fewer than 12
 *   characters.
 * - An ASCII file can end its lines with LF instead of CRLF, can start with the
 *   UTF-8 byte order mark, and can lack the final line `*` after the trailer.
 * - A manufacturer identifier of an ASCII file can have more than 10
 *   characters. The reader keeps the first 10.
 * - A field of an ASCII header that the standard does not define is ignored,
 *   and a field that appears more than once counts with its last value.
 *
 * The library does not enforce the 7-bit ASCII encoding of the manufacturer's
 * identifier and of the trailer, and the tagged format of the trailer of an
 * ISO-2.0 file.
 */

#ifndef SDFIO_SDFIO_H
#define SDFIO_SDFIO_H

#include <sdfio/header.h>
#include <sdfio/heights.h>
#include <sdfio/layout.h>
#include <sdfio/reader.h>
#include <sdfio/sizes.h>
#include <sdfio/trailer.h>
#include <sdfio/types.h>
#include <sdfio/version.h>
#include <sdfio/writer.h>

#endif /* SDFIO_SDFIO_H */
