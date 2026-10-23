/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file binary.h
 * @brief The binary representation: the fixed header.
 */

#ifndef SDFIO_SRC_BINARY_H
#define SDFIO_SRC_BINARY_H

#include "common.h"

/**
 * Finds the revision of a magic.
 *
 * @param magic The magic, `SDFIO_MAGIC_SIZE` bytes of a binary file.
 * @param[out] revision Receives the revision.
 * @param[out] message Receives the reason of a failure.
 * @retval SDFIO_ERR_FORMAT The magic is unknown, or that of an ASCII file.
 */
sdfio_status sdfio_binary_revision(const unsigned char *magic,
                                   sdfio_revision *revision,
                                   sdfio_message *message);

/**
 * Parses the header of a binary file.
 *
 * @param bytes The header, as long as sdfio_binary_header_size() says.
 * @param[out] header Receives the header.
 * @param zone The time zone of the caller. May be null.
 * @param[out] message Receives the reason of a failure.
 * @retval SDFIO_ERR_FORMAT The header is damaged.
 */
sdfio_status sdfio_binary_parse(const unsigned char *bytes,
                                sdfio_header *header,
                                const sdfio_time_zone *zone,
                                sdfio_message *message);

/**
 * Writes the header of a binary file.
 *
 * @param header The header.
 * @param zone The time zone of the caller. May be null.
 * @param[out] out Receives the bytes, `SDFIO_BINARY_HEADER_MAX` at most.
 * @param[out] size Receives the number of bytes.
 * @param[out] message Receives the reason of a failure.
 * @retval SDFIO_ERR_RANGE A date cannot be written.
 */
sdfio_status sdfio_binary_header(const sdfio_header *header,
                                 const sdfio_time_zone *zone,
                                 unsigned char *out, size_t *size,
                                 sdfio_message *message);

#endif /* SDFIO_SRC_BINARY_H */
