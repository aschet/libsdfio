#include <sdfio/file.h>
#include <sdfio/sdfio.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

enum
{
    COLUMNS = 256,
    ROWS = 192
};

static float heights[ROWS * COLUMNS];

static void fill(void)
{
    for (size_t row = 0; row < ROWS; ++row)
    {
        for (size_t column = 0; column < COLUMNS; ++column)
        {
            const bool hole = row > 100 && row < 110 && column < 40;
            heights[row * COLUMNS + column] =
                hole ? NAN
                     : (float)(2e-6 * sin(0.1 * (double)row)
                               * cos(0.07 * (double)column));
        }
    }
}

static int write_surface(const char *path, sdfio_data_type type)
{
    sdfio_header header;
    sdfio_header_init(&header);
    header.data_type = type;
    header.columns = COLUMNS;
    header.rows = ROWS;
    header.x_scale = 1e-6;
    header.y_scale = 1e-6;

    char memory[256];
    sdfio_trailer trailer;
    sdfio_trailer_init(&trailer, memory, sizeof memory);
    sdfio_status status =
        sdfio_trailer_add_field(&trailer, "OperatorName", "Jane Doe");
    if (status == SDFIO_OK)
    {
        status = sdfio_trailer_add_field(&trailer, "Magnification", "20.5");
    }
    sdfio_writer *writer = sdfio_writer_new(NULL);
    if (status == SDFIO_OK)
    {
        status = sdfio_create_file(writer, path, &header);
    }
    if (status == SDFIO_OK)
    {
        status = sdfio_write_heights(writer, sdfio_metres(SDFIO_FLOAT32),
                                     heights, COLUMNS * sizeof(float));
    }
    if (status == SDFIO_OK)
    {
        status = sdfio_write_trailer(writer, trailer.text, trailer.size);
    }
    if (status != SDFIO_OK)
    {
        fprintf(stderr, "%s\n", sdfio_writer_error_message(writer));
    }
    sdfio_writer_free(writer);
    return status == SDFIO_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}

int main(void)
{
    fill();
    const int floats = write_surface("float.sdf", SDFIO_DATA_BINARY32);
    const int integers = write_surface("integer.sdf", SDFIO_DATA_INT16);
    return floats == EXIT_SUCCESS ? integers : floats;
}
