/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

#include <sdfio/sdfio.h>

#include <stdio.h>

int main(void)
{
    sdfio_reader *reader = NULL;
    const sdfio_status status =
        sdfio_reader_create(sdfio_default_allocator(), &reader);
    sdfio_reader_free(reader);
    printf("libsdfio %s: %s\n", sdfio_version(),
           status == SDFIO_OK ? "ok" : "failed");
    return status != SDFIO_OK;
}
