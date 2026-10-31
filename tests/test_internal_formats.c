/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

#include "check.h"
#include "reader_format.h"
#include "writer_format.h"

static void check_reader_formats(void)
{
    sdfio_reader_format first_read = {.prefix = 'x'};
    sdfio_reader_format second_read = {.prefix = 'y'};
    sdfio_reader_format third_read = {.prefix = 'z'};
    sdfio_reader *reader = NULL;
    SETUP(sdfio_reader_create(sdfio_default_allocator(), &reader));
    CHECK_OK(sdfio_reader_install_format(reader, &first_read));
    CHECK_OK(sdfio_reader_install_format(reader, &second_read));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_reader_install_format(reader, &first_read));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_reader_install_format(reader, &third_read));
    sdfio_reader_free(reader);
}

static void check_writer_formats(void)
{
    sdfio_writer_format first_write = {.writes = SDFIO_BINARY};
    sdfio_writer_format second_write = {.writes = SDFIO_ASCII};
    sdfio_writer_format third_write = {.writes = SDFIO_BINARY};
    sdfio_writer *writer = NULL;
    SETUP(sdfio_writer_create(sdfio_default_allocator(), &writer));
    CHECK_OK(sdfio_writer_install_format(writer, &first_write));
    CHECK_OK(sdfio_writer_install_format(writer, &second_write));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_writer_install_format(writer, &first_write));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_writer_install_format(writer, &third_write));
    sdfio_writer_free(writer);
}

int main(void)
{
    check_reader_formats();
    check_writer_formats();
    return finish_tests();
}
