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

static time_t read_creation_date(const sdfio_config *config, const char *path)
{
    sdfio_reader *reader = sdfio_reader_new(config);
    time_t date = SDFIO_NO_TIME;
    if (sdfio_open_file(reader, path) == SDFIO_OK)
    {
        date = sdfio_get_header(reader)->creation_date;
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
    const sdfio_config with_zone = {NULL, 0, 0, &time_zone};

    sdfio_header header;
    sdfio_header_init(&header);
    header.revision = SDFIO_ISO_1_0;
    header.data_type = SDFIO_TYPE_INT16;
    header.columns = 1;
    header.rows = 1;
    header.creation_date = 1782900000; /* 2026-07-01 10:00 UTC */
    header.modification_date = header.creation_date;
    const float height = 1e-6F;

    sdfio_writer *writer = sdfio_writer_new(&with_zone);
    sdfio_status status = sdfio_create_file(writer, path, &header);
    if (status == SDFIO_OK)
    {
        status = sdfio_write_heights(writer, SDFIO_TYPE_FLOAT, &height);
    }
    if (status == SDFIO_OK)
    {
        status = sdfio_write_trailer(writer, NULL, 0);
    }
    if (status != SDFIO_OK)
    {
        fprintf(stderr, "%s\n", sdfio_writer_error_message(writer));
    }
    sdfio_writer_free(writer);
    if (status != SDFIO_OK)
    {
        return EXIT_FAILURE;
    }

    print_date("written, UTC:", header.creation_date);
    print_date("read with the time zone, UTC:",
               read_creation_date(&with_zone, path));
    print_date("read without, the clock of the file:",
               read_creation_date(NULL, path));
    return EXIT_SUCCESS;
}
