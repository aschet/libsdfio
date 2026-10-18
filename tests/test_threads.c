/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* Threads with handles of their own: some read files, some write them, none
 * shares a handle. Built with ThreadSanitizer this shows a race in the
 * library. */

#include "check.h"

#include <pthread.h>

#define SAMPLES SDF_TEST_DATA_DIR "/samples/iso25178-71-2026/"
#define BCR_SAMPLES SDF_TEST_DATA_DIR "/samples/bcr-1.0/"

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
    BCR_SAMPLES "bcr1_surface_ascii.sdf",
    BCR_SAMPLES "bcr1_surface_binary.sdf",
    SAMPLES "iso1_profile_ascii.sdf",
};
#define FILE_COUNT (sizeof files / sizeof files[0])

typedef struct job
{
    size_t id;
    int errors;
} job;

/* The sum of the heights of a file, or NaN if it cannot be read. */
static double read_sum(const char *path, sdf_reader *reader)
{
    if (sdf_open_file(reader, path) != SDF_OK)
    {
        return NAN;
    }
    const sdf_info *info = sdf_get_info(reader);
    const size_t count = info->rows * info->columns;
    double *heights = malloc(count * sizeof *heights);
    double sum = NAN;
    if (heights != NULL
        && sdf_read_heights(reader, sdf_metres(SDF_FLOAT64), heights,
                            (ptrdiff_t)(info->columns * sizeof(double)))
               == SDF_OK)
    {
        sum = 0.0;
        for (size_t i = 0; i < count; ++i)
        {
            sum += isfinite(heights[i]) ? heights[i] : 0.0;
        }
        const char *trailer = NULL;
        size_t size = 0;
        if (sdf_read_trailer(reader, &trailer, &size) != SDF_OK)
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
        sdf_reader *reader = sdf_reader_new();
        const double sum = read_sum(files[index], reader);
        if (isnan(sum) || sum != expected_sums[index])
        {
            ++self->errors;
        }
        sdf_reader_free(reader);
    }
    return NULL;
}

static void *write_and_read(void *argument)
{
    job *self = argument;
    const sdf_revision revisions[] = {SDF_ISO_1_0, SDF_ISO_2_0, SDF_BCR_1_0};
    for (int round = 0; round < ROUNDS; ++round)
    {
        sdf_info info;
        sdf_info_init(&info);
        info.revision = revisions[(self->id + (size_t)round) % 3];
        info.storage =
            (self->id + (size_t)round) % 2 == 0 ? SDF_BINARY : SDF_ASCII;
        info.data_type = SDF_DATA_INT32;
        info.columns = COLUMNS;
        info.rows = ROWS;
        double heights[ROWS * COLUMNS];
        for (size_t i = 0; i < ROWS * COLUMNS; ++i)
        {
            heights[i] = 1e-6 * (double)(i + self->id);
        }
        membuf file = {0};
        const sdf_stream stream = membuf_stream(&file);
        sdf_writer *writer = sdf_writer_new();
        if (sdf_create_stream(writer, &stream, &info) != SDF_OK
            || sdf_write_heights(writer, sdf_metres(SDF_FLOAT64), heights,
                                 COLUMNS * sizeof(double))
                   != SDF_OK
            || sdf_write_end(writer, NULL, 0) != SDF_OK)
        {
            ++self->errors;
        }
        sdf_writer_free(writer);

        double back[ROWS * COLUMNS];
        sdf_reader *reader = sdf_reader_new();
        if (sdf_open_memory(reader, file.data, file.size) != SDF_OK
            || sdf_read_heights(reader, sdf_metres(SDF_FLOAT64), back,
                                COLUMNS * sizeof(double))
                   != SDF_OK)
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
        sdf_reader_free(reader);
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
        sdf_reader *reader = sdf_reader_new();
        if (sdf_open_memory(reader, shared_bytes, shared_size) != SDF_OK)
        {
            ++self->errors;
        }
        else
        {
            const sdf_info *info = sdf_get_info(reader);
            double *heights =
                malloc(info->rows * info->columns * sizeof *heights);
            if (sdf_read_heights(reader, sdf_metres(SDF_FLOAT64), heights,
                                 (ptrdiff_t)(info->columns * sizeof(double)))
                != SDF_OK)
            {
                ++self->errors;
            }
            free(heights);
        }
        sdf_reader_free(reader);
    }
    return NULL;
}

int main(void)
{
    for (size_t i = 0; i < FILE_COUNT; ++i)
    {
        sdf_reader *reader = sdf_reader_new();
        expected_sums[i] = read_sum(files[i], reader);
        CHECK(!isnan(expected_sums[i]));
        sdf_reader_free(reader);
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
