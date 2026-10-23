/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file read_header.h
 * @brief Selection of the codec and reading of the header.
 */

#ifndef SDFIO_SRC_READ_HEADER_H
#define SDFIO_SRC_READ_HEADER_H

#include "message.h"
#include "reader_codec.h"

/**
 * Finds the codec of the file at the start of the source by its magic. The
 * source stays at the start.
 *
 * @param source The source.
 * @param codecs The listed reader codecs.
 * @param[out] codec Receives the codec.
 * @param[out] message Receives the reason of a failure.
 * @retval SDFIO_ERR_FORMAT The magic is not one of an SDF file.
 * @retval SDFIO_ERR_UNSUPPORTED The codec of the file is not listed.
 * @retval SDFIO_ERR_IO The stream failed.
 */
sdfio_status sdfio_select_reader_codec(sdfio_source *source,
                                       const sdfio_reader_codec *const *codecs,
                                       const sdfio_reader_codec **codec,
                                       sdfio_message *message);

/**
 * Reads the header at the start of the source with a codec, and checks what
 * every representation has in common.
 *
 * @param source The source.
 * @param codec The codec that sdfio_select_reader_codec() found.
 * @param[out] header Receives the header.
 * @param[out] state The room of `state_size` bytes for the codec, zeroed.
 * @param zone The time zone of the caller. May be null.
 * @param[out] message Receives the reason of a failure.
 * @retval SDFIO_ERR_FORMAT The header is damaged.
 * @retval SDFIO_ERR_IO The stream failed.
 */
sdfio_status sdfio_read_header(sdfio_source *source,
                               const sdfio_reader_codec *codec,
                               sdfio_header *header, void *state,
                               const sdfio_time_zone *zone,
                               sdfio_message *message);

#endif /* SDFIO_SRC_READ_HEADER_H */
