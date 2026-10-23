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

/* Generates a ripple of heights in metres, with a region of points that are not
 * measured, which are NaN. */
static void generate_heights(void)
{
    for (size_t row = 0; row < ROWS; ++row)
    {
        for (size_t column = 0; column < COLUMNS; ++column)
        {
            const bool not_measured = row > 100 && row < 110 && column < 40;
            heights[row * COLUMNS + column] =
                not_measured ? NAN
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

    const sdfio_config config = {
        .writer_formats = {&sdfio_binary_writer_format}};
    sdfio_writer *writer = sdfio_writer_new(&config);
    sdfio_status status = sdfio_create_file(writer, path, &header);
    if (status == SDFIO_OK)
    {
        status = sdfio_write_heights(
            writer, sdfio_layout_packed(SDFIO_TYPE_FLOAT), heights);
    }
    if (status == SDFIO_OK)
    {
        status = sdfio_write_trailer_field(writer, "OperatorName", "Jane Doe");
    }
    if (status == SDFIO_OK)
    {
        status = sdfio_write_trailer_field(writer, "Magnification", "20.5");
    }
    if (status == SDFIO_OK)
    {
        status = sdfio_end_trailer(writer);
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
    generate_heights();
    const int floats = write_surface("float.sdf", SDFIO_TYPE_FLOAT);
    const int integers = write_surface("integer.sdf", SDFIO_TYPE_INT16);
    return floats == EXIT_SUCCESS ? integers : floats;
}
