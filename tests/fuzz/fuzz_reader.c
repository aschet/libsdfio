/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* Reads any bytes as an SDF file, through memory and through a stream, with
 * handles of the usual and of a tiny configuration. The library must answer
 * with a status and never fail otherwise. */

#include "../options.h"
#include <sdfio/sdfio.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum
{
    /* The file is what follows the control byte. */
    CONTROL_SIZE = 1,
    CONTROL_STREAM = 1,
    CONTROL_SMALL = 2,
    CONTROL_TINY = 4,
    CONTROL_HEIGHTS = 8,
    /* What is read as a whole matrix, and as rows. */
    MATRIX_LIMIT = 1U << 16,
    ROW_LIMIT = 1U << 16
};

typedef struct source
{
    const uint8_t *data;
    size_t size;
    size_t position;
} source;

/* A stream returns fewer bytes than asked only at the end. */
static size_t source_read(void *user, void *out, size_t size)
{
    source *input = user;
    const size_t left = input->size - input->position;
    const size_t count = size < left ? size : left;
    if (count > 0)
    {
        memcpy(out, input->data + input->position, count);
    }
    input->position += count;
    return count;
}

/* The data area as rows in the data type of the file, or as a whole matrix of
 * heights, and then the trailer. */
static void read_everything(sdfio_reader *reader, bool as_heights)
{
    const sdfio_header *header = sdfio_reader_header(reader);
    if (header == NULL)
    {
        return;
    }
    if (as_heights)
    {
        if (sdfio_header_columns(header) <= ROW_LIMIT
            && sdfio_header_rows(header) <= ROW_LIMIT
            && (size_t)sdfio_header_columns(header) * sdfio_header_rows(header)
                   <= MATRIX_LIMIT)
        {
            size_t bytes = 0;
            float *heights = NULL;
            if (sdfio_heights_size(header, SDFIO_TYPE_FLOAT, &bytes)
                == SDFIO_OK)
            {
                heights = calloc(bytes + 1, 1);
            }
            if (heights != NULL)
            {
                (void)sdfio_read_heights(
                    reader, sdfio_layout_packed(SDFIO_TYPE_FLOAT), heights);
                free(heights);
            }
        }
    }
    else if (sdfio_header_columns(header) <= ROW_LIMIT)
    {
        size_t bytes = 0;
        void *row = NULL;
        if (sdfio_data_row_size(header, &bytes) == SDFIO_OK)
        {
            row = calloc(bytes + 1, 1);
        }
        if (row != NULL)
        {
            const sdfio_layout layout =
                sdfio_layout_packed(sdfio_header_data_type(header));
            for (size_t i = 0; i < sdfio_header_rows(header) && i < ROW_LIMIT;
                 ++i)
            {
                if (sdfio_read_data(reader, layout, row, 1) != SDFIO_OK)
                {
                    break;
                }
            }
            free(row);
        }
    }
    char part[256];
    size_t got = 1;
    while (got > 0
           && sdfio_read_trailer(reader, part, sizeof part, &got) == SDFIO_OK)
    {
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < CONTROL_SIZE)
    {
        return 0;
    }
    const uint8_t control = data[0];
    const test_options tiny = {.buffer_size = 1,
                               .chunk_values = 1,
                               .reader_formats = {TEST_BINARY, TEST_ASCII}};
    const test_options small = {.buffer_size = 100,
                                .chunk_values = 7,
                                .reader_formats = {TEST_BINARY, TEST_ASCII}};
    const test_options defaults = {.reader_formats = {TEST_BINARY, TEST_ASCII}};
    const test_options *config = (control & CONTROL_TINY)    ? &tiny
                                 : (control & CONTROL_SMALL) ? &small
                                                             : &defaults;
    source input = {data + CONTROL_SIZE, size - CONTROL_SIZE, 0};
    sdfio_reader *reader = NULL;
    if (create_reader(config, &reader) != SDFIO_OK)
    {
        return 0;
    }
    sdfio_status status = SDFIO_ERR_ARGUMENT;
    if (control & CONTROL_STREAM)
    {
        const sdfio_stream stream = {&input, source_read, NULL};
        status = sdfio_open_stream(reader, &stream);
    }
    else
    {
        status = sdfio_open_memory(reader, input.data, input.size);
    }
    if (status == SDFIO_OK)
    {
        read_everything(reader, (control & CONTROL_HEIGHTS) != 0);
    }
    sdfio_reader_free(reader);
    return 0;
}
