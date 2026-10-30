/* Write an SDF file with the coded values API. */

#include <sdfio/file.h>
#include <sdfio/sdfio.h>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define COLUMNS 512
#define ROWS 64
#define DROPOUT_EVERY 97

/* Generates the coded values of the row y: a pattern in which a point is not
 * measured at intervals of DROPOUT_EVERY points. */
static void generate_coded_row(int16_t *row, size_t y)
{
    const size_t row_offset = y * 31;
    for (size_t column = 0; column < COLUMNS; ++column)
    {
        if ((column + y) % DROPOUT_EVERY == 0)
        {
            row[column] = SDFIO_BAD_INT16;
        }
        else
        {
            row[column] = (int16_t)(((row_offset + column * 7) % 4000) - 2000);
        }
    }
}

static void init_header(sdfio_header *header)
{
    const double z_scale =
        sdfio_suggest_z_scale_for_range(SDFIO_TYPE_INT16, -80e-6, 80e-6);
    sdfio_header_set_dates(header, time(NULL));
    sdfio_header_set_data_type(header, SDFIO_TYPE_INT16);
    sdfio_header_set_columns(header, COLUMNS);
    sdfio_header_set_rows(header, ROWS);
    sdfio_header_set_x_scale(header, 2e-6);
    sdfio_header_set_y_scale(header, 2e-6);
    sdfio_header_set_z_scale(header, z_scale);
    sdfio_header_set_z_resolution(header, z_scale);
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "usage: %s output.sdf\n", argv[0]);
        return EXIT_FAILURE;
    }
    sdfio_header *header = NULL;
    sdfio_header_create(sdfio_default_allocator(), &header);
    init_header(header);
    sdfio_writer *writer = NULL;
    sdfio_writer_create(sdfio_default_allocator(), &writer);
    sdfio_writer_add_binary(writer);

    int16_t row[COLUMNS];
    const sdfio_layout layout = *sdfio_layout_packed(SDFIO_TYPE_INT16);
    sdfio_status status = sdfio_create_file(writer, argv[1], header);
    for (size_t y = 0; y < ROWS && status == SDFIO_OK; ++y)
    {
        generate_coded_row(row, y);
        status = sdfio_write_data(writer, &layout, row, 1);
    }
    if (status == SDFIO_OK)
    {
        status = sdfio_end_file(writer);
    }
    if (status != SDFIO_OK)
    {
        fprintf(stderr, "%s\n", sdfio_writer_error_message(writer));
    }
    sdfio_writer_free(writer);
    sdfio_header_free(header);
    return status == SDFIO_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}
