#include <sdfio/file.h>
#include <sdfio/sdfio.h>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* A time zone with a fixed offset from UTC. A zone with daylight saving time
 * would look up the offset for the time that it receives. */
typedef struct zone
{
    time_t offset;
} zone;

static time_t local_to_utc(void *user, time_t local)
{
    return local - ((const zone *)user)->offset;
}

static time_t utc_to_local(void *user, time_t utc)
{
    return utc + ((const zone *)user)->offset;
}

static void print_date(const char *label, time_t date)
{
    char text[32];
    strftime(text, sizeof text, "%Y-%m-%d %H:%M", gmtime(&date));
    printf("%-38s %s\n", label, text);
}

static time_t read_creation_date(const sdfio_time_zone *time_zone,
                                 const char *path)
{
    sdfio_reader *reader = NULL;
    time_t date = SDFIO_NO_TIME;
    if (sdfio_reader_create(sdfio_default_allocator(), &reader) == SDFIO_OK
        && sdfio_reader_add_binary_format(reader) == SDFIO_OK
        && sdfio_reader_set_time_zone(reader, time_zone) == SDFIO_OK
        && sdfio_open_file(reader, path) == SDFIO_OK)
    {
        date = sdfio_header_creation_date(sdfio_get_header(reader));
    }
    sdfio_reader_free(reader);
    return date;
}

/* The files of ISO-1.0 hold the clock of the writer, whose time
 * zone they do not record. With a time zone the library converts the dates, and
 * without one the clock reading is the date. */
int main(void)
{
    const char *path = "time_zone.sdf";
    zone central_europe = {3600};
    const sdfio_time_zone time_zone = {&central_europe, local_to_utc,
                                       utc_to_local};

    const time_t written = 1782900000; /* 2026-07-01 10:00 UTC */
    const float height = 1e-6F;

    sdfio_header *header = NULL;
    sdfio_status status =
        sdfio_header_create(sdfio_default_allocator(), &header);
    if (status == SDFIO_OK)
    {
        status = sdfio_header_set_revision(header, SDFIO_ISO_1_0);
    }
    if (status == SDFIO_OK)
    {
        status = sdfio_header_set_data_type(header, SDFIO_TYPE_INT16);
    }
    if (status == SDFIO_OK)
    {
        status = sdfio_header_set_columns(header, 1);
    }
    if (status == SDFIO_OK)
    {
        status = sdfio_header_set_rows(header, 1);
    }
    if (status == SDFIO_OK)
    {
        status = sdfio_header_set_creation_date(header, written);
    }
    if (status == SDFIO_OK)
    {
        status = sdfio_header_set_modification_date(header, written);
    }
    sdfio_writer *writer = NULL;
    if (status == SDFIO_OK)
    {
        status = sdfio_writer_create(sdfio_default_allocator(), &writer);
    }
    if (status == SDFIO_OK)
    {
        status = sdfio_writer_add_binary_format(writer);
    }
    if (status == SDFIO_OK)
    {
        status = sdfio_writer_set_time_zone(writer, &time_zone);
    }
    if (status == SDFIO_OK)
    {
        status = sdfio_create_file(writer, path, header);
    }
    if (status == SDFIO_OK)
    {
        status = sdfio_write_heights(
            writer, sdfio_layout_packed(SDFIO_TYPE_FLOAT), &height);
    }
    if (status == SDFIO_OK)
    {
        status = sdfio_end_trailer(writer);
    }
    if (status != SDFIO_OK)
    {
        fprintf(stderr, "%s\n", sdfio_writer_error_message(writer));
    }
    sdfio_writer_free(writer);
    sdfio_header_free(header);
    if (status != SDFIO_OK)
    {
        return EXIT_FAILURE;
    }

    print_date("written, UTC:", written);
    print_date("read with the time zone, UTC:",
               read_creation_date(&time_zone, path));
    print_date("read without, the clock of the file:",
               read_creation_date(NULL, path));
    return EXIT_SUCCESS;
}
