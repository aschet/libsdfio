#include <sdfio/sdfio.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define COLUMNS 64
#define ROWS 48

typedef struct buffer
{
    unsigned char *data;
    size_t size;
    size_t capacity;
} buffer;

/* Appends to the buffer, which does not grow. */
static size_t append(void *user, const void *data, size_t size)
{
    buffer *file = user;
    if (size > file->capacity - file->size)
    {
        return 0;
    }
    memcpy(file->data + file->size, data, size);
    file->size += size;
    return size;
}

int main(void)
{
    static float heights[ROWS * COLUMNS];
    for (size_t row = 0; row < ROWS; ++row)
    {
        for (size_t column = 0; column < COLUMNS; ++column)
        {
            heights[row * COLUMNS + column] =
                (float)(1e-6 * sin(0.2 * (double)row)
                        * cos(0.2 * (double)column));
        }
    }

    sdfio_header *header = NULL;
    sdfio_header_create(sdfio_default_allocator(), &header);
    sdfio_header_set_dates(header, time(NULL));
    sdfio_header_set_revision(header, SDFIO_ISO_2_0);
    sdfio_header_set_data_type(header, SDFIO_TYPE_FLOAT);
    sdfio_header_set_columns(header, COLUMNS);
    sdfio_header_set_rows(header, ROWS);
    sdfio_header_set_x_scale(header, 1e-6);
    sdfio_header_set_y_scale(header, 1e-6);

    sdfio_writer *writer = NULL;
    sdfio_writer_create(sdfio_default_allocator(), &writer);
    sdfio_writer_add_binary(writer);

    size_t capacity = 0;
    buffer file = {sdfio_writer_estimate_size(writer, header, &capacity)
                           == SDFIO_OK
                       ? malloc(capacity)
                       : NULL,
                   0, capacity};
    if (file.data == NULL)
    {
        fprintf(stderr, "the file does not fit in memory\n");
        sdfio_writer_free(writer);
        sdfio_header_free(header);
        return EXIT_FAILURE;
    }
    const sdfio_stream stream = {&file, NULL, append};
    sdfio_status status = sdfio_create_stream(writer, &stream, header);
    if (status == SDFIO_OK)
    {
        status = sdfio_write_heights(
            writer, sdfio_layout_packed(SDFIO_TYPE_FLOAT), heights);
    }
    if (status == SDFIO_OK)
    {
        status = sdfio_end_file(writer);
    }
    if (status != SDFIO_OK)
    {
        fprintf(stderr, "%s\n", sdfio_writer_error_message(writer));
    }
    else
    {
        printf("%zu bytes of file in a buffer of %zu bytes\n", file.size,
               file.capacity);
    }
    free(file.data);
    sdfio_writer_free(writer);
    sdfio_header_free(header);
    return status == SDFIO_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}
