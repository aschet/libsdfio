/* Track the memory use of the library with a custom allocator. */

#include <sdfio/file.h>
#include <sdfio/sdfio.h>

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

/* The memory that the library has taken: now, at most at once, and the number
 * of blocks. */
typedef struct usage
{
    size_t current;
    size_t peak;
    size_t blocks;
} usage;

/* Stored in front of every block, with the size of the block: the function that
 * releases a block is not told the size, and subtracts it from the memory in
 * use. The union keeps the block itself aligned for any type. */
typedef union block_header
{
    size_t size;
    long long integer;
    long double real;
    void *pointer;
} block_header;

static void *counting_allocate(void *user, size_t size)
{
    usage *memory = user;
    block_header *block = malloc(sizeof *block + size);
    if (block == NULL)
    {
        return NULL;
    }
    block->size = size;
    memory->current += size;
    memory->peak =
        memory->current > memory->peak ? memory->current : memory->peak;
    ++memory->blocks;
    return block + 1;
}

static void counting_release(void *user, void *data)
{
    usage *memory = user;
    block_header *block = (block_header *)data - 1;
    memory->current -= block->size;
    free(block);
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "usage: %s input.sdf\n", argv[0]);
        return EXIT_FAILURE;
    }
    usage memory = {0, 0, 0};
    const sdfio_allocator allocator = {&memory, counting_allocate,
                                       counting_release};
    sdfio_reader *reader = NULL;
    sdfio_reader_create(&allocator, &reader);
    sdfio_reader_add_binary(reader);
    sdfio_reader_add_ascii(reader);
    const sdfio_status status = sdfio_open_file(reader, argv[1]);
    if (status != SDFIO_OK)
    {
        fprintf(stderr, "%s: %s\n", sdfio_status_string(status),
                sdfio_reader_error_message(reader));
        sdfio_reader_free(reader);
        return EXIT_FAILURE;
    }
    const sdfio_header *header = sdfio_reader_header(reader);
    printf("%lu x %lu points, %zu blocks of the library, %zu bytes at most\n",
           (unsigned long)sdfio_header_columns(header),
           (unsigned long)sdfio_header_rows(header), memory.blocks,
           memory.peak);
    sdfio_reader_free(reader);
    printf("%zu bytes left after the reader is freed\n", memory.current);
    return memory.current == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
