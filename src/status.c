/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file status.c
 * @brief Texts of the status codes.
 */

#include <sdfio/sdfio.h>

const char *sdfio_status_string(sdfio_status status)
{
    switch (status)
    {
    case SDFIO_OK:
        return "no error";
    case SDFIO_ERR_ARGUMENT:
        return "invalid argument";
    case SDFIO_ERR_STATE:
        return "the call does not fit the state of the handle";
    case SDFIO_ERR_IO:
        return "input or output error";
    case SDFIO_ERR_FORMAT:
        return "not an SDF file, or a damaged one";
    case SDFIO_ERR_UNSUPPORTED:
        return "a feature of the file is not supported";
    case SDFIO_ERR_RANGE:
        return "a number does not fit its type";
    case SDFIO_ERR_BAD_VALUE:
        return "a height equals the bad value of its data type";
    case SDFIO_ERR_MEMORY:
        return "out of memory";
    }
    return "unknown status";
}
