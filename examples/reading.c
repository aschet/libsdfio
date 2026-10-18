#include <sdf/sdf.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "surface.sdf";
    sdf_reader *reader = sdf_reader_new();
    sdf_status status = sdf_open_file(reader, path);
    if (status != SDF_OK)
    {
        fprintf(stderr, "%s\n", sdf_reader_message(reader));
        sdf_reader_free(reader);
        return EXIT_FAILURE;
    }

    const sdf_info *info = sdf_get_info(reader);
    float *heights = malloc(info->rows * info->columns * sizeof *heights);
    status = sdf_read_heights(reader, sdf_metres(SDF_FLOAT32), heights,
                              (ptrdiff_t)(info->columns * sizeof *heights));
    const char *trailer = NULL;
    size_t trailer_size = 0;
    if (status == SDF_OK)
    {
        status = sdf_read_trailer(reader, &trailer, &trailer_size);
    }
    if (status != SDF_OK)
    {
        fprintf(stderr, "%s\n", sdf_reader_message(reader));
    }
    else
    {
        double sum = 0.0;
        size_t measured = 0;
        for (size_t i = 0; i < info->rows * info->columns; ++i)
        {
            if (!isnan(heights[i]))
            {
                sum += heights[i];
                ++measured;
            }
        }
        const char *operator_name = NULL;
        size_t operator_size = 0;
        sdf_trailer_find(trailer, trailer_size, "OperatorName", &operator_name,
                         &operator_size);
        printf("%s %zu x %zu, mean %g m, operator %.*s\n",
               sdf_revision_name(info->revision), info->columns, info->rows,
               measured > 0 ? sum / (double)measured : 0.0, (int)operator_size,
               operator_name != NULL ? operator_name : "");
    }
    free(heights);
    sdf_reader_free(reader);
    return status == SDF_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}
