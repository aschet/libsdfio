/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* Writes a file with the installed library and reads it back. */

#include <sdfio/file.h>
#include <sdfio/sdfio.h>

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

    sdfio_header header;
    sdfio_header_init(&header);
    header.columns = COLUMNS;
    header.rows = ROWS;
    header.x_scale = 1.0e-6;
    header.y_scale = 1.0e-6;

    int failed = 0;
    const sdfio_config config = {
        .codecs = {&sdfio_binary_codec, &sdfio_ascii_codec}};
    sdfio_writer *writer = sdfio_writer_new(&config);
    failed |= writer == NULL;
    if (writer != NULL)
    {
        failed |= check(sdfio_create_file(writer, path, &header), "create");
        failed |= check(sdfio_write_heights(writer, SDFIO_TYPE_FLOAT, heights),
                        "write");
        failed |= check(sdfio_write_trailer(writer, NULL, 0), "trailer");
    }
    sdfio_writer_free(writer);

    sdfio_reader *reader = sdfio_reader_new(&config);
    failed |= reader == NULL;
    if (reader != NULL)
    {
        failed |= check(sdfio_open_file(reader, path), "open");
        failed |=
            check(sdfio_read_heights(reader, SDFIO_TYPE_FLOAT, read), "read");
    }
    sdfio_reader_free(reader);
    remove(path);

    failed |= memcmp(heights, read, sizeof heights) != 0;
    printf("libsdfio %s: %s\n", sdfio_version(), failed ? "failed" : "ok");
    return failed;
}
