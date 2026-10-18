#include <sdf/sdf.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

/* The statistics of a file from a pipe, a row at a time: no matrix is held. */
int main(void)
{
    const sdf_stream stream = sdf_stdio_stream(stdin);
    sdf_reader *reader = sdf_reader_new();
    sdf_status status = sdf_open_stream(reader, &stream);
    if (status != SDF_OK)
    {
        fprintf(stderr, "%s\n", sdf_reader_message(reader));
        sdf_reader_free(reader);
        return EXIT_FAILURE;
    }

    const sdf_info *info = sdf_get_info(reader);
    void *stored = malloc(info->columns * sdf_data_type_size(info->data_type));
    double *heights = malloc(info->columns * sizeof *heights);
    double lowest = INFINITY;
    double highest = -INFINITY;
    for (size_t row = 0; row < info->rows && status == SDF_OK; ++row)
    {
        status = sdf_read_rows(reader, stored, 1);
        if (status == SDF_OK)
        {
            status = sdf_decode_heights(info, sdf_metres(SDF_FLOAT64), stored,
                                        info->columns, heights);
        }
        for (size_t column = 0; column < info->columns && status == SDF_OK;
             ++column)
        {
            if (!isnan(heights[column]))
            {
                lowest = fmin(lowest, heights[column]);
                highest = fmax(highest, heights[column]);
            }
        }
    }
    if (status != SDF_OK)
    {
        fprintf(stderr, "%s\n", sdf_reader_message(reader));
    }
    else
    {
        printf("from %g m to %g m\n", lowest, highest);
    }
    free(stored);
    free(heights);
    sdf_reader_free(reader);
    return status == SDF_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}
