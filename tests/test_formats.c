/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

#include "check.h"
#include "reader_format.h"
#include "writer_format.h"
#include <string.h>

static void check_format_list(void)
{
    sdfio_reader_format first_read = {.prefix = 'x'};
    sdfio_reader_format second_read = {.prefix = 'y'};
    sdfio_reader_format third_read = {.prefix = 'z'};
    sdfio_reader *reader = NULL;
    CHECK_OK(sdfio_reader_create(sdfio_default_allocator(), &reader));
    CHECK_OK(sdfio_reader_install_format(reader, &first_read));
    CHECK_OK(sdfio_reader_install_format(reader, &second_read));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_reader_install_format(reader, &first_read));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_reader_install_format(reader, &third_read));
    sdfio_reader_free(reader);

    sdfio_writer_format first_write = {.writes = SDFIO_BINARY};
    sdfio_writer_format second_write = {.writes = SDFIO_ASCII};
    sdfio_writer_format third_write = {.writes = SDFIO_BINARY};
    sdfio_writer *writer = NULL;
    CHECK_OK(sdfio_writer_create(sdfio_default_allocator(), &writer));
    CHECK_OK(sdfio_writer_install_format(writer, &first_write));
    CHECK_OK(sdfio_writer_install_format(writer, &second_write));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_writer_install_format(writer, &first_write));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_writer_install_format(writer, &third_write));
    sdfio_writer_free(writer);
}

static void check_magics(void)
{
    sdfio_reader *all = new_reader(all_formats());
    CHECK(sdfio_reader_magic_count(all) == 4U);
    CHECK(strcmp(sdfio_reader_magic(all, 0), "bISO-1.0") == 0);
    CHECK(strcmp(sdfio_reader_magic(all, 1), "bISO-2.0") == 0);
    CHECK(strcmp(sdfio_reader_magic(all, 2), "aISO-1.0") == 0);
    CHECK(strcmp(sdfio_reader_magic(all, 3), "aISO-2.0") == 0);
    CHECK(sdfio_reader_magic(all, sdfio_reader_magic_count(all)) == NULL);
    sdfio_reader_free(all);

    const test_options ascii_first = {
        .reader_formats = {TEST_ASCII, TEST_BINARY}};
    sdfio_reader *ascii = new_reader(&ascii_first);
    CHECK(strcmp(sdfio_reader_magic(ascii, 0), "aISO-1.0") == 0);
    sdfio_reader_free(ascii);
    const test_options binary_only = {.reader_formats = {TEST_BINARY}};
    sdfio_reader *binary = new_reader(&binary_only);
    CHECK(sdfio_reader_magic_count(binary) == 2U);
    CHECK(sdfio_reader_magic(binary, 2) == NULL);
    sdfio_reader_free(binary);
    CHECK(sdfio_reader_magic_count(NULL) == 0U);
    CHECK(sdfio_reader_magic(NULL, 0) == NULL);
}

/* The list holds only the formats of the library. */

int main(void)
{
    check_format_list();
    check_magics();
    return finish_tests();
}
