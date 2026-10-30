/* Write SDF files of both revisions with the heights API. */

#include <sdfio/file.h>
#include <sdfio/sdfio.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define COLUMNS 256
#define ROWS 192

static float heights[ROWS * COLUMNS];

/* Generates a ripple of heights in metres, with a region of points that are not
 * measured, which are NaN. */
static void generate_heights(void)
{
    for (size_t row = 0; row < ROWS; ++row)
    {
        const bool in_region = row > 100 && row < 110;
        const double row_factor = 2e-6 * sin(0.1 * (double)row);
        float *line = heights + row * COLUMNS;
        for (size_t column = 0; column < COLUMNS; ++column)
        {
            line[column] =
                in_region && column < 40
                    ? NAN
                    : (float)(row_factor * cos(0.07 * (double)column));
        }
    }
}

static void init_header(sdfio_header *header, sdfio_revision revision,
                        sdfio_data_type type)
{
    sdfio_header_set_dates(header, time(NULL));
    sdfio_header_set_revision(header, revision);
    sdfio_header_set_data_type(header, type);
    sdfio_header_set_columns(header, COLUMNS);
    sdfio_header_set_rows(header, ROWS);
    sdfio_header_set_x_scale(header, 1e-6);
    sdfio_header_set_y_scale(header, 1e-6);
}

/* An ISO-1.0 file holds the clock of the writer, so the writer gets the time
 * zone of the machine. An ISO-2.0 file holds UTC and does not use it. */
static int write_surface(const char *path, sdfio_revision revision,
                         sdfio_data_type type)
{
    sdfio_header *header = NULL;
    sdfio_header_create(sdfio_default_allocator(), &header);
    init_header(header, revision, type);

    sdfio_writer *writer = NULL;
    sdfio_writer_create(sdfio_default_allocator(), &writer);
    sdfio_writer_add_binary(writer);
    sdfio_writer_set_time_zone(writer, sdfio_system_time_zone());

    sdfio_status status = sdfio_create_file(writer, path, header);
    if (status == SDFIO_OK)
    {
        status = sdfio_write_heights(
            writer, sdfio_layout_packed(SDFIO_TYPE_FLOAT), heights);
    }
    if (status == SDFIO_OK)
    {
        sdfio_write_trailer_field(writer, "OperatorName", "Jane Doe");
        sdfio_write_trailer_field(writer, "Magnification", "20.5");
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

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        fprintf(stderr, "usage: %s iso2.sdf iso1.sdf\n", argv[0]);
        return EXIT_FAILURE;
    }
    generate_heights();
    const int floats = write_surface(argv[1], SDFIO_ISO_2_0, SDFIO_TYPE_FLOAT);
    const int integers =
        write_surface(argv[2], SDFIO_ISO_1_0, SDFIO_TYPE_INT16);
    return floats == EXIT_SUCCESS && integers == EXIT_SUCCESS ? EXIT_SUCCESS
                                                              : EXIT_FAILURE;
}
