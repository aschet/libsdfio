/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* Writing a trailer: the fields, what a line may be, and what a file does with
 * the text it is given. */

#include "check.h"
#include <stdlib.h>
#include <string.h>

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
    small->writer = sdfio_writer_new(all_formats());
    CHECK_OK(sdfio_create_stream(small->writer, &stream, &header));
    CHECK_OK(sdfio_write_heights(
        small->writer, sdfio_layout_packed(SDFIO_TYPE_FLOAT), heights));
}

/* Ends the file and reads its trailer back: the text, or none, which a binary
 * file reports as null and an ASCII file as an empty text. */
static bool read_trailer_of(small_file *small, const char *expected)
{
    CHECK_OK(sdfio_end_trailer(small->writer));
    sdfio_writer_free(small->writer);
    sdfio_reader *reader = sdfio_reader_new(all_formats());
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

        /* Fields and text follow each other in any order. */
        start_file(&small, representations[i]);
        CHECK_OK(sdfio_write_trailer_text(small.writer, "free text", 9));
        CHECK_OK(sdfio_write_trailer_field(small.writer, "A", "b"));
        CHECK_OK(sdfio_write_trailer_text(small.writer, "more\ntext", 9));
        CHECK_OK(sdfio_write_trailer_field(small.writer, "C", "d"));
        CHECK(read_trailer_of(&small, "free text\nA = b\nmore\ntext\nC = d"));
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
    sdfio_writer *writer = sdfio_writer_new(all_formats());
    CHECK_STATUS(SDFIO_ERR_STATE, sdfio_write_trailer_field(writer, "A", "b"));
    CHECK_OK(sdfio_create_stream(writer, &stream, &header));
    CHECK_OK(sdfio_write_data(writer, sdfio_layout_packed(SDFIO_TYPE_FLOAT),
                              &height, 1));
    CHECK_STATUS(SDFIO_ERR_STATE, sdfio_write_trailer_field(writer, "A", "b"));
    CHECK_OK(sdfio_write_data(writer, sdfio_layout_packed(SDFIO_TYPE_FLOAT),
                              &height, 1));
    CHECK_OK(sdfio_write_trailer_field(writer, "A", "b"));
    CHECK_OK(sdfio_end_trailer(writer));
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
            sdfio_writer *writer = sdfio_writer_new(all_formats());
            CHECK_OK(sdfio_create_stream(writer, &stream, &header));
            CHECK_OK(sdfio_write_heights(
                writer, sdfio_layout_packed(SDFIO_TYPE_DOUBLE), &height));
            CHECK_OK(sdfio_write_trailer_text(writer, trailers[t],
                                              strlen(trailers[t])));
            CHECK_OK(sdfio_end_trailer(writer));
            sdfio_writer_free(writer);

            sdfio_reader *reader = sdfio_reader_new(all_formats());
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
    sdfio_writer *writer = sdfio_writer_new(all_formats());
    CHECK_OK(sdfio_create_stream(writer, &stream, &header));
    CHECK_OK(sdfio_write_heights(writer, sdfio_layout_packed(SDFIO_TYPE_FLOAT),
                                 &height));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_write_trailer_text(writer, NULL, 5));
    CHECK_OK(sdfio_end_trailer(writer));
    sdfio_writer_free(writer);
    free(file.data);
}

/* A file with a trailer of three fields, or without a trailer. */
static membuf file_with_trailer(sdfio_representation representation,
                                bool with_fields)
{
    small_file small;
    start_file(&small, representation);
    if (with_fields)
    {
        CHECK_OK(sdfio_write_trailer_field(small.writer, "OperatorName",
                                           "Jane Doe"));
        CHECK_OK(sdfio_write_trailer_field(small.writer, "PartName",
                                           "Synthetic bumps"));
        CHECK_OK(
            sdfio_write_trailer_field(small.writer, "Magnification", "20.5"));
    }
    CHECK_OK(sdfio_end_trailer(small.writer));
    sdfio_writer_free(small.writer);
    return small.file;
}

/* The status of reading the trailer of a file with a limit. */
static sdfio_status read_with_limit(const membuf *file, size_t limit,
                                    size_t *size)
{
    const sdfio_config config = {.max_trailer_size = limit, ALL_FORMATS};
    sdfio_reader *reader = sdfio_reader_new(&config);
    CHECK_OK(sdfio_open_memory(reader, file->data, file->size));
    const char *text = NULL;
    *size = 0;
    const sdfio_status status = sdfio_read_trailer(reader, &text, size);
    if (status != SDFIO_OK)
    {
        /* After the failure the reader does not read on. */
        CHECK(sdfio_read_trailer(reader, &text, size) != SDFIO_OK);
    }
    sdfio_reader_free(reader);
    return status;
}

/* A trailer longer than the limit of the configuration is not read: the limit
 * counts the bytes of the file in a binary file, and the characters of the text
 * in an ASCII file. A limit of 0 is none. */
static void check_the_limit(void)
{
    const sdfio_representation representations[] = {SDFIO_BINARY, SDFIO_ASCII};
    for (size_t r = 0; r < 2; ++r)
    {
        membuf file = file_with_trailer(representations[r], true);
        membuf empty = file_with_trailer(representations[r], false);
        size_t full = 0;
        CHECK_OK(read_with_limit(&file, 0, &full));
        /* An ASCII file has white space around the trailer, which the limit
         * counts and the trailer then lacks. */
        const size_t white_space = 8;
        const size_t bytes =
            r == 0 ? file.size - empty.size : full + white_space;
        size_t size = 0;
        CHECK_OK(read_with_limit(&file, bytes, &size));
        CHECK(size == full);
        CHECK_STATUS(
            SDFIO_ERR_LIMIT,
            read_with_limit(&file, r == 0 ? bytes - 1 : full - 1, &size));
        CHECK_STATUS(SDFIO_ERR_LIMIT, read_with_limit(&file, 1, &size));
        free(file.data);
        free(empty.data);

        /* One line without an end is not collected beyond the limit. */
        small_file big;
        start_file(&big, representations[r]);
        static char line[5000];
        memset(line, 'x', sizeof line - 1);
        CHECK_OK(sdfio_write_trailer_text(big.writer, line, sizeof line - 1));
        CHECK_OK(sdfio_end_trailer(big.writer));
        sdfio_writer_free(big.writer);
        CHECK_STATUS(SDFIO_ERR_LIMIT, read_with_limit(&big.file, 100, &size));
        CHECK_OK(read_with_limit(&big.file, 0, &size));
        CHECK(size == sizeof line - 1);
        free(big.file.data);
    }
}

int main(void)
{
    check_fields();
    check_what_a_line_may_be();
    check_the_order();
    check_the_text_is_written_as_given();
    check_the_limit();
    return finish_tests();
}
