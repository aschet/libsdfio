/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

#include "check.h"
#include "data_type.h"
#include "revision.h"
#include <float.h>
#include <stdlib.h>
#include <string.h>

#include "layout.h"

static sdfio_header *description(sdfio_revision revision, sdfio_data_type type,
                                 double z_scale)
{
    sdfio_header *header = new_header();
    CHECK_OK(sdfio_header_set_revision(header, revision));
    CHECK_OK(sdfio_header_set_data_type(header, type));
    CHECK_OK(sdfio_header_set_z_scale(header, z_scale));
    return header;
}

static void check_decode(void)
{
    const sdfio_header *header =
        description(SDFIO_ISO_2_0, SDFIO_TYPE_INT16, 1e-9);
    const int16_t coded[] = {100, -200, INT16_MIN, 0};
    float heights[4];
    CHECK_OK(sdfio_decode_heights(header, sdfio_layout_packed(SDFIO_TYPE_FLOAT),
                                  coded, 4, heights));
    CHECK(near_relative(heights[0], 1e-7f, 1e-6));
    CHECK(near_relative(heights[1], -2e-7f, 1e-6));
    CHECK(isnan(heights[2]));
    CHECK(heights[3] == 0.0f);

    double doubles[4];
    CHECK_OK(sdfio_decode_heights(
        header, sdfio_layout_packed(SDFIO_TYPE_DOUBLE), coded, 4, doubles));
    CHECK(isnan(doubles[2]) && doubles[3] == 0.0);

    const sdfio_header *scaled =
        description(SDFIO_ISO_2_0, SDFIO_TYPE_INT32, 1e300);
    const int32_t large[2] = {1, 2};
    float narrow[2];
    CHECK_STATUS(SDFIO_ERR_RANGE,
                 sdfio_decode_heights(scaled,
                                      sdfio_layout_packed(SDFIO_TYPE_FLOAT),
                                      large, 2, narrow));

    /* The minimum of a binary32 marks the point. */
    const sdfio_header *binary =
        description(SDFIO_ISO_2_0, SDFIO_TYPE_FLOAT, 1.0);
    const float floats[] = {1.5f, -FLT_MAX, FLT_MAX};
    double back[3];
    CHECK_OK(sdfio_decode_heights(
        binary, sdfio_layout_packed(SDFIO_TYPE_DOUBLE), floats, 3, back));
    CHECK(back[0] == 1.5 && isnan(back[1]) && back[2] == (double)FLT_MAX);

    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_decode_heights(NULL,
                                      sdfio_layout_packed(SDFIO_TYPE_DOUBLE),
                                      coded, 4, doubles));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_decode_heights(header,
                                      sdfio_layout_packed((sdfio_data_type)99),
                                      coded, 4, doubles));
    CHECK_OK(sdfio_decode_heights(
        header, sdfio_layout_packed(SDFIO_TYPE_DOUBLE), NULL, 0, NULL));
}

static void check_encode(void)
{
    const sdfio_header *header =
        description(SDFIO_ISO_2_0, SDFIO_TYPE_INT16, 1e-9);
    const double heights[] = {1.004e-7, -2e-7, NAN, INFINITY};
    int16_t coded[4];
    CHECK_OK(sdfio_encode_heights(
        header, sdfio_layout_packed(SDFIO_TYPE_DOUBLE), heights, 4, coded));
    CHECK(coded[0] == 100 && coded[1] == -200);
    CHECK(coded[2] == INT16_MIN && coded[3] == INT16_MIN);

    const double too_high[] = {1.0};
    CHECK_STATUS(SDFIO_ERR_RANGE,
                 sdfio_encode_heights(header,
                                      sdfio_layout_packed(SDFIO_TYPE_DOUBLE),
                                      too_high, 1, coded));
    const double bad_height[] = {-32768e-9};
    CHECK_STATUS(SDFIO_ERR_BAD_VALUE,
                 sdfio_encode_heights(header,
                                      sdfio_layout_packed(SDFIO_TYPE_DOUBLE),
                                      bad_height, 1, coded));

    /* The maximum is a height. */
    const double top[] = {32767e-9};
    CHECK_OK(sdfio_encode_heights(
        header, sdfio_layout_packed(SDFIO_TYPE_DOUBLE), top, 1, coded));
    CHECK(coded[0] == INT16_MAX);

    const sdfio_header *unscaled =
        description(SDFIO_ISO_2_0, SDFIO_TYPE_INT16, SDFIO_AUTO_Z_SCALE);
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_encode_heights(unscaled,
                                      sdfio_layout_packed(SDFIO_TYPE_DOUBLE),
                                      too_high, 1, coded));

    /* A float element holds a float file, and is checked against its range. */
    const sdfio_header *single =
        description(SDFIO_ISO_2_0, SDFIO_TYPE_FLOAT, 1.0);
    const double large[] = {1e300};
    float floats[1];
    CHECK_STATUS(SDFIO_ERR_RANGE,
                 sdfio_encode_heights(single,
                                      sdfio_layout_packed(SDFIO_TYPE_DOUBLE),
                                      large, 1, floats));
}

/* Sizes around the chunk size. */
static void check_chunks(void)
{
    const sdfio_header *header =
        description(SDFIO_ISO_2_0, SDFIO_TYPE_INT32, 1e-6);
    const size_t sizes[] = {0, 1, 127, 128, 129, 389};
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
        CHECK_OK(sdfio_encode_heights(header,
                                      sdfio_layout_packed(SDFIO_TYPE_DOUBLE),
                                      heights, count, coded));
        CHECK_OK(sdfio_decode_heights(header,
                                      sdfio_layout_packed(SDFIO_TYPE_DOUBLE),
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
    /* NaN and infinity are not heights, and do not set the scale. */
    const float heights[] = {0.5f, NAN, -2.0f, INFINITY, 3.0f, 1.0f};
    sdfio_layout layout = *sdfio_layout_packed(SDFIO_TYPE_FLOAT);
    double z_scale = 0.0;
    /* Two rows of three, the second one read bottom up. */
    layout.row_stride = -(ptrdiff_t)(3 * sizeof(float));
    CHECK_OK(sdfio_suggest_z_scale(SDFIO_TYPE_INT16, &layout, heights + 3, 2, 3,
                                   &z_scale));
    CHECK(near_relative(z_scale, 3.0 / 32767.0, 1e-12));
    const float none[] = {NAN, INFINITY};
    CHECK_OK(sdfio_suggest_z_scale(SDFIO_TYPE_INT16,
                                   sdfio_layout_packed(SDFIO_TYPE_FLOAT), none,
                                   1, 2, &z_scale));
    CHECK(z_scale == 1.0);
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_suggest_z_scale(SDFIO_TYPE_INT16,
                                       sdfio_layout_packed(SDFIO_TYPE_FLOAT),
                                       none, 1, 2, NULL));
}

/* The elements of the caller need not follow each other: the z of a point, one
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
    const sdfio_header *header =
        description(SDFIO_ISO_2_0, SDFIO_TYPE_INT16, 1e-3);
    const int16_t coded[4] = {1000, INT16_MIN, -2000, 3000};

    /* Decoded into the z of points: the rest of a point is left alone. */
    point points[4];
    for (size_t i = 0; i < 4; ++i)
    {
        points[i] = (point){1.0F, 2.0F, 99.0F, 4.0F};
    }
    sdfio_layout layout = *sdfio_layout_packed(SDFIO_TYPE_FLOAT);
    layout.element_stride = sizeof(point);
    CHECK_OK(sdfio_decode_heights(header, &layout, coded, 4, &points[0].z));
    CHECK(points[0].z == 1.0F && isnan(points[1].z) && points[2].z == -2.0F
          && points[3].z == 3.0F);
    for (size_t i = 0; i < 4; ++i)
    {
        CHECK(points[i].x == 1.0F && points[i].y == 2.0F
              && points[i].w == 4.0F);
    }

    int16_t back[4];
    CHECK_OK(sdfio_encode_heights(header, &layout, &points[0].z, 4, back));
    CHECK(back[0] == 1000 && back[1] == INT16_MIN && back[2] == -2000
          && back[3] == 3000);

    /* The range of the z alone, whatever the other members say. */
    points[0].x = 1e30F;
    points[3].w = -1e30F;
    double z_scale = 0.0;
    CHECK_OK(sdfio_suggest_z_scale(SDFIO_TYPE_INT16, &layout, &points[0].z, 1,
                                   4, &z_scale));
    CHECK(near_relative(z_scale, 3.0 / 32767.0, 1e-9));

    /* A negative stride runs backwards from the pointer. */
    float mirrored[4] = {0};
    layout = *sdfio_layout_packed(SDFIO_TYPE_FLOAT);
    layout.element_stride = -(ptrdiff_t)sizeof(float);
    CHECK_OK(sdfio_decode_heights(header, &layout, coded, 4, &mirrored[3]));
    CHECK(mirrored[3] == 1.0F && isnan(mirrored[2]) && mirrored[1] == -2.0F
          && mirrored[0] == 3.0F);

    /* A stride that is smaller than a element would overwrite itself. */
    layout = *sdfio_layout_packed(SDFIO_TYPE_FLOAT);
    layout.element_stride = 2;
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_decode_heights(header, &layout, coded, 4, mirrored));
    CHECK_OK(sdfio_decode_heights(header, &layout, coded, 1, mirrored));
}

/* Pseudo-random numbers independent of the C library. */
static uint32_t next_random(uint32_t *state)
{
    *state ^= *state << 13;
    *state ^= *state >> 17;
    *state ^= *state << 5;
    return *state;
}

#define ALL_VALUES (1U << 16)
#define RANDOM_HEIGHTS 20000

/* All values of the 8-bit and 16-bit types, random values of the others. */
static size_t stored_values(sdfio_data_type type, uint32_t *state,
                            unsigned char *bytes)
{
    size_t count = 0;
    switch (type)
    {
    case SDFIO_TYPE_INT8:
        for (unsigned v = 0; v < 256; ++v)
        {
            bytes[count++] = (unsigned char)v;
        }
        return count;
    case SDFIO_TYPE_INT16:
        for (unsigned v = 0; v < ALL_VALUES; ++v)
        {
            const uint16_t value = (uint16_t)v;
            memcpy(bytes + count * sizeof value, &value, sizeof value);
            ++count;
        }
        return count;
    case SDFIO_TYPE_INT32:
        for (size_t i = 0; i < RANDOM_HEIGHTS; ++i)
        {
            const int32_t value =
                i % 97 == 0 ? INT32_MIN : (int32_t)next_random(state);
            memcpy(bytes + i * sizeof value, &value, sizeof value);
        }
        return RANDOM_HEIGHTS;
    case SDFIO_TYPE_FLOAT:
        for (size_t i = 0; i < RANDOM_HEIGHTS; ++i)
        {
            const float value =
                i % 97 == 0
                    ? -FLT_MAX
                    : ((float)(next_random(state) % 2000001) - 1e6F) * 1e-3F;
            memcpy(bytes + i * sizeof value, &value, sizeof value);
        }
        return RANDOM_HEIGHTS;
    case SDFIO_TYPE_DOUBLE:
        for (size_t i = 0; i < RANDOM_HEIGHTS; ++i)
        {
            const double value =
                i % 97 == 0
                    ? -DBL_MAX
                    : ((double)(next_random(state) % 2000001) - 1e6) * 1e-3;
            memcpy(bytes + i * sizeof value, &value, sizeof value);
        }
        return RANDOM_HEIGHTS;
    }
    return 0;
}

/* Whether two floats are equal or a few representable steps apart. */
static bool within_steps(float a, float b, int steps)
{
    if (isnan(a) || isnan(b))
    {
        return isnan(a) && isnan(b);
    }
    float up = a;
    float down = a;
    for (int i = 0; i <= steps; ++i)
    {
        if (b == up || b == down)
        {
            return true;
        }
        up = nextafterf(up, INFINITY);
        down = nextafterf(down, -INFINITY);
    }
    return false;
}

/* Calculating in float matches calculating in double to the last float digit,
 * or to a few digits for the types that a float cannot hold. */
static int decode_steps(sdfio_data_type type)
{
    return type == SDFIO_TYPE_INT32 || type == SDFIO_TYPE_DOUBLE ? 2 : 0;
}

/* Equal or adjacent coded values: a quotient near a tie may round either way,
 * and a float has 24 bits against 32 of an integer. */
static bool same_coded(sdfio_data_type type, const unsigned char *a,
                       const unsigned char *b)
{
    switch (type)
    {
    case SDFIO_TYPE_INT8:
    case SDFIO_TYPE_INT16:
        return memcmp(a, b, sdfio_data_type_size(type)) == 0;
    case SDFIO_TYPE_INT32:
    {
        int32_t x;
        int32_t y;
        memcpy(&x, a, sizeof x);
        memcpy(&y, b, sizeof y);
        return fabs((double)x - (double)y) <= 1.0 + fabs((double)y) * 1.5e-7;
    }
    case SDFIO_TYPE_FLOAT:
    {
        float x;
        float y;
        memcpy(&x, a, sizeof x);
        memcpy(&y, b, sizeof y);
        return within_steps(x, y, 1);
    }
    case SDFIO_TYPE_DOUBLE:
    {
        double x;
        double y;
        memcpy(&x, a, sizeof x);
        memcpy(&y, b, sizeof y);
        return fabs(x - y) <= fabs(y) * 1.5e-7;
    }
    }
    return false;
}

/* The highest height that a data type holds at a scale, or a large height. */
static double top_of(sdfio_data_type type, double scale)
{
    switch (type)
    {
    case SDFIO_TYPE_INT8:
        return INT8_MAX * scale;
    case SDFIO_TYPE_INT16:
        return INT16_MAX * scale;
    case SDFIO_TYPE_INT32:
        return INT32_MAX * scale;
    case SDFIO_TYPE_FLOAT:
    case SDFIO_TYPE_DOUBLE:
        return 1e6 * scale;
    }
    return 0.0;
}

/* Heights round to the nearest count and ties to even, for both signs,
 * including near the magnitude from which a float has no fraction. */
static void check_rounding(void)
{
    static const double heights[] = {-2.5,  -1.5,    -0.5,    -0.0,  0.5,
                                     1.5,   2.5,     0.49,    -0.49, 0.51,
                                     -0.51, 32766.5, -32766.5};
    static const int16_t expected[] = {-2, -2, 0, 0,  0,     2,     2,
                                       0,  0,  1, -1, 32766, -32766};
    const size_t count = sizeof heights / sizeof heights[0];
    const sdfio_header *header =
        description(SDFIO_ISO_2_0, SDFIO_TYPE_INT16, 1.0);
    int16_t coded[sizeof heights / sizeof heights[0]];
    CHECK_OK(sdfio_encode_heights(
        header, sdfio_layout_packed(SDFIO_TYPE_DOUBLE), heights, count, coded));
    CHECK(memcmp(coded, expected, sizeof expected) == 0);

    float singles[sizeof heights / sizeof heights[0]];
    for (size_t i = 0; i < count; ++i)
    {
        singles[i] = (float)heights[i];
    }
    CHECK_OK(sdfio_encode_heights(header, sdfio_layout_packed(SDFIO_TYPE_FLOAT),
                                  singles, count, coded));
    CHECK(memcmp(coded, expected, sizeof expected) == 0);

    const sdfio_header *wide =
        description(SDFIO_ISO_2_0, SDFIO_TYPE_INT32, 1.0);
    const float large[] = {8388607.0F,  -8388607.0F, 8388608.0F,
                           -8388608.0F, 16777216.0F, -16777216.0F};
    int32_t large_coded[6];
    CHECK_OK(sdfio_encode_heights(wide, sdfio_layout_packed(SDFIO_TYPE_FLOAT),
                                  large, 6, large_coded));
    for (size_t i = 0; i < 6; ++i)
    {
        CHECK(large_coded[i] == (int32_t)large[i]);
    }
}

/* Calculating in float gives the heights and the data that calculating in
 * double gives. */
static void check_float_path(void)
{
    static const sdfio_data_type types[] = {SDFIO_TYPE_INT8, SDFIO_TYPE_INT16,
                                            SDFIO_TYPE_INT32, SDFIO_TYPE_FLOAT,
                                            SDFIO_TYPE_DOUBLE};
    static const sdfio_revision revisions[] = {SDFIO_ISO_1_0, SDFIO_ISO_2_0};
    static const double scales[] = {1.0, 1e-3, 3e-9, 0.25e-6, 7.0, 0.1, 1e-35};
    static unsigned char bytes[RANDOM_HEIGHTS * sizeof(double)];
    static float singles[ALL_VALUES];
    static double doubles[ALL_VALUES];
    uint32_t state = 2463534242U;
    for (size_t t = 0; t < sizeof types / sizeof types[0]; ++t)
    {
        for (size_t r = 0; r < sizeof revisions / sizeof revisions[0]; ++r)
        {
            for (size_t s = 0; s < sizeof scales / sizeof scales[0]; ++s)
            {
                if (!sdfio_revision_allows(revisions[r], types[t]))
                {
                    continue;
                }
                const sdfio_header *header =
                    description(revisions[r], types[t], scales[s]);
                const size_t count = stored_values(types[t], &state, bytes);
                CHECK(count <= ALL_VALUES);
                const sdfio_status decoded_single = sdfio_decode_heights(
                    header, sdfio_layout_packed(SDFIO_TYPE_FLOAT), bytes, count,
                    singles);
                const sdfio_status decoded_double = sdfio_decode_heights(
                    header, sdfio_layout_packed(SDFIO_TYPE_DOUBLE), bytes,
                    count, doubles);
                CHECK(decoded_double == SDFIO_OK);
                CHECK(decoded_single == SDFIO_OK);
                size_t different = 0;
                for (size_t i = 0; i < count && decoded_single == SDFIO_OK; ++i)
                {
                    different += within_steps(singles[i], (float)doubles[i],
                                              decode_steps(types[t]))
                                     ? 0
                                     : 1;
                }
                CHECK(different == 0);

                /* The heights back to the data, one at a time, so that a
                 * failure belongs to its height. */
                const double top = top_of(types[t], scales[s]);
                size_t disagree = 0;
                unsigned char coded_single[sizeof(double)] = {0};
                unsigned char coded_double[sizeof(double)] = {0};
                for (size_t i = 0; i < RANDOM_HEIGHTS; ++i)
                {
                    const double fraction =
                        ((double)(next_random(&state) % 2400001) / 1e6 - 1.2);
                    const float height =
                        i % 101 == 0 ? INFINITY : (float)(fraction * top);
                    const double widened = height;
                    const sdfio_status single = sdfio_encode_heights(
                        header, sdfio_layout_packed(SDFIO_TYPE_FLOAT), &height,
                        1, coded_single);
                    const sdfio_status wide_status = sdfio_encode_heights(
                        header, sdfio_layout_packed(SDFIO_TYPE_DOUBLE),
                        &widened, 1, coded_double);
                    disagree +=
                        single != wide_status
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
    check_element_stride();
    check_float_path();
    check_rounding();
    return finish_tests();
}
