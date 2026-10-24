/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file owner.h
 * @brief What a file adds to a handle: finishing and closing.
 */

#ifndef SDFIO_SRC_OWNER_H
#define SDFIO_SRC_OWNER_H

#include "message.h"

/**
 * What a file adds to a reader or a writer, created by sdfio_open_file() and
 * sdfio_create_file().
 */
typedef struct sdfio_owner
{
    /** Ends the file of a writer, after which `close` is not called. Null for a
     * reader. */
    sdfio_status (*finish)(void *user, sdfio_message *message);
    /** Closes the file of a reader, or of a writer that was not ended. */
    void (*close)(void *user);
    /** The state that the functions get. */
    void *user;
} sdfio_owner;

#endif /* SDFIO_SRC_OWNER_H */
