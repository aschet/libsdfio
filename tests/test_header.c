/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

#include "check.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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

/* Setters round down to the minute, and set_dates sets both dates or neither.
 */
static void check_dates(void)
{
    sdfio_header *header = NULL;
    CHECK_OK(sdfio_header_create(sdfio_default_allocator(), &header));
    /* 2026-03-04 05:06:59 UTC */
    CHECK_OK(sdfio_header_set_created(header, 1772600819));
    CHECK(sdfio_header_created(header) == 1772600760);
    CHECK_OK(sdfio_header_set_modified(header, 1772600790));
    CHECK(sdfio_header_modified(header) == 1772600760);
    /* A time before 1970 is rounded down as well. */
    CHECK_OK(sdfio_header_set_created(header, -61));
    CHECK(sdfio_header_created(header) == -120);
    CHECK_OK(sdfio_header_set_dates(header, 1772600819));
    CHECK(sdfio_header_created(header) == 1772600760);
    CHECK(sdfio_header_modified(header) == 1772600760);
    CHECK_OK(sdfio_header_set_dates(header, SDFIO_NO_TIME));
    CHECK(sdfio_header_created(header) == SDFIO_NO_TIME);
    CHECK(sdfio_header_modified(header) == SDFIO_NO_TIME);
    CHECK_OK(sdfio_header_set_dates(header, 1772600760));
    if (sizeof(time_t) >= 8)
    {
        const time_t far_away = (time_t)(INT64_C(1) << 40);
        CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                     sdfio_header_set_dates(header, far_away));
        CHECK(sdfio_header_created(header) == 1772600760);
        CHECK(sdfio_header_modified(header) == 1772600760);
    }
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_header_set_dates(NULL, 0));
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
    CHECK_OK(sdfio_header_set_manufacturer(header, "01234567890123"));
    CHECK(strcmp(sdfio_header_manufacturer(header), "0123456789") == 0);
    CHECK_OK(sdfio_header_set_manufacturer(header, "0123456789\r\n"));
    CHECK(strcmp(sdfio_header_manufacturer(header), "0123456789") == 0);
}

/* A setter refuses only an invalid value of its own field and leaves the
 * header unchanged. */
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

static void check_copy(void)
{
    counting first = COUNTING_UNLIMITED;
    counting second = COUNTING_UNLIMITED;
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
    CHECK(first.calls == 1 && first.released == 1);
    CHECK(second.calls == 1 && second.released == 1);
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

/* An identifier is stored and returned byte by byte, with no check of its
 * encoding. */
static void check_bytes_of_the_identifier(void)
{
    const char *const identifiers[] = {"0123456789", "caf\xC3\xA9",
                                       "\x7F\t\x1F", "\xE9\xFF"};
    sdfio_header *header = new_header();
    for (size_t i = 0; i < sizeof identifiers / sizeof identifiers[0]; ++i)
    {
        CHECK_OK(sdfio_header_set_manufacturer(header, identifiers[i]));
        CHECK(strcmp(sdfio_header_manufacturer(header), identifiers[i]) == 0);
    }
}

int main(void)
{
    check_defaults();
    check_dates();
    check_setters();
    check_refusals();
    check_copy();
    check_creation();
    check_bytes_of_the_identifier();
    return finish_tests();
}
