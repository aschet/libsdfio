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
    sdf_info info;
    sdf_info_init(&info);
    info.data_type = SDF_DATA_INT16;
    info.columns = 5;
    info.rows = 3;

    float *heights = sdf_alloc_heights(&info, SDF_FLOAT32, info.rows);
    CHECK(heights != NULL);
    if (heights != NULL)
    {
        for (size_t i = 0; i < 15; ++i)
        {
            CHECK(heights[i] == 0.0F);
        }
        heights[14] = 1.0F;
    }
    sdf_free(heights);
    int16_t *stored = sdf_alloc_stored(&info, 1);
    CHECK(stored != NULL);
    sdf_free(stored);
    sdf_free(NULL);

    info.columns = 0;
    void *empty = sdf_alloc_stored(&info, 4);
    CHECK(empty != NULL);
    sdf_free(empty);

    info.columns = SIZE_MAX / 2;
    CHECK(sdf_alloc_heights(&info, SDF_FLOAT64, 3) == NULL);
    info.columns = SIZE_MAX / 4;
    CHECK(sdf_alloc_heights(&info, SDF_FLOAT64, 2) == NULL);
    CHECK(sdf_alloc_heights(&info, (sdf_sample)99, 1) == NULL);
    CHECK(sdf_alloc_stored(NULL, 1) == NULL);
}

int main(void)
{
    check_memory();
    CHECK(strcmp(sdf_version(), SDF_VERSION_STRING) == 0);

    sdf_info info;
    sdf_info_init(&info);
    info.columns = 3;
    info.rows = 2;
    info.data_type = SDF_DATA_INT16;
    const float heights[6] = {1e-6f, 2e-6f, NAN, 4e-6f, 5e-6f, 6e-6f};

    membuf file = {0};
    const sdf_stream stream = membuf_stream(&file);
    sdf_writer *writer = sdf_writer_new();
    CHECK_OK(sdf_create_stream(writer, &stream, &info));
    CHECK_OK(sdf_write_heights(writer, sdf_metres(SDF_FLOAT32), heights,
                               3 * sizeof(float)));
    CHECK_OK(sdf_write_end(writer, "Note = public", 13));
    CHECK(strcmp(sdf_writer_message(writer), "") == 0);
    sdf_writer_free(writer);

    sdf_reader *reader = sdf_reader_new();
    CHECK_OK(sdf_open_memory(reader, file.data, file.size));
    const sdf_info *read = sdf_get_info(reader);
    CHECK(read != NULL && read->columns == 3 && read->rows == 2);
    if (read == NULL)
    {
        return finish_tests();
    }
    float back[6];
    CHECK_OK(sdf_read_heights(reader, sdf_metres(SDF_FLOAT32), back,
                              3 * sizeof(float)));
    CHECK(isnan(back[2]) && fabsf(back[5] - 6e-6f) < 1e-9f);
    const char *trailer = NULL;
    size_t size = 0;
    CHECK_OK(sdf_read_trailer(reader, &trailer, &size));
    const char *value = NULL;
    size_t value_size = 0;
    CHECK(sdf_trailer_find(trailer, size, "Note", &value, &value_size));
    CHECK_OK(sdf_rewind(reader));
    int16_t counts[6];
    CHECK_OK(sdf_read_rows(reader, counts, 2));
    CHECK(sdf_is_sentinel(read->data_type, read->revision, counts[2]));
    CHECK_STATUS(SDF_OK, sdf_decode_heights(read, sdf_metres(SDF_FLOAT32),
                                            counts, 6, back));
    CHECK(sdf_row_y(read, 0) == 0 && !sdf_rows_reversed(read->revision));
    sdf_reader_free(reader);
    free(file.data);
    return finish_tests();
}
