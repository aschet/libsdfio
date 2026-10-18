/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* The step from stored values to heights and back, without a file. */

#include "check.h"

#include "internal.h"

static sdfio_header description(sdfio_revision revision, sdfio_data_type type,
                                double z_scale)
{
    sdfio_header header;
    sdfio_header_init(&header);
    header.revision = revision;
    header.data_type = type;
    header.z_scale = z_scale;
    return header;
}

static void check_decode(void)
{
    const sdfio_header header =
        description(SDFIO_ISO_2_0, SDFIO_DATA_INT16, 1e-9);
    const int16_t coded[] = {100, -200, INT16_MIN, 0};
    float heights[4];
    CHECK_OK(sdfio_decode_heights(&header, sdfio_metres(SDFIO_FLOAT32), coded,
                                  4, heights));
    CHECK(near_relative(heights[0], 1e-7f, 1e-6));
    CHECK(near_relative(heights[1], -2e-7f, 1e-6));
    CHECK(isnan(heights[2]));
    CHECK(heights[3] == 0.0f);

    /* A marker of the caller instead of NaN, 0 in an integer type. */
    sdfio_format format = sdfio_metres(SDFIO_FLOAT64);
    format.missing = -9999.0;
    double doubles[4];
    CHECK_OK(sdfio_decode_heights(&header, format, coded, 4, doubles));
    CHECK(doubles[2] == -9999.0);
    int32_t counts[4];
    const sdfio_header scaled =
        description(SDFIO_ISO_2_0, SDFIO_DATA_INT16, 1.0);
    CHECK_OK(sdfio_decode_heights(&scaled, sdfio_metres(SDFIO_INT32), coded, 4,
                                  counts));
    CHECK(counts[0] == 100 && counts[1] == -200 && counts[2] == 0);

    /* A height that does not fit the sample type. */
    int8_t small[4];
    CHECK_STATUS(SDFIO_ERR_RANGE,
                 sdfio_decode_heights(&scaled, sdfio_metres(SDFIO_INT8), coded,
                                      4, small));

    /* In BCR-1.0 the maximum marks the point, and a float from a threshold. */
    const sdfio_header bcr =
        description(SDFIO_BCR_1_0, SDFIO_DATA_BINARY32, 1.0);
    const float floats[] = {1.5f, 3.4e38f, FLT_MAX};
    double back[3];
    CHECK_OK(sdfio_decode_heights(&bcr, sdfio_metres(SDFIO_FLOAT64), floats, 3,
                                  back));
    CHECK(back[0] == 1.5 && isnan(back[1]) && isnan(back[2]));

    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_decode_heights(NULL, sdfio_metres(SDFIO_FLOAT64), coded,
                                      4, doubles));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_decode_heights(&header, sdfio_metres((sdfio_sample)99),
                                      coded, 4, doubles));
    CHECK_OK(sdfio_decode_heights(&header, sdfio_metres(SDFIO_FLOAT64), NULL, 0,
                                  NULL));
}

static void check_encode(void)
{
    const sdfio_header header =
        description(SDFIO_ISO_2_0, SDFIO_DATA_INT16, 1e-9);
    const double heights[] = {1.004e-7, -2e-7, NAN, INFINITY, -9999.0};
    sdfio_format format = sdfio_metres(SDFIO_FLOAT64);
    format.missing = -9999.0;
    int16_t coded[5];
    CHECK_OK(sdfio_encode_heights(&header, format, heights, 5, coded));
    CHECK(coded[0] == 100 && coded[1] == -200);
    CHECK(coded[2] == INT16_MIN && coded[3] == INT16_MIN
          && coded[4] == INT16_MIN);

    const double too_high[] = {1.0};
    CHECK_STATUS(SDFIO_ERR_RANGE,
                 sdfio_encode_heights(&header, sdfio_metres(SDFIO_FLOAT64),
                                      too_high, 1, coded));
    const double marker[] = {-32768e-9};
    CHECK_STATUS(SDFIO_ERR_SENTINEL,
                 sdfio_encode_heights(&header, sdfio_metres(SDFIO_FLOAT64),
                                      marker, 1, coded));

    /* BCR-1.0 keeps the maximum, and the minimum is a height. */
    const sdfio_header bcr = description(SDFIO_BCR_1_0, SDFIO_DATA_INT16, 1e-9);
    const double top[] = {32767e-9};
    const double bottom[] = {-32768e-9};
    CHECK_STATUS(
        SDFIO_ERR_SENTINEL,
        sdfio_encode_heights(&bcr, sdfio_metres(SDFIO_FLOAT64), top, 1, coded));
    CHECK_OK(sdfio_encode_heights(&bcr, sdfio_metres(SDFIO_FLOAT64), bottom, 1,
                                  coded));
    CHECK(coded[0] == INT16_MIN);

    /* Without a z-scale there is no way to a count. */
    const sdfio_header unscaled =
        description(SDFIO_ISO_2_0, SDFIO_DATA_INT16, SDFIO_AUTO_Z_SCALE);
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_encode_heights(&unscaled, sdfio_metres(SDFIO_FLOAT64),
                                      too_high, 1, coded));

    /* A float sample holds a float file, and is checked against its range. */
    const sdfio_header single =
        description(SDFIO_ISO_2_0, SDFIO_DATA_BINARY32, 1.0);
    const double large[] = {1e300};
    float floats[1];
    CHECK_STATUS(SDFIO_ERR_RANGE,
                 sdfio_encode_heights(&single, sdfio_metres(SDFIO_FLOAT64),
                                      large, 1, floats));
}

/* Values go through in chunks, so the sizes around one chunk are tried. */
static void check_chunks(void)
{
    const sdfio_header header =
        description(SDFIO_ISO_2_0, SDFIO_DATA_INT32, 1e-6);
    const size_t sizes[] = {
        0, 1, SDFI_CHUNK - 1, SDFI_CHUNK, SDFI_CHUNK + 1, 3 * SDFI_CHUNK + 5};
    for (size_t s = 0; s < sizeof sizes / sizeof sizes[0]; ++s)
    {
        const size_t count = sizes[s];
        double *heights = malloc((count + 1) * sizeof(double));
        double *back = malloc((count + 1) * sizeof(double));
        int32_t *coded = malloc((count + 1) * sizeof(int32_t));
        for (size_t i = 0; i < count; ++i)
        {
            heights[i] = i % 17 == 0 ? NAN : (double)i * 1e-3 - 1.0;
        }
        CHECK_OK(sdfio_encode_heights(&header, sdfio_metres(SDFIO_FLOAT64),
                                      heights, count, coded));
        CHECK_OK(sdfio_decode_heights(&header, sdfio_metres(SDFIO_FLOAT64),
                                      coded, count, back));
        for (size_t i = 0; i < count; ++i)
        {
            CHECK(isnan(heights[i]) ? isnan(back[i])
                                    : fabs(back[i] - heights[i]) <= 0.5e-6);
        }
        free(heights);
        free(back);
        free(coded);
    }
}

static void check_range(void)
{
    const float heights[] = {0.5f, NAN, -2.0f, 9999.1f, 3.0f, 1.0f};
    /* A marker of a double literal for the floats that hold 9999.1f. */
    sdfio_format format = sdfio_metres(SDFIO_FLOAT32);
    format.missing = 9999.1;
    double z_scale = 0.0;
    /* Two rows of three, the second one read bottom up. */
    CHECK_OK(sdfio_suggest_z_scale(SDFIO_DATA_INT16, SDFIO_ISO_2_0, format,
                                   heights + 3, 2, 3,
                                   -(ptrdiff_t)(3 * sizeof(float)), &z_scale));
    CHECK(near_relative(z_scale, 3.0 / 32767.0, 1e-12));
    /* Without the marker the large height sets the scale. */
    CHECK_OK(sdfio_suggest_z_scale(
        SDFIO_DATA_INT16, SDFIO_ISO_2_0, sdfio_metres(SDFIO_FLOAT32),
        heights + 3, 2, 3, -(ptrdiff_t)(3 * sizeof(float)), &z_scale));
    CHECK(z_scale > 0.3);
    const float none[] = {NAN, INFINITY};
    CHECK_OK(sdfio_suggest_z_scale(SDFIO_DATA_INT16, SDFIO_ISO_2_0,
                                   sdfio_metres(SDFIO_FLOAT32), none, 1, 2, 0,
                                   &z_scale));
    CHECK(z_scale == 1.0);
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_suggest_z_scale(SDFIO_DATA_INT16, SDFIO_ISO_2_0,
                                       sdfio_metres(SDFIO_FLOAT32), none, 1, 2,
                                       0, NULL));
}

/* A float buffer works with a marker that is a double literal: it is taken in
 * the precision of the buffer in both directions. */
static void check_float_marker(void)
{
    const sdfio_header header =
        description(SDFIO_ISO_2_0, SDFIO_DATA_INT16, 1e-3);
    sdfio_format format = sdfio_metres(SDFIO_FLOAT32);
    format.missing = -9999.1;
    const float heights[] = {1.0f, -9999.1f, NAN, 2.0f};
    int16_t coded[4];
    CHECK_OK(sdfio_encode_heights(&header, format, heights, 4, coded));
    CHECK(coded[0] == 1000 && coded[1] == INT16_MIN && coded[2] == INT16_MIN
          && coded[3] == 2000);
    float back[4];
    CHECK_OK(sdfio_decode_heights(&header, format, coded, 4, back));
    CHECK(back[0] == 1.0f && back[1] == -9999.1f && back[2] == -9999.1f
          && back[3] == 2.0f);

    /* A marker in an integer buffer is a whole number. */
    format = sdfio_metres(SDFIO_INT16);
    format.missing = -7.2;
    const sdfio_header counts =
        description(SDFIO_ISO_2_0, SDFIO_DATA_INT16, 1.0);
    int16_t out[4];
    CHECK_OK(sdfio_decode_heights(&counts, format, coded, 4, out));
    CHECK(out[1] == -7);
}

static void check_tables(void)
{
    CHECK(sdfio_sentinel(SDFIO_DATA_INT16, SDFIO_ISO_2_0) == INT16_MIN);
    CHECK(sdfio_sentinel(SDFIO_DATA_INT16, SDFIO_BCR_1_0) == INT16_MAX);
    CHECK(sdfio_sentinel(SDFIO_DATA_BINARY64, SDFIO_ISO_1_0) == -DBL_MAX);
    CHECK(sdfio_is_sentinel(SDFIO_DATA_BINARY64, SDFIO_BCR_1_0, 1.7e308));
    CHECK(!sdfio_is_sentinel(SDFIO_DATA_BINARY64, SDFIO_ISO_2_0, 1.7e308));
    CHECK(sdfio_data_type_size(SDFIO_DATA_UINT8) == 1);
    CHECK(sdfio_data_type_size(SDFIO_DATA_BINARY32) == 4);
    CHECK(sdfio_data_type_size(SDFIO_DATA_BINARY64) == 8);
    CHECK(sdfio_sample_size(SDFIO_INT16) == 2);
    CHECK(sdfio_sample_of(SDFIO_DATA_BINARY32) == SDFIO_FLOAT32);
    CHECK(sdfio_revision_allows(SDFIO_ISO_1_0, SDFIO_DATA_INT16));
    CHECK(!sdfio_revision_allows(SDFIO_ISO_1_0, SDFIO_DATA_INT8));
    CHECK(!sdfio_revision_allows(SDFIO_ISO_2_0, SDFIO_DATA_UINT16));
    CHECK(sdfio_revision_allows(SDFIO_BCR_1_0, SDFIO_DATA_UINT32));
    CHECK(strcmp(sdfio_revision_name(SDFIO_BCR_1_0), "BCR-1.0") == 0);
    CHECK(strcmp(sdfio_data_type_name(SDFIO_DATA_BINARY64), "binary64") == 0);
    CHECK(strlen(sdfio_version()) > 0);
    for (int i = SDFIO_OK; i <= SDFIO_ERR_MEMORY; ++i)
    {
        CHECK(strlen(sdfio_status_string((sdfio_status)i)) > 0);
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
