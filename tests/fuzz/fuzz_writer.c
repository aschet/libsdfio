/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* Writes a file from a header and heights that the input makes up, with
 * numbers of every kind. A file that the writer wrote has to be read. */

#include "../options.h"
#include <sdfio/sdfio.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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
    sdfio_header *header = NULL;
    if (sdfio_header_create(sdfio_default_allocator(), &header) != SDFIO_OK)
    {
        return 0;
    }
    (void)sdfio_header_set_dates(header, time(NULL));
    /* A setter refuses a value that is not valid, and the header keeps the
     * default: the input makes valid and invalid values alike. */
    const uint32_t columns = 1 + (uint32_t)(data[3] % DIMENSION_LIMIT);
    const uint32_t rows =
        1 + (uint32_t)((data[3] / DIMENSION_LIMIT) % DIMENSION_LIMIT);
    (void)sdfio_header_set_revision(header,
                                    (sdfio_revision)(data[0] % REVISIONS));
    (void)sdfio_header_set_format(header, (data[1] & 1) != 0 ? SDFIO_ASCII
                                                             : SDFIO_BINARY);
    (void)sdfio_header_set_data_type(header,
                                     (sdfio_data_type)(data[2] % TYPES));
    (void)sdfio_header_set_columns(header, columns);
    (void)sdfio_header_set_rows(header, rows);
    (void)sdfio_header_set_x_scale(header, double_from(data + 4));
    (void)sdfio_header_set_y_scale(header,
                                   double_from(data + 4 + sizeof(double)));
    (void)sdfio_header_set_z_scale(
        header, (data[1] & 2) != 0
                    ? SDFIO_AUTO_Z_SCALE
                    : double_from(data + 4 + 2 * sizeof(double)));
    (void)sdfio_header_set_z_resolution(
        header, double_from(data + 4 + 3 * sizeof(double)));

    const size_t count = (size_t)columns * rows;
    float *heights = calloc(count, sizeof *heights);
    if (heights == NULL)
    {
        sdfio_header_free(header);
        return 0;
    }
    const uint8_t *rest = data + DESCRIPTION_SIZE;
    const size_t rest_size = size - DESCRIPTION_SIZE;
    for (size_t i = 0; i < count && (i + 1) * sizeof(float) <= rest_size; ++i)
    {
        memcpy(&heights[i], rest + i * sizeof(float), sizeof(float));
    }

    const test_options config = {.reader_formats = {TEST_BINARY, TEST_ASCII},
                                 .writer_formats = {TEST_BINARY, TEST_ASCII}};
    sink output = {NULL, 0};
    sdfio_writer *writer = NULL;
    sdfio_status status = create_writer(&config, &writer);
    if (status == SDFIO_OK)
    {
        const sdfio_stream stream = {&output, NULL, sink_write};
        status = sdfio_create_stream(writer, &stream, header);
        if (status == SDFIO_OK)
        {
            status = sdfio_write_heights(
                writer, sdfio_layout_packed(SDFIO_TYPE_FLOAT), heights);
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
         * element type, which is not a fault of the file. */
        sdfio_reader *reader = NULL;
        sdfio_status read_status = create_reader(&config, &reader);
        if (read_status == SDFIO_OK)
        {
            read_status = sdfio_open_memory(reader, output.data, output.size);
            const sdfio_header *read = sdfio_reader_header(reader);
            if (read_status == SDFIO_OK
                && (sdfio_header_columns(read) != columns
                    || sdfio_header_rows(read) != rows))
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
                read_status = sdfio_read_data(
                    reader, sdfio_layout_packed(sdfio_header_data_type(read)),
                    coded, sdfio_header_rows(read));
            }
            char part[256];
            size_t got = 1;
            while (coded != NULL && read_status == SDFIO_OK && got > 0)
            {
                read_status =
                    sdfio_read_trailer(reader, part, sizeof part, &got);
            }
            free(coded);
        }
        sdfio_reader_free(reader);
        if (read_status != SDFIO_OK && read_status != SDFIO_ERR_MEMORY)
        {
            fprintf(stderr,
                    "written file not read: %s (revision %d, %s, type %d, "
                    "%lu x %lu, z_scale %g)\n",
                    sdfio_status_string(read_status),
                    (int)sdfio_header_revision(header),
                    sdfio_header_format(header) == SDFIO_ASCII ? "text"
                                                               : "binary",
                    (int)sdfio_header_data_type(header), (unsigned long)columns,
                    (unsigned long)rows, sdfio_header_z_scale(header));
            abort();
        }
    }
    free(output.data);
    free(heights);
    sdfio_header_free(header);
    return 0;
}
