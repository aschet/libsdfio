#include <sdfio/sdfio.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    COLUMNS = 256,
    ROWS = 192
};

int main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "surface.sdf";
    sdfio_header header;
    sdfio_header_init(&header);
    header.data_type = SDFIO_DATA_INT16;
    header.columns = COLUMNS;
    header.rows = ROWS;
    header.x_scale = 1e-6;
    header.y_scale = 1e-6;

    float *heights = sdfio_alloc_heights(&header, SDFIO_FLOAT32, header.rows);
    if (heights == NULL)
    {
        return EXIT_FAILURE;
    }
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

    const char trailer[] = "OperatorName = Jane Doe";
    sdfio_writer *writer = sdfio_writer_new();
    sdfio_status status = sdfio_create_file(writer, path, &header);
    if (status == SDFIO_OK)
    {
        status = sdfio_write_heights(writer, sdfio_metres(SDFIO_FLOAT32),
                                     heights, COLUMNS * sizeof(float));
    }
    if (status == SDFIO_OK)
    {
        status = sdfio_write_trailer(writer, trailer, strlen(trailer));
    }
    if (status != SDFIO_OK)
    {
        fprintf(stderr, "%s\n", sdfio_writer_message(writer));
    }
    sdfio_writer_free(writer);
    sdfio_free(heights);
    return status == SDFIO_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}
