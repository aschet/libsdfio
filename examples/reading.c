#include <sdfio/file.h>
#include <sdfio/sdfio.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "integer.sdf";
    sdfio_reader *reader = sdfio_reader_new(NULL);
    sdfio_status status = sdfio_open_file(reader, path);
    if (status != SDFIO_OK)
    {
        fprintf(stderr, "%s\n", sdfio_reader_error_message(reader));
        sdfio_reader_free(reader);
        return EXIT_FAILURE;
    }

    const sdfio_header *header = sdfio_get_header(reader);
    float *heights = sdfio_alloc_heights(header, SDFIO_FLOAT32, header->rows);
    status = sdfio_read_heights(reader, sdfio_metres(SDFIO_FLOAT32), heights,
                                (ptrdiff_t)(header->columns * sizeof *heights));
    const char *trailer = NULL;
    size_t trailer_size = 0;
    if (status == SDFIO_OK)
    {
        status = sdfio_read_trailer(reader, &trailer, &trailer_size);
    }
    if (status != SDFIO_OK)
    {
        fprintf(stderr, "%s\n", sdfio_reader_error_message(reader));
    }
    else
    {
        double sum = 0.0;
        size_t measured = 0;
        for (size_t i = 0; i < header->rows * header->columns; ++i)
        {
            if (!isnan(heights[i]))
            {
                sum += heights[i];
                ++measured;
            }
        }
        printf("%s %zu x %zu, mean %g m\ntrailer:\n%.*s\n",
               sdfio_revision_name(header->revision), header->columns,
               header->rows, measured > 0 ? sum / (double)measured : 0.0,
               (int)trailer_size, trailer != NULL ? trailer : "");
    }
    sdfio_free(heights);
    sdfio_reader_free(reader);
    return status == SDFIO_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}
