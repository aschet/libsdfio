/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* The step from stored values to heights and back, without a file. */

#include "check.h"

#include "internal.h"

static sdf_info description(sdf_revision revision, sdf_data_type type,
                            double z_scale)
{
    sdf_info info;
    sdf_info_init(&info);
    info.revision = revision;
    info.data_type = type;
    info.z_scale = z_scale;
    return info;
}

static void check_decode(void)
{
    const sdf_info info = description(SDF_ISO_2_0, SDF_DATA_INT16, 1e-9);
    const int16_t stored[] = {100, -200, INT16_MIN, 0};
    float heights[4];
    CHECK_OK(
        sdf_decode_heights(&info, sdf_metres(SDF_FLOAT32), stored, 4, heights));
    CHECK(near_relative(heights[0], 1e-7f, 1e-6));
    CHECK(near_relative(heights[1], -2e-7f, 1e-6));
    CHECK(isnan(heights[2]));
    CHECK(heights[3] == 0.0f);

    /* A marker of the caller instead of NaN, 0 in an integer type. */
    sdf_format format = sdf_metres(SDF_FLOAT64);
    format.missing = -9999.0;
    double doubles[4];
    CHECK_OK(sdf_decode_heights(&info, format, stored, 4, doubles));
    CHECK(doubles[2] == -9999.0);
    int32_t counts[4];
    const sdf_info scaled = description(SDF_ISO_2_0, SDF_DATA_INT16, 1.0);
    CHECK_OK(
        sdf_decode_heights(&scaled, sdf_metres(SDF_INT32), stored, 4, counts));
    CHECK(counts[0] == 100 && counts[1] == -200 && counts[2] == 0);

    /* A height that does not fit the sample type. */
    int8_t small[4];
    CHECK_STATUS(
        SDF_ERR_RANGE,
        sdf_decode_heights(&scaled, sdf_metres(SDF_INT8), stored, 4, small));

    /* In BCR-1.0 the maximum marks the point, and a float from a threshold. */
    const sdf_info bcr = description(SDF_BCR_1_0, SDF_DATA_BINARY32, 1.0);
    const float floats[] = {1.5f, 3.4e38f, FLT_MAX};
    double back[3];
    CHECK_OK(
        sdf_decode_heights(&bcr, sdf_metres(SDF_FLOAT64), floats, 3, back));
    CHECK(back[0] == 1.5 && isnan(back[1]) && isnan(back[2]));

    CHECK_STATUS(
        SDF_ERR_ARGUMENT,
        sdf_decode_heights(NULL, sdf_metres(SDF_FLOAT64), stored, 4, doubles));
    CHECK_STATUS(SDF_ERR_ARGUMENT,
                 sdf_decode_heights(&info, sdf_metres((sdf_sample)99), stored,
                                    4, doubles));
    CHECK_OK(sdf_decode_heights(&info, sdf_metres(SDF_FLOAT64), NULL, 0, NULL));
}

static void check_encode(void)
{
    const sdf_info info = description(SDF_ISO_2_0, SDF_DATA_INT16, 1e-9);
    const double heights[] = {1.004e-7, -2e-7, NAN, INFINITY, -9999.0};
    sdf_format format = sdf_metres(SDF_FLOAT64);
    format.missing = -9999.0;
    int16_t stored[5];
    CHECK_OK(sdf_encode_heights(&info, format, heights, 5, stored));
    CHECK(stored[0] == 100 && stored[1] == -200);
    CHECK(stored[2] == INT16_MIN && stored[3] == INT16_MIN
          && stored[4] == INT16_MIN);

    const double too_high[] = {1.0};
    CHECK_STATUS(SDF_ERR_RANGE,
                 sdf_encode_heights(&info, sdf_metres(SDF_FLOAT64), too_high, 1,
                                    stored));
    const double marker[] = {-32768e-9};
    CHECK_STATUS(
        SDF_ERR_SENTINEL,
        sdf_encode_heights(&info, sdf_metres(SDF_FLOAT64), marker, 1, stored));

    /* BCR-1.0 keeps the maximum, and the minimum is a height. */
    const sdf_info bcr = description(SDF_BCR_1_0, SDF_DATA_INT16, 1e-9);
    const double top[] = {32767e-9};
    const double bottom[] = {-32768e-9};
    CHECK_STATUS(
        SDF_ERR_SENTINEL,
        sdf_encode_heights(&bcr, sdf_metres(SDF_FLOAT64), top, 1, stored));
    CHECK_OK(
        sdf_encode_heights(&bcr, sdf_metres(SDF_FLOAT64), bottom, 1, stored));
    CHECK(stored[0] == INT16_MIN);

    /* Without a z-scale there is no way to a count. */
    const sdf_info unscaled =
        description(SDF_ISO_2_0, SDF_DATA_INT16, SDF_AUTO_Z_SCALE);
    CHECK_STATUS(SDF_ERR_ARGUMENT,
                 sdf_encode_heights(&unscaled, sdf_metres(SDF_FLOAT64),
                                    too_high, 1, stored));

    /* A float sample holds a float file, and is checked against its range. */
    const sdf_info single = description(SDF_ISO_2_0, SDF_DATA_BINARY32, 1.0);
    const double large[] = {1e300};
    float floats[1];
    CHECK_STATUS(
        SDF_ERR_RANGE,
        sdf_encode_heights(&single, sdf_metres(SDF_FLOAT64), large, 1, floats));
}

/* Values go through in chunks, so the sizes around one chunk are tried. */
static void check_chunks(void)
{
    const sdf_info info = description(SDF_ISO_2_0, SDF_DATA_INT32, 1e-6);
    const size_t sizes[] = {
        0, 1, SDFI_CHUNK - 1, SDFI_CHUNK, SDFI_CHUNK + 1, 3 * SDFI_CHUNK + 5};
    for (size_t s = 0; s < sizeof sizes / sizeof sizes[0]; ++s)
    {
        const size_t count = sizes[s];
        double *heights = malloc((count + 1) * sizeof(double));
        double *back = malloc((count + 1) * sizeof(double));
        int32_t *stored = malloc((count + 1) * sizeof(int32_t));
        for (size_t i = 0; i < count; ++i)
        {
            heights[i] = i % 17 == 0 ? NAN : (double)i * 1e-3 - 1.0;
        }
        CHECK_OK(sdf_encode_heights(&info, sdf_metres(SDF_FLOAT64), heights,
                                    count, stored));
        CHECK_OK(sdf_decode_heights(&info, sdf_metres(SDF_FLOAT64), stored,
                                    count, back));
        for (size_t i = 0; i < count; ++i)
        {
            CHECK(isnan(heights[i]) ? isnan(back[i])
                                    : fabs(back[i] - heights[i]) <= 0.5e-6);
        }
        free(heights);
        free(back);
        free(stored);
    }
}

static void check_range(void)
{
    const float heights[] = {0.5f, NAN, -2.0f, 9999.1f, 3.0f, 1.0f};
    /* A marker of a double literal for the floats that hold 9999.1f. */
    sdf_format format = sdf_metres(SDF_FLOAT32);
    format.missing = 9999.1;
    double z_scale = 0.0;
    /* Two rows of three, the second one read bottom up. */
    CHECK_OK(sdf_suggest_z_scale(SDF_DATA_INT16, SDF_ISO_2_0, format,
                                 heights + 3, 2, 3,
                                 -(ptrdiff_t)(3 * sizeof(float)), &z_scale));
    CHECK(near_relative(z_scale, 3.0 / 32767.0, 1e-12));
    /* Without the marker the large height sets the scale. */
    CHECK_OK(sdf_suggest_z_scale(SDF_DATA_INT16, SDF_ISO_2_0,
                                 sdf_metres(SDF_FLOAT32), heights + 3, 2, 3,
                                 -(ptrdiff_t)(3 * sizeof(float)), &z_scale));
    CHECK(z_scale > 0.3);
    const float none[] = {NAN, INFINITY};
    CHECK_OK(sdf_suggest_z_scale(SDF_DATA_INT16, SDF_ISO_2_0,
                                 sdf_metres(SDF_FLOAT32), none, 1, 2, 0,
                                 &z_scale));
    CHECK(z_scale == 1.0);
    CHECK_STATUS(SDF_ERR_ARGUMENT,
                 sdf_suggest_z_scale(SDF_DATA_INT16, SDF_ISO_2_0,
                                     sdf_metres(SDF_FLOAT32), none, 1, 2, 0,
                                     NULL));
}

/* A float buffer works with a marker that is a double literal: it is taken in
 * the precision of the buffer in both directions. */
static void check_float_marker(void)
{
    const sdf_info info = description(SDF_ISO_2_0, SDF_DATA_INT16, 1e-3);
    sdf_format format = sdf_metres(SDF_FLOAT32);
    format.missing = -9999.1;
    const float heights[] = {1.0f, -9999.1f, NAN, 2.0f};
    int16_t stored[4];
    CHECK_OK(sdf_encode_heights(&info, format, heights, 4, stored));
    CHECK(stored[0] == 1000 && stored[1] == INT16_MIN && stored[2] == INT16_MIN
          && stored[3] == 2000);
    float back[4];
    CHECK_OK(sdf_decode_heights(&info, format, stored, 4, back));
    CHECK(back[0] == 1.0f && back[1] == -9999.1f && back[2] == -9999.1f
          && back[3] == 2.0f);

    /* A marker in an integer buffer is a whole number. */
    format = sdf_metres(SDF_INT16);
    format.missing = -7.2;
    const sdf_info counts = description(SDF_ISO_2_0, SDF_DATA_INT16, 1.0);
    int16_t out[4];
    CHECK_OK(sdf_decode_heights(&counts, format, stored, 4, out));
    CHECK(out[1] == -7);
}

static void check_tables(void)
{
    CHECK(sdf_sentinel(SDF_DATA_INT16, SDF_ISO_2_0) == INT16_MIN);
    CHECK(sdf_sentinel(SDF_DATA_INT16, SDF_BCR_1_0) == INT16_MAX);
    CHECK(sdf_sentinel(SDF_DATA_BINARY64, SDF_ISO_1_0) == -DBL_MAX);
    CHECK(sdf_is_sentinel(SDF_DATA_BINARY64, SDF_BCR_1_0, 1.7e308));
    CHECK(!sdf_is_sentinel(SDF_DATA_BINARY64, SDF_ISO_2_0, 1.7e308));
    CHECK(sdf_data_type_size(SDF_DATA_UINT8) == 1);
    CHECK(sdf_data_type_size(SDF_DATA_BINARY32) == 4);
    CHECK(sdf_data_type_size(SDF_DATA_BINARY64) == 8);
    CHECK(sdf_sample_size(SDF_INT16) == 2);
    CHECK(sdf_sample_of(SDF_DATA_BINARY32) == SDF_FLOAT32);
    CHECK(sdf_revision_allows(SDF_ISO_1_0, SDF_DATA_INT16));
    CHECK(!sdf_revision_allows(SDF_ISO_1_0, SDF_DATA_INT8));
    CHECK(!sdf_revision_allows(SDF_ISO_2_0, SDF_DATA_UINT16));
    CHECK(sdf_revision_allows(SDF_BCR_1_0, SDF_DATA_UINT32));
    CHECK(strcmp(sdf_revision_name(SDF_BCR_1_0), "BCR-1.0") == 0);
    CHECK(strcmp(sdf_data_type_name(SDF_DATA_BINARY64), "binary64") == 0);
    CHECK(strlen(sdf_version()) > 0);
    for (int i = SDF_OK; i <= SDF_ERR_MEMORY; ++i)
    {
        CHECK(strlen(sdf_status_string((sdf_status)i)) > 0);
    }
}

int main(void)
{
    check_decode();
    check_encode();
    check_chunks();
    check_range();
    check_float_marker();
    check_tables();
    return finish_tests();
}
