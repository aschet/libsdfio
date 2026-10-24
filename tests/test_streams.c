/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* Streams that give a few bytes at a time, files that are damaged, and the
 * file that a writer leaves. */

#include "check.h"
#include <stdlib.h>
#include <string.h>

#include <sdfio/file.h>

#include "fields.h"

#define SAMPLES SDFIO_TEST_DATA_DIR "/samples/iso25178-71-2026/"

static unsigned char *load(const char *path, size_t *size)
{
    FILE *file = fopen(path, "rb");
    CHECK(file != NULL);
    if (file == NULL)
    {
        *size = 0;
        return NULL;
    }
    fseek(file, 0, SEEK_END);
    *size = (size_t)ftell(file);
    fseek(file, 0, SEEK_SET);
    unsigned char *data = malloc(*size);
    CHECK(data != NULL && fread(data, 1, *size, file) == *size);
    fclose(file);
    return data;
}

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

/* A stream may return less than asked at any call, in pieces of one byte,
 * of varying sizes or of the whole file: the reader reads the same heights as
 * from memory, and only 0 ends the data. */
static void check_pipe(const char *path, size_t piece)
{
    size_t size = 0;
    unsigned char *data = load(path, &size);
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
    unsigned char *data = load(SAMPLES "iso2_surface_binary.sdf", &size);
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

/* Any prefix of a file, and a file with a damaged byte, is refused or read,
 * but never crashes: the sanitizers watch this. */
static void check_damage(const char *path)
{
    size_t size = 0;
    unsigned char *data = load(path, &size);
    if (data == NULL)
    {
        return;
    }
    for (size_t length = 0; length < size; length += (length < 400 ? 1 : 7))
    {
        sdfio_reader *reader = new_reader(all_formats());
        if (sdfio_open_memory(reader, data, length) == SDFIO_OK)
        {
            const sdfio_header *header = sdfio_reader_header(reader);
            double *heights =
                malloc(sdfio_header_rows(header) * sdfio_header_columns(header)
                           * sizeof(double)
                       + 1);
            const sdfio_status status = sdfio_read_heights(
                reader, sdfio_layout_packed(SDFIO_TYPE_DOUBLE), heights);
            if (length < size / 2)
            {
                CHECK(status != SDFIO_OK);
            }
            const char *trailer = NULL;
            size_t trailer_size = 0;
            (void)read_trailer_text(reader, 0, &trailer, &trailer_size);
            free(heights);
        }
        sdfio_reader_free(reader);
    }
    uint32_t state = 2463534242u;
    unsigned char *copy = malloc(size);
    for (int round = 0; round < 3000 && copy != NULL; ++round)
    {
        memcpy(copy, data, size);
        for (int hits = 0; hits < 3; ++hits)
        {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            const size_t at = state % (round % 2 == 0 ? 400 : size);
            copy[at < size ? at : 0] = (unsigned char)(state >> 8);
        }
        sdfio_reader *reader = new_reader(all_formats());
        if (sdfio_open_memory(reader, copy, size) == SDFIO_OK)
        {
            const sdfio_header *header = sdfio_reader_header(reader);
            const size_t count =
                sdfio_header_rows(header) * sdfio_header_columns(header);
            /* A damaged count could ask for more than a test should allocate.
             */
            if (count < (size_t)1 << 20)
            {
                double *heights = malloc(count * sizeof(double) + 1);
                (void)sdfio_read_heights(
                    reader, sdfio_layout_packed(SDFIO_TYPE_DOUBLE), heights);
                free(heights);
            }
            const char *trailer = NULL;
            size_t trailer_size = 0;
            (void)read_trailer_text(reader, 0, &trailer, &trailer_size);
        }
        sdfio_reader_free(reader);
    }
    free(copy);
    free(data);
}

static void check_header_errors(void)
{
    size_t size = 0;
    unsigned char *data = load(SAMPLES "iso2_surface_binary.sdf", &size);
    sdfio_reader *reader = new_reader(all_formats());
    CHECK_STATUS(SDFIO_ERR_FORMAT, sdfio_open_memory(reader, "", 0));
    sdfio_reader_free(reader);

    reader = new_reader(all_formats());
    CHECK_STATUS(SDFIO_ERR_FORMAT,
                 sdfio_open_memory(reader, "not an sdf file at all", 22));
    CHECK(strlen(sdfio_reader_error_message(reader)) > 0);
    sdfio_reader_free(reader);

    if (data != NULL)
    {
        /* The compression and the checksum bytes end the header, the data type
         * is before the checksum. */
        const size_t header_size = ISO_2_0_BINARY_HEADER_SIZE;
        unsigned char *copy = malloc(size);
        memcpy(copy, data, size);
        copy[header_size - 3] = 1;
        reader = new_reader(all_formats());
        CHECK_STATUS(SDFIO_ERR_FORMAT, sdfio_open_memory(reader, copy, size));
        sdfio_reader_free(reader);
        memcpy(copy, data, size);
        copy[header_size - 1] = 1;
        reader = new_reader(all_formats());
        CHECK_STATUS(SDFIO_ERR_FORMAT, sdfio_open_memory(reader, copy, size));
        sdfio_reader_free(reader);
        memcpy(copy, data, size);
        copy[header_size - 2] = 99;
        reader = new_reader(all_formats());
        CHECK_STATUS(SDFIO_ERR_FORMAT, sdfio_open_memory(reader, copy, size));
        sdfio_reader_free(reader);
        /* The date, the z-scale and the first character of the magic.
         */
        enum
        {
            DATES_AT = sizeof "bISO-2.0" - 1 + SDFIO_ID_SIZE,
            Z_SCALE_AT = DATES_AT + 2 * SDFIO_DATETIME_SIZE
                         + 2 * sizeof(uint32_t) + 2 * sizeof(double)
        };
        memcpy(copy, data, size);
        memset(copy + DATES_AT, '9', SDFIO_DATETIME_SIZE);
        reader = new_reader(all_formats());
        CHECK_STATUS(SDFIO_ERR_FORMAT, sdfio_open_memory(reader, copy, size));
        sdfio_reader_free(reader);
        memcpy(copy, data, size);
        memset(copy + Z_SCALE_AT, 0, sizeof(double));
        reader = new_reader(all_formats());
        CHECK_STATUS(SDFIO_ERR_FORMAT, sdfio_open_memory(reader, copy, size));
        sdfio_reader_free(reader);
        memcpy(copy, data, size);
        copy[0] = 'B';
        reader = new_reader(all_formats());
        CHECK_STATUS(SDFIO_ERR_FORMAT, sdfio_open_memory(reader, copy, size));
        sdfio_reader_free(reader);
        free(copy);

        /* A byte order mark in front of the magic is not an SDF file.
         */
        unsigned char *marked = malloc(size + 3);
        const unsigned char mark[] = {0xEF, 0xBB, 0xBF};
        memcpy(marked, mark, 3);
        memcpy(marked + 3, data, size);
        reader = new_reader(all_formats());
        CHECK_STATUS(SDFIO_ERR_FORMAT,
                     sdfio_open_memory(reader, marked, size + 3));
        sdfio_reader_free(reader);
        free(marked);
    }
    free(data);

    reader = new_reader(all_formats());
    CHECK_STATUS(SDFIO_ERR_IO,
                 sdfio_open_file(reader, SDFIO_TEST_DATA_DIR "/missing.sdf"));
    CHECK(strstr(sdfio_reader_error_message(reader), "cannot be opened")
          != NULL);
    CHECK(sdfio_reader_header(reader) == NULL);
    CHECK_STATUS(SDFIO_ERR_STATE,
                 sdfio_read_data(reader, sdfio_layout_packed(SDFIO_TYPE_FLOAT),
                                 NULL, 0));
    sdfio_reader_free(reader);

    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_open_file(NULL, "x"));
    sdfio_reader_free(NULL);
    sdfio_writer_free(NULL);
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

/* A file is written in place, and a header that cannot be written makes none.
 */
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

/* A header that claims more bytes of data than any stream has: the trailer is
 * looked for by reading through the data area, which ends first. */
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

/* Appends a line to a text. */
static void add_line(char *text, size_t capacity, const char *name,
                     const char *value, const char *line_end)
{
    const size_t used = strlen(text);
    const int written =
        snprintf(text + used, capacity - used, "%s%s%s", name, value, line_end);
    CHECK(written >= 0 && (size_t)written < capacity - used);
}

/* The status of opening an ASCII file whose header is as given. */
static sdfio_status open_ascii(const char *magic, const char *id,
                               const char *date, const char *points,
                               const char *z_scale, const char *line_end)
{
    char text[512] = "";
    add_line(text, sizeof text, magic, "", line_end);
    add_line(text, sizeof text, "ManufacID = ", id, line_end);
    add_line(text, sizeof text, "CreateDate = ", date, line_end);
    add_line(text, sizeof text, "ModDate = ", date, line_end);
    add_line(text, sizeof text, "NumPoints = ", points, line_end);
    add_line(text, sizeof text, "NumProfiles = 1", "", line_end);
    add_line(text, sizeof text, "Xscale = 1.00000000000000e-006", "", line_end);
    add_line(text, sizeof text, "Yscale = 1.00000000000000e-006", "", line_end);
    add_line(text, sizeof text, "Zscale = ", z_scale, line_end);
    add_line(text, sizeof text, "Zresolution = -1", "", line_end);
    add_line(text, sizeof text, "Compression = 0", "", line_end);
    add_line(text, sizeof text, "DataType = 7", "", line_end);
    add_line(text, sizeof text, "CheckType = 0", "", line_end);
    add_line(text, sizeof text, "*", "", line_end);
    add_line(text, sizeof text, "1.00000000000000e+000", "", line_end);
    add_line(text, sizeof text, "*", "", line_end);
    add_line(text, sizeof text, "*", "", line_end);
    sdfio_reader *reader = new_reader(all_formats());
    const sdfio_status status = sdfio_open_memory(reader, text, strlen(text));
    sdfio_reader_free(reader);
    return status;
}

/* What the standard demands of a header, and the reader holds to. */
static void check_standard_rules(void)
{
    const char *const crlf = "\r\n";
    const char *const good_z = "1.00000000000000e-009";
    CHECK_OK(open_ascii("aISO-2.0", "test", "010120261200", "1", good_z, crlf));
    CHECK_OK(open_ascii("aISO-2.0", "test", "010120261200", "1", good_z, "\n"));
    /* The magic starts with a lower case a or b, and names a revision.
     */
    CHECK_STATUS(
        SDFIO_ERR_FORMAT,
        open_ascii("AISO-2.0", "test", "010120261200", "1", good_z, crlf));
    CHECK_STATUS(
        SDFIO_ERR_FORMAT,
        open_ascii("aBCR-1.0", "test", "010120261200", "1", good_z, crlf));
    CHECK_STATUS(
        SDFIO_ERR_FORMAT,
        open_ascii("aISO-3.0", "test", "010120261200", "1", good_z, crlf));
    /* A lone CR does not end a line. */
    CHECK_STATUS(
        SDFIO_ERR_FORMAT,
        open_ascii("aISO-2.0", "test", "010120261200", "1", good_z, "\r"));
    /* The z-scale is a non-zero positive number. */
    CHECK_STATUS(SDFIO_ERR_FORMAT, open_ascii("aISO-2.0", "test",
                                              "010120261200", "1", "0", crlf));
    CHECK_STATUS(
        SDFIO_ERR_FORMAT,
        open_ascii("aISO-2.0", "test", "010120261200", "1", "-1.0e-009", crlf));
    /* A date is DDMMYYYYhhmm with the ranges of the fields. */
    const char *const bad_dates[] = {
        "999999999999", "320120261200",  "310220261200",
        "011320261200", "010120262400",  "010120261260",
        "01012026120",  "0101202612000", "01-01-2026 12"};
    for (size_t i = 0; i < sizeof bad_dates / sizeof bad_dates[0]; ++i)
    {
        CHECK_STATUS(
            SDFIO_ERR_FORMAT,
            open_ascii("aISO-2.0", "test", bad_dates[i], "1", good_z, crlf));
    }
    /* The identifier has 10 characters at most. */
    CHECK_OK(open_ascii("aISO-2.0", "0123456789", "010120261200", "1", good_z,
                        crlf));
    CHECK_STATUS(SDFIO_ERR_FORMAT,
                 open_ascii("aISO-2.0", "01234567890", "010120261200", "1",
                            good_z, crlf));
    /* The number of columns is 2 bytes in ISO-1.0 and 4 bytes in ISO-2.0. */
    CHECK_STATUS(
        SDFIO_ERR_FORMAT,
        open_ascii("aISO-1.0", "test", "010120261200", "65536", good_z, crlf));
    CHECK_STATUS(SDFIO_ERR_FORMAT,
                 open_ascii("aISO-2.0", "test", "010120261200", "4294967296",
                            good_z, crlf));
}

/* A sink that fails. */
static size_t refusing_write(void *user, const void *data, size_t size)
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
    const sdfio_stream stream = {NULL, NULL, refusing_write};
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

/* The stream over a C file reads and writes the file, which stays open. */
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

/* A call without a layout is refused. */
static void check_missing_layout(void)
{
    membuf file = {0};
    const sdfio_stream stream = membuf_stream(&file);
    sdfio_header *header = new_header();
    CHECK_OK(sdfio_header_set_columns(header, 1));
    CHECK_OK(sdfio_header_set_rows(header, 1));
    sdfio_writer *writer = new_writer(all_formats());
    CHECK_OK(sdfio_create_stream(writer, &stream, header));
    const float height = 1e-6F;
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_write_data(writer, NULL, &height, 1));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_write_heights(writer, NULL, &height));
    CHECK_OK(sdfio_write_heights(writer, sdfio_layout_packed(SDFIO_TYPE_FLOAT),
                                 &height));
    CHECK_OK(sdfio_end_file(writer));
    sdfio_writer_free(writer);

    sdfio_reader *reader = new_reader(all_formats());
    CHECK_OK(sdfio_open_memory(reader, file.data, file.size));
    float back = 0.0F;
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_read_data(reader, NULL, &back, 1));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_read_heights(reader, NULL, &back));
    CHECK_OK(sdfio_read_heights(reader, sdfio_layout_packed(SDFIO_TYPE_FLOAT),
                                &back));
    sdfio_reader_free(reader);
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_decode_heights(header, NULL, &back, 1, &back));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_encode_heights(header, NULL, &back, 1, &back));
    double z_scale = 0.0;
    CHECK_STATUS(
        SDFIO_ERR_ARGUMENT,
        sdfio_suggest_z_scale(SDFIO_TYPE_INT16, NULL, &back, 1, 1, &z_scale));
    free(file.data);
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
    check_header_errors();
    check_damage(SAMPLES "iso2_surface_binary.sdf");
    check_damage(SAMPLES "iso1_surface_ascii.sdf");
    check_damage(SAMPLES "iso1_surface_binary.sdf");
    check_damage(SAMPLES "iso2_surface_ascii.sdf");
    check_file();
    check_stdio_stream();
    check_missing_layout();
    check_huge_matrix();
    check_failing_sink();
    check_standard_rules();
    return finish_tests();
}
