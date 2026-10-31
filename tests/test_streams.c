/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

#include "check.h"
#include <stdlib.h>
#include <string.h>

#include <sdfio/file.h>

#include "fields.h"

#define SAMPLES SDFIO_TEST_DATA_DIR "/samples/iso25178-71-2026/"

/* A stream that gives at most `piece` bytes at a time. */
typedef struct pipe_source
{
    const unsigned char *data;
    size_t size;
    size_t position;
    size_t piece;
    size_t fail_at;
    size_t calls;
} pipe_source;

static size_t pipe_read(void *user, void *data, size_t size)
{
    pipe_source *pipe = user;
    if (pipe->position >= pipe->fail_at)
    {
        return SDFIO_STREAM_ERROR;
    }
    /* A piece of 0 stands for pieces of varying sizes, 1 to 13 bytes. */
    const size_t piece =
        pipe->piece != 0 ? pipe->piece : 1 + (pipe->calls++ * 5) % 13;
    size_t count = pipe->size - pipe->position;
    count = count < size ? count : size;
    count = count < piece ? count : piece;
    memcpy(data, pipe->data + pipe->position, count);
    pipe->position += count;
    return count;
}

static sdfio_stream pipe_stream(pipe_source *pipe)
{
    sdfio_stream stream = {0};
    stream.user = pipe;
    stream.read = pipe_read;
    return stream;
}

/* Reads of one byte, of varying sizes or of the whole file give the same
 * heights as memory, and only 0 ends the data. */
static void check_pipe(const char *path, size_t piece)
{
    size_t size = 0;
    unsigned char *data = load_file(path, &size);
    if (data == NULL)
    {
        return;
    }
    pipe_source pipe = {data, size, 0, piece, SIZE_MAX, 0};
    const sdfio_stream stream = pipe_stream(&pipe);

    sdfio_reader *reader = new_reader(all_formats());
    CHECK_OK(sdfio_open_stream(reader, &stream));
    const sdfio_header *header = sdfio_reader_header(reader);
    CHECK(header != NULL);
    if (header != NULL)
    {
        double *heights =
            malloc(sdfio_header_rows(header) * sdfio_header_columns(header)
                   * sizeof(double));
        CHECK_OK(sdfio_read_heights(
            reader, sdfio_layout_packed(SDFIO_TYPE_DOUBLE), heights));
        /* The same heights, in the same places, as from memory. */
        sdfio_reader *in_memory = new_reader(all_formats());
        CHECK_OK(sdfio_open_memory(in_memory, data, size));
        double *expected =
            malloc(sdfio_header_rows(header) * sdfio_header_columns(header)
                   * sizeof(double));
        CHECK_OK(sdfio_read_heights(
            in_memory, sdfio_layout_packed(SDFIO_TYPE_DOUBLE), expected));
        for (size_t i = 0;
             i < sdfio_header_rows(header) * sdfio_header_columns(header); ++i)
        {
            CHECK(isnan(expected[i]) ? isnan(heights[i])
                                     : heights[i] == expected[i]);
        }
        free(expected);
        sdfio_reader_free(in_memory);
        const char *trailer = NULL;
        size_t trailer_size = 0;
        CHECK_OK(read_trailer_text(reader, 0, &trailer, &trailer_size));
        /* The trailer is at its end, and the stream cannot be read again. */
        const char *again = NULL;
        size_t again_size = 0;
        CHECK_OK(read_trailer_text(reader, 0, &again, &again_size));
        CHECK(again_size == 0);
        CHECK_STATUS(SDFIO_ERR_STATE,
                     sdfio_read_heights(reader,
                                        sdfio_layout_packed(SDFIO_TYPE_DOUBLE),
                                        heights));
        free(heights);
    }
    CHECK_STATUS(SDFIO_ERR_STATE, sdfio_open_stream(reader, &stream));
    sdfio_reader_free(reader);

    /* The trailer can be asked for first: the data is passed over. */
    pipe = (pipe_source){data, size, 0, piece, SIZE_MAX, 0};
    reader = new_reader(all_formats());
    CHECK_OK(sdfio_open_stream(reader, &stream));
    const char *trailer = NULL;
    size_t trailer_size = 0;
    CHECK_OK(read_trailer_text(reader, 0, &trailer, &trailer_size));
    CHECK(trailer != NULL && trailer_size > 0);
    sdfio_reader_free(reader);
    free(data);
}

static void check_failing_stream(void)
{
    size_t size = 0;
    unsigned char *data = load_file(SAMPLES "iso2_surface_binary.sdf", &size);
    if (data == NULL)
    {
        return;
    }
    /* A failure in the header, and one in the data, which is a 32 by 48 matrix
     * of int16 after the header. */
    const size_t data_end = ISO_2_0_BINARY_HEADER_SIZE + 32 * 48 * 2;
    const size_t places[] = {10, 100, data_end - 50};
    for (size_t i = 0; i < sizeof places / sizeof places[0]; ++i)
    {
        pipe_source pipe = {data, size, 0, 7, places[i], 0};
        const sdfio_stream stream = pipe_stream(&pipe);
        sdfio_reader *reader = new_reader(all_formats());
        sdfio_status status = sdfio_open_stream(reader, &stream);
        if (status == SDFIO_OK)
        {
            const sdfio_header *header = sdfio_reader_header(reader);
            double *heights =
                malloc(sdfio_header_rows(header) * sdfio_header_columns(header)
                       * sizeof(double));
            status = sdfio_read_heights(
                reader, sdfio_layout_packed(SDFIO_TYPE_DOUBLE), heights);
            free(heights);
        }
        CHECK(status == SDFIO_ERR_IO);
        /* After an error in the data the reader does not go on. */
        if (status == SDFIO_ERR_IO && sdfio_reader_header(reader) != NULL)
        {
            const char *trailer = NULL;
            size_t trailer_size = 0;
            CHECK_STATUS(SDFIO_ERR_STATE,
                         read_trailer_text(reader, 0, &trailer, &trailer_size));
        }
        sdfio_reader_free(reader);
    }
    free(data);
}

static bool file_exists(const char *path)
{
    FILE *file = fopen(path, "rb");
    if (file != NULL)
    {
        fclose(file);
    }
    return file != NULL;
}

/* A file is written in place, and a header that cannot be written creates
 * none. */
static void check_file(void)
{
    const char *path = "test_streams_output.sdf";
    remove(path);
    sdfio_header *header = new_header();
    CHECK_OK(sdfio_header_set_columns(header, 2));
    CHECK_OK(sdfio_header_set_rows(header, 2));
    const double heights[4] = {1, 2, 3, 4};

    sdfio_writer *writer = new_writer(all_formats());
    CHECK_OK(sdfio_create_file(writer, path, header));
    CHECK_OK(sdfio_write_heights(writer, sdfio_layout_packed(SDFIO_TYPE_DOUBLE),
                                 heights));
    CHECK_OK(sdfio_end_file(writer));
    sdfio_writer_free(writer);
    CHECK(file_exists(path));

    sdfio_reader *reader = new_reader(all_formats());
    CHECK_OK(sdfio_open_file(reader, path));
    double back[4] = {0};
    CHECK_OK(sdfio_read_heights(reader, sdfio_layout_packed(SDFIO_TYPE_DOUBLE),
                                back));
    CHECK(back[3] == 4.0 && back[0] == 1.0);
    sdfio_reader_free(reader);
    remove(path);

    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_header_set_z_scale(header, -1.0));

    writer = new_writer(all_formats());
    CHECK_OK(sdfio_header_set_revision(header, SDFIO_ISO_1_0));
    CHECK_OK(sdfio_header_set_data_type(header, SDFIO_TYPE_INT8));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_create_file(writer, path, header));
    CHECK(!file_exists(path));
    sdfio_writer_free(writer);

    CHECK_OK(sdfio_header_set_revision(header, SDFIO_ISO_2_0));
    CHECK_OK(sdfio_header_set_data_type(header, SDFIO_TYPE_FLOAT));
    writer = new_writer(all_formats());
    CHECK_STATUS(SDFIO_ERR_IO,
                 sdfio_create_file(writer, "no_such_folder/out.sdf", header));
    sdfio_writer_free(writer);
}

/* A header that claims more data than the stream holds: the search for the
 * trailer reaches the end of the stream. */
static void check_huge_matrix(void)
{
    if (sizeof(size_t) < sizeof(uint64_t))
    {
        /* The number of values does not fit in size_t, so the file is
         * refused. */
        return;
    }
    sdfio_header *header = new_header();
    CHECK_OK(sdfio_header_set_format(header, SDFIO_BINARY));
    CHECK_OK(sdfio_header_set_data_type(header, SDFIO_TYPE_DOUBLE));
    CHECK_OK(sdfio_header_set_columns(header, 1));
    CHECK_OK(sdfio_header_set_rows(header, 1));
    CHECK_OK(sdfio_header_set_z_scale(header, 1.0));
    membuf file = {0};
    const sdfio_stream out = membuf_stream(&file);
    const double height = 0.0;
    sdfio_writer *writer = new_writer(all_formats());
    CHECK_OK(sdfio_create_stream(writer, &out, header));
    CHECK_OK(sdfio_write_heights(writer, sdfio_layout_packed(SDFIO_TYPE_DOUBLE),
                                 &height));
    CHECK_OK(sdfio_end_file(writer));
    sdfio_writer_free(writer);
    /* The numbers of columns and rows, 4 bytes each in little endian. */
    const uint32_t sizes[] = {UINT32_MAX, 1U << 29};
    for (size_t i = 0; i < 2; ++i)
    {
        for (size_t byte = 0; byte < sizeof(uint32_t); ++byte)
        {
            file.data[ISO_2_0_COLUMNS_AT + i * sizeof(uint32_t) + byte] =
                (unsigned char)(sizes[i] >> (8 * byte));
        }
    }
    pipe_source pipe = {file.data, file.size, 0, SIZE_MAX, SIZE_MAX, 0};
    const sdfio_stream stream = pipe_stream(&pipe);
    sdfio_reader *reader = new_reader(all_formats());
    CHECK_OK(sdfio_open_stream(reader, &stream));
    const char *trailer = NULL;
    size_t trailer_size = 0;
    CHECK(read_trailer_text(reader, 0, &trailer, &trailer_size) != SDFIO_OK);
    sdfio_reader_free(reader);
    free(file.data);
}
/* A write callback that writes half of what it is given. */
static size_t half_write(void *user, const void *data, size_t size)
{
    (void)user;
    (void)data;
    return size / 2;
}

static void check_failing_sink(void)
{
    sdfio_header *header = new_header();
    CHECK_OK(sdfio_header_set_columns(header, 4));
    CHECK_OK(sdfio_header_set_rows(header, 4));
    const sdfio_stream stream = {NULL, NULL, half_write};
    sdfio_writer *writer = new_writer(all_formats());
    CHECK_OK(sdfio_create_stream(writer, &stream, header));
    double heights[16] = {0};
    /* A small file is buffered: the failure shows when it is flushed. */
    CHECK_OK(sdfio_write_heights(writer, sdfio_layout_packed(SDFIO_TYPE_DOUBLE),
                                 heights));
    CHECK_STATUS(SDFIO_ERR_IO, sdfio_end_file(writer));
    CHECK_STATUS(SDFIO_ERR_STATE, sdfio_end_file(writer));
    sdfio_writer_free(writer);
}

/* The stdio stream reads and writes a C file and leaves it open. */
static void check_stdio_stream(void)
{
    FILE *file = tmpfile();
    CHECK(file != NULL);
    if (file == NULL)
    {
        return;
    }
    sdfio_stream stream;
    sdfio_stdio_stream(&stream, file);
    CHECK(stream.user == file && stream.read != NULL && stream.write != NULL);

    sdfio_header *header = new_header();
    CHECK_OK(sdfio_header_set_columns(header, 2));
    CHECK_OK(sdfio_header_set_rows(header, 1));
    const float heights[2] = {1e-6F, 2e-6F};
    sdfio_writer *writer = new_writer(all_formats());
    CHECK_OK(sdfio_create_stream(writer, &stream, header));
    CHECK_OK(sdfio_write_heights(writer, sdfio_layout_packed(SDFIO_TYPE_FLOAT),
                                 heights));
    CHECK_OK(sdfio_end_file(writer));
    sdfio_writer_free(writer);
    CHECK(fflush(file) == 0);

    CHECK(fseek(file, 0, SEEK_SET) == 0);
    float back[2];
    sdfio_reader *reader = new_reader(all_formats());
    CHECK_OK(sdfio_open_stream(reader, &stream));
    CHECK_OK(sdfio_read_heights(reader, sdfio_layout_packed(SDFIO_TYPE_FLOAT),
                                back));
    CHECK(back[0] == heights[0] && back[1] == heights[1]);
    sdfio_reader_free(reader);
    CHECK(fclose(file) == 0);
}
int main(void)
{
    const size_t pieces[] = {0, 1, 7, 4096, SIZE_MAX};
    for (size_t i = 0; i < sizeof pieces / sizeof pieces[0]; ++i)
    {
        check_pipe(SAMPLES "iso2_surface_binary.sdf", pieces[i]);
        check_pipe(SAMPLES "iso2_surface_ascii.sdf", pieces[i]);
        check_pipe(SAMPLES "iso1_surface_ascii.sdf", pieces[i]);
        check_pipe(SAMPLES "iso1_surface_binary.sdf", pieces[i]);
    }
    check_failing_stream();
    check_file();
    check_stdio_stream();
    check_huge_matrix();
    check_failing_sink();
    return finish_tests();
}
