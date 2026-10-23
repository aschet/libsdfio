/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* Assembling a trailer: the lines, the numbers, the buffer, and what a file
 * does with the text it is given. */

#include "check.h"

enum
{
    CAPACITY = 512
};

static bool same(const char *text, size_t size, const char *expected)
{
    return size == strlen(expected) && memcmp(text, expected, size) == 0;
}

static void check_lines(void)
{
    char memory[CAPACITY];
    sdfio_trailer trailer;
    sdfio_trailer_init(&trailer, memory, sizeof memory);
    CHECK(trailer.size == 0 && trailer.capacity == sizeof memory
          && trailer.text == memory && memory[0] == '\0');
    CHECK_OK(sdfio_trailer_add_field(&trailer, "OperatorName", "Jane Doe"));
    CHECK_OK(sdfio_trailer_add_field(&trailer, "Scans", "-42"));
    CHECK_OK(sdfio_trailer_add_field(&trailer, "Magnification", "20.5"));
    CHECK_OK(sdfio_trailer_add_field(&trailer, "Empty", ""));
    CHECK(trailer.size == strlen(trailer.text));
    /* One line each, LF in memory. */
    CHECK(strcmp(trailer.text, "OperatorName = Jane Doe\nScans = -42\n"
                               "Magnification = 20.5\nEmpty = \n")
          == 0);
}

static void check_the_buffer(void)
{
    sdfio_trailer trailer;
    /* "A = b\n" is 6 bytes and the 0 after it makes 7. */
    char exact[7];
    sdfio_trailer_init(&trailer, exact, sizeof exact);
    CHECK_OK(sdfio_trailer_add_field(&trailer, "A", "b"));
    CHECK(trailer.size == 6 && strcmp(exact, "A = b\n") == 0);
    CHECK_STATUS(SDFIO_ERR_RANGE, sdfio_trailer_add_field(&trailer, "C", ""));

    char tight[6];
    sdfio_trailer_init(&trailer, tight, sizeof tight);
    CHECK_STATUS(SDFIO_ERR_RANGE, sdfio_trailer_add_field(&trailer, "A", "b"));
    CHECK(trailer.size == 0 && tight[0] == '\0');

    /* A line that does not fit leaves what is there as it was. */
    char room[20];
    sdfio_trailer_init(&trailer, room, sizeof room);
    CHECK_OK(sdfio_trailer_add_field(&trailer, "A", "b"));
    CHECK_STATUS(SDFIO_ERR_RANGE,
                 sdfio_trailer_add_field(&trailer, "Long",
                                         "a value that is too long to fit"));
    CHECK(trailer.size == 6 && strcmp(room, "A = b\n") == 0);

    /* No memory, no room. */
    sdfio_trailer_init(&trailer, NULL, 100);
    CHECK(trailer.capacity == 0);
    CHECK_STATUS(SDFIO_ERR_RANGE, sdfio_trailer_add_field(&trailer, "A", "b"));
    sdfio_trailer_init(&trailer, room, 0);
    CHECK_STATUS(SDFIO_ERR_RANGE, sdfio_trailer_add_field(&trailer, "A", "b"));

    sdfio_trailer_init(NULL, room, sizeof room);
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_trailer_add_field(NULL, "A", "b"));
    sdfio_trailer_init(&trailer, room, sizeof room);
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_trailer_add_field(&trailer, NULL, "b"));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_trailer_add_field(&trailer, "A", NULL));
    /* A size beyond the capacity is not a state of this trailer. */
    trailer.size = sizeof room + 1;
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_trailer_add_field(&trailer, "A", "b"));
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
                     sdfio_trailer_add_field(&trailer, refused_names[i], "v"));
    }
    const char *refused_values[] = {"two\nlines", "one\rtwo", "Caf\xC3\xA9"};
    for (size_t i = 0; i < sizeof refused_values / sizeof refused_values[0];
         ++i)
    {
        CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                     sdfio_trailer_add_field(&trailer, "A", refused_values[i]));
    }
    CHECK(trailer.size == 0);
    CHECK_OK(sdfio_trailer_add_field(&trailer, "Part_2b", "x"));
}

/* A file writes the text it is given: nothing about it is checked, so a
 * program that does not assemble its trailer is not stopped. */
static void check_the_text_is_written_as_given(void)
{
    const char *const trailers[] = {"free text, not a field",
                                    "Caf\xC3\xA9 = 1\xFF", "A = 1\n\n\nB",
                                    "no = field = twice"};
    const sdfio_revision revisions[] = {SDFIO_ISO_1_0, SDFIO_ISO_2_0};
    for (size_t t = 0; t < sizeof trailers / sizeof trailers[0]; ++t)
    {
        for (size_t r = 0; r < 2; ++r)
        {
            sdfio_header header;
            sdfio_header_init(&header);
            header.revision = revisions[r];
            header.data_type = SDFIO_TYPE_INT16;
            header.z_scale = 1e-9;
            header.columns = 1;
            header.rows = 1;
            const double height = 1e-6;
            membuf file = {0};
            const sdfio_stream stream = membuf_stream(&file);
            sdfio_writer *writer = sdfio_writer_new(&all_codecs);
            CHECK_OK(sdfio_create_stream(writer, &stream, &header));
            CHECK_OK(sdfio_write_heights(writer, SDFIO_TYPE_DOUBLE, &height));
            CHECK_OK(
                sdfio_write_trailer(writer, trailers[t], strlen(trailers[t])));
            sdfio_writer_free(writer);

            sdfio_reader *reader = sdfio_reader_new(&all_codecs);
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
    sdfio_writer *writer = sdfio_writer_new(&all_codecs);
    CHECK_OK(sdfio_create_stream(writer, &stream, &header));
    CHECK_OK(sdfio_write_heights(writer, SDFIO_TYPE_FLOAT, &height));
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
    CHECK_OK(sdfio_trailer_add_field(&trailer, "OperatorName", "Jane Doe"));
    CHECK_OK(sdfio_trailer_add_field(&trailer, "Averages", "16"));
    CHECK_OK(sdfio_trailer_add_field(&trailer, "Magnification", "20.5"));

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
        sdfio_writer *writer = sdfio_writer_new(&all_codecs);
        CHECK_OK(sdfio_create_stream(writer, &stream, &header));
        CHECK_OK(sdfio_write_heights(writer, SDFIO_TYPE_FLOAT, heights));
        CHECK_OK(sdfio_write_trailer(writer, trailer.text, trailer.size));
        sdfio_writer_free(writer);

        sdfio_reader *reader = sdfio_reader_new(&all_codecs);
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

int main(void)
{
    check_lines();
    check_the_buffer();
    check_what_a_line_may_be();
    check_the_text_is_written_as_given();
    check_through_a_file();
    return finish_tests();
}
