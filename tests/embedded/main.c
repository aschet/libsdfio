/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* A scanner that writes a binary file into RAM from a pool of its own, as
 * firmware: no file system, no heap. It is linked without a start
 * file to see what the library asks of the C library, and never run. */

#include <sdfio/sdfio.h>

#include <stdint.h>
#include <time.h>

enum
{
    COLUMNS = 16,
    ROWS = 8,
    POOL_SIZE = 4096,
    IMAGE_SIZE = 2048
};

typedef union pool_cell
{
    long long integer;
    double real;
    void *pointer;
} pool_cell;

static pool_cell pool[POOL_SIZE / sizeof(pool_cell)];
static size_t pool_used;
static unsigned char image[IMAGE_SIZE];
static size_t image_used;
static volatile size_t result;

/* The clock of the scanner, which sdfio_header_init() reads for the dates. */
time_t time(time_t *timer)
{
    static const time_t scan_time = 1700000000;
    if (timer != NULL)
    {
        *timer = scan_time;
    }
    return scan_time;
}

static void *take(void *user, size_t size)
{
    (void)user;
    const size_t rounded =
        (size + sizeof(pool_cell) - 1) / sizeof(pool_cell) * sizeof(pool_cell);
    if (rounded > sizeof pool - pool_used)
    {
        return NULL;
    }
    void *memory = (unsigned char *)pool + pool_used;
    pool_used += rounded;
    return memory;
}

static void give(void *user, void *memory)
{
    (void)user;
    (void)memory;
}

static size_t store(void *user, const void *data, size_t size)
{
    (void)user;
    if (size > sizeof image - image_used)
    {
        return 0;
    }
    for (size_t i = 0; i < size; ++i)
    {
        image[image_used + i] = ((const unsigned char *)data)[i];
    }
    image_used += size;
    return size;
}

int main(void)
{
    static int16_t counts[COLUMNS];
    const sdfio_allocator allocator = {NULL, take, give};
    const sdfio_config config = {&allocator, 128, 16, NULL};
    sdfio_writer *writer = sdfio_writer_new(&config);
    sdfio_header header;
    sdfio_header_init(&header);
    header.data_type = SDFIO_DATA_INT16;
    header.columns = COLUMNS;
    header.rows = ROWS;
    header.x_scale = 1.0e-6;
    header.y_scale = 1.0e-6;
    header.z_scale = 1.0e-9;
    const sdfio_stream stream = {NULL, NULL, store};
    sdfio_status status = sdfio_create_stream(writer, &stream, &header);
    for (size_t row = 0; row < ROWS && status == SDFIO_OK; ++row)
    {
        for (size_t column = 0; column < COLUMNS; ++column)
        {
            counts[column] = (int16_t)(row * COLUMNS + column);
        }
        status = sdfio_write_data(writer, counts, 1);
    }
    if (status == SDFIO_OK)
    {
        status = sdfio_write_trailer(writer, NULL, 0);
    }
    sdfio_writer_free(writer);
    result = status == SDFIO_OK ? image_used : 0;
    for (;;)
    {
    }
}
