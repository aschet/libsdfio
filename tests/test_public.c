/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* Only what a user of the installed library can call: with a shared library
 * this fails to link if something is not exported. */

#include "check.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* The size is that of the matrix, and one that cannot be counted is refused. */
static void check_memory(void)
{
    sdfio_header *header = new_header();
    CHECK_OK(sdfio_header_set_data_type(header, SDFIO_TYPE_INT16));
    CHECK_OK(sdfio_header_set_columns(header, 5));
    CHECK_OK(sdfio_header_set_rows(header, 3));

    size_t bytes = 0;
    CHECK_OK(sdfio_heights_size(header, SDFIO_TYPE_FLOAT, &bytes));
    CHECK(bytes == 15 * sizeof(float));
    CHECK_OK(sdfio_heights_row_size(header, SDFIO_TYPE_DOUBLE, &bytes));
    CHECK(bytes == 5 * sizeof(double));
    CHECK_OK(sdfio_data_size(header, &bytes));
    CHECK(bytes == 15 * sizeof(int16_t));
    CHECK_OK(sdfio_data_row_size(header, &bytes));
    CHECK(bytes == 5 * sizeof(int16_t));

    CHECK_OK(sdfio_header_set_columns(header, 0));
    CHECK_OK(sdfio_data_size(header, &bytes));
    CHECK(bytes == 0);

    CHECK_OK(sdfio_header_set_columns(header, UINT32_MAX));
    CHECK_OK(sdfio_header_set_rows(header, UINT32_MAX));
    CHECK_STATUS(SDFIO_ERR_RANGE,
                 sdfio_heights_size(header, SDFIO_TYPE_DOUBLE, &bytes));
    CHECK_OK(sdfio_header_set_data_type(header, SDFIO_TYPE_INT32));
    CHECK_STATUS(SDFIO_ERR_RANGE, sdfio_data_size(header, &bytes));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_heights_size(header, (sdfio_data_type)99, &bytes));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_data_size(NULL, &bytes));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_data_row_size(header, NULL));
}

/* The initializers set every field to zero, whatever the memory held before. */
static void check_init(void)
{
    sdfio_stream stream;
    memset(&stream, 0xA5, sizeof stream);
    sdfio_stream_init(&stream);
    CHECK(stream.user == NULL);
    CHECK(stream.read == NULL);
    CHECK(stream.write == NULL);

    sdfio_layout layout;
    memset(&layout, 0xA5, sizeof layout);
    sdfio_layout_init(&layout);
    CHECK(layout.type == SDFIO_TYPE_FLOAT);
    CHECK(isnan(layout.not_measured));
    CHECK(layout.element_stride == 0);
    CHECK(layout.row_stride == 0);

    sdfio_time_zone zone;
    memset(&zone, 0xA5, sizeof zone);
    sdfio_time_zone_init(&zone);
    CHECK(zone.user == NULL);
    CHECK(zone.local_to_utc == NULL);
    CHECK(zone.utc_to_local == NULL);
}

int main(void)
{
    check_memory();
    check_init();
    CHECK(strcmp(sdfio_version(), SDFIO_VERSION_STRING) == 0);

    sdfio_header *header = new_header();
    CHECK_OK(sdfio_header_set_columns(header, 3));
    CHECK_OK(sdfio_header_set_rows(header, 2));
    CHECK_OK(sdfio_header_set_data_type(header, SDFIO_TYPE_INT16));
    const float heights[6] = {1e-6f, 2e-6f, NAN, 4e-6f, 5e-6f, 6e-6f};

    membuf file = {0};
    const sdfio_stream stream = membuf_stream(&file);
    sdfio_writer *writer = new_writer(all_formats());
    CHECK_OK(sdfio_create_stream(writer, &stream, header));
    CHECK_OK(sdfio_write_heights(writer, sdfio_layout_packed(SDFIO_TYPE_FLOAT),
                                 heights));
    CHECK_OK(sdfio_write_trailer_field(writer, "Note", "public"));
    CHECK_OK(sdfio_write_trailer_field(writer, "Averages", "16"));
    CHECK_OK(sdfio_end_file(writer));
    CHECK(strcmp(sdfio_writer_error_message(writer), "") == 0);
    sdfio_writer_free(writer);

    sdfio_reader *reader = new_reader(all_formats());
    CHECK_OK(sdfio_open_memory(reader, file.data, file.size));
    const sdfio_header *read = sdfio_reader_header(reader);
    CHECK(read != NULL && sdfio_header_columns(read) == 3
          && sdfio_header_rows(read) == 2);
    if (read == NULL)
    {
        return finish_tests();
    }
    float back[6];
    CHECK_OK(sdfio_read_heights(reader, sdfio_layout_packed(SDFIO_TYPE_FLOAT),
                                back));
    CHECK(isnan(back[2]) && fabsf(back[5] - 6e-6f) < 1e-9f);
    const char *trailer = NULL;
    size_t size = 0;
    CHECK_OK(read_trailer_text(reader, 0, &trailer, &size));
    CHECK(size == strlen("Note = public\nAverages = 16\n")
          && memcmp(trailer, "Note = public\nAverages = 16\n", size) == 0);
    /* A row stride of 0 means rows that follow each other. */
    reader = membuf_reopen(reader, &file);
    float packed[6];
    CHECK_OK(sdfio_read_heights(reader, sdfio_layout_packed(SDFIO_TYPE_FLOAT),
                                packed));
    for (size_t i = 0; i < 6; ++i)
    {
        CHECK(isnan(back[i]) ? isnan(packed[i]) : packed[i] == back[i]);
    }
    reader = membuf_reopen(reader, &file);
    read = sdfio_reader_header(reader);
    int16_t coded[6];
    CHECK_OK(sdfio_read_data(reader, sdfio_layout_packed(SDFIO_TYPE_INT16),
                             coded, 2));
    CHECK(coded[2] == SDFIO_BAD_INT16);
    CHECK_STATUS(SDFIO_OK, sdfio_decode_heights(
                               read, sdfio_layout_packed(SDFIO_TYPE_FLOAT),
                               coded, 6, back));
    sdfio_reader_free(reader);
    free(file.data);
    return finish_tests();
}
