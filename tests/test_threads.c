/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* Threads with handles of their own: some read files, some write them, none
 * shares a handle. Built with ThreadSanitizer this shows a race in the
 * library. */

#include "check.h"
#include <stdlib.h>

#include <sdfio/file.h>

#include <pthread.h>

#define SAMPLES SDFIO_TEST_DATA_DIR "/samples/iso25178-71-2026/"

enum
{
    THREADS = 8,
    ROUNDS = 25,
    ROWS = 9,
    COLUMNS = 13
};

static const char *const files[] = {
    SAMPLES "iso1_surface_binary.sdf",
    SAMPLES "iso2_surface_ascii.sdf",
    SAMPLES "iso2_surface_binary32_binary.sdf",
    SAMPLES "iso1_surface_ascii.sdf",
    SAMPLES "iso2_surface_binary.sdf",
    SAMPLES "iso1_profile_ascii.sdf",
};
#define FILE_COUNT (sizeof files / sizeof files[0])

typedef struct job
{
    size_t id;
    int errors;
} job;

/* The sum of the heights of a file, or NaN if it cannot be read. */
static double read_sum(const char *path, sdfio_reader *reader)
{
    if (sdfio_open_file(reader, path) != SDFIO_OK)
    {
        return NAN;
    }
    const sdfio_header *header = sdfio_get_header(reader);
    const size_t count = header->rows * header->columns;
    double *heights = malloc(count * sizeof *heights);
    double sum = NAN;
    if (heights != NULL
        && sdfio_read_heights(reader, sdfio_layout_packed(SDFIO_TYPE_DOUBLE),
                              heights)
               == SDFIO_OK)
    {
        sum = 0.0;
        for (size_t i = 0; i < count; ++i)
        {
            sum += isfinite(heights[i]) ? heights[i] : 0.0;
        }
        const char *trailer = NULL;
        size_t size = 0;
        if (sdfio_read_trailer(reader, &trailer, &size) != SDFIO_OK)
        {
            sum = NAN;
        }
    }
    free(heights);
    return sum;
}

static double expected_sums[FILE_COUNT];

static void *read_files(void *argument)
{
    job *self = argument;
    for (int round = 0; round < ROUNDS; ++round)
    {
        const size_t index = (self->id + (size_t)round) % FILE_COUNT;
        sdfio_reader *reader = sdfio_reader_new(all_formats());
        const double sum = read_sum(files[index], reader);
        if (isnan(sum) || sum != expected_sums[index])
        {
            ++self->errors;
        }
        sdfio_reader_free(reader);
    }
    return NULL;
}

static void *write_and_read(void *argument)
{
    job *self = argument;
    const sdfio_revision revisions[] = {SDFIO_ISO_1_0, SDFIO_ISO_2_0};
    for (int round = 0; round < ROUNDS; ++round)
    {
        sdfio_header header;
        sdfio_header_init(&header);
        header.revision = revisions[(self->id + (size_t)round) % 2];
        header.representation =
            (self->id + (size_t)round) % 2 == 0 ? SDFIO_BINARY : SDFIO_ASCII;
        header.data_type = SDFIO_TYPE_INT32;
        header.columns = COLUMNS;
        header.rows = ROWS;
        double heights[ROWS * COLUMNS];
        for (size_t i = 0; i < ROWS * COLUMNS; ++i)
        {
            heights[i] = 1e-6 * (double)(i + self->id);
        }
        membuf file = {0};
        const sdfio_stream stream = membuf_stream(&file);
        sdfio_writer *writer = sdfio_writer_new(all_formats());
        if (sdfio_create_stream(writer, &stream, &header) != SDFIO_OK
            || sdfio_write_heights(writer, SDFIO_TYPE_DOUBLE, heights)
                   != SDFIO_OK
            || sdfio_end_trailer(writer) != SDFIO_OK)
        {
            ++self->errors;
        }
        sdfio_writer_free(writer);

        double back[ROWS * COLUMNS];
        sdfio_reader *reader = sdfio_reader_new(all_formats());
        if (sdfio_open_memory(reader, file.data, file.size) != SDFIO_OK
            || sdfio_read_heights(reader,
                                  sdfio_layout_packed(SDFIO_TYPE_DOUBLE), back)
                   != SDFIO_OK)
        {
            ++self->errors;
        }
        else
        {
            for (size_t i = 0; i < ROWS * COLUMNS; ++i)
            {
                if (fabs(back[i] - heights[i]) > 1e-9)
                {
                    ++self->errors;
                    break;
                }
            }
        }
        sdfio_reader_free(reader);
        free(file.data);
    }
    return NULL;
}

/* Several threads read the same bytes at once, each with a reader. */
static unsigned char *shared_bytes;
static size_t shared_size;

static void *read_shared(void *argument)
{
    job *self = argument;
    for (int round = 0; round < ROUNDS; ++round)
    {
        sdfio_reader *reader = sdfio_reader_new(all_formats());
        if (sdfio_open_memory(reader, shared_bytes, shared_size) != SDFIO_OK)
        {
            ++self->errors;
        }
        else
        {
            const sdfio_header *header = sdfio_get_header(reader);
            double *heights =
                malloc(header->rows * header->columns * sizeof *heights);
            if (sdfio_read_heights(
                    reader, sdfio_layout_packed(SDFIO_TYPE_DOUBLE), heights)
                != SDFIO_OK)
            {
                ++self->errors;
            }
            free(heights);
        }
        sdfio_reader_free(reader);
    }
    return NULL;
}

int main(void)
{
    for (size_t i = 0; i < FILE_COUNT; ++i)
    {
        sdfio_reader *reader = sdfio_reader_new(all_formats());
        expected_sums[i] = read_sum(files[i], reader);
        CHECK(!isnan(expected_sums[i]));
        sdfio_reader_free(reader);
    }
    FILE *file = fopen(files[1], "rb");
    CHECK(file != NULL);
    if (file == NULL)
    {
        return finish_tests();
    }
    shared_bytes = malloc(1 << 20);
    shared_size = fread(shared_bytes, 1, 1 << 20, file);
    fclose(file);

    pthread_t threads[THREADS];
    job jobs[THREADS];
    for (size_t i = 0; i < THREADS; ++i)
    {
        jobs[i] = (job){i, 0};
        void *(*work)(void *) = i % 3 == 0   ? read_files
                                : i % 3 == 1 ? write_and_read
                                             : read_shared;
        CHECK(pthread_create(&threads[i], NULL, work, &jobs[i]) == 0);
    }
    for (size_t i = 0; i < THREADS; ++i)
    {
        pthread_join(threads[i], NULL);
        CHECK(jobs[i].errors == 0);
    }
    free(shared_bytes);
    return finish_tests();
}
