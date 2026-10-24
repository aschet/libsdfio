#include <sdfio/file.h>
#include <sdfio/sdfio.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#endif

/* The statistics of a file from a pipe, a row at a time: no matrix is held. */
int main(void)
{
#if defined(_WIN32)
    /* Windows opens stdin in text mode, which would change the bytes of a
     * binary file. */
    _setmode(_fileno(stdin), _O_BINARY);
#endif
    const sdfio_stream stream = sdfio_stdio_stream(stdin);
    sdfio_reader *reader = NULL;
    sdfio_status status =
        sdfio_reader_create(sdfio_default_allocator(), &reader);
    if (status == SDFIO_OK)
    {
        status = sdfio_reader_add_binary(reader);
    }
    if (status == SDFIO_OK)
    {
        status = sdfio_reader_add_ascii(reader);
    }
    if (status == SDFIO_OK)
    {
        status = sdfio_open_stream(reader, &stream);
    }
    if (status != SDFIO_OK)
    {
        fprintf(stderr, "%s: %s\n", sdfio_status_string(status),
                sdfio_reader_error_message(reader));
        sdfio_reader_free(reader);
        return EXIT_FAILURE;
    }

    const sdfio_header *header = sdfio_reader_header(reader);
    size_t coded_size = 0;
    size_t heights_size = 0;
    status = sdfio_data_row_size(header, &coded_size);
    if (status == SDFIO_OK)
    {
        status =
            sdfio_heights_row_size(header, SDFIO_TYPE_DOUBLE, &heights_size);
    }
    void *coded = status == SDFIO_OK ? malloc(coded_size) : NULL;
    double *heights = status == SDFIO_OK ? malloc(heights_size) : NULL;
    if (status == SDFIO_OK
        && ((coded == NULL && coded_size > 0)
            || (heights == NULL && heights_size > 0)))
    {
        status = SDFIO_ERR_MEMORY;
    }
    double lowest = INFINITY;
    double highest = -INFINITY;
    const sdfio_layout coded_layout =
        sdfio_layout_packed(sdfio_header_data_type(header));
    const sdfio_layout heights_layout = sdfio_layout_packed(SDFIO_TYPE_DOUBLE);
    const size_t columns = sdfio_header_columns(header);
    for (size_t row = 0; row < sdfio_header_rows(header) && status == SDFIO_OK;
         ++row)
    {
        status = sdfio_read_data(reader, coded_layout, coded, 1);
        if (status == SDFIO_OK)
        {
            status = sdfio_decode_heights(header, heights_layout, coded,
                                          columns, heights);
        }
        for (size_t column = 0; column < columns && status == SDFIO_OK;
             ++column)
        {
            if (!isnan(heights[column]))
            {
                lowest = fmin(lowest, heights[column]);
                highest = fmax(highest, heights[column]);
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
        printf("from %g m to %g m\n", lowest, highest);
    }
    free(coded);
    free(heights);
    sdfio_reader_free(reader);
    return status == SDFIO_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}
