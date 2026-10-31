/* Read an SDF file with the heights API. */

#include <sdfio/file.h>
#include <sdfio/sdfio.h>

#include <math.h>
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
    sdfio_reader_add_ascii(reader);
    sdfio_status status = sdfio_open_file(reader, argv[1]);
    if (status != SDFIO_OK)
    {
        fprintf(stderr, "%s: %s\n", sdfio_status_string(status),
                sdfio_reader_error_message(reader));
        sdfio_reader_free(reader);
        return EXIT_FAILURE;
    }

    const sdfio_header *header = sdfio_reader_header(reader);
    size_t heights_size = 0;
    float *heights =
        sdfio_heights_size(header, SDFIO_TYPE_FLOAT, &heights_size) == SDFIO_OK
            ? malloc(heights_size)
            : NULL;
    if (heights == NULL)
    {
        fprintf(stderr, "the heights do not fit in memory\n");
        sdfio_reader_free(reader);
        return EXIT_FAILURE;
    }
    char trailer[1024];
    size_t trailer_size = 0;
    status = sdfio_read_heights(reader, sdfio_layout_packed(SDFIO_TYPE_FLOAT),
                                heights);
    if (status == SDFIO_OK)
    {
        status = sdfio_read_trailer(reader, trailer, sizeof trailer - 1,
                                    &trailer_size);
    }
    if (status != SDFIO_OK)
    {
        fprintf(stderr, "%s: %s\n", sdfio_status_string(status),
                sdfio_reader_error_message(reader));
        free(heights);
        sdfio_reader_free(reader);
        return EXIT_FAILURE;
    }

    double sum = 0.0;
    size_t measured = 0;
    const size_t count =
        (size_t)sdfio_header_rows(header) * sdfio_header_columns(header);
    for (size_t i = 0; i < count; ++i)
    {
        if (!isnan(heights[i]))
        {
            sum += heights[i];
            ++measured;
        }
    }
    printf("%s %lu x %lu, mean %g m\ntrailer:\n%.*s",
           sdfio_header_revision(header) == SDFIO_ISO_1_0 ? "ISO-1.0"
                                                          : "ISO-2.0",
           (unsigned long)sdfio_header_columns(header),
           (unsigned long)sdfio_header_rows(header),
           measured > 0 ? sum / (double)measured : 0.0, (int)trailer_size,
           trailer);

    size_t position = 0;
    size_t name_at = 0;
    size_t value_at = 0;
    size_t name_size = 0;
    size_t value_size = 0;
    while (sdfio_trailer_next_field(trailer, trailer_size, &position, &name_at,
                                    &name_size, &value_at, &value_size))
    {
        printf("field %.*s: %.*s\n", (int)name_size, trailer + name_at,
               (int)value_size, trailer + value_at);
    }
    if (sdfio_trailer_find_field(trailer, trailer_size, "operatorname",
                                 &value_at, &value_size))
    {
        printf("operator: %.*s\n", (int)value_size, trailer + value_at);
    }
    free(heights);
    sdfio_reader_free(reader);
    return EXIT_SUCCESS;
}
