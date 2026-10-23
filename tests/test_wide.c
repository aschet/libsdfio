/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* Files by a path of wide characters, on Windows: a name that the code page of
 * the C library cannot hold. */

#include "check.h"

#include <sdfio/file.h>

#include <wchar.h>

static bool file_exists(const wchar_t *path)
{
    FILE *file = _wfopen(path, L"rb");
    if (file != NULL)
    {
        fclose(file);
    }
    return file != NULL;
}

int main(void)
{
    /* u-umlaut, and two characters outside of every Western code page. */
    static const wchar_t path[] = L"test_wide_\u00fc\u6e2c\u5b9a.sdf";
    static const wchar_t missing[] = L"no_such_\u6e2c\u5b9a_folder\\out.sdf";
    _wremove(path);

    sdfio_header header;
    sdfio_header_init(&header);
    header.columns = 2;
    header.rows = 2;
    const double heights[4] = {1, 2, 3, 4};

    sdfio_writer *writer = sdfio_writer_new(all_formats());
    CHECK_OK(sdfio_create_file_w(writer, path, &header));
    CHECK(file_exists(path));
    CHECK_OK(sdfio_write_heights(writer, SDFIO_TYPE_DOUBLE, heights));
    CHECK_OK(sdfio_end_trailer(writer));
    sdfio_writer_free(writer);
    CHECK(file_exists(path));

    sdfio_reader *reader = sdfio_reader_new(all_formats());
    CHECK_OK(sdfio_open_file_w(reader, path));
    double back[4] = {0};
    CHECK_OK(sdfio_read_heights(reader, SDFIO_TYPE_DOUBLE, back));
    CHECK(back[0] == 1.0 && back[3] == 4.0);
    sdfio_reader_free(reader);
    _wremove(path);

    reader = sdfio_reader_new(all_formats());
    CHECK_STATUS(SDFIO_ERR_IO, sdfio_open_file_w(reader, missing));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_open_file_w(reader, NULL));
    sdfio_reader_free(reader);
    writer = sdfio_writer_new(all_formats());
    CHECK_STATUS(SDFIO_ERR_IO, sdfio_create_file_w(writer, missing, &header));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_create_file_w(writer, NULL, &header));
    sdfio_writer_free(writer);

    if (failures != 0)
    {
        fprintf(stderr, "%d of %d checks failed\n", failures, checks);
        return 1;
    }
    return 0;
}
