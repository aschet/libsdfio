/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* Reads any bytes as an SDF file, through memory and through a stream that
 * cannot seek, with handles of the usual and of a tiny configuration. The
 * library must answer with a status and never fail otherwise. */

#include <sdfio/sdfio.h>

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

static void read_everything(sdfio_reader *reader)
{
    const sdfio_header *header = sdfio_get_header(reader);
    if (header == NULL)
    {
        return;
    }
    const sdfio_format format = sdfio_metres(SDFIO_FLOAT32);
    if (header->columns <= ROW_LIMIT)
    {
        /* Row by row, in the data type of the file. */
        void *row = sdfio_alloc_data(header, 1);
        if (row != NULL)
        {
            for (size_t i = 0; i < header->rows && i < ROW_LIMIT; ++i)
            {
                if (sdfio_read_data(reader, row, 1) != SDFIO_OK)
                {
                    break;
                }
            }
            sdfio_free(row);
        }
    }
    const char *trailer = NULL;
    size_t trailer_size = 0;
    (void)sdfio_read_trailer(reader, &trailer, &trailer_size);

    if (header->columns <= ROW_LIMIT && header->rows <= ROW_LIMIT
        && header->columns * header->rows <= MATRIX_LIMIT
        && sdfio_rewind(reader) == SDFIO_OK)
    {
        float *heights =
            sdfio_alloc_heights(header, SDFIO_FLOAT32, header->rows);
        if (heights != NULL)
        {
            (void)sdfio_read_heights(
                reader, format, heights,
                (ptrdiff_t)(header->columns * sizeof *heights));
            sdfio_free(heights);
        }
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
    const sdfio_config tiny = {NULL, 1, 1};
    const sdfio_config small = {NULL, 100, 7};
    const sdfio_config *config = (control & CONTROL_TINY)    ? &tiny
                                 : (control & CONTROL_SMALL) ? &small
                                                             : NULL;
    source input = {data + CONTROL_SIZE, size - CONTROL_SIZE, 0};
    sdfio_reader *reader = sdfio_reader_new(config);
    if (reader == NULL)
    {
        return 0;
    }
    sdfio_status status = SDFIO_ERR_ARGUMENT;
    if (control & CONTROL_STREAM)
    {
        const sdfio_stream stream = {&input, source_read, NULL, NULL};
        status = sdfio_open_stream(reader, &stream);
    }
    else
    {
        status = sdfio_open_memory(reader, input.data, input.size);
    }
    if (status == SDFIO_OK)
    {
        read_everything(reader);
    }
    sdfio_reader_free(reader);
    return 0;
}
