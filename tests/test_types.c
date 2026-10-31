/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif

#include "check.h"
#include <string.h>

#include "zone.h"

#include <stdlib.h>
#include <time.h>

#define DATE_AT 18

static void check_date_conversion(void)
{
    sdfio_date date;
    CHECK_OK(sdfio_date_from_time(1772600760, &date));
    CHECK(date.year == 2026 && date.month == 3 && date.day == 4
          && date.hour == 5 && date.minute == 6);
    time_t utc = 0;
    CHECK_OK(sdfio_date_to_time(&date, &utc));
    CHECK(utc == 1772600760);
    CHECK_OK(sdfio_date_from_time(1772600819, &date));
    CHECK(date.minute == 6);
    CHECK_OK(sdfio_date_from_time(-60, &date));
    CHECK(date.year == 1969 && date.month == 12 && date.day == 31
          && date.hour == 23 && date.minute == 59);
    const sdfio_date last = {9999, 12, 31, 23, 59};
    if (sizeof(time_t) >= 8)
    {
        CHECK_OK(sdfio_date_to_time(&last, &utc));
        CHECK_OK(sdfio_date_from_time(utc, &date));
        CHECK(date.year == 9999 && date.minute == 59);
        CHECK_STATUS(SDFIO_ERR_RANGE, sdfio_date_from_time(utc + 60, &date));
    }
    const sdfio_date not_a_day = {2026, 2, 29, 0, 0};
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_date_to_time(&not_a_day, &utc));
    const sdfio_date zero = {0, 0, 0, 0, 0};
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_date_to_time(&zero, &utc));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_date_to_time(NULL, &utc));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_date_to_time(&last, NULL));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_date_from_time(0, NULL));
}

/* For any zone, a time converts to a clock reading and back unchanged, and the
 * reading equals that of the C library. The times avoid clock changes. */
static void check_system_time_zone(void)
{
    const sdfio_time_zone *zone = sdfio_system_time_zone();
    CHECK(zone != NULL && zone->local_to_utc != NULL
          && zone->utc_to_local != NULL);
    if (zone == NULL || zone->local_to_utc == NULL
        || zone->utc_to_local == NULL)
    {
        return;
    }
    const time_t times[] = {1768478400, 1768521600, 1784203200, 1784246400};
    for (size_t i = 0; i < sizeof times / sizeof times[0]; ++i)
    {
        sdfio_date local;
        CHECK(zone->utc_to_local(zone->user, times[i], &local));
        CHECK(zone->local_to_utc(zone->user, &local) == times[i]);
        struct tm expected;
#ifdef _WIN32
        CHECK(localtime_s(&expected, &times[i]) == 0);
#else
        CHECK(localtime_r(&times[i], &expected) != NULL);
#endif
        CHECK(local.year == expected.tm_year + 1900
              && local.month == expected.tm_mon + 1
              && local.day == expected.tm_mday && local.hour == expected.tm_hour
              && local.minute == expected.tm_min);
    }
}

/* With a time zone, the writer stores the clock reading and the reader
 * restores the UTC time. Without one, the reading is the date. */
static void check_zone_file(void)
{
    const sdfio_time_zone zone = {NULL, central_europe_to_utc,
                                  central_europe_to_local};
    const test_options with_zone = {.time_zone = &zone, ALL_FORMATS};
    sdfio_header *header = new_header();
    SETUP(sdfio_header_set_revision(header, SDFIO_ISO_1_0));
    SETUP(sdfio_header_set_data_type(header, SDFIO_TYPE_INT16));
    SETUP(sdfio_header_set_columns(header, 1));
    SETUP(sdfio_header_set_rows(header, 1));
    SETUP(sdfio_header_set_created(header, 1782900000));
    SETUP(sdfio_header_set_modified(header, 1768474800));

    membuf sink = {0};
    const sdfio_stream stream = membuf_stream(&sink);
    const float height = 1.0F;
    sdfio_writer *writer = new_writer(&with_zone);
    SETUP(sdfio_create_stream(writer, &stream, header));
    SETUP(sdfio_write_heights(writer, sdfio_layout_packed(SDFIO_TYPE_FLOAT),
                              &height));
    SETUP(sdfio_end_file(writer));
    sdfio_writer_free(writer);
    CHECK(sink.size > DATE_AT + 2 * DATE_SIZE);
    CHECK(memcmp(sink.data + DATE_AT, "010720261200", DATE_SIZE) == 0);
    CHECK(memcmp(sink.data + DATE_AT + DATE_SIZE, "150120261200", DATE_SIZE)
          == 0);

    sdfio_reader *reader = new_reader(&with_zone);
    SETUP(sdfio_open_memory(reader, sink.data, sink.size));
    CHECK(sdfio_header_created(sdfio_reader_header(reader)) == 1782900000);
    CHECK(sdfio_header_modified(sdfio_reader_header(reader)) == 1768474800);
    sdfio_reader_free(reader);

    reader = open_memory_file(&sink);
    CHECK(sdfio_header_created(sdfio_reader_header(reader)) == 1782907200);
    sdfio_reader_free(reader);

    const sdfio_time_zone refusing = {NULL, refuse_local, refuse_utc};
    const test_options refusing_config = {.time_zone = &refusing, ALL_FORMATS};
    membuf refused = {0};
    const sdfio_stream refused_stream = membuf_stream(&refused);
    writer = new_writer(&refusing_config);
    SETUP(sdfio_create_stream(writer, &refused_stream, header));
    CHECK_STATUS(SDFIO_ERR_RANGE,
                 sdfio_write_heights(
                     writer, sdfio_layout_packed(SDFIO_TYPE_FLOAT), &height));
    sdfio_writer_free(writer);
    free(refused.data);
    free(sink.data);
}

static void check_init(void)
{
    sdfio_stream stream;
    memset(&stream, 0xA5, sizeof stream);
    sdfio_stream_init(&stream);
    CHECK(stream.user == NULL);
    CHECK(stream.read == NULL);
    CHECK(stream.write == NULL);

    sdfio_time_zone zone;
    memset(&zone, 0xA5, sizeof zone);
    sdfio_time_zone_init(&zone);
    CHECK(zone.user == NULL);
    CHECK(zone.local_to_utc == NULL);
    CHECK(zone.utc_to_local == NULL);
}

static void check_status_strings(void)
{
    for (int i = SDFIO_OK; i <= SDFIO_ERR_MEMORY; ++i)
    {
        CHECK(strlen(sdfio_status_string((sdfio_status)i)) > 0);
    }
}

static void check_version(void)
{
    CHECK(strcmp(sdfio_version(), SDFIO_VERSION_STRING) == 0);
}

#define COLUMNS 37
#define ROWS 5
#define ARENA_SIZE (1U << 16)

static void fill(float *heights)
{
    for (size_t i = 0; i < (size_t)COLUMNS * ROWS; ++i)
    {
        heights[i] = (float)i * 1.0e-7F;
    }
    heights[3] = NAN;
}

static sdfio_status round_trip(const test_options *config, sdfio_format format,
                               sdfio_data_type type, bool *equal)
{
    static float written[ROWS * COLUMNS];
    static float read[ROWS * COLUMNS];
    fill(written);
    *equal = false;

    sdfio_header *header = new_header();
    SETUP(sdfio_header_set_format(header, format));
    SETUP(sdfio_header_set_data_type(header, type));
    SETUP(sdfio_header_set_columns(header, COLUMNS));
    SETUP(sdfio_header_set_rows(header, ROWS));
    SETUP(sdfio_header_set_x_scale(header, 1.0e-6));
    SETUP(sdfio_header_set_y_scale(header, 1.0e-6));
    SETUP(sdfio_header_set_z_scale(header, SDFIO_AUTO_Z_SCALE));

    membuf sink = {0};
    sdfio_writer *writer = NULL;
    sdfio_status status = create_writer(config, &writer);
    if (status == SDFIO_OK)
    {
        const sdfio_stream stream = membuf_stream(&sink);
        status = sdfio_create_stream(writer, &stream, header);
        if (status == SDFIO_OK)
        {
            status = sdfio_write_heights(
                writer, sdfio_layout_packed(SDFIO_TYPE_FLOAT), written);
        }
        if (status == SDFIO_OK)
        {
            status = sdfio_write_trailer_text(writer, "a = 1\n", 6);
        }
        if (status == SDFIO_OK)
        {
            status = sdfio_end_file(writer);
        }
        sdfio_writer_free(writer);
    }
    if (status != SDFIO_OK)
    {
        free(sink.data);
        return status;
    }

    sdfio_reader *reader = NULL;
    status = create_reader(config, &reader);
    if (status == SDFIO_OK)
    {
        status = sdfio_open_memory(reader, sink.data, sink.size);
        if (status == SDFIO_OK)
        {
            status = sdfio_read_heights(
                reader, sdfio_layout_packed(SDFIO_TYPE_FLOAT), read);
        }
        const char *trailer = NULL;
        size_t trailer_size = 0;
        if (status == SDFIO_OK)
        {
            status = read_trailer_text(reader, 0, &trailer, &trailer_size);
        }
        if (status == SDFIO_OK)
        {
            *equal = trailer_size == 6 && memcmp(trailer, "a = 1\n", 6) == 0;
            for (size_t i = 0; i < (size_t)COLUMNS * ROWS; ++i)
            {
                if (isnan(written[i]) ? !isnan(read[i])
                                      : fabsf(read[i] - written[i]) > 1.0e-6F)
                {
                    *equal = false;
                }
            }
        }
        sdfio_reader_free(reader);
    }
    free(sink.data);
    return status;
}

static void check_allocation_count(void)
{
    static const sdfio_format formats[] = {SDFIO_BINARY, SDFIO_ASCII};
    for (size_t i = 0; i < sizeof formats / sizeof formats[0]; ++i)
    {
        counting state = COUNTING_UNLIMITED;
        const sdfio_allocator allocator = {&state, counting_allocate,
                                           counting_release};
        const test_options config = {.allocator = &allocator, ALL_FORMATS};
        bool equal = false;
        CHECK_OK(round_trip(&config, formats[i], SDFIO_TYPE_INT16, &equal));
        CHECK(equal);
        CHECK(state.calls > 0);
        CHECK(state.live == 0);

        /* Nothing is left behind wherever the memory runs out. */
        const long total = state.calls;
        for (long allow = 0; allow < total; ++allow)
        {
            counting failing = COUNTING_UNLIMITED;
            failing.allow = allow;
            const sdfio_allocator failing_allocator = {
                &failing, counting_allocate, counting_release};
            const test_options failing_config = {
                .allocator = &failing_allocator, ALL_FORMATS};
            const sdfio_status status = round_trip(&failing_config, formats[i],
                                                   SDFIO_TYPE_INT16, &equal);
            CHECK_STATUS(SDFIO_ERR_MEMORY, status);
            CHECK(failing.live == 0);
        }
    }
}

static void check_small_buffers(void)
{
    static const test_options small[] = {
        {.buffer_size = 1, .chunk_values = 1, ALL_FORMATS},
        {.buffer_size = 64, .chunk_values = 3, ALL_FORMATS},
        {.buffer_size = 100, .chunk_values = 7, ALL_FORMATS},
        {.chunk_values = 1, ALL_FORMATS}};
    static const sdfio_data_type types[] = {SDFIO_TYPE_INT16, SDFIO_TYPE_FLOAT};
    for (size_t i = 0; i < sizeof small / sizeof small[0]; ++i)
    {
        for (const sdfio_data_type *type = types; type != types + 2; ++type)
        {
            bool equal = false;
            CHECK_OK(round_trip(&small[i], SDFIO_BINARY, *type, &equal));
            CHECK(equal);
            CHECK_OK(round_trip(&small[i], SDFIO_ASCII, *type, &equal));
            CHECK(equal);
        }
    }
}

/* An arena that never gives anything back, as a program without a heap has. */
/* Memory that suits every type: the union has the alignment of the largest. */
typedef union arena_cell
{
    long long integer;
    long double real;
    void *pointer;
    void (*function)(void);
} arena_cell;

typedef struct arena
{
    arena_cell memory[ARENA_SIZE / sizeof(arena_cell)];
    size_t used;
} arena;

static void *arena_allocate(void *user, size_t size)
{
    arena *state = user;
    const size_t step = sizeof(arena_cell);
    const size_t rounded = (size + step - 1) / step * step;
    if (rounded > ARENA_SIZE - state->used)
    {
        return NULL;
    }
    void *memory = (unsigned char *)state->memory + state->used;
    state->used += rounded;
    return memory;
}

static void arena_release(void *user, void *memory)
{
    (void)user;
    (void)memory;
}

static void check_arena_allocator(void)
{
    static arena state;
    const sdfio_allocator allocator = {&state, arena_allocate, arena_release};
    const test_options config = {.allocator = &allocator,
                                 .buffer_size = 256,
                                 .chunk_values = 16,
                                 ALL_FORMATS};
    bool equal = false;
    CHECK_OK(round_trip(&config, SDFIO_BINARY, SDFIO_TYPE_FLOAT, &equal));
    CHECK(equal);
    CHECK(state.used < ARENA_SIZE);
}

static void check_default_allocator(void)
{
    const sdfio_allocator *standard = sdfio_default_allocator();
    CHECK(sdfio_default_allocator() == standard);
    CHECK(standard != NULL && standard->allocate != NULL
          && standard->release != NULL);
    if (standard != NULL && standard->allocate != NULL
        && standard->release != NULL)
    {
        void *block = standard->allocate(standard->user, 16);
        CHECK(block != NULL);
        standard->release(standard->user, block);
    }
}

int main(void)
{
    check_init();
    check_status_strings();
    check_version();
    check_date_conversion();
    check_system_time_zone();
    check_zone_file();
    check_allocation_count();
    check_small_buffers();
    check_arena_allocator();
    check_default_allocator();
    return finish_tests();
}
