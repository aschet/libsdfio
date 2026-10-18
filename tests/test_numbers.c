/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* Numbers as text, also where the locale writes a decimal comma. */

#include "check.h"

#include "internal.h"

#include <locale.h>

static bool parses(const sdfi_point *point, const char *text, double expected)
{
    double value = 0.0;
    return sdfi_parse_number(point, text, strlen(text), &value)
           && (value == expected || (isnan(value) && isnan(expected)));
}

static bool refuses(const sdfi_point *point, const char *text)
{
    double value = 0.0;
    return !sdfi_parse_number(point, text, strlen(text), &value);
}

static bool formats(const sdfi_point *point, double value, int digits,
                    int exponent, const char *expected)
{
    char text[SDFI_NUMBER_CAPACITY];
    sdfi_format_scientific(point, text, sizeof text, value, digits, exponent);
    return strcmp(text, expected) == 0;
}

static void check_numbers(const sdfi_point *point)
{
    CHECK(parses(point, "1.5", 1.5));
    CHECK(parses(point, "-1.5e-3", -1.5e-3));
    CHECK(parses(point, "  2 ", 2.0));
    CHECK(parses(point, "+1", 1.0));
    CHECK(parses(point, ".5", 0.5));
    CHECK(parses(point, "5.", 5.0));
    CHECK(parses(point, "1E3", 1000.0));
    CHECK(parses(point, "inf", INFINITY));
    CHECK(parses(point, "-Infinity", -INFINITY));
    CHECK(parses(point, "NaN", NAN));
    CHECK(parses(point, "1e400", INFINITY));
    CHECK(parses(point, "1e-400", 0.0));
    CHECK(refuses(point, ""));
    CHECK(refuses(point, "e5"));
    CHECK(refuses(point, "1e"));
    CHECK(refuses(point, "1.2.3"));
    CHECK(refuses(point, "--1"));
    CHECK(refuses(point, "+-1"));
    CHECK(refuses(point, "0x10"));
    CHECK(refuses(point, "1,5"));
    CHECK(refuses(point, "12abc"));

    long long integer = 0;
    CHECK(sdfi_parse_integer("123", 3, &integer) && integer == 123);
    CHECK(sdfi_parse_integer("-123", 4, &integer) && integer == -123);
    CHECK(sdfi_parse_integer("+5", 2, &integer) && integer == 5);
    CHECK(sdfi_parse_integer("9223372036854775807", 19, &integer)
          && integer == LLONG_MAX);
    CHECK(sdfi_parse_integer("-9223372036854775808", 20, &integer)
          && integer == LLONG_MIN);
    CHECK(!sdfi_parse_integer("9223372036854775808", 19, &integer));
    CHECK(!sdfi_parse_integer("1.0", 3, &integer));
    CHECK(!sdfi_parse_integer("", 0, &integer));
    CHECK(!sdfi_parse_integer("-", 1, &integer));

    CHECK(formats(point, 1.5, 14, 3, "1.50000000000000e+000"));
    CHECK(formats(point, 1.5, 6, 2, "1.500000e+00"));
    CHECK(formats(point, 0.0, 6, 2, "0.000000e+00"));
    CHECK(formats(point, -0.0, 6, 2, "0.000000e+00"));
    CHECK(formats(point, -2.5e-300, 3, 3, "-2.500e-300"));
    CHECK(formats(point, 1e100, 1, 3, "1.0e+100"));
    CHECK(formats(point, 12345.0, 2, 2, "1.23e+04"));

    /* What is printed is read back. */
    uint32_t state = 12345u;
    for (int i = 0; i < 2000; ++i)
    {
        state = state * 1664525u + 1013904223u;
        const double value =
            ((double)state / 4294967296.0 - 0.5)
            * pow(10.0, (double)((int)(state >> 24) % 40 - 20));
        char text[SDFI_NUMBER_CAPACITY];
        const size_t length =
            sdfi_format_scientific(point, text, sizeof text, value,
                                   SDFI_DOUBLE_DIGITS, SDFI_DOUBLE_EXPONENT);
        double back = 0.0;
        CHECK(sdfi_parse_number(point, text, length, &back)
              && near_relative(back, value, 1e-13));
    }
}

static void check_locale(void)
{
    const char *names[] = {"de_DE.utf8", "de_AT.utf8", "de_DE", "de_CH.utf8"};
    const char *chosen = NULL;
    for (size_t i = 0; i < sizeof names / sizeof names[0] && chosen == NULL;
         ++i)
    {
        chosen = setlocale(LC_NUMERIC, names[i]);
    }
    if (chosen == NULL)
    {
        printf("no locale with a decimal comma: skipped\n");
        return;
    }
    char probe[16];
    snprintf(probe, sizeof probe, "%.1f", 1.5);
    CHECK(strchr(probe, ',') != NULL);

    const sdfi_point point = sdfi_point_now();
    CHECK(strcmp(point.text, ",") == 0);
    check_numbers(&point);

    /* A file written and read in such a locale has the dots of the format. */
    sdf_info info;
    sdf_info_init(&info);
    info.storage = SDF_ASCII;
    info.columns = 3;
    info.rows = 1;
    info.x_scale = 1.5e-6;
    info.y_scale = 1.5e-6;
    const double heights[3] = {1.25e-6, NAN, -3.5e-7};
    membuf file = {0};
    const sdf_stream stream = membuf_stream(&file);
    sdf_writer *writer = sdf_writer_new();
    CHECK_OK(sdf_create_stream(writer, &stream, &info));
    CHECK_OK(sdf_write_heights(writer, sdf_metres(SDF_FLOAT64), heights,
                               sizeof heights));
    CHECK_OK(sdf_write_end(writer, NULL, 0));
    sdf_writer_free(writer);
    CHECK(memchr(file.data, ',', file.size) == NULL);

    sdf_reader *reader = sdf_reader_new();
    CHECK_OK(sdf_open_memory(reader, file.data, file.size));
    double back[3];
    CHECK_OK(
        sdf_read_heights(reader, sdf_metres(SDF_FLOAT64), back, sizeof back));
    CHECK(near_relative(back[0], 1.25e-6, 1e-12) && isnan(back[1])
          && near_relative(back[2], -3.5e-7, 1e-12));
    CHECK(sdf_get_info(reader)->x_scale == 1.5e-6);
    sdf_reader_free(reader);
    free(file.data);
    setlocale(LC_NUMERIC, "C");
}

int main(void)
{
    const sdfi_point point = sdfi_point_now();
    CHECK(strcmp(point.text, ".") == 0);
    check_numbers(&point);
    check_locale();
    return finish_tests();
}
