#include <sdfio/file.h>
#include <sdfio/sdfio.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    COLUMNS = 512,
    ROWS = 64,
    DROPOUT_EVERY = 97
};

/* Generates the coded values of the row y: a pattern in which a point is not
 * measured at intervals of DROPOUT_EVERY points. */
static void generate_coded_row(int16_t *row, size_t y, int16_t bad_value)
{
    for (size_t column = 0; column < COLUMNS; ++column)
    {
        if ((column + y) % DROPOUT_EVERY == 0)
        {
            row[column] = bad_value;
        }
        else
        {
            row[column] = (int16_t)(((y * 31 + column * 7) % 4000) - 2000);
        }
    }
}

static sdfio_status describe_coded_values(sdfio_header *header)
{
    const double z_scale =
        sdfio_z_scale_for_range(SDFIO_TYPE_INT16, -80e-6, 80e-6);
    sdfio_status status = sdfio_header_set_now(header);
    if (status == SDFIO_OK)
    {
        status = sdfio_header_set_data_type(header, SDFIO_TYPE_INT16);
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
        status = sdfio_header_set_x_scale(header, 2e-6);
    }
    if (status == SDFIO_OK)
    {
        status = sdfio_header_set_y_scale(header, 2e-6);
    }
    if (status == SDFIO_OK)
    {
        status = sdfio_header_set_z_scale(header, z_scale);
    }
    if (status == SDFIO_OK)
    {
        status = sdfio_header_set_z_resolution(header, z_scale);
    }
    return status;
}

static int write_coded_values(const char *path)
{
    sdfio_header *header = NULL;
    sdfio_status status =
        sdfio_header_create(sdfio_default_allocator(), &header);
    if (status == SDFIO_OK)
    {
        status = describe_coded_values(header);
    }
    const int16_t bad_value = SDFIO_BAD_INT16;
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
    int16_t row[COLUMNS];
    const sdfio_layout layout = sdfio_layout_packed(SDFIO_TYPE_INT16);
    for (size_t y = 0; y < ROWS && status == SDFIO_OK; ++y)
    {
        generate_coded_row(row, y, bad_value);
        status = sdfio_write_data(writer, layout, row, 1);
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

static int read_coded_values(const char *path)
{
    sdfio_reader *reader = NULL;
    sdfio_status status =
        sdfio_reader_create(sdfio_default_allocator(), &reader);
    if (status == SDFIO_OK)
    {
        status = sdfio_reader_add_binary(reader);
    }
    if (status == SDFIO_OK)
    {
        status = sdfio_open_file(reader, path);
    }
    const sdfio_header *header = sdfio_reader_header(reader);
    if (status == SDFIO_OK
        && sdfio_header_data_type(header) != SDFIO_TYPE_INT16)
    {
        fprintf(stderr, "the file does not hold int16 coded values\n");
        sdfio_reader_free(reader);
        return EXIT_FAILURE;
    }
    size_t row_size = 0;
    status = sdfio_data_row_size(header, &row_size);
    int16_t *row = status == SDFIO_OK ? malloc(row_size) : NULL;
    int16_t *expected = status == SDFIO_OK ? malloc(row_size) : NULL;
    if (status == SDFIO_OK
        && ((row == NULL || expected == NULL) && row_size > 0))
    {
        status = SDFIO_ERR_MEMORY;
    }
    const int16_t bad_value = SDFIO_BAD_INT16;
    size_t not_measured = 0;
    size_t measured = 0;
    size_t out_of_place = 0;
    double sum = 0.0;
    const sdfio_layout layout = sdfio_layout_packed(SDFIO_TYPE_INT16);
    const size_t columns = sdfio_header_columns(header);
    for (size_t y = 0; status == SDFIO_OK && y < sdfio_header_rows(header); ++y)
    {
        status = sdfio_read_data(reader, layout, row, 1);
        generate_coded_row(expected, y, bad_value);
        out_of_place +=
            memcmp(row, expected, columns * sizeof *row) != 0 ? 1 : 0;
        for (size_t x = 0; status == SDFIO_OK && x < columns; ++x)
        {
            if (row[x] == SDFIO_BAD_INT16)
            {
                ++not_measured;
            }
            else
            {
                sum += row[x];
                ++measured;
            }
        }
    }
    if (status != SDFIO_OK)
    {
        fprintf(stderr, "%s: %s\n", sdfio_status_string(status),
                sdfio_reader_error_message(reader));
    }
    else
    {
        const double mean = sum / (double)measured;
        printf("%s %lu x %lu, %zu points not measured, mean %g coded value = "
               "%g m, "
               "%zu rows out of place\n",
               sdfio_header_revision(header) == SDFIO_ISO_1_0 ? "ISO-1.0"
                                                              : "ISO-2.0",
               (unsigned long)columns, (unsigned long)sdfio_header_rows(header),
               not_measured, mean, mean * sdfio_header_z_scale(header),
               out_of_place);
    }
    free(row);
    free(expected);
    sdfio_reader_free(reader);
    return status == SDFIO_OK && out_of_place == 0 ? EXIT_SUCCESS
                                                   : EXIT_FAILURE;
}

int main(void)
{
    const int failed = write_coded_values("coded_values.sdf");
    return failed == EXIT_SUCCESS ? read_coded_values("coded_values.sdf")
                                  : failed;
}
