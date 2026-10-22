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
    sdfio_reader *reader = sdfio_reader_new(NULL);
    sdfio_status status = sdfio_open_stream(reader, &stream);
    if (status != SDFIO_OK)
    {
        fprintf(stderr, "%s\n", sdfio_reader_error_message(reader));
        sdfio_reader_free(reader);
        return EXIT_FAILURE;
    }

    const sdfio_header *header = sdfio_get_header(reader);
    void *coded = sdfio_alloc_data(NULL, header, 1);
    double *heights = sdfio_alloc_heights(NULL, header, SDFIO_FLOAT64, 1);
    double lowest = INFINITY;
    double highest = -INFINITY;
    for (size_t row = 0; row < header->rows && status == SDFIO_OK; ++row)
    {
        status = sdfio_read_data(reader, coded, 1);
        if (status == SDFIO_OK)
        {
            status = sdfio_decode_heights(header, sdfio_metres(SDFIO_FLOAT64),
                                          coded, header->columns, heights);
        }
        for (size_t column = 0; column < header->columns && status == SDFIO_OK;
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
        fprintf(stderr, "%s\n", sdfio_reader_error_message(reader));
    }
    else
    {
        printf("from %g m to %g m\n", lowest, highest);
    }
    sdfio_free(NULL, coded);
    sdfio_free(NULL, heights);
    sdfio_reader_free(reader);
    return status == SDFIO_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}
