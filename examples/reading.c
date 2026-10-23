#include <sdfio/file.h>
#include <sdfio/sdfio.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "integer.sdf";
    const sdfio_config config = {
        .reader_formats = {&sdfio_binary_reader_format,
                           &sdfio_ascii_reader_format}};
    sdfio_reader *reader = sdfio_reader_new(&config);
    sdfio_status status = sdfio_open_file(reader, path);
    if (status != SDFIO_OK)
    {
        fprintf(stderr, "%s\n", sdfio_reader_error_message(reader));
        sdfio_reader_free(reader);
        return EXIT_FAILURE;
    }

    const sdfio_header *header = sdfio_get_header(reader);
    size_t heights_size = 0;
    status = sdfio_heights_size(header, SDFIO_TYPE_FLOAT, &heights_size);
    float *heights = status == SDFIO_OK ? malloc(heights_size) : NULL;
    if (status == SDFIO_OK && heights == NULL && heights_size > 0)
    {
        status = SDFIO_ERR_MEMORY;
    }
    if (status == SDFIO_OK)
    {
        status = sdfio_read_heights(reader, SDFIO_TYPE_FLOAT, heights);
    }
    const char *trailer = NULL;
    size_t trailer_size = 0;
    if (status == SDFIO_OK)
    {
        status = sdfio_read_trailer(reader, &trailer, &trailer_size);
    }
    if (status != SDFIO_OK)
    {
        fprintf(stderr, "%s: %s\n", sdfio_status_string(status),
                sdfio_reader_error_message(reader));
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
               header->revision == SDFIO_ISO_1_0 ? "ISO-1.0" : "ISO-2.0",
               header->columns, header->rows,
               measured > 0 ? sum / (double)measured : 0.0, (int)trailer_size,
               trailer != NULL ? trailer : "");
    }
    free(heights);
    sdfio_reader_free(reader);
    return status == SDFIO_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}
