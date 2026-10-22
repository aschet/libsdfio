/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* Only what a user of the installed library can call: with a shared library
 * this fails to link if something is not exported. */

#include "check.h"

/* The size is that of the matrix, and one that cannot be counted is refused. */
static void check_memory(void)
{
    sdfio_header header;
    sdfio_header_init(&header);
    header.data_type = SDFIO_DATA_INT16;
    header.columns = 5;
    header.rows = 3;

    size_t bytes = 0;
    CHECK_OK(sdfio_heights_size(&header, SDFIO_FLOAT32, &bytes));
    CHECK(bytes == 15 * sizeof(float));
    CHECK_OK(sdfio_heights_row_size(&header, SDFIO_FLOAT64, &bytes));
    CHECK(bytes == 5 * sizeof(double));
    CHECK_OK(sdfio_data_size(&header, &bytes));
    CHECK(bytes == 15 * sizeof(int16_t));
    CHECK_OK(sdfio_data_row_size(&header, &bytes));
    CHECK(bytes == 5 * sizeof(int16_t));

    header.columns = 0;
    CHECK_OK(sdfio_data_size(&header, &bytes));
    CHECK(bytes == 0);

    header.columns = SIZE_MAX / 2;
    header.rows = 3;
    CHECK_STATUS(SDFIO_ERR_RANGE,
                 sdfio_heights_size(&header, SDFIO_FLOAT64, &bytes));
    header.columns = SIZE_MAX / 4;
    header.rows = 2;
    CHECK_STATUS(SDFIO_ERR_RANGE,
                 sdfio_heights_size(&header, SDFIO_FLOAT64, &bytes));
    header.data_type = SDFIO_DATA_INT32;
    CHECK_STATUS(SDFIO_ERR_RANGE, sdfio_data_size(&header, &bytes));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_heights_size(&header, (sdfio_sample)99, &bytes));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_data_size(NULL, &bytes));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_data_row_size(&header, NULL));
}

int main(void)
{
    check_memory();
    CHECK(strcmp(sdfio_version(), SDFIO_VERSION_STRING) == 0);

    sdfio_header header;
    sdfio_header_init(&header);
    header.columns = 3;
    header.rows = 2;
    header.data_type = SDFIO_DATA_INT16;
    const float heights[6] = {1e-6f, 2e-6f, NAN, 4e-6f, 5e-6f, 6e-6f};

    membuf file = {0};
    const sdfio_stream stream = membuf_stream(&file);
    sdfio_writer *writer = sdfio_writer_new(NULL);
    CHECK_OK(sdfio_create_stream(writer, &stream, &header));
    CHECK_OK(sdfio_write_heights(writer, SDFIO_FLOAT32, heights));
    char composed[128];
    sdfio_trailer assembled;
    sdfio_trailer_init(&assembled, composed, sizeof composed);
    CHECK_OK(sdfio_trailer_add_field(&assembled, "Note", "public"));
    CHECK_OK(sdfio_trailer_add_field(&assembled, "Averages", "16"));
    CHECK_OK(sdfio_write_trailer(writer, assembled.text, assembled.size));
    CHECK(strcmp(sdfio_writer_error_message(writer), "") == 0);
    sdfio_writer_free(writer);

    sdfio_reader *reader = sdfio_reader_new(NULL);
    CHECK_OK(sdfio_open_memory(reader, file.data, file.size));
    const sdfio_header *read = sdfio_get_header(reader);
    CHECK(read != NULL && read->columns == 3 && read->rows == 2);
    if (read == NULL)
    {
        return finish_tests();
    }
    float back[6];
    CHECK_OK(sdfio_read_heights(reader, SDFIO_FLOAT32, back));
    CHECK(isnan(back[2]) && fabsf(back[5] - 6e-6f) < 1e-9f);
    const char *trailer = NULL;
    size_t size = 0;
    CHECK_OK(sdfio_read_trailer(reader, &trailer, &size));
    CHECK(size == strlen("Note = public\nAverages = 16")
          && memcmp(trailer, "Note = public\nAverages = 16", size) == 0);
    /* A row stride of 0 means rows that follow each other. */
    reader = membuf_reopen(reader, &file);
    float packed[6];
    CHECK_OK(sdfio_read_heights(reader, SDFIO_FLOAT32, packed));
    for (size_t i = 0; i < 6; ++i)
    {
        CHECK(isnan(back[i]) ? isnan(packed[i]) : packed[i] == back[i]);
    }
    reader = membuf_reopen(reader, &file);
    read = sdfio_get_header(reader);
    int16_t counts[6];
    CHECK_OK(sdfio_read_data(reader, counts, 2));
    CHECK(sdfio_is_bad_value(read->data_type, counts[2]));
    CHECK_STATUS(SDFIO_OK,
                 sdfio_decode_heights(read, sdfio_format_packed(SDFIO_FLOAT32),
                                      counts, 6, back));
    sdfio_reader_free(reader);
    free(file.data);
    return finish_tests();
}
