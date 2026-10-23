/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* The step from stored values to heights and back, without a file. */

#include "check.h"
#include "data_type.h"
#include "revision.h"
#include <float.h>
#include <stdlib.h>
#include <string.h>

#include "heights.h"

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
        description(SDFIO_ISO_2_0, SDFIO_TYPE_INT16, 1e-9);
    const int16_t coded[] = {100, -200, INT16_MIN, 0};
    float heights[4];
    CHECK_OK(sdfio_decode_heights(
        &header, sdfio_layout_packed(SDFIO_TYPE_FLOAT), coded, 4, heights));
    CHECK(near_relative(heights[0], 1e-7f, 1e-6));
    CHECK(near_relative(heights[1], -2e-7f, 1e-6));
    CHECK(isnan(heights[2]));
    CHECK(heights[3] == 0.0f);

    /* A marker of the caller instead of NaN. */
    sdfio_layout layout = sdfio_layout_packed(SDFIO_TYPE_DOUBLE);
    layout.missing = -9999.0;
    double doubles[4];
    CHECK_OK(sdfio_decode_heights(&header, layout, coded, 4, doubles));
    CHECK(doubles[2] == -9999.0);

    /* A height that does not fit a float. */
    const sdfio_header scaled =
        description(SDFIO_ISO_2_0, SDFIO_TYPE_INT32, 1e300);
    const int32_t large[2] = {1, 2};
    float narrow[2];
    CHECK_STATUS(SDFIO_ERR_RANGE,
                 sdfio_decode_heights(&scaled,
                                      sdfio_layout_packed(SDFIO_TYPE_FLOAT),
                                      large, 2, narrow));

    /* The minimum of a binary32 marks the point. */
    const sdfio_header binary =
        description(SDFIO_ISO_2_0, SDFIO_TYPE_FLOAT, 1.0);
    const float floats[] = {1.5f, -FLT_MAX, FLT_MAX};
    double back[3];
    CHECK_OK(sdfio_decode_heights(
        &binary, sdfio_layout_packed(SDFIO_TYPE_DOUBLE), floats, 3, back));
    CHECK(back[0] == 1.5 && isnan(back[1]) && back[2] == (double)FLT_MAX);

    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_decode_heights(NULL,
                                      sdfio_layout_packed(SDFIO_TYPE_DOUBLE),
                                      coded, 4, doubles));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_decode_heights(&header,
                                      sdfio_layout_packed((sdfio_data_type)99),
                                      coded, 4, doubles));
    CHECK_OK(sdfio_decode_heights(
        &header, sdfio_layout_packed(SDFIO_TYPE_DOUBLE), NULL, 0, NULL));
}

static void check_encode(void)
{
    const sdfio_header header =
        description(SDFIO_ISO_2_0, SDFIO_TYPE_INT16, 1e-9);
    const double heights[] = {1.004e-7, -2e-7, NAN, INFINITY, -9999.0};
    sdfio_layout layout = sdfio_layout_packed(SDFIO_TYPE_DOUBLE);
    layout.missing = -9999.0;
    int16_t coded[5];
    CHECK_OK(sdfio_encode_heights(&header, layout, heights, 5, coded));
    CHECK(coded[0] == 100 && coded[1] == -200);
    CHECK(coded[2] == INT16_MIN && coded[3] == INT16_MIN
          && coded[4] == INT16_MIN);

    const double too_high[] = {1.0};
    CHECK_STATUS(SDFIO_ERR_RANGE,
                 sdfio_encode_heights(&header,
                                      sdfio_layout_packed(SDFIO_TYPE_DOUBLE),
                                      too_high, 1, coded));
    const double marker[] = {-32768e-9};
    CHECK_STATUS(SDFIO_ERR_BAD_VALUE,
                 sdfio_encode_heights(&header,
                                      sdfio_layout_packed(SDFIO_TYPE_DOUBLE),
                                      marker, 1, coded));

    /* The maximum is a height. */
    const double top[] = {32767e-9};
    CHECK_OK(sdfio_encode_heights(
        &header, sdfio_layout_packed(SDFIO_TYPE_DOUBLE), top, 1, coded));
    CHECK(coded[0] == INT16_MAX);

    /* Without a z-scale there is no way to a count. */
    const sdfio_header unscaled =
        description(SDFIO_ISO_2_0, SDFIO_TYPE_INT16, SDFIO_AUTO_Z_SCALE);
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_encode_heights(&unscaled,
                                      sdfio_layout_packed(SDFIO_TYPE_DOUBLE),
                                      too_high, 1, coded));

    /* A float element holds a float file, and is checked against its range. */
    const sdfio_header single =
        description(SDFIO_ISO_2_0, SDFIO_TYPE_FLOAT, 1.0);
    const double large[] = {1e300};
    float floats[1];
    CHECK_STATUS(SDFIO_ERR_RANGE,
                 sdfio_encode_heights(&single,
                                      sdfio_layout_packed(SDFIO_TYPE_DOUBLE),
                                      large, 1, floats));
}

/* Values go through in chunks, so the sizes around one chunk are tried. */
static void check_chunks(void)
{
    const sdfio_header header =
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
        CHECK_OK(sdfio_encode_heights(&header,
                                      sdfio_layout_packed(SDFIO_TYPE_DOUBLE),
                                      heights, count, coded));
        CHECK_OK(sdfio_decode_heights(&header,
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
    const float heights[] = {0.5f, NAN, -2.0f, 9999.1f, 3.0f, 1.0f};
    /* A marker of a double literal for the floats that hold 9999.1f. */
    sdfio_layout layout = sdfio_layout_packed(SDFIO_TYPE_FLOAT);
    layout.missing = 9999.1;
    double z_scale = 0.0;
    /* Two rows of three, the second one read bottom up. */
    layout.row_stride = -(ptrdiff_t)(3 * sizeof(float));
    CHECK_OK(sdfio_suggest_z_scale(SDFIO_TYPE_INT16, layout, heights + 3, 2, 3,
                                   &z_scale));
    CHECK(near_relative(z_scale, 3.0 / 32767.0, 1e-12));
    /* Without the marker the large height sets the scale. */
    layout.missing = NAN;
    CHECK_OK(sdfio_suggest_z_scale(SDFIO_TYPE_INT16, layout, heights + 3, 2, 3,
                                   &z_scale));
    CHECK(z_scale > 0.3);
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

/* A float buffer works with a marker that is a double literal: it is taken in
 * the precision of the buffer in both directions. */
static void check_float_marker(void)
{
    const sdfio_header header =
        description(SDFIO_ISO_2_0, SDFIO_TYPE_INT16, 1e-3);
    sdfio_layout layout = sdfio_layout_packed(SDFIO_TYPE_FLOAT);
    layout.missing = -9999.1;
    const float heights[] = {1.0f, -9999.1f, NAN, 2.0f};
    int16_t coded[4];
    CHECK_OK(sdfio_encode_heights(&header, layout, heights, 4, coded));
    CHECK(coded[0] == 1000 && coded[1] == INT16_MIN && coded[2] == INT16_MIN
          && coded[3] == 2000);
    float back[4];
    CHECK_OK(sdfio_decode_heights(&header, layout, coded, 4, back));
    /* A variable has the precision of its type, a constant may have more. */
    const float marker = -9999.1f;
    CHECK(back[0] == 1.0f && back[1] == marker && back[2] == marker
          && back[3] == 2.0f);
}

static void check_tables(void)
{
    CHECK(SDFIO_BAD_INT8 == -128 && SDFIO_BAD_INT16 == -32768);
    CHECK(SDFIO_BAD_INT32 == -2147483647 - 1);
    CHECK(SDFIO_BAD_FLOAT == -FLT_MAX && SDFIO_BAD_DOUBLE == -DBL_MAX);
    CHECK(sdfio_data_type_size(SDFIO_TYPE_INT8) == 1);
    CHECK(sdfio_data_type_size((sdfio_data_type)0) == 0);
    CHECK(!sdfio_valid_data_type((sdfio_data_type)2));
    CHECK(sdfio_data_type_size(SDFIO_TYPE_FLOAT) == 4);
    CHECK(sdfio_data_type_size(SDFIO_TYPE_DOUBLE) == 8);
    CHECK(sdfio_data_type_size(SDFIO_TYPE_FLOAT) == 4);
    CHECK(sdfio_data_type_size(SDFIO_TYPE_DOUBLE) == 8);
    CHECK(sdfio_data_type_size((sdfio_data_type)99) == 0);
    CHECK(sdfio_revision_allows(SDFIO_ISO_1_0, SDFIO_TYPE_INT16));
    CHECK(!sdfio_revision_allows(SDFIO_ISO_1_0, SDFIO_TYPE_INT8));
    CHECK(strlen(sdfio_version()) > 0);
    for (int i = SDFIO_OK; i <= SDFIO_ERR_MEMORY; ++i)
    {
        CHECK(strlen(sdfio_status_string((sdfio_status)i)) > 0);
    }
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
    const sdfio_header header =
        description(SDFIO_ISO_2_0, SDFIO_TYPE_INT16, 1e-3);
    const int16_t coded[4] = {1000, INT16_MIN, -2000, 3000};

    /* Decoded into the z of points: the rest of a point is left alone. */
    point points[4];
    for (size_t i = 0; i < 4; ++i)
    {
        points[i] = (point){1.0F, 2.0F, 99.0F, 4.0F};
    }
    sdfio_layout layout = sdfio_layout_packed(SDFIO_TYPE_FLOAT);
    layout.element_stride = sizeof(point);
    CHECK_OK(sdfio_decode_heights(&header, layout, coded, 4, &points[0].z));
    CHECK(points[0].z == 1.0F && isnan(points[1].z) && points[2].z == -2.0F
          && points[3].z == 3.0F);
    for (size_t i = 0; i < 4; ++i)
    {
        CHECK(points[i].x == 1.0F && points[i].y == 2.0F
              && points[i].w == 4.0F);
    }

    /* Encoded from them. */
    int16_t back[4];
    CHECK_OK(sdfio_encode_heights(&header, layout, &points[0].z, 4, back));
    CHECK(back[0] == 1000 && back[1] == INT16_MIN && back[2] == -2000
          && back[3] == 3000);

    /* The range of the z alone, whatever the other members say. */
    points[0].x = 1e30F;
    points[3].w = -1e30F;
    double z_scale = 0.0;
    CHECK_OK(sdfio_suggest_z_scale(SDFIO_TYPE_INT16, layout, &points[0].z, 1, 4,
                                   &z_scale));
    CHECK(near_relative(z_scale, 3.0 / 32767.0, 1e-9));

    /* A negative stride runs backwards from the pointer. */
    float mirrored[4] = {0};
    layout = sdfio_layout_packed(SDFIO_TYPE_FLOAT);
    layout.element_stride = -(ptrdiff_t)sizeof(float);
    CHECK_OK(sdfio_decode_heights(&header, layout, coded, 4, &mirrored[3]));
    CHECK(mirrored[3] == 1.0F && isnan(mirrored[2]) && mirrored[1] == -2.0F
          && mirrored[0] == 3.0F);

    /* A stride that is smaller than a element would overwrite itself. */
    layout = sdfio_layout_packed(SDFIO_TYPE_FLOAT);
    layout.element_stride = 2;
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_decode_heights(&header, layout, coded, 4, mirrored));
    CHECK_OK(sdfio_decode_heights(&header, layout, coded, 1, mirrored));
}

/* The check of a layout whose row stride is given separately. */
static sdfio_status layout_with(sdfio_layout layout, size_t rows,
                                size_t columns, ptrdiff_t row_stride)
{
    layout.row_stride = row_stride;
    return sdfio_check_layout(&layout, rows, columns, NULL);
}

static void check_layout(void)
{
    sdfio_layout layout = sdfio_layout_packed(SDFIO_TYPE_DOUBLE);
    const ptrdiff_t size = sizeof(double);
    /* Row-major, with padding at the end of the rows. */
    CHECK_OK(layout_with(layout, 3, 4, 5 * size));
    /* Column-major: the columns lie beside each other. */
    layout.element_stride = 3 * size;
    CHECK_OK(layout_with(layout, 3, 4, size));
    /* Interleaved: one of three channels. */
    layout.element_stride = 3 * size;
    CHECK_OK(layout_with(layout, 3, 4, 12 * size));
    /* Mirrored. */
    layout.element_stride = -size;
    CHECK_OK(layout_with(layout, 3, 4, -4 * size));
    /* Overlapping: both strides a element. */
    layout.element_stride = 0;
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, layout_with(layout, 3, 4, size));
    /* Rows that interleave but do not fit: a stride between. */
    layout.element_stride = 2 * size;
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, layout_with(layout, 3, 4, 3 * size));
    /* A matrix of one row or column needs only one of the strides. */
    layout.element_stride = 0;
    CHECK_OK(layout_with(layout, 1, 4, 0));
    layout = sdfio_layout_packed(SDFIO_TYPE_DOUBLE);
    layout.element_stride = 0;
    CHECK_OK(layout_with(layout, 4, 1, size));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, layout_with(layout, 4, 1, 3));
    /* Not countable. */
    layout.element_stride = PTRDIFF_MAX;
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 layout_with(layout, 2, SIZE_MAX / 2, PTRDIFF_MAX));
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

enum
{
    ALL_VALUES = 1U << 16,
    RANDOM_HEIGHTS = 20000
};

/* The stored values of a data type: every one of the 8-bit and of the 16-bit
 * integers, and random numbers of the others. */
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

/* Two floats that are the same, or are a few steps apart. */
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

/* A float calculation gives the heights that the calculation in double gives,
 * to the last digit of a float, and a few digits for the types that a float
 * does not hold. */
static int decode_steps(sdfio_data_type type)
{
    return type == SDFIO_TYPE_INT32 || type == SDFIO_TYPE_DOUBLE ? 2 : 0;
}

/* The stored values are the same, or close: a quotient that is half way
 * between two numbers, within the accuracy of the scale, falls to either, and
 * a float has 24 bits for the 32 of an integer. */
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

/* The calculation in float gives the heights, and the data, that the
 * calculation in double gives. */
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
                const sdfio_header header =
                    description(revisions[r], types[t], scales[s]);
                const size_t count = stored_values(types[t], &state, bytes);
                CHECK(count <= ALL_VALUES);
                const sdfio_status decoded_single = sdfio_decode_heights(
                    &header, sdfio_layout_packed(SDFIO_TYPE_FLOAT), bytes,
                    count, singles);
                const sdfio_status decoded_double = sdfio_decode_heights(
                    &header, sdfio_layout_packed(SDFIO_TYPE_DOUBLE), bytes,
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
                        &header, sdfio_layout_packed(SDFIO_TYPE_FLOAT), &height,
                        1, coded_single);
                    const sdfio_status wide_status = sdfio_encode_heights(
                        &header, sdfio_layout_packed(SDFIO_TYPE_DOUBLE),
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
    check_float_marker();
    check_element_stride();
    check_layout();
    check_tables();
    check_float_path();
    return finish_tests();
}
