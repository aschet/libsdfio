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

static void start_file(small_file *small, sdfio_format format)
{
    sdfio_header *header = new_header();
    CHECK_OK(sdfio_header_set_format(header, format));
    CHECK_OK(sdfio_header_set_columns(header, 2));
    CHECK_OK(sdfio_header_set_rows(header, 2));
    const float heights[4] = {1e-6F, 2e-6F, 3e-6F, 4e-6F};
    small->file = (membuf){0};
    const sdfio_stream stream = membuf_stream(&small->file);
    small->writer = new_writer(all_formats());
    CHECK_OK(sdfio_create_stream(small->writer, &stream, header));
    CHECK_OK(sdfio_write_heights(
        small->writer, sdfio_layout_packed(SDFIO_TYPE_FLOAT), heights));
}

/* Ends the file and reads its trailer back: the text, or none, which a binary
 * file reports as null and an ASCII file as an empty text. */
static bool read_trailer_of(small_file *small, const char *expected)
{
    CHECK_OK(sdfio_end_file(small->writer));
    sdfio_writer_free(small->writer);
    sdfio_reader *reader = new_reader(all_formats());
    CHECK_OK(sdfio_open_memory(reader, small->file.data, small->file.size));
    const char *read = NULL;
    size_t size = 0;
    CHECK_OK(read_trailer_text(reader, 0, &read, &size));
    const bool equal = expected == NULL
                           ? read == NULL || size == 0
                           : read != NULL && same(read, size, expected);
    sdfio_reader_free(reader);
    free(small->file.data);
    return equal;
}

static void check_fields(void)
{
    const sdfio_format formats[] = {SDFIO_BINARY, SDFIO_ASCII};
    for (size_t i = 0; i < 2; ++i)
    {
        small_file small;
        start_file(&small, formats[i]);
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
        start_file(&small, formats[i]);
        CHECK(read_trailer_of(&small, NULL));

        /* Fields and text follow each other in any order. */
        start_file(&small, formats[i]);
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
    sdfio_header *header = new_header();
    CHECK_OK(sdfio_header_set_columns(header, 1));
    CHECK_OK(sdfio_header_set_rows(header, 2));
    const float height = 1e-6F;
    membuf file = {0};
    const sdfio_stream stream = membuf_stream(&file);
    sdfio_writer *writer = new_writer(all_formats());
    CHECK_STATUS(SDFIO_ERR_STATE, sdfio_write_trailer_field(writer, "A", "b"));
    CHECK_OK(sdfio_create_stream(writer, &stream, header));
    CHECK_OK(sdfio_write_data(writer, sdfio_layout_packed(SDFIO_TYPE_FLOAT),
                              &height, 1));
    CHECK_STATUS(SDFIO_ERR_STATE, sdfio_write_trailer_field(writer, "A", "b"));
    CHECK_OK(sdfio_write_data(writer, sdfio_layout_packed(SDFIO_TYPE_FLOAT),
                              &height, 1));
    CHECK_OK(sdfio_write_trailer_field(writer, "A", "b"));
    CHECK_OK(sdfio_end_file(writer));
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
            sdfio_header *header = new_header();
            CHECK_OK(sdfio_header_set_revision(header, revisions[r]));
            CHECK_OK(sdfio_header_set_data_type(header, SDFIO_TYPE_INT16));
            CHECK_OK(sdfio_header_set_z_scale(header, 1e-9));
            CHECK_OK(sdfio_header_set_columns(header, 1));
            CHECK_OK(sdfio_header_set_rows(header, 1));
            const double height = 1e-6;
            membuf file = {0};
            const sdfio_stream stream = membuf_stream(&file);
            sdfio_writer *writer = new_writer(all_formats());
            CHECK_OK(sdfio_create_stream(writer, &stream, header));
            CHECK_OK(sdfio_write_heights(
                writer, sdfio_layout_packed(SDFIO_TYPE_DOUBLE), &height));
            CHECK_OK(sdfio_write_trailer_text(writer, trailers[t],
                                              strlen(trailers[t])));
            CHECK_OK(sdfio_end_file(writer));
            sdfio_writer_free(writer);

            sdfio_reader *reader = new_reader(all_formats());
            CHECK_OK(sdfio_open_memory(reader, file.data, file.size));
            const char *read = NULL;
            size_t size = 0;
            CHECK_OK(read_trailer_text(reader, 0, &read, &size));
            CHECK(read != NULL && same(read, size, trailers[t]));
            sdfio_reader_free(reader);
            free(file.data);
        }
    }
    /* Only a missing text is refused. */
    sdfio_header *header = new_header();
    CHECK_OK(sdfio_header_set_columns(header, 1));
    CHECK_OK(sdfio_header_set_rows(header, 1));
    const float height = 1e-6F;
    membuf file = {0};
    const sdfio_stream stream = membuf_stream(&file);
    sdfio_writer *writer = new_writer(all_formats());
    CHECK_OK(sdfio_create_stream(writer, &stream, header));
    CHECK_OK(sdfio_write_heights(writer, sdfio_layout_packed(SDFIO_TYPE_FLOAT),
                                 &height));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_write_trailer_text(writer, NULL, 5));
    CHECK_OK(sdfio_end_file(writer));
    sdfio_writer_free(writer);
    free(file.data);
}

/* The trailer of a file, in parts of at most `part` bytes. */
static size_t read_in_parts(const membuf *file, size_t part, char *out,
                            size_t out_size)
{
    sdfio_reader *reader = new_reader(all_formats());
    CHECK_OK(sdfio_open_memory(reader, file->data, file->size));
    const char *text = NULL;
    size_t size = 0;
    CHECK_OK(read_trailer_text(reader, part, &text, &size));
    CHECK(size < out_size);
    memcpy(out, text, size < out_size ? size : out_size - 1);
    out[size < out_size ? size : out_size - 1] = '\0';
    sdfio_reader_free(reader);
    return size;
}

/* A file whose trailer is the text, written as given. */
static membuf file_with_text(sdfio_format format, const char *text)
{
    small_file small;
    start_file(&small, format);
    CHECK_OK(sdfio_write_trailer_text(small.writer, text, strlen(text)));
    CHECK_OK(sdfio_end_file(small.writer));
    sdfio_writer_free(small.writer);
    return small.file;
}

/* A trailer comes in parts of any size, and the parts give the text that a
 * single call gives, also where a part ends between the CR and the LF of a
 * line end. */
static void check_parts(void)
{
    static const char long_line[] =
        "a long line without a line end, so that every size of a part ends in "
        "the middle of the line";
    const char *const texts[] = {"A = 1\r\nB = 2\r\nC = 3",
                                 "A = 1\n\n\nB = 2\n\n", "x", long_line,
                                 "one\r\n\r\ntwo\r\n"};
    const sdfio_format formats[] = {SDFIO_BINARY, SDFIO_ASCII};
    for (size_t r = 0; r < 2; ++r)
    {
        for (size_t t = 0; t < sizeof texts / sizeof texts[0]; ++t)
        {
            membuf file = file_with_text(formats[r], texts[t]);
            char whole[512];
            const size_t whole_size =
                read_in_parts(&file, 0, whole, sizeof whole);
            CHECK(whole_size > 0);
            for (size_t part = 1; part <= 9; ++part)
            {
                char joined[512];
                const size_t joined_size =
                    read_in_parts(&file, part, joined, sizeof joined);
                CHECK(joined_size == whole_size
                      && memcmp(joined, whole, whole_size) == 0);
            }
            free(file.data);
        }
    }
}

/* The end of the trailer: a buffer that the text fills exactly gets 0 bytes
 * from the next call, and a file without a trailer gets 0 at once. */
static void check_the_end(void)
{
    const sdfio_format formats[] = {SDFIO_BINARY, SDFIO_ASCII};
    for (size_t r = 0; r < 2; ++r)
    {
        membuf file = file_with_text(formats[r], "abc");
        sdfio_reader *reader = new_reader(all_formats());
        CHECK_OK(sdfio_open_memory(reader, file.data, file.size));
        char buffer[3];
        size_t size = 0;
        CHECK_OK(sdfio_read_trailer(reader, buffer, sizeof buffer, &size));
        CHECK(size == 3 && memcmp(buffer, "abc", 3) == 0);
        CHECK_OK(sdfio_read_trailer(reader, buffer, sizeof buffer, &size));
        CHECK(size == 0);
        CHECK_OK(sdfio_read_trailer(reader, buffer, sizeof buffer, &size));
        CHECK(size == 0);
        CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                     sdfio_read_trailer(reader, buffer, 0, &size));
        sdfio_reader_free(reader);
        free(file.data);

        small_file none;
        start_file(&none, formats[r]);
        CHECK_OK(sdfio_end_file(none.writer));
        sdfio_writer_free(none.writer);
        reader = new_reader(all_formats());
        CHECK_OK(sdfio_open_memory(reader, none.file.data, none.file.size));
        CHECK_OK(sdfio_read_trailer(reader, buffer, sizeof buffer, &size));
        CHECK(size == 0);
        sdfio_reader_free(reader);
        free(none.file.data);
    }
}

/* In an ASCII file the white space before the text is skipped, and the trailer
 * ends at the line that consists of a star. */
static void check_the_ascii_record(void)
{
    static const struct
    {
        const char *record;
        const char *text;
    } cases[] = {
        {"  \r\n\t\r\nA = 1\r\n*\r\n", "A = 1"},
        {"A = 1\r\n  *  \r\nignored\r\n", "A = 1"},
        {"A = 1\r\n*\r\n", "A = 1"},
        {"A = 1\r\n*", "A = 1"},
        {"A = 1\r\n*not a star\r\nB = 2\r\n*\r\n", "A = 1\n*not a star\nB = 2"},
        {"A = 1\r\n\r\n\r\n*\r\n", "A = 1"},
        {"*\r\n", ""},
        {"", ""}};
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; ++i)
    {
        small_file small;
        start_file(&small, SDFIO_ASCII);
        CHECK_OK(sdfio_end_file(small.writer));
        sdfio_writer_free(small.writer);
        /* The file ends with the star of the trailer: replace that line. */
        membuf file = small.file;
        CHECK(file.size >= 3);
        file.size -= 3;
        membuf_write(&file, cases[i].record, strlen(cases[i].record));
        for (size_t part = 0; part <= 3; part += 3)
        {
            char text[128];
            const size_t size = read_in_parts(&file, part, text, sizeof text);
            CHECK(size == strlen(cases[i].text)
                  && memcmp(text, cases[i].text, size) == 0);
        }
        free(file.data);
    }
}

/* After the trailer was asked for, the rows cannot be read, whichever rows
 * were read before: the position is behind the data area. */
static void check_rows_after_the_trailer(void)
{
    const sdfio_format formats[] = {SDFIO_BINARY, SDFIO_ASCII};
    for (size_t r = 0; r < 2; ++r)
    {
        membuf file = file_with_text(formats[r], "abc");
        sdfio_reader *reader = new_reader(all_formats());
        CHECK_OK(sdfio_open_memory(reader, file.data, file.size));
        float row[2];
        CHECK_OK(sdfio_read_data(reader, sdfio_layout_packed(SDFIO_TYPE_FLOAT),
                                 row, 1));
        char buffer[8];
        size_t size = 0;
        CHECK_OK(sdfio_read_trailer(reader, buffer, sizeof buffer, &size));
        CHECK(size == 3);
        CHECK_STATUS(SDFIO_ERR_STATE,
                     sdfio_read_data(reader,
                                     sdfio_layout_packed(SDFIO_TYPE_FLOAT), row,
                                     1));
        float heights[4];
        CHECK_STATUS(SDFIO_ERR_STATE,
                     sdfio_read_heights(reader,
                                        sdfio_layout_packed(SDFIO_TYPE_FLOAT),
                                        heights));
        sdfio_reader_free(reader);
        free(file.data);
    }
}

/* The bytes of a file whose trailer text is written in parts of a size, and in
 * a single call for the size 0. */
static membuf file_with_text_in_parts(sdfio_format format, const char *text,
                                      size_t part)
{
    small_file small;
    start_file(&small, format);
    const size_t length = strlen(text);
    for (size_t at = 0; at < length;)
    {
        const size_t size =
            part == 0 || length - at < part ? length - at : part;
        CHECK_OK(sdfio_write_trailer_text(small.writer, text + at, size));
        at += size;
    }
    CHECK_OK(sdfio_end_file(small.writer));
    sdfio_writer_free(small.writer);
    return small.file;
}

/* A text that is written in parts gives the file that a single call gives,
 * wherever a part ends: also between the CR and the LF of a line end, and after
 * a CR that is no line end. */
static void check_text_in_parts(void)
{
    const char *const texts[] = {"alpha\nbeta\n", "alpha\r\nbeta\r\n",
                                 "a\r\n\r\nb",    "x\ry\r\n",
                                 "tail\r",        "\r\r\n",
                                 "open"};
    const sdfio_format formats[] = {SDFIO_BINARY, SDFIO_ASCII};
    for (size_t r = 0; r < 2; ++r)
    {
        for (size_t i = 0; i < sizeof texts / sizeof texts[0]; ++i)
        {
            membuf whole = file_with_text_in_parts(formats[r], texts[i], 0);
            for (size_t part = 1; part <= 4; ++part)
            {
                membuf split =
                    file_with_text_in_parts(formats[r], texts[i], part);
                CHECK(split.size == whole.size
                      && memcmp(split.data, whole.data, whole.size) == 0);
                free(split.data);
            }
            free(whole.data);
        }
    }
}

/* A field ends the line that a text leaves open. */
static void check_a_field_ends_an_open_line(void)
{
    const sdfio_format formats[] = {SDFIO_BINARY, SDFIO_ASCII};
    for (size_t r = 0; r < 2; ++r)
    {
        small_file small;
        start_file(&small, formats[r]);
        CHECK_OK(sdfio_write_trailer_text(small.writer, "open", 4));
        CHECK_OK(sdfio_write_trailer_field(small.writer, "Field", "value"));
        CHECK_OK(sdfio_write_trailer_text(small.writer, "tail\r", 5));
        CHECK(read_trailer_of(&small, "open\nField = value\ntail\r"));
    }
}

/* The rows of a source go to a destination of any format, and the
 * trailer follows with a field of the program at its end. */
static void check_copy_trailer(void)
{
    static const char text[] = "Operator = Ada\nfree text\nsecond line";
    static const char copied[] =
        "Operator = Ada\nfree text\nsecond line\nNote = cropped";
    const sdfio_format formats[] = {SDFIO_BINARY, SDFIO_ASCII};
    for (size_t from = 0; from < 2; ++from)
    {
        for (size_t to = 0; to < 2; ++to)
        {
            membuf source = file_with_text(formats[from], text);
            sdfio_reader *reader = new_reader(all_formats());
            CHECK_OK(sdfio_open_memory(reader, source.data, source.size));
            sdfio_header *header = new_header();
            CHECK_OK(sdfio_header_copy(header, sdfio_reader_header(reader)));
            CHECK_OK(sdfio_header_set_format(header, formats[to]));

            membuf file = {0};
            const sdfio_stream stream = membuf_stream(&file);
            sdfio_writer *writer = new_writer(all_formats());
            CHECK_OK(sdfio_create_stream(writer, &stream, header));
            float heights[4];
            CHECK_OK(sdfio_read_heights(
                reader, sdfio_layout_packed(SDFIO_TYPE_FLOAT), heights));
            CHECK_OK(sdfio_write_heights(
                writer, sdfio_layout_packed(SDFIO_TYPE_FLOAT), heights));
            CHECK_OK(sdfio_copy_trailer(reader, writer));
            CHECK_OK(sdfio_write_trailer_field(writer, "Note", "cropped"));
            CHECK_OK(sdfio_end_file(writer));
            sdfio_writer_free(writer);
            sdfio_reader_free(reader);
            free(source.data);

            reader = new_reader(all_formats());
            CHECK_OK(sdfio_open_memory(reader, file.data, file.size));
            CHECK(sdfio_header_format(sdfio_reader_header(reader))
                  == formats[to]);
            const char *read = NULL;
            size_t size = 0;
            CHECK_OK(read_trailer_text(reader, 0, &read, &size));
            CHECK(same(read, size, copied));
            sdfio_reader_free(reader);
            free(file.data);
        }
    }
}

/* A file without a trailer copies nothing, and the copy refuses what a reader
 * or a writer refuses, with the text of the failure in the writer. */
static void check_copy_trailer_failures(void)
{
    small_file small;
    start_file(&small, SDFIO_BINARY);
    CHECK_OK(sdfio_end_file(small.writer));
    sdfio_writer_free(small.writer);

    sdfio_header *header = new_header();
    sdfio_reader *reader = new_reader(all_formats());
    membuf file = {0};
    const sdfio_stream stream = membuf_stream(&file);
    sdfio_writer *writer = new_writer(all_formats());
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_copy_trailer(NULL, writer));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_copy_trailer(reader, NULL));
    CHECK_STATUS(SDFIO_ERR_STATE, sdfio_copy_trailer(reader, writer));
    CHECK(strlen(sdfio_writer_error_message(writer)) > 0);

    CHECK_OK(sdfio_open_memory(reader, small.file.data, small.file.size));
    CHECK_OK(sdfio_header_set_columns(header, 2));
    CHECK_OK(sdfio_header_set_rows(header, 2));
    CHECK_OK(sdfio_create_stream(writer, &stream, header));
    CHECK_STATUS(SDFIO_ERR_STATE, sdfio_copy_trailer(reader, writer));
    const float heights[4] = {0};
    CHECK_OK(sdfio_write_heights(writer, sdfio_layout_packed(SDFIO_TYPE_FLOAT),
                                 heights));
    sdfio_reader *closed = new_reader(all_formats());
    CHECK_STATUS(SDFIO_ERR_STATE, sdfio_copy_trailer(closed, writer));
    CHECK(strcmp(sdfio_writer_error_message(writer),
                 sdfio_reader_error_message(closed))
              == 0
          && strlen(sdfio_writer_error_message(writer)) > 0);
    sdfio_reader_free(closed);
    CHECK_OK(sdfio_copy_trailer(reader, writer));
    CHECK_OK(sdfio_end_file(writer));
    sdfio_writer_free(writer);
    sdfio_reader_free(reader);
    free(small.file.data);

    reader = new_reader(all_formats());
    CHECK_OK(sdfio_open_memory(reader, file.data, file.size));
    const char *read = NULL;
    size_t size = 0;
    CHECK_OK(read_trailer_text(reader, 0, &read, &size));
    CHECK(size == 0);
    sdfio_reader_free(reader);
    free(file.data);
}

/* The fields of a text: the lines of the form Name = Value in order, and no
 * other line. */
static void check_next_field(void)
{
    static const char text[] =
        "Operator = Ada\nfree text\n  Note=  a = b  \n\nlast=\n= no\n"
        "X Y = no\nCRLF = yes\r\nno separator";
    const char *const names[] = {"Operator", "Note", "last", "CRLF"};
    const char *const values[] = {"Ada", "a = b", "", "yes"};
    size_t position = 0;
    for (size_t i = 0; i < 4; ++i)
    {
        const char *name = NULL;
        const char *value = NULL;
        size_t name_size = 0;
        size_t value_size = 0;
        CHECK(sdfio_trailer_next_field(text, sizeof text - 1, &position, &name,
                                       &name_size, &value, &value_size));
        CHECK(same(name, name_size, names[i]));
        CHECK(same(value, value_size, values[i]));
    }
    const char *name = NULL;
    const char *value = NULL;
    size_t name_size = 0;
    size_t value_size = 0;
    CHECK(!sdfio_trailer_next_field(text, sizeof text - 1, &position, &name,
                                    &name_size, &value, &value_size));
    CHECK(position == sizeof text - 1);
    CHECK(!sdfio_trailer_next_field(text, sizeof text - 1, &position, &name,
                                    &name_size, &value, &value_size));

    position = 0;
    CHECK(!sdfio_trailer_next_field(NULL, 0, &position, &name, &name_size,
                                    &value, &value_size));
    CHECK(!sdfio_trailer_next_field(text, sizeof text - 1, NULL, &name,
                                    &name_size, &value, &value_size));
    CHECK(!sdfio_trailer_next_field(text, sizeof text - 1, &position, NULL,
                                    &name_size, &value, &value_size));
    CHECK(!sdfio_trailer_next_field(text, sizeof text - 1, &position, &name,
                                    NULL, &value, &value_size));
    CHECK(!sdfio_trailer_next_field(text, sizeof text - 1, &position, &name,
                                    &name_size, NULL, &value_size));
    CHECK(!sdfio_trailer_next_field(text, sizeof text - 1, &position, &name,
                                    &name_size, &value, NULL));
    position = sizeof text;
    CHECK(!sdfio_trailer_next_field(text, sizeof text - 1, &position, &name,
                                    &name_size, &value, &value_size));
}

/* A name is found without regard to case, the first of several fields counts,
 * and a missing field leaves no value. */
static void check_find_field(void)
{
    static const char text[] =
        "Operator = Ada\nnote = first\nNOTE = second\nEmpty =\n";
    const char *value = text;
    size_t value_size = 99;
    CHECK(sdfio_trailer_find_field(text, sizeof text - 1, "operator", &value,
                                   &value_size));
    CHECK(same(value, value_size, "Ada"));
    CHECK(sdfio_trailer_find_field(text, sizeof text - 1, "Note", &value,
                                   &value_size));
    CHECK(same(value, value_size, "first"));
    CHECK(sdfio_trailer_find_field(text, sizeof text - 1, "EMPTY", &value,
                                   &value_size));
    CHECK(value_size == 0);
    CHECK(!sdfio_trailer_find_field(text, sizeof text - 1, "Missing", &value,
                                    &value_size));
    CHECK(value == NULL && value_size == 0);
    CHECK(!sdfio_trailer_find_field(text, sizeof text - 1, "Operato", &value,
                                    &value_size));
    CHECK(!sdfio_trailer_find_field(text, sizeof text - 1, "", &value,
                                    &value_size));
    CHECK(!sdfio_trailer_find_field(NULL, 0, "Note", &value, &value_size));
    CHECK(!sdfio_trailer_find_field(text, sizeof text - 1, NULL, &value,
                                    &value_size));
    CHECK(!sdfio_trailer_find_field(text, sizeof text - 1, "Note", NULL,
                                    &value_size));
    CHECK(
        !sdfio_trailer_find_field(text, sizeof text - 1, "Note", &value, NULL));
}

/* The fields that a writer wrote are found in the text that a reader returns,
 * in both formats. */
static void check_fields_of_a_file(void)
{
    const sdfio_format formats[] = {SDFIO_BINARY, SDFIO_ASCII};
    for (size_t r = 0; r < 2; ++r)
    {
        small_file small;
        start_file(&small, formats[r]);
        CHECK_OK(sdfio_write_trailer_field(small.writer, "OperatorName",
                                           "Jane Doe"));
        CHECK_OK(
            sdfio_write_trailer_field(small.writer, "Magnification", "20.5"));
        CHECK_OK(sdfio_end_file(small.writer));
        sdfio_writer_free(small.writer);

        sdfio_reader *reader = new_reader(all_formats());
        CHECK_OK(sdfio_open_memory(reader, small.file.data, small.file.size));
        const char *text = NULL;
        size_t size = 0;
        CHECK_OK(read_trailer_text(reader, 0, &text, &size));
        const char *value = NULL;
        size_t value_size = 0;
        CHECK(sdfio_trailer_find_field(text, size, "magnification", &value,
                                       &value_size));
        CHECK(same(value, value_size, "20.5"));
        CHECK(sdfio_trailer_find_field(text, size, "OperatorName", &value,
                                       &value_size));
        CHECK(same(value, value_size, "Jane Doe"));
        sdfio_reader_free(reader);
        free(small.file.data);
    }
}

int main(void)
{
    check_fields();
    check_what_a_line_may_be();
    check_the_order();
    check_the_text_is_written_as_given();
    check_parts();
    check_the_end();
    check_the_ascii_record();
    check_rows_after_the_trailer();
    check_text_in_parts();
    check_a_field_ends_an_open_line();
    check_copy_trailer();
    check_copy_trailer_failures();
    check_next_field();
    check_find_field();
    check_fields_of_a_file();
    return finish_tests();
}
