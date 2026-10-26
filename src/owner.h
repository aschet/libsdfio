/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief Ending and closing of the file that a handle owns.
 */

#ifndef SDFIO_SRC_OWNER_H
#define SDFIO_SRC_OWNER_H

#include "message.h"

/**
 * The functions that end and close the file that sdfio_open_file() or
 * sdfio_create_file() opened for a reader or a writer.
 */
typedef struct sdfio_owner
{
    /** Ends the file of a writer, after which `close` is not called. Null for a
     * reader. */
    sdfio_status (*finish)(void *user, const char **message);
    /** Closes the file of a reader, or of a writer that was not ended. */
    void (*close)(void *user);
    /** The state that the functions get. */
    void *user;
} sdfio_owner;

#endif /* SDFIO_SRC_OWNER_H */
