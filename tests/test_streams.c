/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* Sources that cannot seek, sources that give a byte at a time, files that
 * are damaged, and the file that a writer leaves. */

#include "check.h"

#include "internal.h"

#define SAMPLES SDFIO_TEST_DATA_DIR "/samples/iso25178-71-2026/"
#define BCR_SAMPLES SDFIO_TEST_DATA_DIR "/samples/bcr-1.0/"

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

/* A source that has no seek, and gives at most `piece` bytes at a time. */
typedef struct pipe_source
{
    const unsigned char *data;
    size_t size;
    size_t position;
    size_t piece;
    size_t fail_at;
} pipe_source;

static size_t pipe_read(void *user, void *data, size_t size)
{
    pipe_source *pipe = user;
    if (pipe->position >= pipe->fail_at)
    {
        return SDFIO_STREAM_ERROR;
    }
    size_t count = pipe->size - pipe->position;
    count = count < size ? count : size;
    count = count < pipe->piece ? count : pipe->piece;
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

static void check_pipe(const char *path, size_t piece)
{
    size_t size = 0;
    unsigned char *data = load(path, &size);
    if (data == NULL)
    {
        return;
    }
    pipe_source pipe = {data, size, 0, piece, SIZE_MAX};
    const sdfio_stream stream = pipe_stream(&pipe);

    sdfio_reader *reader = sdfio_reader_new();
    CHECK_OK(sdfio_open_stream(reader, &stream));
    const sdfio_header *header = sdfio_get_header(reader);
    CHECK(header != NULL);
    if (header != NULL)
    {
        double *heights =
            malloc(header->rows * header->columns * sizeof(double));
        CHECK_OK(
            sdfio_read_heights(reader, sdfio_metres(SDFIO_FLOAT64), heights,
                               (ptrdiff_t)(header->columns * sizeof(double))));
        const char *trailer = NULL;
        size_t trailer_size = 0;
        CHECK_OK(sdfio_read_trailer(reader, &trailer, &trailer_size));
        /* The trailer stays, as the stream cannot be read again. */
        const char *again = NULL;
        size_t again_size = 0;
        CHECK_OK(sdfio_read_trailer(reader, &again, &again_size));
        CHECK(again == trailer && again_size == trailer_size);
        CHECK_STATUS(SDFIO_ERR_STATE, sdfio_rewind(reader));
        CHECK_STATUS(
            SDFIO_ERR_STATE,
            sdfio_read_heights(reader, sdfio_metres(SDFIO_FLOAT64), heights,
                               (ptrdiff_t)(header->columns * sizeof(double))));
        free(heights);
    }
    CHECK_STATUS(SDFIO_ERR_STATE, sdfio_open_stream(reader, &stream));
    sdfio_reader_free(reader);

    /* The trailer can be asked for first: the data is passed over. */
    pipe = (pipe_source){data, size, 0, piece, SIZE_MAX};
    reader = sdfio_reader_new();
    CHECK_OK(sdfio_open_stream(reader, &stream));
    const char *trailer = NULL;
    size_t trailer_size = 0;
    CHECK_OK(sdfio_read_trailer(reader, &trailer, &trailer_size));
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
    const size_t data_end =
        sdfi_binary_header_size(SDFIO_ISO_2_0) + 32 * 48 * 2;
    const size_t places[] = {10, 100, data_end - 50};
    for (size_t i = 0; i < sizeof places / sizeof places[0]; ++i)
    {
        pipe_source pipe = {data, size, 0, 7, places[i]};
        const sdfio_stream stream = pipe_stream(&pipe);
        sdfio_reader *reader = sdfio_reader_new();
        sdfio_status status = sdfio_open_stream(reader, &stream);
        if (status == SDFIO_OK)
        {
            const sdfio_header *header = sdfio_get_header(reader);
            double *heights =
                malloc(header->rows * header->columns * sizeof(double));
            status = sdfio_read_heights(
                reader, sdfio_metres(SDFIO_FLOAT64), heights,
                (ptrdiff_t)(header->columns * sizeof(double)));
            free(heights);
        }
        CHECK(status == SDFIO_ERR_IO);
        /* After an error in the data the reader does not go on. */
        if (status == SDFIO_ERR_IO && sdfio_get_header(reader) != NULL)
        {
            const char *trailer = NULL;
            size_t trailer_size = 0;
            CHECK_STATUS(SDFIO_ERR_STATE,
                         sdfio_read_trailer(reader, &trailer, &trailer_size));
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
        sdfio_reader *reader = sdfio_reader_new();
        if (sdfio_open_memory(reader, data, length) == SDFIO_OK)
        {
            const sdfio_header *header = sdfio_get_header(reader);
            double *heights =
                malloc(header->rows * header->columns * sizeof(double) + 1);
            const sdfio_status status = sdfio_read_heights(
                reader, sdfio_metres(SDFIO_FLOAT64), heights,
                (ptrdiff_t)(header->columns * sizeof(double)));
            if (length < size / 2)
            {
                CHECK(status != SDFIO_OK);
            }
            const char *trailer = NULL;
            size_t trailer_size = 0;
            (void)sdfio_read_trailer(reader, &trailer, &trailer_size);
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
        sdfio_reader *reader = sdfio_reader_new();
        if (sdfio_open_memory(reader, copy, size) == SDFIO_OK)
        {
            const sdfio_header *header = sdfio_get_header(reader);
            const size_t count = header->rows * header->columns;
            /* A damaged count could ask for more than a test should allocate.
             */
            if (count < (size_t)1 << 20)
            {
                double *heights = malloc(count * sizeof(double) + 1);
                (void)sdfio_read_heights(
                    reader, sdfio_metres(SDFIO_FLOAT64), heights,
                    (ptrdiff_t)(header->columns * sizeof(double)));
                free(heights);
            }
            const char *trailer = NULL;
            size_t trailer_size = 0;
            (void)sdfio_read_trailer(reader, &trailer, &trailer_size);
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
    sdfio_reader *reader = sdfio_reader_new();
    CHECK_STATUS(SDFIO_ERR_FORMAT, sdfio_open_memory(reader, "", 0));
    sdfio_reader_free(reader);

    reader = sdfio_reader_new();
    CHECK_STATUS(SDFIO_ERR_FORMAT,
                 sdfio_open_memory(reader, "not an sdf file at all", 22));
    CHECK(strlen(sdfio_reader_error_message(reader)) > 0);
    sdfio_reader_free(reader);

    if (data != NULL)
    {
        /* The compression and the checksum bytes end the header, the data type
         * is before the checksum. */
        const size_t header_size = sdfi_binary_header_size(SDFIO_ISO_2_0);
        unsigned char *copy = malloc(size);
        memcpy(copy, data, size);
        copy[header_size - 3] = 1;
        reader = sdfio_reader_new();
        CHECK_STATUS(SDFIO_ERR_UNSUPPORTED,
                     sdfio_open_memory(reader, copy, size));
        sdfio_reader_free(reader);
        memcpy(copy, data, size);
        copy[header_size - 1] = 1;
        reader = sdfio_reader_new();
        CHECK_STATUS(SDFIO_ERR_UNSUPPORTED,
                     sdfio_open_memory(reader, copy, size));
        sdfio_reader_free(reader);
        memcpy(copy, data, size);
        copy[header_size - 2] = 99;
        reader = sdfio_reader_new();
        CHECK_STATUS(SDFIO_ERR_FORMAT, sdfio_open_memory(reader, copy, size));
        sdfio_reader_free(reader);
        free(copy);

        /* A mark at the start is not part of the file. */
        unsigned char *marked = malloc(size + 3);
        const unsigned char mark[] = {0xEF, 0xBB, 0xBF};
        memcpy(marked, mark, 3);
        memcpy(marked + 3, data, size);
        reader = sdfio_reader_new();
        CHECK_OK(sdfio_open_memory(reader, marked, size + 3));
        sdfio_reader_free(reader);
        free(marked);
    }
    free(data);

    reader = sdfio_reader_new();
    CHECK_STATUS(SDFIO_ERR_IO,
                 sdfio_open_file(reader, SDFIO_TEST_DATA_DIR "/missing.sdf"));
    CHECK(strstr(sdfio_reader_error_message(reader), "missing.sdf") != NULL);
    CHECK(sdfio_get_header(reader) == NULL);
    CHECK_STATUS(SDFIO_ERR_STATE, sdfio_read_data(reader, NULL, 0));
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

/* A file is replaced whole or not at all. */
static void check_atomic_file(void)
{
    const char *path = "test_streams_output.sdf";
    const char *partial = "test_streams_output.sdf.sdftmp";
    remove(path);
    sdfio_header header;
    sdfio_header_init(&header);
    header.columns = 2;
    header.rows = 2;
    const double heights[4] = {1, 2, 3, 4};

    sdfio_writer *writer = sdfio_writer_new();
    CHECK_OK(sdfio_create_file(writer, path, &header));
    CHECK(file_exists(partial) && !file_exists(path));
    CHECK_OK(sdfio_write_heights(writer, sdfio_metres(SDFIO_FLOAT64), heights,
                                 2 * sizeof(double)));
    CHECK_OK(sdfio_write_trailer(writer, NULL, 0));
    sdfio_writer_free(writer);
    CHECK(file_exists(path) && !file_exists(partial));

    /* An error leaves the old file as it was. */
    writer = sdfio_writer_new();
    CHECK_OK(sdfio_create_file(writer, path, &header));
    CHECK_OK(sdfio_write_data(writer, heights, 1));
    sdfio_writer_free(writer);
    CHECK(file_exists(path) && !file_exists(partial));
    sdfio_reader *reader = sdfio_reader_new();
    CHECK_OK(sdfio_open_file(reader, path));
    double back[4] = {0};
    CHECK_OK(sdfio_read_heights(reader, sdfio_metres(SDFIO_FLOAT64), back,
                                2 * sizeof(double)));
    CHECK(back[3] == 4.0 && back[0] == 1.0);
    sdfio_reader_free(reader);
    remove(path);

    writer = sdfio_writer_new();
    CHECK_STATUS(SDFIO_ERR_IO,
                 sdfio_create_file(writer, "no_such_folder/out.sdf", &header));
    sdfio_writer_free(writer);
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
    sdfio_header header;
    sdfio_header_init(&header);
    header.columns = 4;
    header.rows = 4;
    const sdfio_stream stream = {NULL, NULL, refusing_write, NULL};
    sdfio_writer *writer = sdfio_writer_new();
    CHECK_OK(sdfio_create_stream(writer, &stream, &header));
    double heights[16] = {0};
    /* A small file is buffered: the failure shows when it is flushed. */
    CHECK_OK(sdfio_write_heights(writer, sdfio_metres(SDFIO_FLOAT64), heights,
                                 4 * sizeof(double)));
    CHECK_STATUS(SDFIO_ERR_IO, sdfio_write_trailer(writer, NULL, 0));
    CHECK_STATUS(SDFIO_ERR_STATE, sdfio_write_trailer(writer, NULL, 0));
    sdfio_writer_free(writer);
}

int main(void)
{
    const size_t pieces[] = {1, 7, 4096, SIZE_MAX};
    for (size_t i = 0; i < sizeof pieces / sizeof pieces[0]; ++i)
    {
        check_pipe(SAMPLES "iso2_surface_binary.sdf", pieces[i]);
        check_pipe(SAMPLES "iso2_surface_ascii.sdf", pieces[i]);
        check_pipe(BCR_SAMPLES "bcr1_surface_ascii.sdf", pieces[i]);
        check_pipe(BCR_SAMPLES "bcr1_surface_binary.sdf", pieces[i]);
    }
    check_failing_stream();
    check_header_errors();
    check_damage(SAMPLES "iso2_surface_binary.sdf");
    check_damage(SAMPLES "iso1_surface_ascii.sdf");
    check_damage(BCR_SAMPLES "bcr1_surface_ascii.sdf");
    check_damage(BCR_SAMPLES "bcr1_surface_binary.sdf");
    check_atomic_file();
    check_failing_sink();
    return finish_tests();
}
