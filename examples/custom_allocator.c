#include <sdfio/sdfio.h>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define COLUMNS 16
#define ROWS 8
#define POOL_SIZE 4096
#define FILE_SIZE 2048
#define BUFFER_SIZE 128
#define CHUNK_VALUES 16

/* A pool that hands out pieces in order and takes none back. The cell gives
 * every piece the alignment of any object. */
typedef union pool_cell
{
    long long integer;
    double real;
    void *pointer;
} pool_cell;

static pool_cell pool[POOL_SIZE / sizeof(pool_cell)];
static size_t pool_used;

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

/* The file lives in RAM: a stream writes into an array. */
static unsigned char file[FILE_SIZE];
static size_t file_size;

static size_t store(void *user, const void *data, size_t size)
{
    (void)user;
    if (size > sizeof file - file_size)
    {
        return 0;
    }
    for (size_t i = 0; i < size; ++i)
    {
        file[file_size + i] = ((const unsigned char *)data)[i];
    }
    file_size += size;
    return size;
}

/* The dates are a constant, so that the program does not read the clock. */
static void init_header(sdfio_header *header)
{
    const time_t date = 1700000000;
    sdfio_header_set_created(header, date);
    sdfio_header_set_modified(header, date);
    sdfio_header_set_data_type(header, SDFIO_TYPE_INT16);
    sdfio_header_set_columns(header, COLUMNS);
    sdfio_header_set_rows(header, ROWS);
    sdfio_header_set_z_scale(header, 1.0e-9);
}

/* The writer and the header take their memory from the pool, with a small
 * buffer and a small chunk, so that the pool stays small. The pool is the one
 * resource that can run out, so the calls that allocate are checked. */
static sdfio_status write_file(void)
{
    const sdfio_allocator allocator = {NULL, take, give};
    sdfio_writer *writer = NULL;
    sdfio_header *header = NULL;
    const sdfio_stream stream = {NULL, NULL, store};
    sdfio_status status = sdfio_writer_create(&allocator, &writer);
    if (status == SDFIO_OK)
    {
        status = sdfio_header_create(&allocator, &header);
    }
    if (status == SDFIO_OK)
    {
        sdfio_writer_add_binary(writer);
        sdfio_writer_set_buffer(writer, BUFFER_SIZE);
        sdfio_writer_set_chunk(writer, CHUNK_VALUES);
        init_header(header);
        status = sdfio_create_stream(writer, &stream, header);
    }
    int16_t coded[COLUMNS];
    const sdfio_layout layout = *sdfio_layout_packed(SDFIO_TYPE_INT16);
    for (size_t row = 0; row < ROWS && status == SDFIO_OK; ++row)
    {
        const size_t first = row * COLUMNS;
        for (size_t column = 0; column < COLUMNS; ++column)
        {
            coded[column] = (int16_t)(first + column);
        }
        status = sdfio_write_data(writer, &layout, coded, 1);
    }
    if (status == SDFIO_OK)
    {
        status = sdfio_end_file(writer);
    }
    sdfio_writer_free(writer);
    sdfio_header_free(header);
    return status;
}

/* Writes a file into RAM with every byte of library memory from a pool of the
 * program. The function prints nothing and is not static: the firmware job of
 * the continuous integration links this file with it as the entry point, so
 * that the image holds neither the output functions of the C library nor its
 * heap. */
sdfio_status custom_allocator(void);

sdfio_status custom_allocator(void)
{
    return write_file();
}

int main(void)
{
    const sdfio_status status = custom_allocator();
    if (status != SDFIO_OK)
    {
        fprintf(stderr, "%s\n", sdfio_status_string(status));
        return EXIT_FAILURE;
    }
    printf("%zu bytes of file; %zu of the %d bytes of the pool used\n",
           file_size, pool_used, POOL_SIZE);
    return EXIT_SUCCESS;
}
