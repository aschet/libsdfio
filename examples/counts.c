#include <sdf/sdf.h>

#include <stdio.h>
#include <stdlib.h>

enum
{
    COLUMNS = 512,
    ROWS = 64
};

/* A scanner that delivers integer counts: they stay counts from the sensor to
 * the file, row by row. */
static void scan(int16_t *row, size_t index)
{
    for (size_t column = 0; column < COLUMNS; ++column)
    {
        row[column] = (int16_t)(((index * 31 + column * 7) % 4000) - 2000);
    }
}

int main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "counts.sdf";
    sdf_info info;
    sdf_info_init(&info);
    info.data_type = SDF_DATA_INT16;
    info.columns = COLUMNS;
    info.rows = ROWS;
    info.x_scale = 2e-6;
    info.y_scale = 2e-6;
    info.z_scale =
        sdf_z_scale_for_range(info.data_type, info.revision, -80e-6, 80e-6);
    info.z_resolution = info.z_scale;

    sdf_writer *writer = sdf_writer_new();
    sdf_status status = sdf_create_file(writer, path, &info);
    int16_t row[COLUMNS];
    for (size_t index = 0; index < ROWS && status == SDF_OK; ++index)
    {
        scan(row, index);
        status = sdf_write_rows(writer, row, 1);
    }
    if (status == SDF_OK)
    {
        status = sdf_write_end(writer, NULL, 0);
    }
    if (status != SDF_OK)
    {
        fprintf(stderr, "%s\n", sdf_writer_message(writer));
    }
    sdf_writer_free(writer);
    return status == SDF_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}
