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

static int write_counts(const char *path, sdfio_revision revision)
{
    sdfio_header header;
    sdfio_header_init(&header);
    header.revision = revision;
    header.data_type = SDFIO_DATA_INT16;
    header.columns = COLUMNS;
    header.rows = ROWS;
    header.x_scale = 2e-6;
    header.y_scale = 2e-6;
    header.z_scale = sdfio_z_scale_for_range(header.data_type, header.revision,
                                             -80e-6, 80e-6);
    header.z_resolution = header.z_scale;

    const int16_t not_measured =
        (int16_t)sdfio_bad_value(header.data_type, header.revision);
    sdfio_writer *writer = sdfio_writer_new(NULL);
    sdfio_status status = sdfio_create_file(writer, path, &header);
    int16_t row[COLUMNS];
    for (size_t n = 0; n < header.rows && status == SDFIO_OK; ++n)
    {
        scan(row, sdfio_row_y(&header, n), not_measured);
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

static int read_counts(const char *path)
{
    sdfio_reader *reader = sdfio_reader_new(NULL);
    sdfio_status status = sdfio_open_file(reader, path);
    const sdfio_header *header = sdfio_get_header(reader);
    if (status == SDFIO_OK && header->data_type != SDFIO_DATA_INT16)
    {
        fprintf(stderr, "the file does not hold int16 counts\n");
        sdfio_reader_free(reader);
        return EXIT_FAILURE;
    }
    int16_t *row = sdfio_alloc_data(NULL, header, 1);
    int16_t *expected = sdfio_alloc_data(NULL, header, 1);
    const int16_t not_measured =
        (int16_t)sdfio_bad_value(header->data_type, header->revision);
    size_t unmeasured = 0;
    size_t measured = 0;
    size_t out_of_place = 0;
    double sum = 0.0;
    for (size_t n = 0; status == SDFIO_OK && n < header->rows; ++n)
    {
        status = sdfio_read_data(reader, row, 1);
        scan(expected, sdfio_row_y(header, n), not_measured);
        out_of_place +=
            memcmp(row, expected, header->columns * sizeof *row) != 0 ? 1 : 0;
        for (size_t x = 0; status == SDFIO_OK && x < header->columns; ++x)
        {
            if (sdfio_is_bad_value(header->data_type, header->revision, row[x]))
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
        fprintf(stderr, "%s\n", sdfio_reader_error_message(reader));
    }
    else
    {
        const double mean = sum / (double)measured;
        printf("%s %zu x %zu, %zu points not measured, mean %g counts = %g m, "
               "%zu rows out of place\n",
               sdfio_revision_name(header->revision), header->columns,
               header->rows, unmeasured, mean, mean * header->z_scale,
               out_of_place);
    }
    sdfio_free(NULL, row);
    sdfio_free(NULL, expected);
    sdfio_reader_free(reader);
    return status == SDFIO_OK && out_of_place == 0 ? EXIT_SUCCESS
                                                   : EXIT_FAILURE;
}

int main(void)
{
    int failed = write_counts("counts.sdf", SDFIO_ISO_2_0);
    failed = failed == EXIT_SUCCESS ? read_counts("counts.sdf") : failed;
    failed = failed == EXIT_SUCCESS ? write_counts("bcr.sdf", SDFIO_BCR_1_0)
                                    : failed;
    return failed == EXIT_SUCCESS ? read_counts("bcr.sdf") : failed;
}
