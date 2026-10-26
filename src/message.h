/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief Text of the last failure of a handle.
 */

#ifndef SDFIO_SRC_MESSAGE_H
#define SDFIO_SRC_MESSAGE_H

#include <sdfio/sdfio.h>

/**
 * Records the text of a failure and returns its code. The text is a string of
 * the library, which stays valid, so a handle holds no memory for it.
 *
 * @param[out] message Receives the text. May be null.
 * @param code The status to return.
 * @param text The text, a string that stays valid.
 * @return `code`.
 */
static inline sdfio_status sdfio_fail(const char **message, sdfio_status code,
                                      const char *text)
{
    if (message != NULL)
    {
        *message = text;
    }
    return code;
}

#endif /* SDFIO_SRC_MESSAGE_H */
