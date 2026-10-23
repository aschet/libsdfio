/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* Writes a file from a header and heights that the input makes up, with
 * numbers of every kind. A file that the writer wrote has to be read. */

#include <sdfio/sdfio.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    /* The bytes that describe the header, before the heights. */
    DESCRIPTION_SIZE = 4 + 4 * sizeof(double),
    DIMENSION_LIMIT = 24,
    REVISIONS = 3,
    TYPES = 8
};

typedef struct sink
{
    unsigned char *data;
    size_t size;
} sink;

static size_t sink_write(void *user, const void *bytes, size_t count)
{
    sink *output = user;
    unsigned char *grown = realloc(output->data, output->size + count);
    if (grown == NULL)
    {
        return 0;
    }
    memcpy(grown + output->size, bytes, count);
    output->data = grown;
    output->size += count;
    return count;
}

static double double_from(const uint8_t *bytes)
{
    double value = 0.0;
    memcpy(&value, bytes, sizeof value);
    return value;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < DESCRIPTION_SIZE)
    {
        return 0;
    }
    sdfio_header header;
    sdfio_header_init(&header);
    header.revision = (sdfio_revision)(data[0] % REVISIONS);
    header.representation = (data[1] & 1) != 0 ? SDFIO_ASCII : SDFIO_BINARY;
    header.data_type = (sdfio_data_type)(data[2] % TYPES);
    header.columns = 1 + (size_t)(data[3] % DIMENSION_LIMIT);
    header.rows = 1 + (size_t)((data[3] / DIMENSION_LIMIT) % DIMENSION_LIMIT);
    header.x_scale = double_from(data + 4);
    header.y_scale = double_from(data + 4 + sizeof(double));
    header.z_scale = double_from(data + 4 + 2 * sizeof(double));
    header.z_resolution = double_from(data + 4 + 3 * sizeof(double));
    if ((data[1] & 2) != 0)
    {
        header.z_scale = SDFIO_AUTO_Z_SCALE;
    }

    const size_t count = header.columns * header.rows;
    float *heights = calloc(count, sizeof *heights);
    if (heights == NULL)
    {
        return 0;
    }
    const uint8_t *rest = data + DESCRIPTION_SIZE;
    const size_t rest_size = size - DESCRIPTION_SIZE;
    for (size_t i = 0; i < count && (i + 1) * sizeof(float) <= rest_size; ++i)
    {
        memcpy(&heights[i], rest + i * sizeof(float), sizeof(float));
    }

    const sdfio_config config = {.reader_codecs = {&sdfio_binary_reader_codec,
                                                   &sdfio_ascii_reader_codec},
                                 .writer_codecs = {&sdfio_binary_writer_codec,
                                                   &sdfio_ascii_writer_codec}};
    sink output = {NULL, 0};
    sdfio_writer *writer = sdfio_writer_new(&config);
    sdfio_status status = SDFIO_ERR_MEMORY;
    if (writer != NULL)
    {
        const sdfio_stream stream = {&output, NULL, sink_write};
        status = sdfio_create_stream(writer, &stream, &header);
        if (status == SDFIO_OK)
        {
            status = sdfio_write_heights(writer, SDFIO_TYPE_FLOAT, heights);
        }
        if (status == SDFIO_OK)
        {
            status = sdfio_write_trailer_text(writer, "field = value\n", 14);
        }
        if (status == SDFIO_OK)
        {
            status = sdfio_end_file(writer);
        }
    }
    sdfio_writer_free(writer);

    if (status == SDFIO_OK)
    {
        /* A file that was written is a file that can be read: the rows as
         * they are coded, and the trailer. Heights in metres may overflow the
         * sample type, which is not a fault of the file. */
        sdfio_reader *reader = sdfio_reader_new(&config);
        sdfio_status read_status = SDFIO_ERR_MEMORY;
        if (reader != NULL)
        {
            read_status = sdfio_open_memory(reader, output.data, output.size);
            const sdfio_header *read = sdfio_get_header(reader);
            if (read_status == SDFIO_OK
                && (read->columns != header.columns
                    || read->rows != header.rows))
            {
                abort();
            }
            size_t coded_size = 0;
            void *coded = NULL;
            if (read_status == SDFIO_OK
                && sdfio_data_size(read, &coded_size) == SDFIO_OK)
            {
                coded = calloc(coded_size + 1, 1);
            }
            if (coded != NULL)
            {
                read_status = sdfio_read_data(reader, coded, read->rows);
            }
            const char *trailer = NULL;
            size_t trailer_size = 0;
            if (coded != NULL && read_status == SDFIO_OK)
            {
                read_status =
                    sdfio_read_trailer(reader, &trailer, &trailer_size);
            }
            free(coded);
        }
        sdfio_reader_free(reader);
        if (read_status != SDFIO_OK && read_status != SDFIO_ERR_MEMORY)
        {
            fprintf(stderr,
                    "written file not read: %s (revision %d, %s, type %d, "
                    "%zu x %zu, z_scale %g)\n",
                    sdfio_status_string(read_status), (int)header.revision,
                    header.representation == SDFIO_ASCII ? "text" : "binary",
                    (int)header.data_type, header.columns, header.rows,
                    header.z_scale);
            abort();
        }
    }
    free(output.data);
    free(heights);
    return 0;
}
