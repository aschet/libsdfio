/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file header.h
 * @brief Header reading and validation.
 */

#ifndef SDFIO_SRC_HEADER_H
#define SDFIO_SRC_HEADER_H

#include "codec.h"

/**
 * Reads the header at the start of the source with the codec of its magic.
 *
 * @param source The source.
 * @param codecs The listed reader codecs.
 * @param[out] codec Receives the codec that read the header.
 * @param[out] header Receives the header.
 * @param[out] state Receives what the codec keeps for the file.
 * @param zone The time zone of the caller. May be null.
 * @param[out] message Receives the reason of a failure.
 * @retval SDFIO_ERR_FORMAT The header is damaged.
 * @retval SDFIO_ERR_UNSUPPORTED The codec of the file is not listed.
 * @retval SDFIO_ERR_IO The stream failed.
 */
sdfio_status sdfio_read_header(sdfio_source *source,
                               const sdfio_reader_codec *const *codecs,
                               const sdfio_reader_codec **codec,
                               sdfio_header *header, sdfio_codec_state *state,
                               const sdfio_time_zone *zone,
                               sdfio_message *message);

/**
 * Checks what makes a file that cannot be written.
 *
 * @param header The header.
 * @param[out] message Receives the reason of a failure.
 * @retval SDFIO_ERR_ARGUMENT A field of the header is invalid.
 */
sdfio_status sdfio_validate(const sdfio_header *header, sdfio_message *message);

#endif /* SDFIO_SRC_HEADER_H */
