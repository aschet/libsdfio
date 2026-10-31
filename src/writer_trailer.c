/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief Writing of the trailer and the end of the file.
 */

#include "crlf.h"
#include "message.h"
#include "tagged.h"
#include "writer_state.h"
#include <string.h>

/* Every row of the data area has to be written before the trailer. */
static sdfio_status require_complete(sdfio_writer *writer)
{
    if (writer->next_row != writer->header.rows)
    {
        return sdfio_fail(&writer->message, SDFIO_ERR_STATE,
                          "Not all rows of the data area have been written");
    }
    return SDFIO_OK;
}

/* The header is out, and the format has opened the record of the trailer. */
static sdfio_status open_trailer(sdfio_writer *writer)
{
    sdfio_status status = sdfio_writer_start_data(writer);
    if (status != SDFIO_OK)
    {
        return status;
    }
    if (!writer->trailer_open)
    {
        status = writer->format->begin_trailer(&writer->sink, &writer->message);
        if (status != SDFIO_OK)
        {
            return sdfio_writer_fail(writer, status);
        }
        writer->trailer_open = true;
    }
    return SDFIO_OK;
}

sdfio_status sdfio_write_trailer_field(sdfio_writer *writer, const char *name,
                                       const char *value)
{
    static const char separator[] = " = ";
    static const char line_end[] = "\r\n";
    sdfio_status status = sdfio_writer_require_writable(writer);
    if (status == SDFIO_OK)
    {
        status = require_complete(writer);
    }
    if (status != SDFIO_OK)
    {
        return status;
    }
    if (name == NULL || value == NULL || !sdfio_tagged_name_valid(name)
        || !sdfio_tagged_value_valid(value))
    {
        return sdfio_fail(&writer->message, SDFIO_ERR_ARGUMENT,
                          "A field name consists of letters, digits and _, and "
                          "a value is one line");
    }
    status = open_trailer(writer);
    if (status != SDFIO_OK)
    {
        return status;
    }
    sdfio_sink *sink = &writer->sink;
    if (!sdfio_crlf_end(sink, &writer->trailer_text)
        || !sdfio_sink_write(sink, name, strlen(name))
        || !sdfio_sink_write(sink, separator, sizeof separator - 1)
        || !sdfio_sink_write(sink, value, strlen(value))
        || !sdfio_sink_write(sink, line_end, sizeof line_end - 1))
    {
        return sdfio_writer_sink_failure(writer);
    }
    return SDFIO_OK;
}

sdfio_status sdfio_write_trailer_text(sdfio_writer *writer, const char *text,
                                      size_t size)
{
    sdfio_status status = sdfio_writer_require_writable(writer);
    if (status == SDFIO_OK)
    {
        status = require_complete(writer);
    }
    if (status != SDFIO_OK)
    {
        return status;
    }
    if (text == NULL && size > 0)
    {
        return sdfio_fail(&writer->message, SDFIO_ERR_ARGUMENT,
                          "The text of the trailer is null although its size "
                          "is greater than 0");
    }
    status = open_trailer(writer);
    if (status != SDFIO_OK)
    {
        return status;
    }
    if (!sdfio_crlf_write(&writer->sink, &writer->trailer_text, text, size))
    {
        return sdfio_writer_sink_failure(writer);
    }
    return SDFIO_OK;
}

sdfio_status sdfio_end_file(sdfio_writer *writer)
{
    sdfio_status status = sdfio_writer_require_writable(writer);
    if (status == SDFIO_OK)
    {
        status = require_complete(writer);
    }
    if (status == SDFIO_OK)
    {
        status = open_trailer(writer);
    }
    if (status != SDFIO_OK)
    {
        return status;
    }
    if (!(writer->format->trailer_ends_line
              ? sdfio_crlf_end(&writer->sink, &writer->trailer_text)
              : sdfio_crlf_flush(&writer->sink, &writer->trailer_text)))
    {
        return sdfio_writer_sink_failure(writer);
    }
    status = writer->format->end_trailer(&writer->sink, &writer->message);
    if (status != SDFIO_OK)
    {
        return sdfio_writer_fail(writer, status);
    }
    if (!sdfio_sink_flush(&writer->sink))
    {
        return sdfio_writer_sink_failure(writer);
    }
    status = SDFIO_OK;
    if (writer->owner.finish != NULL)
    {
        status = writer->owner.finish(writer->owner.user, &writer->message);
        /* The file is ended, whatever the result. */
        writer->owner.close = NULL;
    }
    if (status != SDFIO_OK)
    {
        return sdfio_writer_fail(writer, status);
    }
    writer->state = SDFIO_WRITER_ENDED;
    return SDFIO_OK;
}
