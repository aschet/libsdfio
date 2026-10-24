/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file copy_trailer.c
 * @brief Copy of the trailer from a reader to a writer, in parts.
 */

#include "reader_state.h"
#include "writer_state.h"

enum
{
    /* The size of the parts in which the trailer is copied. */
    COPY_PART = 256
};

sdfio_status sdfio_copy_trailer(sdfio_reader *reader, sdfio_writer *writer)
{
    if (reader == NULL || writer == NULL)
    {
        return SDFIO_ERR_ARGUMENT;
    }
    /* The writer refuses here, before the reader is read, if it cannot take a
     * trailer. */
    sdfio_status status = sdfio_write_trailer_text(writer, NULL, 0);
    if (status != SDFIO_OK)
    {
        return status;
    }
    char part[COPY_PART];
    size_t size = 0;
    for (;;)
    {
        status = sdfio_read_trailer(reader, part, sizeof part, &size);
        if (status != SDFIO_OK)
        {
            return sdfio_fail(&writer->message, status,
                              sdfio_reader_error_message(reader));
        }
        if (size == 0)
        {
            return SDFIO_OK;
        }
        status = sdfio_write_trailer_text(writer, part, size);
        if (status != SDFIO_OK)
        {
            return status;
        }
    }
}
