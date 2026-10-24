/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* The header: its defaults, the check of each setter, the copy, and the memory
 * that it takes from its allocator. */

#include "check.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct counting
{
    long allocated;
    long released;
} counting;

static void *counting_allocate(void *user, size_t size)
{
    counting *state = user;
    ++state->allocated;
    return malloc(size);
}

static void counting_release(void *user, void *memory)
{
    counting *state = user;
    ++state->released;
    free(memory);
}

static void *failing_allocate(void *user, size_t size)
{
    (void)user;
    (void)size;
    return NULL;
}

static void check_defaults(void)
{
    sdfio_header *header = NULL;
    CHECK_OK(sdfio_header_create(sdfio_default_allocator(), &header));
    CHECK(sdfio_header_revision(header) == SDFIO_ISO_2_0);
    CHECK(sdfio_header_format(header) == SDFIO_BINARY);
    CHECK(sdfio_header_data_type(header) == SDFIO_TYPE_FLOAT);
    CHECK(sdfio_header_columns(header) == 0 && sdfio_header_rows(header) == 0);
    CHECK(sdfio_header_x_scale(header) == 1.0);
    CHECK(sdfio_header_y_scale(header) == 1.0);
    CHECK(sdfio_header_z_scale(header) == SDFIO_AUTO_Z_SCALE);
    CHECK(sdfio_header_z_resolution(header) == SDFIO_NO_RESOLUTION);
    CHECK(sdfio_header_created(header) == SDFIO_NO_TIME);
    CHECK(sdfio_header_modified(header) == SDFIO_NO_TIME);
    CHECK(strcmp(sdfio_header_manufacturer(header), "libsdfio") == 0);
    sdfio_header_free(header);
}

/* The dates of the clock are one time, rounded down to the minute. */
static void check_dates_now(void)
{
    sdfio_header *header = NULL;
    CHECK_OK(sdfio_header_create(sdfio_default_allocator(), &header));
    CHECK_OK(sdfio_header_set_now(header));
    const time_t created = sdfio_header_created(header);
    CHECK(created != SDFIO_NO_TIME);
    CHECK(created == sdfio_header_modified(header));
    CHECK(created % 60 == 0);
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_header_set_now(NULL));
    sdfio_header_free(header);
}

static void check_setters(void)
{
    sdfio_header *header = new_header();
    CHECK_OK(sdfio_header_set_revision(header, SDFIO_ISO_1_0));
    CHECK_OK(sdfio_header_set_format(header, SDFIO_ASCII));
    CHECK_OK(sdfio_header_set_data_type(header, SDFIO_TYPE_INT16));
    CHECK_OK(sdfio_header_set_columns(header, UINT32_MAX));
    CHECK_OK(sdfio_header_set_rows(header, 7));
    CHECK_OK(sdfio_header_set_x_scale(header, -2e-6));
    CHECK_OK(sdfio_header_set_y_scale(header, 3e-6));
    CHECK_OK(sdfio_header_set_z_scale(header, 1e-9));
    CHECK_OK(sdfio_header_set_z_resolution(header, -5.0));
    CHECK_OK(sdfio_header_set_created(header, 1772600760));
    CHECK_OK(sdfio_header_set_modified(header, SDFIO_NO_TIME));
    CHECK_OK(sdfio_header_set_manufacturer(header, "0123456789"));
    CHECK(sdfio_header_revision(header) == SDFIO_ISO_1_0);
    CHECK(sdfio_header_format(header) == SDFIO_ASCII);
    CHECK(sdfio_header_data_type(header) == SDFIO_TYPE_INT16);
    CHECK(sdfio_header_columns(header) == UINT32_MAX);
    CHECK(sdfio_header_rows(header) == 7);
    CHECK(sdfio_header_x_scale(header) == (double)-2e-6);
    CHECK(sdfio_header_y_scale(header) == (double)3e-6);
    CHECK(sdfio_header_z_scale(header) == (double)1e-9);
    CHECK(sdfio_header_z_resolution(header) == SDFIO_NO_RESOLUTION);
    CHECK_OK(sdfio_header_set_z_resolution(header, 1e-9));
    CHECK(sdfio_header_z_resolution(header) == (double)1e-9);
    CHECK_OK(sdfio_header_set_z_resolution(header, 0.0));
    CHECK(sdfio_header_z_resolution(header) == SDFIO_NO_RESOLUTION);
    CHECK(sdfio_header_created(header) == 1772600760);
    CHECK(sdfio_header_modified(header) == SDFIO_NO_TIME);
    CHECK(strcmp(sdfio_header_manufacturer(header), "0123456789") == 0);
    CHECK_OK(sdfio_header_set_z_scale(header, SDFIO_AUTO_Z_SCALE));
    CHECK_OK(sdfio_header_set_manufacturer(header, ""));
    CHECK(strcmp(sdfio_header_manufacturer(header), "") == 0);
    CHECK_OK(sdfio_header_set_manufacturer(header, " !~"));
    CHECK(strcmp(sdfio_header_manufacturer(header), " !~") == 0);
}

/* A setter refuses what is wrong in its field alone, and changes nothing. */
static void check_refusals(void)
{
    sdfio_header *header = new_header();
    CHECK_OK(sdfio_header_set_manufacturer(header, "kept"));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_header_set_revision(header, (sdfio_revision)99));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_header_set_format(header, (sdfio_format)99));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_header_set_data_type(header, (sdfio_data_type)99));
    const double not_finite[] = {NAN, INFINITY, -INFINITY};
    for (size_t i = 0; i < sizeof not_finite / sizeof not_finite[0]; ++i)
    {
        CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                     sdfio_header_set_x_scale(header, not_finite[i]));
        CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                     sdfio_header_set_y_scale(header, not_finite[i]));
        CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                     sdfio_header_set_z_scale(header, not_finite[i]));
        CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                     sdfio_header_set_z_resolution(header, not_finite[i]));
    }
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_header_set_z_scale(header, -1.0));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_header_set_manufacturer(header, "01234567890"));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_header_set_manufacturer(header, NULL));
    /* A line end would corrupt a header of the ASCII format. */
    const char *const line_ends[] = {"a\nb", "a\r\n*", "a\r"};
    for (size_t i = 0; i < sizeof line_ends / sizeof line_ends[0]; ++i)
    {
        CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                     sdfio_header_set_manufacturer(header, line_ends[i]));
    }
    CHECK(strcmp(sdfio_header_manufacturer(header), "kept") == 0);
    if (sizeof(time_t) >= 8)
    {
        const time_t far_away =
            (time_t)(sizeof(time_t) >= 8 ? INT64_C(1) << 60 : 0);
        CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                     sdfio_header_set_created(header, far_away));
        CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                     sdfio_header_set_modified(header, -far_away));
    }
    CHECK(sdfio_header_revision(header) == SDFIO_ISO_2_0);
    CHECK(sdfio_header_x_scale(header) == 1.0);
    CHECK(sdfio_header_z_scale(header) == SDFIO_AUTO_Z_SCALE);
    CHECK(strcmp(sdfio_header_manufacturer(header), "kept") == 0);

    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_header_set_revision(NULL, 0));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_header_set_columns(NULL, 1));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_header_set_rows(NULL, 1));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_header_set_x_scale(NULL, 1.0));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_header_set_manufacturer(NULL, ""));
}

/* The limits that depend on several fields are checked when a file is
 * created. */
static void check_cross_field_checks(void)
{
    sdfio_header *header = new_header();
    CHECK_OK(sdfio_header_set_revision(header, SDFIO_ISO_1_0));
    CHECK_OK(sdfio_header_set_data_type(header, SDFIO_TYPE_INT8));
    membuf file = {0};
    const sdfio_stream stream = membuf_stream(&file);
    sdfio_writer *writer = new_writer(all_formats());
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_create_stream(writer, &stream, header));
    CHECK_OK(sdfio_header_set_data_type(header, SDFIO_TYPE_INT16));
    CHECK_OK(sdfio_header_set_columns(header, (uint32_t)UINT16_MAX + 1));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_create_stream(writer, &stream, header));
    CHECK_OK(sdfio_header_set_columns(header, 1));
    CHECK_OK(sdfio_header_set_rows(header, 1));
    CHECK_OK(sdfio_header_set_created(header, SDFIO_NO_TIME));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_create_stream(writer, &stream, header));
    sdfio_writer_free(writer);
    free(file.data);
}

static void check_copy(void)
{
    counting first = {0, 0};
    counting second = {0, 0};
    const sdfio_allocator first_allocator = {&first, counting_allocate,
                                             counting_release};
    const sdfio_allocator second_allocator = {&second, counting_allocate,
                                              counting_release};
    sdfio_header *source = NULL;
    sdfio_header *target = NULL;
    CHECK_OK(sdfio_header_create(&first_allocator, &source));
    CHECK_OK(sdfio_header_create(&second_allocator, &target));
    CHECK_OK(sdfio_header_set_revision(source, SDFIO_ISO_1_0));
    CHECK_OK(sdfio_header_set_data_type(source, SDFIO_TYPE_INT32));
    CHECK_OK(sdfio_header_set_columns(source, 11));
    CHECK_OK(sdfio_header_set_rows(source, 13));
    CHECK_OK(sdfio_header_set_x_scale(source, 4e-6));
    CHECK_OK(sdfio_header_set_z_scale(source, 2e-9));
    CHECK_OK(sdfio_header_set_modified(source, 1772604360));
    CHECK_OK(sdfio_header_set_manufacturer(source, "ACME 7"));
    CHECK_OK(sdfio_header_copy(target, source));
    CHECK(sdfio_header_revision(target) == SDFIO_ISO_1_0);
    CHECK(sdfio_header_data_type(target) == SDFIO_TYPE_INT32);
    CHECK(sdfio_header_columns(target) == 11
          && sdfio_header_rows(target) == 13);
    CHECK(sdfio_header_x_scale(target) == (double)4e-6);
    CHECK(sdfio_header_z_scale(target) == (double)2e-9);
    CHECK(sdfio_header_modified(target) == 1772604360);
    CHECK(strcmp(sdfio_header_manufacturer(target), "ACME 7") == 0);
    CHECK(sdfio_header_manufacturer(target)
          != sdfio_header_manufacturer(source));
    CHECK_OK(sdfio_header_set_columns(source, 1));
    CHECK(sdfio_header_columns(target) == 11);

    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_header_copy(NULL, source));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_header_copy(target, NULL));

    /* Each header goes back to the allocator that created it. */
    sdfio_header_free(source);
    sdfio_header_free(target);
    sdfio_header_free(NULL);
    CHECK(first.allocated == 1 && first.released == 1);
    CHECK(second.allocated == 1 && second.released == 1);
}

static void check_creation(void)
{
    sdfio_header *header = NULL;
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_header_create(sdfio_default_allocator(), NULL));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_header_create(NULL, &header));
    CHECK(header == NULL);
    const sdfio_allocator incomplete = {NULL, counting_allocate, NULL};
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_header_create(&incomplete, &header));
    CHECK(header == NULL);
    const sdfio_allocator failing = {NULL, failing_allocate, counting_release};
    CHECK_STATUS(SDFIO_ERR_MEMORY, sdfio_header_create(&failing, &header));
    CHECK(header == NULL);
}

/* The encoding of the identifier is not checked: the bytes of the program go
 * into a file of either format and come back as they are. */
static void check_bytes_of_the_identifier(void)
{
    const char *const identifiers[] = {"caf\xC3\xA9", "\x7F\t\x1F", "\xE9\xFF"};
    const sdfio_format formats[] = {SDFIO_BINARY, SDFIO_ASCII};
    for (size_t i = 0; i < sizeof identifiers / sizeof identifiers[0]; ++i)
    {
        for (size_t f = 0; f < 2; ++f)
        {
            sdfio_header *header = new_header();
            CHECK_OK(sdfio_header_set_format(header, formats[f]));
            CHECK_OK(sdfio_header_set_columns(header, 1));
            CHECK_OK(sdfio_header_set_rows(header, 1));
            CHECK_OK(sdfio_header_set_manufacturer(header, identifiers[i]));
            CHECK(strcmp(sdfio_header_manufacturer(header), identifiers[i])
                  == 0);
            const float height = 1e-6F;
            membuf file = {0};
            const sdfio_stream stream = membuf_stream(&file);
            sdfio_writer *writer = new_writer(all_formats());
            CHECK_OK(sdfio_create_stream(writer, &stream, header));
            CHECK_OK(sdfio_write_heights(
                writer, sdfio_layout_packed(SDFIO_TYPE_FLOAT), &height));
            CHECK_OK(sdfio_end_file(writer));
            sdfio_writer_free(writer);

            sdfio_reader *reader = new_reader(all_formats());
            CHECK_OK(sdfio_open_memory(reader, file.data, file.size));
            const sdfio_header *read = sdfio_reader_header(reader);
            CHECK(read != NULL
                  && strcmp(sdfio_header_manufacturer(read), identifiers[i])
                         == 0);
            sdfio_reader_free(reader);
            free(file.data);
        }
    }
}

/* The header of a reader is copied to keep it after the reader is freed. */
static void check_copy_of_a_read_header(void)
{
    sdfio_header *header = new_header();
    CHECK_OK(sdfio_header_set_columns(header, 3));
    CHECK_OK(sdfio_header_set_rows(header, 2));
    CHECK_OK(sdfio_header_set_data_type(header, SDFIO_TYPE_INT16));
    CHECK_OK(sdfio_header_set_z_scale(header, 1e-9));
    CHECK_OK(sdfio_header_set_manufacturer(header, "ACME"));
    const int16_t coded[6] = {1, 2, 3, 4, 5, 6};
    membuf file = {0};
    const sdfio_stream stream = membuf_stream(&file);
    sdfio_writer *writer = new_writer(all_formats());
    CHECK_OK(sdfio_create_stream(writer, &stream, header));
    CHECK_OK(sdfio_write_data(writer, sdfio_layout_packed(SDFIO_TYPE_INT16),
                              coded, 2));
    CHECK_OK(sdfio_end_file(writer));
    sdfio_writer_free(writer);

    sdfio_reader *reader = new_reader(all_formats());
    CHECK_OK(sdfio_open_memory(reader, file.data, file.size));
    sdfio_header *kept = new_header();
    CHECK_OK(sdfio_header_copy(kept, sdfio_reader_header(reader)));
    sdfio_reader_free(reader);
    CHECK(sdfio_header_columns(kept) == 3 && sdfio_header_rows(kept) == 2);
    CHECK(sdfio_header_z_scale(kept) == (double)1e-9);
    CHECK(strcmp(sdfio_header_manufacturer(kept), "ACME") == 0);
    free(file.data);
}

int main(void)
{
    check_defaults();
    check_dates_now();
    check_setters();
    check_refusals();
    check_cross_field_checks();
    check_copy();
    check_creation();
    check_copy_of_a_read_header();
    check_bytes_of_the_identifier();
    return finish_tests();
}
