/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* What is written is what is read: every revision, representation and data
 * type, the order of the rows, the trailer and the dates. */

#include "check.h"

#include "internal.h"

#include <stdlib.h>

enum
{
    ROWS = 7,
    COLUMNS = 11
};

static const char tagged_trailer[] = "OperatorName = Tester\nPartName = Sample";

static double height(size_t row, size_t column)
{
    return 1e-6 * sin(0.7 * (double)row) * cos(0.4 * (double)column)
           + 2e-7 * (double)row - 3e-7;
}

static bool is_missing(size_t row, size_t column)
{
    return row == 2 || (column == 5 && row != 0);
}

static void make_heights(double *heights)
{
    for (size_t row = 0; row < ROWS; ++row)
    {
        for (size_t column = 0; column < COLUMNS; ++column)
        {
            heights[row * COLUMNS + column] =
                is_missing(row, column) ? NAN : height(row, column);
        }
    }
}

static void describe(sdf_info *info, sdf_revision revision, sdf_storage storage,
                     sdf_data_type type)
{
    sdf_info_init(info);
    info->revision = revision;
    info->storage = storage;
    info->data_type = type;
    info->columns = COLUMNS;
    info->rows = ROWS;
    info->x_scale = 2e-6;
    info->y_scale = 3e-6;
    info->z_resolution = 1e-9;
    info->create_date = 1772600760; /* 2026-03-04 05:06 UTC */
    info->mod_date = 1772604360;
    snprintf(info->manufacturer_id, sizeof info->manufacturer_id, "ACME 7");
}

static void check_roundtrip(sdf_revision revision, sdf_storage storage,
                            sdf_data_type type)
{
    sdf_info info;
    describe(&info, revision, storage, type);
    double heights[ROWS * COLUMNS];
    make_heights(heights);

    membuf file = {0};
    const sdf_stream stream = membuf_stream(&file);
    sdf_writer *writer = sdf_writer_new();
    CHECK_OK(sdf_create_stream(writer, &stream, &info));
    CHECK_OK(sdf_write_heights(writer, sdf_metres(SDF_FLOAT64), heights,
                               COLUMNS * sizeof(double)));
    CHECK_OK(sdf_write_end(writer, tagged_trailer, strlen(tagged_trailer)));
    sdf_writer_free(writer);

    sdf_reader *reader = sdf_reader_new();
    CHECK_OK(sdf_open_memory(reader, file.data, file.size));
    const sdf_info *read = sdf_get_info(reader);
    CHECK(read != NULL);
    if (read == NULL)
    {
        sdf_reader_free(reader);
        free(file.data);
        return;
    }
    CHECK(read->revision == revision);
    CHECK(read->storage == storage);
    CHECK(read->data_type == type);
    CHECK(read->columns == COLUMNS && read->rows == ROWS);
    CHECK(read->x_scale == info.x_scale && read->y_scale == info.y_scale);
    CHECK(read->z_resolution == info.z_resolution);
    CHECK(read->create_date == info.create_date);
    CHECK(read->mod_date == info.mod_date);
    CHECK(strcmp(read->manufacturer_id, info.manufacturer_id) == 0);
    CHECK(read->z_scale > 0.0);

    double back[ROWS * COLUMNS];
    CHECK_OK(sdf_read_heights(reader, sdf_metres(SDF_FLOAT64), back,
                              COLUMNS * sizeof(double)));
    /* An integer type is as exact as one count; text and binary32 keep
     * about as many digits as they print. */
    double tolerance = 1e-13;
    if (type == SDF_DATA_BINARY32)
    {
        tolerance = 1e-6;
    }
    for (size_t i = 0; i < ROWS * COLUMNS; ++i)
    {
        if (isnan(heights[i]))
        {
            CHECK(isnan(back[i]));
        }
        else if (!sdfi_is_floating(type))
        {
            CHECK(fabs(back[i] - heights[i]) <= 0.5 * read->z_scale * 1.0001);
        }
        else
        {
            CHECK(fabs(back[i] - heights[i])
                  <= tolerance * fmax(1e-6, fabs(heights[i])));
        }
    }

    const char *trailer = NULL;
    size_t size = 0;
    CHECK_OK(sdf_read_trailer(reader, &trailer, &size));
    CHECK(trailer != NULL && size == strlen(tagged_trailer)
          && memcmp(trailer, tagged_trailer, size) == 0);
    const char *value = NULL;
    size_t value_size = 0;
    CHECK(sdf_trailer_find(trailer, size, "partname", &value, &value_size)
          && value_size == strlen("Sample")
          && memcmp(value, "Sample", value_size) == 0);

    sdf_reader_free(reader);
    free(file.data);
}

static void check_all_types(void)
{
    const sdf_revision revisions[] = {SDF_ISO_1_0, SDF_ISO_2_0, SDF_BCR_1_0};
    const sdf_storage storages[] = {SDF_BINARY, SDF_ASCII};
    for (size_t r = 0; r < sizeof revisions / sizeof revisions[0]; ++r)
    {
        for (size_t s = 0; s < sizeof storages / sizeof storages[0]; ++s)
        {
            for (int t = SDF_DATA_UINT8; t <= SDF_DATA_BINARY64; ++t)
            {
                const sdf_data_type type = (sdf_data_type)t;
                /* The unsigned types cannot hold the negative heights. */
                if (!sdf_revision_allows(revisions[r], type)
                    || type == SDF_DATA_UINT8 || type == SDF_DATA_UINT16
                    || type == SDF_DATA_UINT32)
                {
                    continue;
                }
                check_roundtrip(revisions[r], storages[s], type);
            }
        }
    }
}

/* The rows API moves the stored values as they are, in the order of the file,
 * which is reversed in BCR-1.0. */
static void check_rows(sdf_revision revision, sdf_storage storage)
{
    sdf_info info;
    describe(&info, revision, storage, SDF_DATA_INT16);
    info.z_scale = 1e-9;
    int16_t counts[ROWS * COLUMNS];
    for (size_t i = 0; i < ROWS * COLUMNS; ++i)
    {
        counts[i] = (int16_t)((int)i * 37 - 100);
    }
    counts[3] = INT16_MIN; /* the marker of a point not measured in ISO */
    counts[4] = INT16_MAX; /* the marker of BCR-1.0 */

    membuf file = {0};
    const sdf_stream stream = membuf_stream(&file);
    sdf_writer *writer = sdf_writer_new();
    CHECK_OK(sdf_create_stream(writer, &stream, &info));
    CHECK_OK(sdf_write_rows(writer, counts, 3));
    CHECK_OK(sdf_write_rows(writer, counts + 3 * COLUMNS, ROWS - 3));
    CHECK_OK(sdf_write_end(writer, NULL, 0));
    sdf_writer_free(writer);

    sdf_reader *reader = sdf_reader_new();
    CHECK_OK(sdf_open_memory(reader, file.data, file.size));
    int16_t back[ROWS * COLUMNS];
    CHECK_OK(sdf_read_rows(reader, back, 2));
    CHECK_OK(sdf_read_rows(reader, back + 2 * COLUMNS, ROWS - 2));
    CHECK(memcmp(counts, back, sizeof counts) == 0);
    const char *trailer = (const char *)1;
    size_t size = 1;
    CHECK_OK(sdf_read_trailer(reader, &trailer, &size));
    /* A text file closes the trailer record, so it has an empty one. */
    CHECK(storage == SDF_ASCII ? (trailer != NULL && size == 0)
                               : (trailer == NULL && size == 0));

    /* The heights are in the order of y in every revision. */
    sdf_info as_read = *sdf_get_info(reader);
    CHECK_OK(sdf_rewind(reader));
    double heights[ROWS * COLUMNS];
    CHECK_OK(sdf_read_heights(reader, sdf_metres(SDF_FLOAT64), heights,
                              COLUMNS * sizeof(double)));
    for (size_t file_row = 0; file_row < ROWS; ++file_row)
    {
        const size_t y = sdf_row_y(&as_read, file_row);
        CHECK(y == (revision == SDF_BCR_1_0 ? ROWS - 1 - file_row : file_row));
        for (size_t column = 5; column < COLUMNS; ++column)
        {
            const int16_t count = counts[file_row * COLUMNS + column];
            if (!sdf_is_sentinel(SDF_DATA_INT16, revision, count))
            {
                CHECK(near_relative(heights[y * COLUMNS + column], count * 1e-9,
                                    1e-12));
            }
        }
    }
    sdf_reader_free(reader);
    free(file.data);
}

static void check_errors(void)
{
    sdf_info info;
    describe(&info, SDF_ISO_2_0, SDF_BINARY, SDF_DATA_INT16);
    double heights[ROWS * COLUMNS];
    make_heights(heights);
    const ptrdiff_t stride = COLUMNS * sizeof(double);
    membuf file = {0};
    const sdf_stream stream = membuf_stream(&file);

    /* Counts need a z-scale: the writer cannot know what they mean. */
    sdf_writer *writer = sdf_writer_new();
    int16_t counts[ROWS * COLUMNS] = {0};
    CHECK_OK(sdf_create_stream(writer, &stream, &info));
    CHECK_STATUS(SDF_ERR_ARGUMENT, sdf_write_rows(writer, counts, 1));
    CHECK(strstr(sdf_writer_message(writer), "z-scale") != NULL);
    sdf_writer_free(writer);

    /* A height that does not fit the z-scale that was asked for. */
    info.z_scale = 1e-12;
    writer = sdf_writer_new();
    CHECK_OK(sdf_create_stream(writer, &stream, &info));
    CHECK_STATUS(
        SDF_ERR_RANGE,
        sdf_write_heights(writer, sdf_metres(SDF_FLOAT64), heights, stride));
    sdf_writer_free(writer);

    /* A height that would be read as a point not measured. */
    info.z_scale = 1.0;
    double marker[1] = {-32768.0};
    info.columns = 1;
    info.rows = 1;
    writer = sdf_writer_new();
    CHECK_OK(sdf_create_stream(writer, &stream, &info));
    CHECK_STATUS(SDF_ERR_SENTINEL,
                 sdf_write_heights(writer, sdf_metres(SDF_FLOAT64), marker,
                                   sizeof(double)));
    sdf_writer_free(writer);

    /* A description that cannot be written. */
    describe(&info, SDF_ISO_1_0, SDF_BINARY, SDF_DATA_INT8);
    writer = sdf_writer_new();
    CHECK_STATUS(SDF_ERR_ARGUMENT, sdf_create_stream(writer, &stream, &info));
    sdf_writer_free(writer);
    describe(&info, SDF_ISO_1_0, SDF_BINARY, SDF_DATA_INT16);
    info.columns = (size_t)UINT16_MAX + 1;
    writer = sdf_writer_new();
    CHECK_STATUS(SDF_ERR_ARGUMENT, sdf_create_stream(writer, &stream, &info));
    sdf_writer_free(writer);

    /* The end before the rows, and a trailer ISO-2.0 cannot have. */
    describe(&info, SDF_ISO_2_0, SDF_BINARY, SDF_DATA_BINARY64);
    writer = sdf_writer_new();
    CHECK_OK(sdf_create_stream(writer, &stream, &info));
    CHECK_STATUS(SDF_ERR_STATE, sdf_write_end(writer, NULL, 0));
    CHECK_OK(
        sdf_write_heights(writer, sdf_metres(SDF_FLOAT64), heights, stride));
    CHECK_STATUS(SDF_ERR_ARGUMENT, sdf_write_end(writer, "free text", 9));
    CHECK_STATUS(SDF_ERR_ARGUMENT, sdf_write_end(writer, "caf\xC3\xA9 = 1", 8));
    CHECK_OK(sdf_write_end(writer, "Note = a", 8));
    CHECK_STATUS(SDF_ERR_STATE, sdf_write_end(writer, NULL, 0));
    sdf_writer_free(writer);
    free(file.data);
}

/* The z-scale a writer chooses uses the range of the type. */
static void check_z_scale(void)
{
    const double scale =
        sdf_z_scale_for_range(SDF_DATA_INT16, SDF_ISO_2_0, -80e-6, 80e-6);
    CHECK(near_relative(scale, 80e-6 / 32767.0, 1e-12));
    CHECK(sdf_z_scale_for_range(SDF_DATA_BINARY64, SDF_ISO_2_0, -1, 1) == 1.0);
    CHECK(sdf_z_scale_for_range(SDF_DATA_INT16, SDF_ISO_2_0, NAN, NAN) == 1.0);
    /* BCR-1.0 keeps the maximum for the marker. */
    CHECK(sdf_z_scale_for_range(SDF_DATA_INT16, SDF_BCR_1_0, 0, 32767.0) > 1.0);
}

int main(void)
{
    check_all_types();
    for (int r = SDF_ISO_1_0; r <= SDF_BCR_1_0; ++r)
    {
        check_rows((sdf_revision)r, SDF_BINARY);
        check_rows((sdf_revision)r, SDF_ASCII);
    }
    check_errors();
    check_z_scale();
    return finish_tests();
}
