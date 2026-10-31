/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

#include "check.h"
#include <stdio.h>
#include <stdlib.h>

#include <sdfio/file.h>

#ifdef _WIN32
#include <wchar.h>
#endif

static bool file_exists(const char *path)
{
    FILE *file = fopen(path, "rb");
    if (file != NULL)
    {
        fclose(file);
    }
    return file != NULL;
}

/* A file is written in place, and a header that cannot be written creates
 * none. */
static void check_file(void)
{
    const char *path = "test_file_output.sdf";
    remove(path);
    sdfio_header *header = new_header();
    SETUP(sdfio_header_set_columns(header, 2));
    SETUP(sdfio_header_set_rows(header, 2));
    const double heights[4] = {1, 2, 3, 4};

    sdfio_writer *writer = new_writer(all_formats());
    CHECK_OK(sdfio_create_file(writer, path, header));
    SETUP(sdfio_write_heights(writer, sdfio_layout_packed(SDFIO_TYPE_DOUBLE),
                              heights));
    SETUP(sdfio_end_file(writer));
    sdfio_writer_free(writer);
    CHECK(file_exists(path));

    sdfio_reader *reader = new_reader(all_formats());
    CHECK_OK(sdfio_open_file(reader, path));
    double back[4] = {0};
    SETUP(sdfio_read_heights(reader, sdfio_layout_packed(SDFIO_TYPE_DOUBLE),
                             back));
    CHECK(back[3] == 4.0 && back[0] == 1.0);
    sdfio_reader_free(reader);
    remove(path);

    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_header_set_z_scale(header, -1.0));

    writer = new_writer(all_formats());
    SETUP(sdfio_header_set_revision(header, SDFIO_ISO_1_0));
    SETUP(sdfio_header_set_data_type(header, SDFIO_TYPE_INT8));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_create_file(writer, path, header));
    CHECK(!file_exists(path));
    sdfio_writer_free(writer);

    SETUP(sdfio_header_set_revision(header, SDFIO_ISO_2_0));
    SETUP(sdfio_header_set_data_type(header, SDFIO_TYPE_FLOAT));
    writer = new_writer(all_formats());
    CHECK_STATUS(SDFIO_ERR_IO,
                 sdfio_create_file(writer, "no_such_folder/out.sdf", header));
    sdfio_writer_free(writer);
}

/* The stdio stream reads and writes a C file and leaves it open. */
static void check_stdio_stream(void)
{
    FILE *file = tmpfile();
    CHECK(file != NULL);
    if (file == NULL)
    {
        return;
    }
    sdfio_stream stream;
    sdfio_stdio_stream(&stream, file);
    CHECK(stream.user == file && stream.read != NULL && stream.write != NULL);

    sdfio_header *header = new_header();
    SETUP(sdfio_header_set_columns(header, 2));
    SETUP(sdfio_header_set_rows(header, 1));
    const float heights[2] = {1e-6F, 2e-6F};
    sdfio_writer *writer = new_writer(all_formats());
    CHECK_OK(sdfio_create_stream(writer, &stream, header));
    SETUP(sdfio_write_heights(writer, sdfio_layout_packed(SDFIO_TYPE_FLOAT),
                              heights));
    SETUP(sdfio_end_file(writer));
    sdfio_writer_free(writer);
    CHECK(fflush(file) == 0);

    CHECK(fseek(file, 0, SEEK_SET) == 0);
    float back[2];
    sdfio_reader *reader = new_reader(all_formats());
    CHECK_OK(sdfio_open_stream(reader, &stream));
    SETUP(sdfio_read_heights(reader, sdfio_layout_packed(SDFIO_TYPE_FLOAT),
                             back));
    CHECK(back[0] == heights[0] && back[1] == heights[1]);
    sdfio_reader_free(reader);
    CHECK(fclose(file) == 0);
}

#ifdef _WIN32

static bool wide_file_exists(const wchar_t *path)
{
    FILE *file = _wfopen(path, L"rb");
    if (file != NULL)
    {
        fclose(file);
    }
    return file != NULL;
}

static void check_wide_file(void)
{

    /* u-umlaut, and two characters outside of every Western code page. */
    static const wchar_t path[] = L"test_wide_\u00fc\u6e2c\u5b9a.sdf";
    static const wchar_t missing[] = L"no_such_\u6e2c\u5b9a_folder\\out.sdf";
    _wremove(path);

    sdfio_header *header = new_header();
    SETUP(sdfio_header_set_columns(header, 2));
    SETUP(sdfio_header_set_rows(header, 2));
    const double heights[4] = {1, 2, 3, 4};

    sdfio_writer *writer = new_writer(all_formats());
    CHECK_OK(sdfio_create_wfile(writer, path, header));
    CHECK(wide_file_exists(path));
    CHECK_OK(sdfio_write_heights(writer, sdfio_layout_packed(SDFIO_TYPE_DOUBLE),
                                 heights));
    SETUP(sdfio_end_file(writer));
    sdfio_writer_free(writer);
    CHECK(wide_file_exists(path));

    sdfio_reader *reader = new_reader(all_formats());
    CHECK_OK(sdfio_open_wfile(reader, path));
    double back[4] = {0};
    CHECK_OK(sdfio_read_heights(reader, sdfio_layout_packed(SDFIO_TYPE_DOUBLE),
                                back));
    CHECK(back[0] == 1.0 && back[3] == 4.0);
    sdfio_reader_free(reader);
    _wremove(path);

    reader = new_reader(all_formats());
    CHECK_STATUS(SDFIO_ERR_IO, sdfio_open_wfile(reader, missing));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_open_wfile(reader, NULL));
    sdfio_reader_free(reader);
    writer = new_writer(all_formats());
    CHECK_STATUS(SDFIO_ERR_IO, sdfio_create_wfile(writer, missing, header));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_create_wfile(writer, NULL, header));
    sdfio_writer_free(writer);
}

#endif

int main(void)
{
    check_file();
    check_stdio_stream();
#ifdef _WIN32
    check_wide_file();
#endif
    return finish_tests();
}
