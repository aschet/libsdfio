/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

#include "check.h"
#include <sdfio/file.h>
#include <stdlib.h>
#include <string.h>

#define COLUMNS 4
#define ROWS 3
#define SAMPLE SDFIO_TEST_DATA_DIR "/samples/iso25178-71-2026/"

typedef struct counter
{
    size_t allocations;
    size_t releases;
} counter;

static void *counting_allocate(void *user, size_t size)
{
    counter *count = user;
    ++count->allocations;
    return malloc(size);
}

static void counting_release(void *user, void *memory)
{
    counter *count = user;
    ++count->releases;
    free(memory);
}

static sdfio_header *new_file_header(sdfio_format format)
{
    sdfio_header *header = new_header();
    CHECK_OK(sdfio_header_set_format(header, format));
    CHECK_OK(sdfio_header_set_data_type(header, SDFIO_TYPE_INT16));
    CHECK_OK(sdfio_header_set_columns(header, COLUMNS));
    CHECK_OK(sdfio_header_set_rows(header, ROWS));
    CHECK_OK(sdfio_header_set_z_scale(header, 1e-9));
    return header;
}

static void fill(int16_t *values, int seed)
{
    for (size_t i = 0; i < ROWS * COLUMNS; ++i)
    {
        values[i] = (int16_t)(seed * 100 + (int)i);
    }
}

/* Writes the first `rows` rows of the file for `seed`, and ends the file if
 * all rows are written. */
static void write_rows(sdfio_writer *writer, const sdfio_header *header,
                       int seed, size_t rows, membuf *file)
{
    free(file->data);
    *file = (membuf){0};
    const sdfio_stream stream = membuf_stream(file);
    int16_t values[ROWS * COLUMNS];
    fill(values, seed);
    CHECK_OK(sdfio_create_stream(writer, &stream, header));

    const sdfio_layout layout = *sdfio_layout_packed(SDFIO_TYPE_INT16);
    for (size_t row = 0; row < rows; ++row)
    {
        CHECK_OK(sdfio_write_data(writer, &layout, values + row * COLUMNS, 1));
    }
    if (rows == ROWS)
    {
        CHECK_OK(sdfio_end_file(writer));
    }
}

/* Compares the rows of the open file with those for `seed`. */
static void check_rows(sdfio_reader *reader, int seed)
{
    int16_t expected[ROWS * COLUMNS];
    fill(expected, seed);
    const sdfio_layout layout = *sdfio_layout_packed(SDFIO_TYPE_INT16);
    for (size_t row = 0; row < ROWS; ++row)
    {
        int16_t values[COLUMNS];
        CHECK_OK(sdfio_read_data(reader, &layout, values, 1));
        CHECK(memcmp(values, expected + row * COLUMNS, sizeof values) == 0);
    }
}

static void check_file(sdfio_reader *reader, const membuf *file, int seed)
{
    CHECK_OK(sdfio_open_memory(reader, file->data, file->size));
    check_rows(reader, seed);
}

/* One writer writes three files with the headers `first`, `second` and
 * `first`, and one reader reads them. */
static void check_sequence(sdfio_writer *writer, sdfio_reader *reader,
                           const sdfio_header *first,
                           const sdfio_header *second)
{
    membuf file = {0};
    write_rows(writer, first, 1, ROWS, &file);
    check_file(reader, &file, 1);
    CHECK_OK(sdfio_writer_reset(writer));
    CHECK_OK(sdfio_reader_reset(reader));

    write_rows(writer, second, 2, ROWS, &file);
    check_file(reader, &file, 2);
    CHECK_OK(sdfio_writer_reset(writer));
    CHECK_OK(sdfio_reader_reset(reader));

    write_rows(writer, first, 3, ROWS, &file);
    check_file(reader, &file, 3);
    CHECK_OK(sdfio_writer_reset(writer));
    CHECK_OK(sdfio_reader_reset(reader));
    free(file.data);
}

static void check_state_after_reset(void)
{
    sdfio_header *binary = new_file_header(SDFIO_BINARY);
    membuf file = {0};
    sdfio_writer *writer = new_writer(all_formats());
    sdfio_reader *reader = new_reader(all_formats());

    /* A file that is not ended, and a file that is not read to its end. */
    write_rows(writer, binary, 1, 1, &file);
    CHECK_STATUS(SDFIO_ERR_STATE, sdfio_end_file(writer));
    CHECK_OK(sdfio_writer_reset(writer));
    write_rows(writer, binary, 2, ROWS, &file);
    CHECK_OK(sdfio_open_memory(reader, file.data, file.size));
    CHECK_STATUS(SDFIO_ERR_STATE,
                 sdfio_open_memory(reader, file.data, file.size));
    CHECK_OK(sdfio_reader_reset(reader));
    CHECK(sdfio_reader_header(reader) == NULL);
    check_file(reader, &file, 2);

    /* A file that is cut short ends the reading with an error, and a reset
     * makes the reader usable again. */
    const sdfio_layout layout = *sdfio_layout_packed(SDFIO_TYPE_INT16);
    CHECK_OK(sdfio_reader_reset(reader));
    CHECK_OK(sdfio_open_memory(reader, file.data, file.size - 3));
    int16_t values[COLUMNS];
    sdfio_status status = SDFIO_OK;
    for (size_t row = 0; row < ROWS && status == SDFIO_OK; ++row)
    {
        status = sdfio_read_data(reader, &layout, values, 1);
    }
    CHECK(status != SDFIO_OK);
    CHECK_STATUS(SDFIO_ERR_STATE, sdfio_read_data(reader, &layout, values, 1));
    CHECK_OK(sdfio_reader_reset(reader));
    CHECK(strcmp(sdfio_reader_error_message(reader), "") == 0);
    check_file(reader, &file, 2);

    /* The options stay fixed. */
    CHECK_STATUS(SDFIO_ERR_STATE, sdfio_reader_set_buffer(reader, 4096));
    CHECK_STATUS(SDFIO_ERR_STATE, sdfio_writer_set_buffer(writer, 4096));
    CHECK_STATUS(SDFIO_ERR_STATE, sdfio_writer_add_ascii(writer));

    free(file.data);
    sdfio_reader_free(reader);
    sdfio_writer_free(writer);
}

/* The reset closes a file that the reader opened by name. */
static void check_named_files(void)
{
    sdfio_reader *reader = new_reader(all_formats());
    CHECK_OK(sdfio_open_file(reader, SAMPLE "iso2_surface_binary.sdf"));
    CHECK_OK(sdfio_reader_reset(reader));
    CHECK_OK(sdfio_open_file(reader, SAMPLE "iso2_surface_ascii.sdf"));
    CHECK_OK(sdfio_reader_reset(reader));
    CHECK_STATUS(SDFIO_ERR_IO, sdfio_open_file(reader, SAMPLE "missing.sdf"));
    CHECK_OK(sdfio_open_file(reader, SAMPLE "iso2_surface_binary.sdf"));
    sdfio_reader_free(reader);
}

static void check_allocations(void)
{
    counter count = {0, 0};
    const sdfio_allocator allocator = {&count, counting_allocate,
                                       counting_release};
    const test_options options = {.allocator = &allocator, ALL_FORMATS};
    sdfio_header *binary = new_file_header(SDFIO_BINARY);
    sdfio_header *ascii = new_file_header(SDFIO_ASCII);
    sdfio_writer *writer = new_writer(&options);
    sdfio_reader *reader = new_reader(&options);
    check_sequence(writer, reader, binary, ascii);
    const size_t after_first = count.allocations;
    check_sequence(writer, reader, binary, ascii);
    check_sequence(writer, reader, ascii, binary);
    CHECK(count.allocations == after_first);
    sdfio_reader_free(reader);
    sdfio_writer_free(writer);
    CHECK(count.releases == count.allocations);
}

int main(void)
{
    sdfio_header *binary = new_file_header(SDFIO_BINARY);
    sdfio_header *ascii = new_file_header(SDFIO_ASCII);
    sdfio_writer *writer = new_writer(all_formats());
    sdfio_reader *reader = new_reader(all_formats());
    check_sequence(writer, reader, binary, ascii);
    sdfio_reader_free(reader);
    sdfio_writer_free(writer);

    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_reader_reset(NULL));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_writer_reset(NULL));
    check_state_after_reset();
    check_named_files();
    check_allocations();
    return finish_tests();
}
