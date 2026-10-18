/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* Sources that cannot seek, sources that give a byte at a time, files that
 * are damaged, and the file that a writer leaves. */

#include "check.h"

#include "internal.h"

#define SAMPLES SDF_TEST_DATA_DIR "/samples/iso25178-71-2026/"
#define BCR_SAMPLES SDF_TEST_DATA_DIR "/samples/bcr-1.0/"

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
        return SDF_STREAM_ERROR;
    }
    size_t count = pipe->size - pipe->position;
    count = count < size ? count : size;
    count = count < pipe->piece ? count : pipe->piece;
    memcpy(data, pipe->data + pipe->position, count);
    pipe->position += count;
    return count;
}

static sdf_stream pipe_stream(pipe_source *pipe)
{
    sdf_stream stream = {0};
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
    const sdf_stream stream = pipe_stream(&pipe);

    sdf_reader *reader = sdf_reader_new();
    CHECK_OK(sdf_open_stream(reader, &stream));
    const sdf_info *info = sdf_get_info(reader);
    CHECK(info != NULL);
    if (info != NULL)
    {
        double *heights = malloc(info->rows * info->columns * sizeof(double));
        CHECK_OK(sdf_read_heights(reader, sdf_metres(SDF_FLOAT64), heights,
                                  (ptrdiff_t)(info->columns * sizeof(double))));
        const char *trailer = NULL;
        size_t trailer_size = 0;
        CHECK_OK(sdf_read_trailer(reader, &trailer, &trailer_size));
        /* The trailer stays, as the stream cannot be read again. */
        const char *again = NULL;
        size_t again_size = 0;
        CHECK_OK(sdf_read_trailer(reader, &again, &again_size));
        CHECK(again == trailer && again_size == trailer_size);
        CHECK_STATUS(SDF_ERR_STATE, sdf_rewind(reader));
        CHECK_STATUS(
            SDF_ERR_STATE,
            sdf_read_heights(reader, sdf_metres(SDF_FLOAT64), heights,
                             (ptrdiff_t)(info->columns * sizeof(double))));
        free(heights);
    }
    CHECK_STATUS(SDF_ERR_STATE, sdf_open_stream(reader, &stream));
    sdf_reader_free(reader);

    /* The trailer can be asked for first: the data is passed over. */
    pipe = (pipe_source){data, size, 0, piece, SIZE_MAX};
    reader = sdf_reader_new();
    CHECK_OK(sdf_open_stream(reader, &stream));
    const char *trailer = NULL;
    size_t trailer_size = 0;
    CHECK_OK(sdf_read_trailer(reader, &trailer, &trailer_size));
    CHECK(trailer != NULL && trailer_size > 0);
    sdf_reader_free(reader);
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
    const size_t data_end = sdfi_binary_header_size(SDF_ISO_2_0) + 32 * 48 * 2;
    const size_t places[] = {10, 100, data_end - 50};
    for (size_t i = 0; i < sizeof places / sizeof places[0]; ++i)
    {
        pipe_source pipe = {data, size, 0, 7, places[i]};
        const sdf_stream stream = pipe_stream(&pipe);
        sdf_reader *reader = sdf_reader_new();
        sdf_status status = sdf_open_stream(reader, &stream);
        if (status == SDF_OK)
        {
            const sdf_info *info = sdf_get_info(reader);
            double *heights =
                malloc(info->rows * info->columns * sizeof(double));
            status =
                sdf_read_heights(reader, sdf_metres(SDF_FLOAT64), heights,
                                 (ptrdiff_t)(info->columns * sizeof(double)));
            free(heights);
        }
        CHECK(status == SDF_ERR_IO);
        /* After an error in the data the reader does not go on. */
        if (status == SDF_ERR_IO && sdf_get_info(reader) != NULL)
        {
            const char *trailer = NULL;
            size_t trailer_size = 0;
            CHECK_STATUS(SDF_ERR_STATE,
                         sdf_read_trailer(reader, &trailer, &trailer_size));
        }
        sdf_reader_free(reader);
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
        sdf_reader *reader = sdf_reader_new();
        if (sdf_open_memory(reader, data, length) == SDF_OK)
        {
            const sdf_info *info = sdf_get_info(reader);
            double *heights =
                malloc(info->rows * info->columns * sizeof(double) + 1);
            const sdf_status status =
                sdf_read_heights(reader, sdf_metres(SDF_FLOAT64), heights,
                                 (ptrdiff_t)(info->columns * sizeof(double)));
            if (length < size / 2)
            {
                CHECK(status != SDF_OK);
            }
            const char *trailer = NULL;
            size_t trailer_size = 0;
            (void)sdf_read_trailer(reader, &trailer, &trailer_size);
            free(heights);
        }
        sdf_reader_free(reader);
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
        sdf_reader *reader = sdf_reader_new();
        if (sdf_open_memory(reader, copy, size) == SDF_OK)
        {
            const sdf_info *info = sdf_get_info(reader);
            const size_t count = info->rows * info->columns;
            /* A damaged count could ask for more than a test should allocate.
             */
            if (count < (size_t)1 << 20)
            {
                double *heights = malloc(count * sizeof(double) + 1);
                (void)sdf_read_heights(
                    reader, sdf_metres(SDF_FLOAT64), heights,
                    (ptrdiff_t)(info->columns * sizeof(double)));
                free(heights);
            }
            const char *trailer = NULL;
            size_t trailer_size = 0;
            (void)sdf_read_trailer(reader, &trailer, &trailer_size);
        }
        sdf_reader_free(reader);
    }
    free(copy);
    free(data);
}

static void check_header_errors(void)
{
    size_t size = 0;
    unsigned char *data = load(SAMPLES "iso2_surface_binary.sdf", &size);
    sdf_reader *reader = sdf_reader_new();
    CHECK_STATUS(SDF_ERR_FORMAT, sdf_open_memory(reader, "", 0));
    sdf_reader_free(reader);

    reader = sdf_reader_new();
    CHECK_STATUS(SDF_ERR_FORMAT,
                 sdf_open_memory(reader, "not an sdf file at all", 22));
    CHECK(strlen(sdf_reader_message(reader)) > 0);
    sdf_reader_free(reader);

    if (data != NULL)
    {
        /* The compression and the checksum bytes end the header, the data type
         * is before the checksum. */
        const size_t header = sdfi_binary_header_size(SDF_ISO_2_0);
        unsigned char *copy = malloc(size);
        memcpy(copy, data, size);
        copy[header - 3] = 1;
        reader = sdf_reader_new();
        CHECK_STATUS(SDF_ERR_UNSUPPORTED, sdf_open_memory(reader, copy, size));
        sdf_reader_free(reader);
        memcpy(copy, data, size);
        copy[header - 1] = 1;
        reader = sdf_reader_new();
        CHECK_STATUS(SDF_ERR_UNSUPPORTED, sdf_open_memory(reader, copy, size));
        sdf_reader_free(reader);
        memcpy(copy, data, size);
        copy[header - 2] = 99;
        reader = sdf_reader_new();
        CHECK_STATUS(SDF_ERR_FORMAT, sdf_open_memory(reader, copy, size));
        sdf_reader_free(reader);
        free(copy);

        /* A mark at the start is not part of the file. */
        unsigned char *marked = malloc(size + 3);
        const unsigned char mark[] = {0xEF, 0xBB, 0xBF};
        memcpy(marked, mark, 3);
        memcpy(marked + 3, data, size);
        reader = sdf_reader_new();
        CHECK_OK(sdf_open_memory(reader, marked, size + 3));
        sdf_reader_free(reader);
        free(marked);
    }
    free(data);

    reader = sdf_reader_new();
    CHECK_STATUS(SDF_ERR_IO,
                 sdf_open_file(reader, SDF_TEST_DATA_DIR "/missing.sdf"));
    CHECK(strstr(sdf_reader_message(reader), "missing.sdf") != NULL);
    CHECK(sdf_get_info(reader) == NULL);
    CHECK_STATUS(SDF_ERR_STATE, sdf_read_rows(reader, NULL, 0));
    sdf_reader_free(reader);

    CHECK_STATUS(SDF_ERR_ARGUMENT, sdf_open_file(NULL, "x"));
    sdf_reader_free(NULL);
    sdf_writer_free(NULL);
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
    sdf_info info;
    sdf_info_init(&info);
    info.columns = 2;
    info.rows = 2;
    const double heights[4] = {1, 2, 3, 4};

    sdf_writer *writer = sdf_writer_new();
    CHECK_OK(sdf_create_file(writer, path, &info));
    CHECK(file_exists(partial) && !file_exists(path));
    CHECK_OK(sdf_write_heights(writer, sdf_metres(SDF_FLOAT64), heights,
                               2 * sizeof(double)));
    CHECK_OK(sdf_write_end(writer, NULL, 0));
    sdf_writer_free(writer);
    CHECK(file_exists(path) && !file_exists(partial));

    /* An error leaves the old file as it was. */
    writer = sdf_writer_new();
    CHECK_OK(sdf_create_file(writer, path, &info));
    CHECK_OK(sdf_write_rows(writer, heights, 1));
    sdf_writer_free(writer);
    CHECK(file_exists(path) && !file_exists(partial));
    sdf_reader *reader = sdf_reader_new();
    CHECK_OK(sdf_open_file(reader, path));
    double back[4] = {0};
    CHECK_OK(sdf_read_heights(reader, sdf_metres(SDF_FLOAT64), back,
                              2 * sizeof(double)));
    CHECK(back[3] == 4.0 && back[0] == 1.0);
    sdf_reader_free(reader);
    remove(path);

    writer = sdf_writer_new();
    CHECK_STATUS(SDF_ERR_IO,
                 sdf_create_file(writer, "no_such_folder/out.sdf", &info));
    sdf_writer_free(writer);
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
    sdf_info info;
    sdf_info_init(&info);
    info.columns = 4;
    info.rows = 4;
    const sdf_stream stream = {NULL, NULL, refusing_write, NULL};
    sdf_writer *writer = sdf_writer_new();
    CHECK_OK(sdf_create_stream(writer, &stream, &info));
    double heights[16] = {0};
    /* A small file is buffered: the failure shows when it is flushed. */
    CHECK_OK(sdf_write_heights(writer, sdf_metres(SDF_FLOAT64), heights,
                               4 * sizeof(double)));
    CHECK_STATUS(SDF_ERR_IO, sdf_write_end(writer, NULL, 0));
    CHECK_STATUS(SDF_ERR_STATE, sdf_write_end(writer, NULL, 0));
    sdf_writer_free(writer);
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
