/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* The trailer as fields: composing, reading, listing and checking them. */

#include "check.h"

#include "internal.h"

#include <locale.h>

enum
{
    CAPACITY = 512
};

static bool same(const char *text, size_t size, const char *expected)
{
    return size == strlen(expected) && memcmp(text, expected, size) == 0;
}

static void check_put_and_get(void)
{
    char buffer[CAPACITY];
    size_t size = 0;
    CHECK_OK(sdfio_trailer_put_text(buffer, sizeof buffer, &size,
                                    "OperatorName", "Jane Doe"));
    CHECK_OK(
        sdfio_trailer_put_integer(buffer, sizeof buffer, &size, "Scans", -42));
    CHECK_OK(sdfio_trailer_put_real(buffer, sizeof buffer, &size, "FieldOfView",
                                    1.5e-3));
    CHECK_OK(sdfio_trailer_put_text(buffer, sizeof buffer, &size, "Empty", ""));
    CHECK(size == strlen(buffer));
    CHECK(
        strncmp(buffer, "OperatorName = Jane Doe\nScans = -42\nFieldOfView = ",
                strlen("OperatorName = Jane Doe\nScans = -42\nFieldOfView = "))
        == 0);

    const char *value = NULL;
    size_t value_size = 0;
    CHECK(sdfio_trailer_find(buffer, size, "operatorname", &value, &value_size)
          && same(value, value_size, "Jane Doe"));
    CHECK(sdfio_trailer_find(buffer, size, "Empty", &value, &value_size)
          && value_size == 0);

    long long scans = 0;
    CHECK_OK(sdfio_trailer_get_integer(buffer, size, "SCANS", &scans));
    CHECK(scans == -42);
    double view = 0.0;
    CHECK_OK(sdfio_trailer_get_real(buffer, size, "FieldOfView", &view));
    CHECK(view == 1.5e-3);
    /* An integer is a number too. */
    CHECK_OK(sdfio_trailer_get_real(buffer, size, "Scans", &view));
    CHECK(view == -42.0);
}

static void check_integers_and_reals_exactly(void)
{
    const long long integers[] = {0, 1, -1, LLONG_MAX, LLONG_MIN};
    for (size_t i = 0; i < sizeof integers / sizeof integers[0]; ++i)
    {
        char buffer[CAPACITY];
        size_t size = 0;
        long long back = 7;
        CHECK_OK(sdfio_trailer_put_integer(buffer, sizeof buffer, &size, "N",
                                           integers[i]));
        CHECK_OK(sdfio_trailer_get_integer(buffer, size, "N", &back));
        CHECK(back == integers[i]);
    }
    /* What is written is read back to the last bit. */
    uint32_t state = 777u;
    for (int round = 0; round < 3000; ++round)
    {
        state = state * 1664525u + 1013904223u;
        const double value =
            ((double)state / 4294967296.0 - 0.5)
            * pow(10.0, (double)((int)(state >> 24) % 600 - 300));
        char buffer[CAPACITY];
        size_t size = 0;
        double back = 0.0;
        CHECK_OK(
            sdfio_trailer_put_real(buffer, sizeof buffer, &size, "X", value));
        CHECK_OK(sdfio_trailer_get_real(buffer, size, "X", &back));
        CHECK(back == value);
    }
    const double extremes[] = {DBL_MAX, -DBL_MAX, DBL_MIN, 4.9e-324,
                               0.0,     1.0,      0.1};
    for (size_t i = 0; i < sizeof extremes / sizeof extremes[0]; ++i)
    {
        char buffer[CAPACITY];
        size_t size = 0;
        double back = -1.0;
        CHECK_OK(sdfio_trailer_put_real(buffer, sizeof buffer, &size, "X",
                                        extremes[i]));
        CHECK_OK(sdfio_trailer_get_real(buffer, size, "X", &back));
        CHECK(back == extremes[i]);
    }
}

static void check_the_buffer(void)
{
    char buffer[CAPACITY];
    size_t size = 0;
    /* The line, its LF and the 0 after it have to fit: "A = b\n" is 6. */
    char small[7];
    CHECK_OK(sdfio_trailer_put_text(small, sizeof small, &size, "A", "b"));
    CHECK(size == 6 && strcmp(small, "A = b\n") == 0);

    char tight[6];
    size = 0;
    CHECK_STATUS(SDFIO_ERR_RANGE,
                 sdfio_trailer_put_text(tight, sizeof tight, &size, "A", "b"));
    CHECK(size == 0);

    /* A line that does not fit leaves what is there as it was. */
    char room[20];
    size = 0;
    CHECK_OK(sdfio_trailer_put_text(room, sizeof room, &size, "A", "b"));
    const size_t before = size;
    CHECK_STATUS(SDFIO_ERR_RANGE,
                 sdfio_trailer_put_text(room, sizeof room, &size, "Long",
                                        "a value that is too long to fit"));
    CHECK(size == before && strcmp(room, "A = b\n") == 0);

    size = 0;
    CHECK_STATUS(SDFIO_ERR_RANGE,
                 sdfio_trailer_put_text(NULL, 0, &size, "A", "b"));
    /* A size beyond the capacity is not a size of this buffer. */
    size = sizeof buffer + 1;
    CHECK_STATUS(
        SDFIO_ERR_ARGUMENT,
        sdfio_trailer_put_text(buffer, sizeof buffer, &size, "A", "b"));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_trailer_put_text(buffer, sizeof buffer, NULL, "A", "b"));
    size = 0;
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_trailer_put_text(NULL, 10, &size, "A", "b"));
    CHECK_STATUS(
        SDFIO_ERR_ARGUMENT,
        sdfio_trailer_put_text(buffer, sizeof buffer, &size, NULL, "b"));
    CHECK_STATUS(
        SDFIO_ERR_ARGUMENT,
        sdfio_trailer_put_text(buffer, sizeof buffer, &size, "A", NULL));
}

static void check_what_a_line_may_be(void)
{
    char buffer[CAPACITY];
    size_t size = 0;
    const char *refused_names[] = {
        "", "two words", "a=b", "A;", "Caf\xC3\xA9", "tab\t", "line\n"};
    for (size_t i = 0; i < sizeof refused_names / sizeof refused_names[0]; ++i)
    {
        CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                     sdfio_trailer_put_text(buffer, sizeof buffer, &size,
                                            refused_names[i], "v"));
    }
    const char *refused_values[] = {"two\nlines", "one\rtwo", "Caf\xC3\xA9"};
    for (size_t i = 0; i < sizeof refused_values / sizeof refused_values[0];
         ++i)
    {
        CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                     sdfio_trailer_put_text(buffer, sizeof buffer, &size, "A",
                                            refused_values[i]));
    }
    CHECK_STATUS(
        SDFIO_ERR_ARGUMENT,
        sdfio_trailer_put_real(buffer, sizeof buffer, &size, "A", NAN));
    CHECK_STATUS(
        SDFIO_ERR_ARGUMENT,
        sdfio_trailer_put_real(buffer, sizeof buffer, &size, "A", INFINITY));
    CHECK(size == 0);
    /* Digits, letters and the underscore make a name. */
    CHECK_OK(
        sdfio_trailer_put_text(buffer, sizeof buffer, &size, "Part_2b", "x"));
}

static void check_get_errors(void)
{
    const char trailer[] = "Name = Jane\nCount = twelve\nHalf = 1.5\nBig = "
                           "99999999999999999999\nPad =   17  \nAgain = 1\n"
                           "again = 2\n";
    const size_t size = strlen(trailer);
    long long integer = 0;
    double real = 0.0;
    CHECK_STATUS(SDFIO_ERR_NOT_FOUND,
                 sdfio_trailer_get_integer(trailer, size, "Missing", &integer));
    CHECK_STATUS(SDFIO_ERR_NOT_FOUND,
                 sdfio_trailer_get_real(trailer, size, "Missing", &real));
    CHECK_STATUS(SDFIO_ERR_FORMAT,
                 sdfio_trailer_get_integer(trailer, size, "Name", &integer));
    CHECK_STATUS(SDFIO_ERR_FORMAT,
                 sdfio_trailer_get_integer(trailer, size, "Count", &integer));
    CHECK_STATUS(SDFIO_ERR_FORMAT,
                 sdfio_trailer_get_integer(trailer, size, "Half", &integer));
    CHECK_STATUS(SDFIO_ERR_FORMAT,
                 sdfio_trailer_get_integer(trailer, size, "Big", &integer));
    CHECK_STATUS(SDFIO_ERR_FORMAT,
                 sdfio_trailer_get_real(trailer, size, "Name", &real));
    CHECK_OK(sdfio_trailer_get_real(trailer, size, "Half", &real));
    CHECK(real == 1.5);
    /* White space around a value is not part of it, and the last field wins. */
    CHECK_OK(sdfio_trailer_get_integer(trailer, size, "Pad", &integer));
    CHECK(integer == 17);
    CHECK_OK(sdfio_trailer_get_integer(trailer, size, "Again", &integer));
    CHECK(integer == 2);
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_trailer_get_integer(NULL, size, "Pad", &integer));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_trailer_get_integer(trailer, size, NULL, &integer));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_trailer_get_integer(trailer, size, "Pad", NULL));
    CHECK_STATUS(SDFIO_ERR_NOT_FOUND,
                 sdfio_trailer_get_integer("", 0, "Pad", &integer));
}

static void check_listing(void)
{
    /* All three line ends, blank lines and a line that is not a field. */
    const char trailer[] = "A = 1\r\nB=two\n\nnot a field\rC = 3 \n  D  =  4";
    const char *const names[] = {"A", "B", "C", "D"};
    const char *const values[] = {"1", "two", "3", "4"};
    size_t position = 0;
    size_t count = 0;
    sdfio_field field;
    while (sdfio_trailer_next(trailer, strlen(trailer), &position, &field))
    {
        CHECK(count < 4);
        if (count < 4)
        {
            CHECK(same(field.name, field.name_size, names[count]));
            CHECK(same(field.value, field.value_size, values[count]));
        }
        ++count;
    }
    CHECK(count == 4);
    CHECK(position == strlen(trailer));
    /* At the end it stays at the end. */
    CHECK(!sdfio_trailer_next(trailer, strlen(trailer), &position, &field));

    position = 0;
    CHECK(!sdfio_trailer_next("", 0, &position, &field));
    CHECK(!sdfio_trailer_next("only text", 9, &position, &field));
    CHECK(!sdfio_trailer_next(NULL, 0, &position, &field));
    CHECK(!sdfio_trailer_next(trailer, strlen(trailer), NULL, &field));
    CHECK(!sdfio_trailer_next(trailer, strlen(trailer), &position, NULL));
}

static void check_the_check(void)
{
    const char tagged[] = "A = 1\n\nB = two\r\n";
    const char free_text[] = "A free text that is not a field";
    const char not_ascii[] = "A = caf\xC3\xA9";
    CHECK_OK(sdfio_check_trailer(SDFIO_ISO_2_0, tagged, strlen(tagged)));
    CHECK_OK(sdfio_check_trailer(SDFIO_ISO_2_0, NULL, 0));
    CHECK_OK(sdfio_check_trailer(SDFIO_ISO_2_0, "", 0));
    CHECK_STATUS(
        SDFIO_ERR_ARGUMENT,
        sdfio_check_trailer(SDFIO_ISO_2_0, free_text, strlen(free_text)));
    /* ISO-1.0 and BCR-1.0 take any 7-bit text. */
    CHECK_OK(sdfio_check_trailer(SDFIO_ISO_1_0, free_text, strlen(free_text)));
    CHECK_OK(sdfio_check_trailer(SDFIO_BCR_1_0, free_text, strlen(free_text)));
    for (int r = SDFIO_ISO_1_0; r <= SDFIO_BCR_1_0; ++r)
    {
        CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                     sdfio_check_trailer((sdfio_revision)r, not_ascii,
                                         strlen(not_ascii)));
    }
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_check_trailer(SDFIO_ISO_2_0, NULL, 5));
    CHECK_STATUS(
        SDFIO_ERR_ARGUMENT,
        sdfio_check_trailer((sdfio_revision)99, tagged, strlen(tagged)));
}

/* A trailer composed with the helpers is a trailer that can be written, and
 * what a file gives back is read as typed fields. */
static void check_through_a_file(void)
{
    char trailer[CAPACITY];
    size_t trailer_size = 0;
    CHECK_OK(sdfio_trailer_put_text(trailer, sizeof trailer, &trailer_size,
                                    "OperatorName", "Jane Doe"));
    CHECK_OK(sdfio_trailer_put_integer(trailer, sizeof trailer, &trailer_size,
                                       "Averages", 16));
    CHECK_OK(sdfio_trailer_put_real(trailer, sizeof trailer, &trailer_size,
                                    "Magnification", 20.5));

    const sdfio_representation representations[] = {SDFIO_BINARY, SDFIO_ASCII};
    for (size_t i = 0; i < 2; ++i)
    {
        sdfio_header header;
        sdfio_header_init(&header);
        header.representation = representations[i];
        header.columns = 2;
        header.rows = 2;
        const float heights[4] = {1e-6F, 2e-6F, 3e-6F, 4e-6F};
        /* Checked before the first row. */
        CHECK_OK(sdfio_check_trailer(header.revision, trailer, trailer_size));
        membuf file = {0};
        const sdfio_stream stream = membuf_stream(&file);
        sdfio_writer *writer = sdfio_writer_new();
        CHECK_OK(sdfio_create_stream(writer, &stream, &header));
        CHECK_OK(sdfio_write_heights(writer, sdfio_metres(SDFIO_FLOAT32),
                                     heights, 2 * sizeof(float)));
        CHECK_OK(sdfio_write_trailer(writer, trailer, trailer_size));
        sdfio_writer_free(writer);

        sdfio_reader *reader = sdfio_reader_new();
        CHECK_OK(sdfio_open_memory(reader, file.data, file.size));
        const char *read = NULL;
        size_t read_size = 0;
        CHECK_OK(sdfio_read_trailer(reader, &read, &read_size));
        long long averages = 0;
        double magnification = 0.0;
        const char *name = NULL;
        size_t name_size = 0;
        CHECK_OK(
            sdfio_trailer_get_integer(read, read_size, "Averages", &averages));
        CHECK_OK(sdfio_trailer_get_real(read, read_size, "Magnification",
                                        &magnification));
        CHECK(sdfio_trailer_find(read, read_size, "OperatorName", &name,
                                 &name_size)
              && same(name, name_size, "Jane Doe"));
        CHECK(averages == 16 && magnification == 20.5);
        /* The three lines, in order, as LF lines. */
        CHECK(read_size == trailer_size - 1
              && memcmp(read, trailer, read_size) == 0);
        sdfio_reader_free(reader);
        free(file.data);
    }
}

/* Numbers have a dot, also where the locale has a comma. */
static void check_the_locale(void)
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
    char buffer[CAPACITY];
    size_t size = 0;
    CHECK_OK(sdfio_trailer_put_real(buffer, sizeof buffer, &size, "X", 0.25));
    CHECK(strchr(buffer, ',') == NULL && strchr(buffer, '.') != NULL);
    double back = 0.0;
    CHECK_OK(sdfio_trailer_get_real(buffer, size, "X", &back));
    CHECK(back == 0.25);
    CHECK_OK(sdfio_trailer_get_real("X = 1.75", 8, "X", &back));
    CHECK(back == 1.75);
    CHECK_STATUS(SDFIO_ERR_FORMAT,
                 sdfio_trailer_get_real("X = 1,75", 8, "X", &back));
    setlocale(LC_NUMERIC, "C");
}

int main(void)
{
    check_put_and_get();
    check_integers_and_reals_exactly();
    check_the_buffer();
    check_what_a_line_may_be();
    check_get_errors();
    check_listing();
    check_the_check();
    check_through_a_file();
    check_the_locale();
    return finish_tests();
}
