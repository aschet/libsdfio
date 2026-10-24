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

static sdfio_status describe_surface(sdfio_header *header, sdfio_data_type type)
{
    sdfio_status status = sdfio_header_set_now(header);
    if (status == SDFIO_OK)
    {
        status = sdfio_header_set_data_type(header, type);
    }
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
        status = sdfio_header_set_x_scale(header, 1e-6);
    }
    if (status == SDFIO_OK)
    {
        status = sdfio_header_set_y_scale(header, 1e-6);
    }
    return status;
}

static int write_surface(const char *path, sdfio_data_type type)
{
    sdfio_header *header = NULL;
    sdfio_status status =
        sdfio_header_create(sdfio_default_allocator(), &header);
    if (status == SDFIO_OK)
    {
        status = describe_surface(header, type);
    }
    sdfio_writer *writer = NULL;
    if (status == SDFIO_OK)
    {
        status = sdfio_writer_create(sdfio_default_allocator(), &writer);
    }
    if (status == SDFIO_OK)
    {
        status = sdfio_writer_add_binary(writer);
    }
    if (status == SDFIO_OK)
    {
        status = sdfio_create_file(writer, path, header);
    }
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
    sdfio_header_free(header);
    return status == SDFIO_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}

int main(void)
{
    generate_heights();
    const int floats = write_surface("float.sdf", SDFIO_TYPE_FLOAT);
    const int integers = write_surface("integer.sdf", SDFIO_TYPE_INT16);
    return floats == EXIT_SUCCESS ? integers : floats;
}
