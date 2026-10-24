#include <sdfio/file.h>
#include <sdfio/sdfio.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    TRAILER_CAPACITY = 1024,
    NUMBER_CAPACITY = 64
};

/* Reads the whole trailer: it has to fit in the buffer. */
static sdfio_status read_whole_trailer(sdfio_reader *reader, char *trailer,
                                       size_t *trailer_size)
{
    sdfio_status status =
        sdfio_read_trailer(reader, trailer, TRAILER_CAPACITY, trailer_size);
    if (status == SDFIO_OK)
    {
        char more = '\0';
        size_t more_size = 0;
        status = sdfio_read_trailer(reader, &more, 1, &more_size);
        if (status == SDFIO_OK && more_size > 0)
        {
            status = SDFIO_ERR_RANGE;
        }
    }
    return status;
}

/* Converts the value of a field to a number. The value is not terminated, so
 * a copy is. */
static int field_number(const char *value, size_t value_size, double *number)
{
    char text[NUMBER_CAPACITY];
    if (value_size >= sizeof text)
    {
        return 0;
    }
    memcpy(text, value, value_size);
    text[value_size] = '\0';
    char *end = NULL;
    *number = strtod(text, &end);
    return value_size > 0 && *end == '\0';
}

int main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "integer.sdf";
    const char *wanted = argc > 2 ? argv[2] : "Magnification";
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
        status = sdfio_open_file(reader, path);
    }
    char trailer[TRAILER_CAPACITY];
    size_t trailer_size = 0;
    if (status == SDFIO_OK)
    {
        status = read_whole_trailer(reader, trailer, &trailer_size);
    }
    if (status != SDFIO_OK)
    {
        fprintf(stderr, "%s: %s\n", sdfio_status_string(status),
                sdfio_reader_error_message(reader));
        sdfio_reader_free(reader);
        return EXIT_FAILURE;
    }

    size_t position = 0;
    const char *name = NULL;
    const char *value = NULL;
    size_t name_size = 0;
    size_t value_size = 0;
    while (sdfio_trailer_next_field(trailer, trailer_size, &position, &name,
                                    &name_size, &value, &value_size))
    {
        printf("%.*s = %.*s\n", (int)name_size, name, (int)value_size, value);
    }

    double number = 0.0;
    if (!sdfio_trailer_find_field(trailer, trailer_size, wanted, &value,
                                  &value_size))
    {
        printf("%s: no such field\n", wanted);
    }
    else if (field_number(value, value_size, &number))
    {
        printf("%s as a number: %g\n", wanted, number);
    }
    else
    {
        printf("%s is no number: %.*s\n", wanted, (int)value_size, value);
    }
    sdfio_reader_free(reader);
    return EXIT_SUCCESS;
}
