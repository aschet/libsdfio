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

    /* A marker of the caller instead of NaN. */
    sdfio_format format = sdfio_metres(SDFIO_FLOAT64);
    format.missing = -9999.0;
    double doubles[4];
    CHECK_OK(sdfio_decode_heights(&header, format, coded, 4, doubles));
    CHECK(doubles[2] == -9999.0);

    /* A height that does not fit a float. */
    const sdfio_header scaled =
        description(SDFIO_ISO_2_0, SDFIO_DATA_INT32, 1e300);
    const int32_t large[2] = {1, 2};
    float narrow[2];
    CHECK_STATUS(SDFIO_ERR_RANGE,
                 sdfio_decode_heights(&scaled, sdfio_metres(SDFIO_FLOAT32),
                                      large, 2, narrow));

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
    CHECK_STATUS(SDFIO_ERR_BAD_VALUE,
                 sdfio_encode_heights(&header, sdfio_metres(SDFIO_FLOAT64),
                                      marker, 1, coded));

    /* BCR-1.0 keeps the maximum, and the minimum is a height. */
    const sdfio_header bcr = description(SDFIO_BCR_1_0, SDFIO_DATA_INT16, 1e-9);
    const double top[] = {32767e-9};
    const double bottom[] = {-32768e-9};
    CHECK_STATUS(
        SDFIO_ERR_BAD_VALUE,
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
    const size_t sizes[] = {0,
                            1,
                            SDFIO_STACK_CHUNK - 1,
                            SDFIO_STACK_CHUNK,
                            SDFIO_STACK_CHUNK + 1,
                            3 * SDFIO_STACK_CHUNK + 5};
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
}

static void check_tables(void)
{
    CHECK(sdfio_bad_value(SDFIO_DATA_INT16, SDFIO_ISO_2_0) == INT16_MIN);
    CHECK(sdfio_bad_value(SDFIO_DATA_INT16, SDFIO_BCR_1_0) == INT16_MAX);
    CHECK(sdfio_bad_value(SDFIO_DATA_BINARY64, SDFIO_ISO_1_0) == -DBL_MAX);
    CHECK(sdfio_is_bad_value(SDFIO_DATA_BINARY64, SDFIO_BCR_1_0, 1.7e308));
    CHECK(!sdfio_is_bad_value(SDFIO_DATA_BINARY64, SDFIO_ISO_2_0, 1.7e308));
    CHECK(sdfio_data_type_size(SDFIO_DATA_UINT8) == 1);
    CHECK(sdfio_data_type_size(SDFIO_DATA_BINARY32) == 4);
    CHECK(sdfio_data_type_size(SDFIO_DATA_BINARY64) == 8);
    CHECK(sdfio_sample_size(SDFIO_FLOAT32) == 4);
    CHECK(sdfio_sample_size(SDFIO_FLOAT64) == 8);
    CHECK(sdfio_sample_size((sdfio_sample)99) == 0);
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

/* The samples of the caller need not follow each other: the z of a point, one
 * channel, a row that runs backwards. */
typedef struct point
{
    float x;
    float y;
    float z;
    float w;
} point;

static void check_element_stride(void)
{
    const sdfio_header header =
        description(SDFIO_ISO_2_0, SDFIO_DATA_INT16, 1e-3);
    const int16_t coded[4] = {1000, INT16_MIN, -2000, 3000};

    /* Decoded into the z of points: the rest of a point is left alone. */
    point points[4];
    for (size_t i = 0; i < 4; ++i)
    {
        points[i] = (point){1.0F, 2.0F, 99.0F, 4.0F};
    }
    sdfio_format format = sdfio_metres(SDFIO_FLOAT32);
    format.element_stride = sizeof(point);
    CHECK_OK(sdfio_decode_heights(&header, format, coded, 4, &points[0].z));
    CHECK(points[0].z == 1.0F && isnan(points[1].z) && points[2].z == -2.0F
          && points[3].z == 3.0F);
    for (size_t i = 0; i < 4; ++i)
    {
        CHECK(points[i].x == 1.0F && points[i].y == 2.0F
              && points[i].w == 4.0F);
    }

    /* Encoded from them. */
    int16_t back[4];
    CHECK_OK(sdfio_encode_heights(&header, format, &points[0].z, 4, back));
    CHECK(back[0] == 1000 && back[1] == INT16_MIN && back[2] == -2000
          && back[3] == 3000);

    /* The range of the z alone, whatever the other members say. */
    points[0].x = 1e30F;
    points[3].w = -1e30F;
    double z_scale = 0.0;
    CHECK_OK(sdfio_suggest_z_scale(SDFIO_DATA_INT16, SDFIO_ISO_2_0, format,
                                   &points[0].z, 1, 4, 0, &z_scale));
    CHECK(near_relative(z_scale, 3.0 / 32767.0, 1e-9));

    /* A negative stride runs backwards from the pointer. */
    float mirrored[4] = {0};
    format = sdfio_metres(SDFIO_FLOAT32);
    format.element_stride = -(ptrdiff_t)sizeof(float);
    CHECK_OK(sdfio_decode_heights(&header, format, coded, 4, &mirrored[3]));
    CHECK(mirrored[3] == 1.0F && isnan(mirrored[2]) && mirrored[1] == -2.0F
          && mirrored[0] == 3.0F);

    /* A stride that is smaller than a sample would overwrite itself. */
    format = sdfio_metres(SDFIO_FLOAT32);
    format.element_stride = 2;
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_decode_heights(&header, format, coded, 4, mirrored));
    CHECK_OK(sdfio_decode_heights(&header, format, coded, 1, mirrored));
}

static void check_layout(void)
{
    sdfio_format format = sdfio_metres(SDFIO_FLOAT64);
    const ptrdiff_t size = sizeof(double);
    /* Row-major, with padding at the end of the rows. */
    CHECK_OK(sdfio_check_layout(format, 3, 4, 5 * size, NULL));
    /* Column-major: the columns lie beside each other. */
    format.element_stride = 3 * size;
    CHECK_OK(sdfio_check_layout(format, 3, 4, size, NULL));
    /* Interleaved: one of three channels. */
    format.element_stride = 3 * size;
    CHECK_OK(sdfio_check_layout(format, 3, 4, 12 * size, NULL));
    /* Mirrored. */
    format.element_stride = -size;
    CHECK_OK(sdfio_check_layout(format, 3, 4, -4 * size, NULL));
    /* Overlapping: both strides a sample. */
    format.element_stride = 0;
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_check_layout(format, 3, 4, size, NULL));
    /* Rows that interleave but do not fit: a stride between. */
    format.element_stride = 2 * size;
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_check_layout(format, 3, 4, 3 * size, NULL));
    /* A matrix of one row or column needs only one of the strides. */
    format.element_stride = 0;
    CHECK_OK(sdfio_check_layout(format, 1, 4, 0, NULL));
    format = sdfio_metres(SDFIO_FLOAT64);
    format.element_stride = 0;
    CHECK_OK(sdfio_check_layout(format, 4, 1, size, NULL));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_check_layout(format, 4, 1, 3, NULL));
    /* Not countable. */
    format.element_stride = PTRDIFF_MAX;
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_check_layout(format, 2, SIZE_MAX / 2,
                                                        PTRDIFF_MAX, NULL));
}

/* A generator of numbers that does not depend on the library of the C
 * compiler. */
static uint32_t next_random(uint32_t *state)
{
    *state ^= *state << 13;
    *state ^= *state >> 17;
    *state ^= *state << 5;
    return *state;
}

static bool same_float(float a, float b)
{
    return isnan(a) ? isnan(b) : a == b;
}

enum
{
    ALL_VALUES = 1U << 16,
    RANDOM_HEIGHTS = 20000
};

/* The stored values of a data type that a float holds exactly: every one of
 * the small integer types, and random numbers of a binary32. */
static size_t stored_values(sdfio_data_type type, uint32_t *state,
                            unsigned char *bytes)
{
    size_t count = 0;
    switch (type)
    {
    case SDFIO_DATA_INT8:
    case SDFIO_DATA_UINT8:
        for (unsigned v = 0; v < 256; ++v)
        {
            bytes[count++] = (unsigned char)v;
        }
        return count;
    case SDFIO_DATA_INT16:
    case SDFIO_DATA_UINT16:
        for (unsigned v = 0; v < ALL_VALUES; ++v)
        {
            const uint16_t value = (uint16_t)v;
            memcpy(bytes + count * sizeof value, &value, sizeof value);
            ++count;
        }
        return count;
    default:
        for (size_t i = 0; i < RANDOM_HEIGHTS; ++i)
        {
            const float value =
                i % 97 == 0
                    ? NAN
                    : ((float)(next_random(state) % 2000001) - 1e6F) * 1e-3F;
            memcpy(bytes + i * sizeof value, &value, sizeof value);
        }
        return RANDOM_HEIGHTS;
    }
}

/* Stored values are the same, or are two binary32 numbers beside each other:
 * a quotient between two numbers that is half way, within the accuracy of the
 * scale, falls to either. */
static bool same_coded(sdfio_data_type type, const unsigned char *a,
                       const unsigned char *b)
{
    if (type != SDFIO_DATA_BINARY32)
    {
        return memcmp(a, b, sdfio_data_type_size(type)) == 0;
    }
    float x;
    float y;
    memcpy(&x, a, sizeof x);
    memcpy(&y, b, sizeof y);
    return x == y || nextafterf(x, INFINITY) == y
           || nextafterf(x, -INFINITY) == y;
}

/* The calculation in float gives the heights, and the data, that the
 * calculation in double gives. */
static void check_float_path(void)
{
    static const sdfio_data_type types[] = {SDFIO_DATA_INT8, SDFIO_DATA_UINT8,
                                            SDFIO_DATA_INT16, SDFIO_DATA_UINT16,
                                            SDFIO_DATA_BINARY32};
    static const sdfio_revision revisions[] = {SDFIO_ISO_2_0, SDFIO_BCR_1_0};
    static const double scales[] = {1.0, 1e-3, 3e-9, 0.25e-6, 7.0, 0.1, 1e-35};
    static unsigned char bytes[ALL_VALUES * sizeof(uint16_t)];
    static float singles[ALL_VALUES];
    static double doubles[ALL_VALUES];
    uint32_t state = 2463534242U;
    for (size_t t = 0; t < sizeof types / sizeof types[0]; ++t)
    {
        for (size_t r = 0; r < sizeof revisions / sizeof revisions[0]; ++r)
        {
            for (size_t s = 0; s < sizeof scales / sizeof scales[0]; ++s)
            {
                const sdfio_header header =
                    description(revisions[r], types[t], scales[s]);
                const size_t count = stored_values(types[t], &state, bytes);
                const sdfio_status decoded_single =
                    sdfio_decode_heights(&header, sdfio_metres(SDFIO_FLOAT32),
                                         bytes, count, singles);
                const sdfio_status decoded_double =
                    sdfio_decode_heights(&header, sdfio_metres(SDFIO_FLOAT64),
                                         bytes, count, doubles);
                CHECK(decoded_double == SDFIO_OK);
                CHECK(decoded_single == SDFIO_OK);
                size_t different = 0;
                for (size_t i = 0; i < count && decoded_single == SDFIO_OK; ++i)
                {
                    different +=
                        same_float(singles[i], (float)doubles[i]) ? 0 : 1;
                }
                CHECK(different == 0);

                /* The heights back to the data, one at a time, so that a
                 * failure belongs to its height. */
                const double top = sdfio_highest(types[t]) * scales[s];
                size_t disagree = 0;
                unsigned char coded_single[sizeof(uint32_t)] = {0};
                unsigned char coded_double[sizeof(uint32_t)] = {0};
                for (size_t i = 0; i < RANDOM_HEIGHTS; ++i)
                {
                    const double fraction =
                        ((double)(next_random(&state) % 2400001) / 1e6 - 1.2);
                    const float height =
                        i % 101 == 0 ? INFINITY : (float)(fraction * top);
                    const double widened = height;
                    const sdfio_status single = sdfio_encode_heights(
                        &header, sdfio_metres(SDFIO_FLOAT32), &height, 1,
                        coded_single);
                    const sdfio_status wide = sdfio_encode_heights(
                        &header, sdfio_metres(SDFIO_FLOAT64), &widened, 1,
                        coded_double);
                    disagree +=
                        single != wide
                                || (single == SDFIO_OK
                                    && !same_coded(types[t], coded_single,
                                                   coded_double))
                            ? 1
                            : 0;
                }
                CHECK(disagree == 0);
            }
        }
    }
}

int main(void)
{
    check_decode();
    check_encode();
    check_chunks();
    check_range();
    check_float_marker();
    check_element_stride();
    check_layout();
    check_tables();
    check_float_path();
    return finish_tests();
}
