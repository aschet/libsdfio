/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* Numbers as text, also where the locale writes a decimal comma. */

#include "check.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "number_text.h"

#include <locale.h>

static bool parses(const sdfio_decimal_point *decimal_point, const char *text,
                   double expected)
{
    double value = 0.0;
    return sdfio_parse_number(decimal_point, text, strlen(text), &value)
           && (value == expected || (isnan(value) && isnan(expected)));
}

static bool refuses(const sdfio_decimal_point *decimal_point, const char *text)
{
    double value = 0.0;
    return !sdfio_parse_number(decimal_point, text, strlen(text), &value);
}

static bool formats(const sdfio_decimal_point *decimal_point, double value,
                    int digits, int exponent, const char *expected)
{
    char text[SDFIO_NUMBER_CAPACITY];
    sdfio_format_scientific(decimal_point, text, sizeof text, value, digits,
                            exponent);
    return strcmp(text, expected) == 0;
}

static void check_numbers(const sdfio_decimal_point *decimal_point)
{
    CHECK(parses(decimal_point, "1.5", 1.5));
    CHECK(parses(decimal_point, "-1.5e-3", -1.5e-3));
    CHECK(parses(decimal_point, "  2 ", 2.0));
    CHECK(parses(decimal_point, "+1", 1.0));
    CHECK(parses(decimal_point, ".5", 0.5));
    CHECK(parses(decimal_point, "5.", 5.0));
    CHECK(parses(decimal_point, "1E3", 1000.0));
    CHECK(refuses(decimal_point, "inf"));
    CHECK(refuses(decimal_point, "NaN"));
    CHECK(parses(decimal_point, "1e400", INFINITY));
    CHECK(parses(decimal_point, "1e-400", 0.0));
    CHECK(refuses(decimal_point, ""));
    CHECK(refuses(decimal_point, "e5"));
    CHECK(refuses(decimal_point, "1e"));
    CHECK(refuses(decimal_point, "1.2.3"));
    CHECK(refuses(decimal_point, "--1"));
    CHECK(refuses(decimal_point, "+-1"));
    CHECK(refuses(decimal_point, "0x10"));
    CHECK(refuses(decimal_point, "1,5"));
    CHECK(refuses(decimal_point, "12abc"));

    long long integer = 0;
    CHECK(sdfio_parse_integer("123", 3, &integer) && integer == 123);
    CHECK(sdfio_parse_integer("-123", 4, &integer) && integer == -123);
    CHECK(sdfio_parse_integer("+5", 2, &integer) && integer == 5);
    CHECK(sdfio_parse_integer("9223372036854775807", 19, &integer)
          && integer == LLONG_MAX);
    CHECK(sdfio_parse_integer("-9223372036854775808", 20, &integer)
          && integer == LLONG_MIN);
    CHECK(!sdfio_parse_integer("9223372036854775808", 19, &integer));
    CHECK(!sdfio_parse_integer("1.0", 3, &integer));
    CHECK(!sdfio_parse_integer("", 0, &integer));
    CHECK(!sdfio_parse_integer("-", 1, &integer));

    CHECK(formats(decimal_point, 1.5, 14, 3, "1.50000000000000e+000"));
    CHECK(formats(decimal_point, 1.5, 6, 2, "1.500000e+00"));
    CHECK(formats(decimal_point, 0.0, 6, 2, "0.000000e+00"));
    CHECK(formats(decimal_point, -0.0, 6, 2, "0.000000e+00"));
    CHECK(formats(decimal_point, -2.5e-300, 3, 3, "-2.500e-300"));
    CHECK(formats(decimal_point, 1e100, 1, 3, "1.0e+100"));
    CHECK(formats(decimal_point, 12345.0, 2, 2, "1.23e+04"));

    /* What is printed is read back. */
    uint32_t state = 12345u;
    for (int i = 0; i < 2000; ++i)
    {
        state = state * 1664525u + 1013904223u;
        const double value =
            ((double)state / 4294967296.0 - 0.5)
            * pow(10.0, (double)((int)(state >> 24) % 40 - 20));
        char text[SDFIO_NUMBER_CAPACITY];
        const size_t length =
            sdfio_format_scientific(decimal_point, text, sizeof text, value,
                                    SDFIO_DOUBLE_DIGITS, SDFIO_DOUBLE_EXPONENT);
        double back = 0.0;
        CHECK(sdfio_parse_number(decimal_point, text, length, &back)
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

    const sdfio_decimal_point decimal_point = sdfio_decimal_point_now();
    CHECK(strcmp(decimal_point.text, ",") == 0);
    check_numbers(&decimal_point);

    /* A file written and read in such a locale has the dots of the format. */
    sdfio_header header;
    sdfio_header_init(&header);
    header.representation = SDFIO_ASCII;
    header.data_type = SDFIO_TYPE_DOUBLE;
    header.columns = 3;
    header.rows = 1;
    header.x_scale = 1.5e-6;
    header.y_scale = 1.5e-6;
    const double heights[3] = {1.25e-6, NAN, -3.5e-7};
    membuf file = {0};
    const sdfio_stream stream = membuf_stream(&file);
    sdfio_writer *writer = sdfio_writer_new(all_formats());
    CHECK_OK(sdfio_create_stream(writer, &stream, &header));
    CHECK_OK(sdfio_write_heights(writer, sdfio_layout_packed(SDFIO_TYPE_DOUBLE),
                                 heights));
    CHECK_OK(sdfio_end_trailer(writer));
    sdfio_writer_free(writer);
    CHECK(memchr(file.data, ',', file.size) == NULL);

    sdfio_reader *reader = sdfio_reader_new(all_formats());
    CHECK_OK(sdfio_open_memory(reader, file.data, file.size));
    double back[3];
    CHECK_OK(sdfio_read_heights(reader, sdfio_layout_packed(SDFIO_TYPE_DOUBLE),
                                back));
    CHECK(near_relative(back[0], 1.25e-6, 1e-12) && isnan(back[1])
          && near_relative(back[2], -3.5e-7, 1e-12));
    /* A variable has the precision of its type, a constant may have more. */
    const double x_scale = 1.5e-6;
    CHECK(sdfio_get_header(reader)->x_scale == x_scale);
    sdfio_reader_free(reader);
    free(file.data);
    setlocale(LC_NUMERIC, "C");
}

int main(void)
{
    const sdfio_decimal_point decimal_point = sdfio_decimal_point_now();
    CHECK(strcmp(decimal_point.text, ".") == 0);
    check_numbers(&decimal_point);
    check_locale();
    return finish_tests();
}
