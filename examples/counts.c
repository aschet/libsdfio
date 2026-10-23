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

static void scan(int16_t *row, size_t y, int16_t not_measured)
{
    for (size_t column = 0; column < COLUMNS; ++column)
    {
        if ((column + y) % DROPOUT_EVERY == 0)
        {
            row[column] = not_measured;
        }
        else
        {
            row[column] = (int16_t)(((y * 31 + column * 7) % 4000) - 2000);
        }
    }
}

static int write_counts(const char *path)
{
    sdfio_header header;
    sdfio_header_init(&header);
    header.data_type = SDFIO_TYPE_INT16;
    header.columns = COLUMNS;
    header.rows = ROWS;
    header.x_scale = 2e-6;
    header.y_scale = 2e-6;
    header.z_scale = sdfio_z_scale_for_range(header.data_type, -80e-6, 80e-6);
    header.z_resolution = header.z_scale;

    const int16_t not_measured = SDFIO_BAD_INT16;
    const sdfio_config config = {.writer_codecs = {&sdfio_binary_writer_codec}};
    sdfio_writer *writer = sdfio_writer_new(&config);
    sdfio_status status = sdfio_create_file(writer, path, &header);
    int16_t row[COLUMNS];
    for (size_t n = 0; n < header.rows && status == SDFIO_OK; ++n)
    {
        scan(row, n, not_measured);
        status = sdfio_write_data_int16(writer, row, 1);
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
    return status == SDFIO_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}

static int read_counts(const char *path)
{
    const sdfio_config config = {.reader_codecs = {&sdfio_binary_reader_codec}};
    sdfio_reader *reader = sdfio_reader_new(&config);
    sdfio_status status = sdfio_open_file(reader, path);
    const sdfio_header *header = sdfio_get_header(reader);
    if (status == SDFIO_OK && header->data_type != SDFIO_TYPE_INT16)
    {
        fprintf(stderr, "the file does not hold int16 counts\n");
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
    const int16_t not_measured = SDFIO_BAD_INT16;
    size_t unmeasured = 0;
    size_t measured = 0;
    size_t out_of_place = 0;
    double sum = 0.0;
    for (size_t n = 0; status == SDFIO_OK && n < header->rows; ++n)
    {
        status = sdfio_read_data_int16(reader, row, 1);
        scan(expected, n, not_measured);
        out_of_place +=
            memcmp(row, expected, header->columns * sizeof *row) != 0 ? 1 : 0;
        for (size_t x = 0; status == SDFIO_OK && x < header->columns; ++x)
        {
            if (row[x] == SDFIO_BAD_INT16)
            {
                ++unmeasured;
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
        printf("%s %zu x %zu, %zu points not measured, mean %g counts = %g m, "
               "%zu rows out of place\n",
               header->revision == SDFIO_ISO_1_0 ? "ISO-1.0" : "ISO-2.0",
               header->columns, header->rows, unmeasured, mean,
               mean * header->z_scale, out_of_place);
    }
    free(row);
    free(expected);
    sdfio_reader_free(reader);
    return status == SDFIO_OK && out_of_place == 0 ? EXIT_SUCCESS
                                                   : EXIT_FAILURE;
}

int main(void)
{
    const int failed = write_counts("counts.sdf");
    return failed == EXIT_SUCCESS ? read_counts("counts.sdf") : failed;
}
