#include <sdfio/sdfio.h>

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
    sdfio_header header;
    sdfio_header_init(&header);
    header.data_type = SDFIO_DATA_INT16;
    header.columns = COLUMNS;
    header.rows = ROWS;
    header.x_scale = 2e-6;
    header.y_scale = 2e-6;
    header.z_scale = sdfio_z_scale_for_range(header.data_type, header.revision,
                                             -80e-6, 80e-6);
    header.z_resolution = header.z_scale;

    sdfio_writer *writer = sdfio_writer_new();
    sdfio_status status = sdfio_create_file(writer, path, &header);
    int16_t row[COLUMNS];
    for (size_t index = 0; index < ROWS && status == SDFIO_OK; ++index)
    {
        scan(row, index);
        status = sdfio_write_data(writer, row, 1);
    }
    if (status == SDFIO_OK)
    {
        status = sdfio_write_trailer(writer, NULL, 0);
    }
    if (status != SDFIO_OK)
    {
        fprintf(stderr, "%s\n", sdfio_writer_error_message(writer));
    }
    sdfio_writer_free(writer);
    return status == SDFIO_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}
