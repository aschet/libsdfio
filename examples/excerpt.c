#include <sdfio/file.h>
#include <sdfio/sdfio.h>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* The header of the excerpt: the header of the file with the size of the
 * window, double heights that every revision permits, the binary
 * format and the time of the change. */
static void describe_excerpt(sdfio_header *header, const sdfio_header *source,
                             uint32_t columns, uint32_t rows)
{
    sdfio_header_copy(header, source);
    sdfio_header_set_columns(header, columns);
    sdfio_header_set_rows(header, rows);
    sdfio_header_set_data_type(header, SDFIO_TYPE_DOUBLE);
    sdfio_header_set_z_scale(header, SDFIO_AUTO_Z_SCALE);
    sdfio_header_set_format(header, SDFIO_BINARY);
    const time_t now = time(NULL);
    sdfio_header_set_modified(header, now);
    if (sdfio_header_created(header) == SDFIO_NO_TIME)
    {
        sdfio_header_set_created(header, now);
    }
}

/* Writes the central part of the matrix, half of the columns and half of the
 * rows, from the heights of the whole matrix. The window is not copied: the
 * layout starts at its first element and steps over the columns around it. */
static sdfio_status write_excerpt(sdfio_writer *writer, sdfio_reader *reader,
                                  const char *path, const double *heights,
                                  uint32_t columns, uint32_t rows)
{
    const sdfio_header *source = sdfio_reader_header(reader);
    const size_t first_column = sdfio_header_columns(source) / 4;
    const size_t first_row = sdfio_header_rows(source) / 4;
    sdfio_header *header = NULL;
    sdfio_header_create(sdfio_default_allocator(), &header);
    describe_excerpt(header, source, columns, rows);
    sdfio_status status = sdfio_create_file(writer, path, header);
    sdfio_header_free(header);

    sdfio_layout layout = *sdfio_layout_packed(SDFIO_TYPE_DOUBLE);
    layout.row_stride =
        (ptrdiff_t)(sdfio_header_columns(source) * sizeof(double));
    if (status == SDFIO_OK)
    {
        status = sdfio_write_heights(
            writer, &layout,
            heights + first_row * sdfio_header_columns(source) + first_column);
    }
    if (status == SDFIO_OK)
    {
        status = sdfio_copy_trailer(reader, writer);
    }
    if (status == SDFIO_OK)
    {
        char note[64];
        snprintf(note, sizeof note, "columns from %zu, rows from %zu",
                 first_column, first_row);
        sdfio_write_trailer_field(writer, "Excerpt", note);
        status = sdfio_end_file(writer);
    }
    return status;
}

/* Saves a part of a file as a new file: the header and the trailer of the file
 * are copied, and the size of the header is that of the part. */
int main(int argc, char **argv)
{
    if (argc < 3)
    {
        fprintf(stderr, "usage: %s input.sdf output.sdf\n", argv[0]);
        return EXIT_FAILURE;
    }
    sdfio_reader *reader = NULL;
    sdfio_reader_create(sdfio_default_allocator(), &reader);
    sdfio_reader_add_binary(reader);
    sdfio_reader_add_ascii(reader);
    sdfio_writer *writer = NULL;
    sdfio_writer_create(sdfio_default_allocator(), &writer);
    sdfio_writer_add_binary(writer);

    double *heights = NULL;
    sdfio_status status = sdfio_open_file(reader, argv[1]);
    if (status == SDFIO_OK)
    {
        size_t size = 0;
        heights = sdfio_heights_size(sdfio_reader_header(reader),
                                     SDFIO_TYPE_DOUBLE, &size)
                          == SDFIO_OK
                      ? malloc(size)
                      : NULL;
        status =
            heights != NULL
                ? sdfio_read_heights(
                      reader, sdfio_layout_packed(SDFIO_TYPE_DOUBLE), heights)
                : SDFIO_ERR_MEMORY;
    }
    if (status == SDFIO_OK)
    {
        const sdfio_header *header = sdfio_reader_header(reader);
        status = write_excerpt(writer, reader, argv[2], heights,
                               sdfio_header_columns(header) / 2,
                               sdfio_header_rows(header) / 2);
    }
    if (status != SDFIO_OK)
    {
        fprintf(stderr, "%s: %s\n", sdfio_status_string(status),
                sdfio_reader_error_message(reader)[0] != '\0'
                    ? sdfio_reader_error_message(reader)
                    : sdfio_writer_error_message(writer));
    }
    else
    {
        printf("%s: %lu x %lu points\n", argv[2],
               (unsigned long)sdfio_header_columns(sdfio_reader_header(reader))
                   / 2,
               (unsigned long)sdfio_header_rows(sdfio_reader_header(reader))
                   / 2);
    }
    free(heights);
    sdfio_writer_free(writer);
    sdfio_reader_free(reader);
    return status == SDFIO_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}
