#include <sdfio/sdfio.h>

#include <stdio.h>
#include <stdlib.h>

enum
{
    COLUMNS = 16,
    ROWS = 8,
    POOL_SIZE = 4096,
    FILE_SIZE = 2048,
    BUFFER_SIZE = 128,
    CHUNK_VALUES = 16
};

/* The memory of a scanner without a heap: a pool that hands out pieces in
 * order and takes none back. The cell gives every piece the alignment of any
 * object. */
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

static sdfio_status describe(sdfio_header *header)
{
    sdfio_status status = sdfio_header_set_data_type(header, SDFIO_TYPE_INT16);
    if (status == SDFIO_OK)
    {
        status = sdfio_header_set_columns(header, COLUMNS);
    }
    if (status == SDFIO_OK)
    {
        status = sdfio_header_set_rows(header, ROWS);
    }
    if (status == SDFIO_OK)
    {
        status = sdfio_header_set_z_scale(header, 1.0e-9);
    }
    return status;
}

static int16_t value_at(size_t row, size_t column)
{
    return (int16_t)(row * COLUMNS + column);
}

/* The writer and the header take their memory from the pool, with a small
 * buffer and a small chunk, so that the pool stays small. */
static sdfio_status write_file(void)
{
    const sdfio_allocator allocator = {NULL, take, give};
    sdfio_writer *writer = NULL;
    sdfio_header *header = NULL;
    sdfio_status status = sdfio_writer_create(&allocator, &writer);
    if (status == SDFIO_OK)
    {
        status = sdfio_writer_add_binary_format(writer);
    }
    if (status == SDFIO_OK)
    {
        status = sdfio_writer_set_buffer_size(writer, BUFFER_SIZE);
    }
    if (status == SDFIO_OK)
    {
        status = sdfio_writer_set_chunk_values(writer, CHUNK_VALUES);
    }
    if (status == SDFIO_OK)
    {
        status = sdfio_header_create(&allocator, &header);
    }
    if (status == SDFIO_OK)
    {
        status = describe(header);
    }
    const sdfio_stream stream = {NULL, NULL, store};
    if (status == SDFIO_OK)
    {
        status = sdfio_create_stream(writer, &stream, header);
    }
    int16_t coded[COLUMNS];
    const sdfio_layout layout = sdfio_layout_packed(SDFIO_TYPE_INT16);
    for (size_t row = 0; row < ROWS && status == SDFIO_OK; ++row)
    {
        for (size_t column = 0; column < COLUMNS; ++column)
        {
            coded[column] = value_at(row, column);
        }
        status = sdfio_write_data(writer, layout, coded, 1);
    }
    if (status == SDFIO_OK)
    {
        status = sdfio_end_trailer(writer);
    }
    if (status != SDFIO_OK && writer != NULL)
    {
        fprintf(stderr, "%s\n", sdfio_writer_error_message(writer));
    }
    sdfio_writer_free(writer);
    sdfio_header_free(header);
    return status;
}

/* The reader takes its memory from the same pool, which is empty again because
 * the writer is gone. The program counts the values that differ. */
static sdfio_status read_file(size_t *different)
{
    const sdfio_allocator allocator = {NULL, take, give};
    sdfio_reader *reader = NULL;
    sdfio_status status = sdfio_reader_create(&allocator, &reader);
    if (status == SDFIO_OK)
    {
        status = sdfio_reader_add_binary_format(reader);
    }
    if (status == SDFIO_OK)
    {
        status = sdfio_reader_set_buffer_size(reader, BUFFER_SIZE);
    }
    if (status == SDFIO_OK)
    {
        status = sdfio_reader_set_chunk_values(reader, CHUNK_VALUES);
    }
    if (status == SDFIO_OK)
    {
        status = sdfio_open_memory(reader, file, file_size);
    }
    int16_t coded[COLUMNS];
    const sdfio_layout layout = sdfio_layout_packed(SDFIO_TYPE_INT16);
    *different = 0;
    for (size_t row = 0; row < ROWS && status == SDFIO_OK; ++row)
    {
        status = sdfio_read_data(reader, layout, coded, 1);
        for (size_t column = 0; column < COLUMNS && status == SDFIO_OK;
             ++column)
        {
            *different += coded[column] != value_at(row, column) ? 1U : 0U;
        }
    }
    if (status != SDFIO_OK && reader != NULL)
    {
        fprintf(stderr, "%s\n", sdfio_reader_error_message(reader));
    }
    sdfio_reader_free(reader);
    return status;
}

/* Writes a file into RAM and reads it back, with every byte of library memory
 * from a pool of the program. The same scenario is linked as firmware in
 * tests/embedded/main.c, without the heap. */
int main(void)
{
    sdfio_status status = write_file();
    const size_t written_pool = pool_used;
    size_t different = 0;
    if (status == SDFIO_OK)
    {
        pool_used = 0;
        status = read_file(&different);
    }
    if (status != SDFIO_OK)
    {
        fprintf(stderr, "%s\n", sdfio_status_string(status));
        return EXIT_FAILURE;
    }
    printf("%zu bytes of file; pool of %d bytes: %zu used by the writer, %zu "
           "by the reader; %zu values differ\n",
           file_size, POOL_SIZE, written_pool, pool_used, different);
    return different == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
