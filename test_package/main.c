/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* Writes a file with the installed library and reads it back. */

#include <sdfio/file.h>
#include <sdfio/sdfio.h>

#include <math.h>
#include <stdio.h>
#include <string.h>

enum
{
    COLUMNS = 4,
    ROWS = 3
};

static int check(sdfio_status status, const char *what)
{
    if (status != SDFIO_OK)
    {
        fprintf(stderr, "%s: %s\n", what, sdfio_status_string(status));
        return 1;
    }
    return 0;
}

int main(void)
{
    static const char path[] = "test_package.sdf";
    float heights[ROWS * COLUMNS];
    float read[ROWS * COLUMNS];
    for (size_t i = 0; i < ROWS * COLUMNS; ++i)
    {
        heights[i] = (float)i * 1.0e-6F;
    }

    int failed = 0;
    sdfio_header *header = NULL;
    sdfio_status described =
        sdfio_header_create(sdfio_default_allocator(), &header);
    if (described == SDFIO_OK)
    {
        described = sdfio_header_set_now(header);
    }
    if (described == SDFIO_OK)
    {
        described = sdfio_header_set_columns(header, COLUMNS);
    }
    if (described == SDFIO_OK)
    {
        described = sdfio_header_set_rows(header, ROWS);
    }
    if (described == SDFIO_OK)
    {
        described = sdfio_header_set_x_scale(header, 1.0e-6);
    }
    if (described == SDFIO_OK)
    {
        described = sdfio_header_set_y_scale(header, 1.0e-6);
    }
    failed |= check(described, "header");
    sdfio_writer *writer = NULL;
    sdfio_status made_writer =
        sdfio_writer_create(sdfio_default_allocator(), &writer);
    if (made_writer == SDFIO_OK)
    {
        made_writer = sdfio_writer_add_binary(writer);
    }
    failed |= check(made_writer, "writer");
    if (made_writer == SDFIO_OK && described == SDFIO_OK)
    {
        failed |= check(sdfio_create_file(writer, path, header), "create");
        failed |= check(
            sdfio_write_heights(writer, sdfio_layout_packed(SDFIO_TYPE_FLOAT),
                                NAN, heights),
            "write");
        failed |= check(sdfio_end_file(writer), "trailer");
    }
    sdfio_writer_free(writer);
    sdfio_header_free(header);

    sdfio_reader *reader = NULL;
    sdfio_status made_reader =
        sdfio_reader_create(sdfio_default_allocator(), &reader);
    if (made_reader == SDFIO_OK)
    {
        made_reader = sdfio_reader_add_binary(reader);
    }
    failed |= check(made_reader, "reader");
    if (made_reader == SDFIO_OK)
    {
        failed |= check(sdfio_open_file(reader, path), "open");
        failed |=
            check(sdfio_read_heights(
                      reader, sdfio_layout_packed(SDFIO_TYPE_FLOAT), NAN, read),
                  "read");
    }
    sdfio_reader_free(reader);
    remove(path);

    failed |= memcmp(heights, read, sizeof heights) != 0;
    printf("libsdfio %s: %s\n", sdfio_version(), failed ? "failed" : "ok");
    return failed;
}
