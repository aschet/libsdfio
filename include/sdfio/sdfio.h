/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * Reading and writing of ISO 25178-71 SDF surface data files.
 *
 * This description holds the rules of the interface: naming, headers, files,
 * versioning and the standard.
 *
 * @par Naming
 * Every identifier that the library provides begins with `sdfio_` (functions
 * and types) or `SDFIO_` (macros and enumerators). The headers reside in the
 * directory `sdfio/`. A function that belongs to an object begins with the
 * object: `sdfio_reader_`, `sdfio_writer_`, `sdfio_header_`. The functions that
 * move data in and out begin with the action: `sdfio_open_`, `sdfio_create_`,
 * `sdfio_read_`, `sdfio_write_`, `sdfio_copy_`, `sdfio_end_`. A function that
 * tells a property of a thing is `sdfio_<thing>_<property>`, for example
 * `sdfio_data_size()`.
 *
 * @par Headers
 * This header includes the others: `<sdfio/types.h>` (status, enumerations,
 * allocator, stream, time zone), `<sdfio/header.h>`, `<sdfio/heights.h>`
 * (sizes, layout, conversion), `<sdfio/reader.h>` and `<sdfio/writer.h>`. A
 * program can include the one that it needs.
 *
 * @par Files
 * sdfio_open_file() and sdfio_create_file() are declared in `<sdfio/file.h>`.
 * This header does not include `<stdio.h>`.
 *
 * @par Versioning
 * The handles and sdfio_header are opaque, so a new option or a new field of
 * the header becomes a new function. The structures that the caller fills are
 * plain structures: sdfio_layout, sdfio_stream, sdfio_allocator and
 * sdfio_time_zone. They are frozen for the major version, and a new property
 * becomes a new function. A structure crosses the interface by pointer, never
 * by value.
 *
 * @par Standard
 * The library follows ISO 25178-71:2026, and a reader checks what the standard
 * defines: the magic, the data types of the revision, a compression
 * and a checksum type of 0, the ranges of the numbers of columns and rows
 * and of the dates, a positive z-scale, a manufacturer identifier of at most 10
 * characters and the string `BAD`. It tolerates these deviations:
 * - Numbers in an ASCII file can be in any decimal or scientific form, where
 *   the standard has the normalized form with a signed exponent. Files of
 *   MountainsMap have such numbers.
 * - A manufacturer identifier in a binary file ends at its first 0 byte, where
 *   the standard fills it with spaces; the bytes after the 0 are ignored. Files
 *   of MountainsMap have such identifiers.
 * - A date of the character 0 only, or of the byte 0 only, is a date that is
 *   not recorded and reads as SDFIO_NO_TIME. In an ASCII file such a date can
 *   have fewer than 12 characters.
 * - An ASCII file can end its lines with LF instead of CRLF, and the final
 *   line `*` after the trailer can be missing.
 * - An ASCII file can start with the UTF-8 byte order mark that some editors
 *   add.
 * - A field of an ASCII header that the standard does not define is ignored,
 *   and a field that appears more than once counts with its last value.
 * - The trailer of an ISO-2.0 file need not be in the tagged format.
 */

#ifndef SDFIO_SDFIO_H
#define SDFIO_SDFIO_H

#include <sdfio/header.h>
#include <sdfio/heights.h>
#include <sdfio/reader.h>
#include <sdfio/types.h>
#include <sdfio/version.h>
#include <sdfio/writer.h>

#endif /* SDFIO_SDFIO_H */
