#include <sdf/sdf.h>

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
    sdf_info info;
    sdf_info_init(&info);
    info.data_type = SDF_DATA_INT16;
    info.columns = COLUMNS;
    info.rows = ROWS;
    info.x_scale = 1e-6;
    info.y_scale = 1e-6;

    float *heights = sdf_alloc_heights(&info, SDF_FLOAT32, info.rows);
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
    sdf_writer *writer = sdf_writer_new();
    sdf_status status = sdf_create_file(writer, path, &info);
    if (status == SDF_OK)
    {
        status = sdf_write_heights(writer, sdf_metres(SDF_FLOAT32), heights,
                                   COLUMNS * sizeof(float));
    }
    if (status == SDF_OK)
    {
        status = sdf_write_end(writer, trailer, strlen(trailer));
    }
    if (status != SDF_OK)
    {
        fprintf(stderr, "%s\n", sdf_writer_message(writer));
    }
    sdf_writer_free(writer);
    sdf_free(heights);
    return status == SDF_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}
