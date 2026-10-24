/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file read_header.h
 * @brief Selection of the format and reading of the header.
 */

#ifndef SDFIO_SRC_READ_HEADER_H
#define SDFIO_SRC_READ_HEADER_H

#include "message.h"
#include "reader_format.h"

/**
 * Selects the reader format of a file from the version number at the start of
 * the source. The version number is not consumed.
 *
 * @param source The source.
 * @param formats The added reader formats.
 * @param[out] format Receives the format.
 * @param[out] message Receives the reason of a failure.
 * @retval SDFIO_ERR_FORMAT The version number is not one of an SDF file.
 * @retval SDFIO_ERR_UNSUPPORTED The format of the file has not been added.
 * @retval SDFIO_ERR_IO The stream failed.
 */
sdfio_status sdfio_select_reader_format(
    sdfio_source *source, const sdfio_reader_format *const *formats,
    const sdfio_reader_format **format, sdfio_message *message);

/**
 * Reads the header at the start of the source with the given format, and
 * verifies the properties that are independent of the format.
 *
 * @param source The source.
 * @param format The format that sdfio_select_reader_format() found.
 * @param[out] header Receives the header.
 * @param[out] state The room of `state_size` bytes for the format, zeroed.
 * @param zone The time zone of the caller. May be null.
 * @param[out] message Receives the reason of a failure.
 * @retval SDFIO_ERR_FORMAT The header is damaged.
 * @retval SDFIO_ERR_IO The stream failed.
 */
sdfio_status sdfio_read_header(sdfio_source *source,
                               const sdfio_reader_format *format,
                               sdfio_header *header, void *state,
                               const sdfio_time_zone *zone,
                               sdfio_message *message);

#endif /* SDFIO_SRC_READ_HEADER_H */
