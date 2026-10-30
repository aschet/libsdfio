/* Read an SDF file with the coded values API. */

#include <sdfio/file.h>
#include <sdfio/sdfio.h>

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "usage: %s input.sdf\n", argv[0]);
        return EXIT_FAILURE;
    }
    sdfio_reader *reader = NULL;
    sdfio_reader_create(sdfio_default_allocator(), &reader);
    sdfio_reader_add_binary(reader);
    sdfio_status status = sdfio_open_file(reader, argv[1]);
    if (status != SDFIO_OK)
    {
        fprintf(stderr, "%s: %s\n", sdfio_status_string(status),
                sdfio_reader_error_message(reader));
        sdfio_reader_free(reader);
        return EXIT_FAILURE;
    }
    const sdfio_header *header = sdfio_reader_header(reader);
    if (sdfio_header_data_type(header) != SDFIO_TYPE_INT16)
    {
        fprintf(stderr, "the file does not hold int16 coded values\n");
        sdfio_reader_free(reader);
        return EXIT_FAILURE;
    }
    size_t row_size = 0;
    int16_t *row = sdfio_data_row_size(header, &row_size) == SDFIO_OK
                       ? malloc(row_size)
                       : NULL;
    if (row == NULL)
    {
        fprintf(stderr, "a row does not fit in memory\n");
        sdfio_reader_free(reader);
        return EXIT_FAILURE;
    }

    size_t not_measured = 0;
    size_t measured = 0;
    double sum = 0.0;
    const sdfio_layout layout = *sdfio_layout_packed(SDFIO_TYPE_INT16);
    const size_t columns = sdfio_header_columns(header);
    const size_t rows = sdfio_header_rows(header);
    for (size_t y = 0; y < rows; ++y)
    {
        status = sdfio_read_data(reader, &layout, row, 1);
        if (status != SDFIO_OK)
        {
            break;
        }
        for (size_t x = 0; x < columns; ++x)
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
               "%g m\n",
               sdfio_header_revision(header) == SDFIO_ISO_1_0 ? "ISO-1.0"
                                                              : "ISO-2.0",
               (unsigned long)columns, (unsigned long)rows, not_measured, mean,
               mean * sdfio_header_z_scale(header));
    }
    free(row);
    sdfio_reader_free(reader);
    return status == SDFIO_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}
