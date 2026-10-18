/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* Assembling a trailer: the lines, the numbers, the buffer, and what a file
 * does with the text it is given. */

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

/* The value of a field of the assembled text, read with the parsers inside. */
static bool find(const sdfio_trailer *trailer, const char *name,
                 const char **value, size_t *size)
{
    return sdfi_tagged_find(trailer->text, trailer->size, name, value, size);
}

static void check_lines(void)
{
    char memory[CAPACITY];
    sdfio_trailer trailer;
    sdfio_trailer_init(&trailer, memory, sizeof memory);
    CHECK(trailer.size == 0 && trailer.capacity == sizeof memory
          && trailer.text == memory && memory[0] == '\0');
    CHECK_OK(sdfio_trailer_add_text(&trailer, "OperatorName", "Jane Doe"));
    CHECK_OK(sdfio_trailer_add_integer(&trailer, "Scans", -42));
    CHECK_OK(sdfio_trailer_add_real(&trailer, "FieldOfView", 1.5e-3));
    CHECK_OK(sdfio_trailer_add_real(&trailer, "Magnification", 20.5));
    CHECK_OK(sdfio_trailer_add_text(&trailer, "Empty", ""));
    CHECK(trailer.size == strlen(trailer.text));
    /* One line each, LF in memory, readable numbers. */
    CHECK(strcmp(trailer.text,
                 "OperatorName = Jane Doe\nScans = -42\n"
                 "FieldOfView = 0.0015\nMagnification = 20.5\nEmpty = \n")
          == 0);
}

static void check_numbers_read_back_exactly(void)
{
    const sdfi_point point = sdfi_point_now();
    const long long integers[] = {0, 1, -1, LLONG_MAX, LLONG_MIN};
    for (size_t i = 0; i < sizeof integers / sizeof integers[0]; ++i)
    {
        char memory[CAPACITY];
        sdfio_trailer trailer;
        sdfio_trailer_init(&trailer, memory, sizeof memory);
        CHECK_OK(sdfio_trailer_add_integer(&trailer, "N", integers[i]));
        const char *text = NULL;
        size_t size = 0;
        long long back = 7;
        CHECK(find(&trailer, "N", &text, &size)
              && sdfi_parse_integer(text, size, &back) && back == integers[i]);
    }
    uint32_t state = 777u;
    for (int round = 0; round < 3000; ++round)
    {
        state = state * 1664525u + 1013904223u;
        const double value =
            ((double)state / 4294967296.0 - 0.5)
            * pow(10.0, (double)((int)(state >> 24) % 600 - 300));
        char memory[CAPACITY];
        sdfio_trailer trailer;
        sdfio_trailer_init(&trailer, memory, sizeof memory);
        CHECK_OK(sdfio_trailer_add_real(&trailer, "X", value));
        const char *text = NULL;
        size_t size = 0;
        double back = 0.0;
        CHECK(find(&trailer, "X", &text, &size)
              && sdfi_parse_number(&point, text, size, &back) && back == value);
    }
    /* The text is the shortest that reads back: no run of digits. */
    const struct
    {
        double value;
        const char *text;
    } shortest[] = {
        {0.1, "0.1"},       {20.5, "20.5"},
        {-3.0, "-3"},       {1e-6, "1e-06"},
        {1.5e-3, "0.0015"}, {16.0, "16"},
        {0.0, "0"},         {-0.0, "0"},
        {1e21, "1e+21"},    {0.30000000000000004, "0.30000000000000004"}};
    for (size_t i = 0; i < sizeof shortest / sizeof shortest[0]; ++i)
    {
        char memory[CAPACITY];
        sdfio_trailer trailer;
        sdfio_trailer_init(&trailer, memory, sizeof memory);
        CHECK_OK(sdfio_trailer_add_real(&trailer, "X", shortest[i].value));
        const char *text = NULL;
        size_t size = 0;
        CHECK(find(&trailer, "X", &text, &size)
              && same(text, size, shortest[i].text));
    }
    const double extremes[] = {DBL_MAX, -DBL_MAX, DBL_MIN, 4.9e-324};
    for (size_t i = 0; i < sizeof extremes / sizeof extremes[0]; ++i)
    {
        char memory[CAPACITY];
        sdfio_trailer trailer;
        sdfio_trailer_init(&trailer, memory, sizeof memory);
        CHECK_OK(sdfio_trailer_add_real(&trailer, "X", extremes[i]));
        const char *text = NULL;
        size_t size = 0;
        double back = 0.0;
        CHECK(find(&trailer, "X", &text, &size)
              && sdfi_parse_number(&point, text, size, &back)
              && back == extremes[i]);
    }
}

static void check_the_buffer(void)
{
    sdfio_trailer trailer;
    /* "A = b\n" is 6 bytes and the 0 after it makes 7. */
    char exact[7];
    sdfio_trailer_init(&trailer, exact, sizeof exact);
    CHECK_OK(sdfio_trailer_add_text(&trailer, "A", "b"));
    CHECK(trailer.size == 6 && strcmp(exact, "A = b\n") == 0);
    CHECK_STATUS(SDFIO_ERR_RANGE, sdfio_trailer_add_text(&trailer, "C", ""));

    char tight[6];
    sdfio_trailer_init(&trailer, tight, sizeof tight);
    CHECK_STATUS(SDFIO_ERR_RANGE, sdfio_trailer_add_text(&trailer, "A", "b"));
    CHECK(trailer.size == 0 && tight[0] == '\0');

    /* A line that does not fit leaves what is there as it was. */
    char room[20];
    sdfio_trailer_init(&trailer, room, sizeof room);
    CHECK_OK(sdfio_trailer_add_text(&trailer, "A", "b"));
    CHECK_STATUS(SDFIO_ERR_RANGE,
                 sdfio_trailer_add_text(&trailer, "Long",
                                        "a value that is too long to fit"));
    CHECK(trailer.size == 6 && strcmp(room, "A = b\n") == 0);

    /* No memory, no room. */
    sdfio_trailer_init(&trailer, NULL, 100);
    CHECK(trailer.capacity == 0);
    CHECK_STATUS(SDFIO_ERR_RANGE, sdfio_trailer_add_text(&trailer, "A", "b"));
    sdfio_trailer_init(&trailer, room, 0);
    CHECK_STATUS(SDFIO_ERR_RANGE, sdfio_trailer_add_text(&trailer, "A", "b"));

    sdfio_trailer_init(NULL, room, sizeof room);
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_trailer_add_text(NULL, "A", "b"));
    sdfio_trailer_init(&trailer, room, sizeof room);
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_trailer_add_text(&trailer, NULL, "b"));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_trailer_add_text(&trailer, "A", NULL));
    /* A size beyond the capacity is not a state of this trailer. */
    trailer.size = sizeof room + 1;
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_trailer_add_text(&trailer, "A", "b"));
}

static void check_what_a_line_may_be(void)
{
    char memory[CAPACITY];
    sdfio_trailer trailer;
    sdfio_trailer_init(&trailer, memory, sizeof memory);
    const char *refused_names[] = {
        "", "two words", "a=b", "A;", "Caf\xC3\xA9", "tab\t", "line\n"};
    for (size_t i = 0; i < sizeof refused_names / sizeof refused_names[0]; ++i)
    {
        CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                     sdfio_trailer_add_text(&trailer, refused_names[i], "v"));
    }
    const char *refused_values[] = {"two\nlines", "one\rtwo", "Caf\xC3\xA9"};
    for (size_t i = 0; i < sizeof refused_values / sizeof refused_values[0];
         ++i)
    {
        CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                     sdfio_trailer_add_text(&trailer, "A", refused_values[i]));
    }
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_trailer_add_real(&trailer, "A", NAN));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_trailer_add_real(&trailer, "A", INFINITY));
    CHECK(trailer.size == 0);
    CHECK_OK(sdfio_trailer_add_text(&trailer, "Part_2b", "x"));
}

/* A file writes the text it is given: nothing about it is checked, so a
 * program that does not assemble its trailer is not stopped. */
static void check_the_text_is_written_as_given(void)
{
    const char *const trailers[] = {"free text, not a field",
                                    "Caf\xC3\xA9 = 1\xFF", "A = 1\n\n\nB",
                                    "no = field = twice"};
    const sdfio_revision revisions[] = {SDFIO_ISO_1_0, SDFIO_ISO_2_0,
                                        SDFIO_BCR_1_0};
    for (size_t t = 0; t < sizeof trailers / sizeof trailers[0]; ++t)
    {
        for (size_t r = 0; r < 3; ++r)
        {
            sdfio_header header;
            sdfio_header_init(&header);
            header.revision = revisions[r];
            header.data_type = SDFIO_DATA_INT16;
            header.z_scale = 1e-9;
            header.columns = 1;
            header.rows = 1;
            const double height = 1e-6;
            membuf file = {0};
            const sdfio_stream stream = membuf_stream(&file);
            sdfio_writer *writer = sdfio_writer_new();
            CHECK_OK(sdfio_create_stream(writer, &stream, &header));
            CHECK_OK(sdfio_write_heights(writer, sdfio_metres(SDFIO_FLOAT64),
                                         &height, sizeof height));
            CHECK_OK(
                sdfio_write_trailer(writer, trailers[t], strlen(trailers[t])));
            sdfio_writer_free(writer);

            sdfio_reader *reader = sdfio_reader_new();
            CHECK_OK(sdfio_open_memory(reader, file.data, file.size));
            const char *read = NULL;
            size_t size = 0;
            CHECK_OK(sdfio_read_trailer(reader, &read, &size));
            CHECK(read != NULL && same(read, size, trailers[t]));
            sdfio_reader_free(reader);
            free(file.data);
        }
    }
    /* Only a missing text is refused. */
    sdfio_header header;
    sdfio_header_init(&header);
    header.columns = 1;
    header.rows = 1;
    const float height = 1e-6F;
    membuf file = {0};
    const sdfio_stream stream = membuf_stream(&file);
    sdfio_writer *writer = sdfio_writer_new();
    CHECK_OK(sdfio_create_stream(writer, &stream, &header));
    CHECK_OK(sdfio_write_heights(writer, sdfio_metres(SDFIO_FLOAT32), &height,
                                 sizeof height));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_write_trailer(writer, NULL, 5));
    CHECK_OK(sdfio_write_trailer(writer, NULL, 0));
    sdfio_writer_free(writer);
    free(file.data);
}

/* An assembled trailer is a trailer of the file, in both representations. */
static void check_through_a_file(void)
{
    char memory[CAPACITY];
    sdfio_trailer trailer;
    sdfio_trailer_init(&trailer, memory, sizeof memory);
    CHECK_OK(sdfio_trailer_add_text(&trailer, "OperatorName", "Jane Doe"));
    CHECK_OK(sdfio_trailer_add_integer(&trailer, "Averages", 16));
    CHECK_OK(sdfio_trailer_add_real(&trailer, "Magnification", 20.5));

    const sdfio_representation representations[] = {SDFIO_BINARY, SDFIO_ASCII};
    for (size_t i = 0; i < 2; ++i)
    {
        sdfio_header header;
        sdfio_header_init(&header);
        header.representation = representations[i];
        header.columns = 2;
        header.rows = 2;
        const float heights[4] = {1e-6F, 2e-6F, 3e-6F, 4e-6F};
        membuf file = {0};
        const sdfio_stream stream = membuf_stream(&file);
        sdfio_writer *writer = sdfio_writer_new();
        CHECK_OK(sdfio_create_stream(writer, &stream, &header));
        CHECK_OK(sdfio_write_heights(writer, sdfio_metres(SDFIO_FLOAT32),
                                     heights, 2 * sizeof(float)));
        CHECK_OK(sdfio_write_trailer(writer, trailer.text, trailer.size));
        sdfio_writer_free(writer);

        sdfio_reader *reader = sdfio_reader_new();
        CHECK_OK(sdfio_open_memory(reader, file.data, file.size));
        const char *read = NULL;
        size_t read_size = 0;
        CHECK_OK(sdfio_read_trailer(reader, &read, &read_size));
        /* The three lines, in order, with LF and without the last one. */
        CHECK(same(read, read_size,
                   "OperatorName = Jane Doe\nAverages = 16\n"
                   "Magnification = 20.5"));
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
    char memory[CAPACITY];
    sdfio_trailer trailer;
    sdfio_trailer_init(&trailer, memory, sizeof memory);
    CHECK_OK(sdfio_trailer_add_real(&trailer, "X", 0.25));
    CHECK_OK(sdfio_trailer_add_real(&trailer, "Y", 1.5e-6));
    CHECK(strcmp(trailer.text, "X = 0.25\nY = 1.5e-06\n") == 0);
    setlocale(LC_NUMERIC, "C");
}

int main(void)
{
    check_lines();
    check_numbers_read_back_exactly();
    check_the_buffer();
    check_what_a_line_may_be();
    check_the_text_is_written_as_given();
    check_through_a_file();
    check_the_locale();
    return finish_tests();
}
