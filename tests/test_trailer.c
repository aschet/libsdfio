/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* Writing a trailer: the fields, what a line may be, and what a file does with
 * the text it is given. */

#include "check.h"

static bool same(const char *text, size_t size, const char *expected)
{
    return size == strlen(expected) && memcmp(text, expected, size) == 0;
}

/* A writer with a file of one row, whose rows are written. */
typedef struct small_file
{
    membuf file;
    sdfio_writer *writer;
} small_file;

static void start_file(small_file *small, sdfio_representation representation)
{
    sdfio_header header;
    sdfio_header_init(&header);
    header.representation = representation;
    header.columns = 2;
    header.rows = 2;
    const float heights[4] = {1e-6F, 2e-6F, 3e-6F, 4e-6F};
    small->file = (membuf){0};
    const sdfio_stream stream = membuf_stream(&small->file);
    small->writer = sdfio_writer_new(&all_codecs);
    CHECK_OK(sdfio_create_stream(small->writer, &stream, &header));
    CHECK_OK(sdfio_write_heights(small->writer, SDFIO_TYPE_FLOAT, heights));
}

/* Ends the file and reads its trailer back: the text, or none, which a binary
 * file reports as null and an ASCII file as an empty text. */
static bool read_trailer_of(small_file *small, const char *expected)
{
    CHECK_OK(sdfio_write_trailer(small->writer, NULL, 0));
    sdfio_writer_free(small->writer);
    sdfio_reader *reader = sdfio_reader_new(&all_codecs);
    CHECK_OK(sdfio_open_memory(reader, small->file.data, small->file.size));
    const char *read = NULL;
    size_t size = 0;
    CHECK_OK(sdfio_read_trailer(reader, &read, &size));
    const bool equal = expected == NULL
                           ? read == NULL || size == 0
                           : read != NULL && same(read, size, expected);
    sdfio_reader_free(reader);
    free(small->file.data);
    return equal;
}

static void check_fields(void)
{
    const sdfio_representation representations[] = {SDFIO_BINARY, SDFIO_ASCII};
    for (size_t i = 0; i < 2; ++i)
    {
        small_file small;
        start_file(&small, representations[i]);
        CHECK_OK(sdfio_write_trailer_field(small.writer, "OperatorName",
                                           "Jane Doe"));
        CHECK_OK(sdfio_write_trailer_field(small.writer, "Empty", ""));
        CHECK_OK(sdfio_write_trailer_field(small.writer, "Scans", "-42"));
        CHECK_OK(
            sdfio_write_trailer_field(small.writer, "Magnification", "20.5"));
        /* One line each, in order, with LF and without the last end. */
        CHECK(read_trailer_of(&small, "OperatorName = Jane Doe\nEmpty = \n"
                                      "Scans = -42\nMagnification = 20.5"));

        /* No field and no text: no trailer. */
        start_file(&small, representations[i]);
        CHECK(read_trailer_of(&small, NULL));

        /* Fields, then text. */
        start_file(&small, representations[i]);
        CHECK_OK(sdfio_write_trailer_field(small.writer, "A", "b"));
        CHECK_OK(sdfio_write_trailer(small.writer, "free text", 9));
        sdfio_writer_free(small.writer);
        sdfio_reader *reader = sdfio_reader_new(&all_codecs);
        CHECK_OK(sdfio_open_memory(reader, small.file.data, small.file.size));
        const char *read = NULL;
        size_t size = 0;
        CHECK_OK(sdfio_read_trailer(reader, &read, &size));
        CHECK(read != NULL && same(read, size, "A = b\nfree text"));
        sdfio_reader_free(reader);
        free(small.file.data);
    }
}

static void check_what_a_line_may_be(void)
{
    small_file small;
    start_file(&small, SDFIO_BINARY);
    const char *refused_names[] = {
        "", "two words", "a=b", "A;", "Caf\xC3\xA9", "tab\t", "line\n"};
    for (size_t i = 0; i < sizeof refused_names / sizeof refused_names[0]; ++i)
    {
        CHECK_STATUS(
            SDFIO_ERR_ARGUMENT,
            sdfio_write_trailer_field(small.writer, refused_names[i], "v"));
    }
    const char *refused_values[] = {"two\nlines", "one\rtwo", "Caf\xC3\xA9"};
    for (size_t i = 0; i < sizeof refused_values / sizeof refused_values[0];
         ++i)
    {
        CHECK_STATUS(
            SDFIO_ERR_ARGUMENT,
            sdfio_write_trailer_field(small.writer, "A", refused_values[i]));
    }
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_write_trailer_field(small.writer, NULL, "b"));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_write_trailer_field(small.writer, "A", NULL));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_write_trailer_field(NULL, "A", "b"));
    /* A refused line is not written, and the writer goes on. */
    CHECK_OK(sdfio_write_trailer_field(small.writer, "Part_2b", "x"));
    CHECK(read_trailer_of(&small, "Part_2b = x"));
}

/* A field belongs after the rows, and not after the end. */
static void check_the_order(void)
{
    sdfio_header header;
    sdfio_header_init(&header);
    header.columns = 1;
    header.rows = 2;
    const float height = 1e-6F;
    membuf file = {0};
    const sdfio_stream stream = membuf_stream(&file);
    sdfio_writer *writer = sdfio_writer_new(&all_codecs);
    CHECK_STATUS(SDFIO_ERR_STATE, sdfio_write_trailer_field(writer, "A", "b"));
    CHECK_OK(sdfio_create_stream(writer, &stream, &header));
    CHECK_OK(sdfio_write_data_float(writer, &height, 1));
    CHECK_STATUS(SDFIO_ERR_STATE, sdfio_write_trailer_field(writer, "A", "b"));
    CHECK_OK(sdfio_write_data_float(writer, &height, 1));
    CHECK_OK(sdfio_write_trailer_field(writer, "A", "b"));
    CHECK_OK(sdfio_write_trailer(writer, NULL, 0));
    CHECK_STATUS(SDFIO_ERR_STATE, sdfio_write_trailer_field(writer, "A", "b"));
    sdfio_writer_free(writer);
    free(file.data);
}

/* A file writes the text it is given: nothing about it is checked, so a
 * program that writes its own text is not stopped. */
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

int main(void)
{
    check_fields();
    check_what_a_line_may_be();
    check_the_order();
    check_the_text_is_written_as_given();
    return finish_tests();
}
