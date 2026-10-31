/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

#include "check.h"
#include <stdint.h>

static void check_sizes(void)
{
    sdfio_header *header = new_header();
    CHECK_OK(sdfio_header_set_data_type(header, SDFIO_TYPE_INT16));
    CHECK_OK(sdfio_header_set_columns(header, 5));
    CHECK_OK(sdfio_header_set_rows(header, 3));

    size_t bytes = 0;
    CHECK_OK(sdfio_heights_size(header, SDFIO_TYPE_FLOAT, &bytes));
    CHECK(bytes == 15 * sizeof(float));
    CHECK_OK(sdfio_heights_row_size(header, SDFIO_TYPE_DOUBLE, &bytes));
    CHECK(bytes == 5 * sizeof(double));
    CHECK_OK(sdfio_data_size(header, &bytes));
    CHECK(bytes == 15 * sizeof(int16_t));
    CHECK_OK(sdfio_data_row_size(header, &bytes));
    CHECK(bytes == 5 * sizeof(int16_t));

    CHECK_OK(sdfio_header_set_columns(header, 0));
    CHECK_OK(sdfio_data_size(header, &bytes));
    CHECK(bytes == 0);

    CHECK_OK(sdfio_header_set_columns(header, UINT32_MAX));
    CHECK_OK(sdfio_header_set_rows(header, UINT32_MAX));
    CHECK_STATUS(SDFIO_ERR_RANGE,
                 sdfio_heights_size(header, SDFIO_TYPE_DOUBLE, &bytes));
    CHECK_OK(sdfio_header_set_data_type(header, SDFIO_TYPE_INT32));
    CHECK_STATUS(SDFIO_ERR_RANGE, sdfio_data_size(header, &bytes));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT,
                 sdfio_heights_size(header, (sdfio_data_type)99, &bytes));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_data_size(NULL, &bytes));
    CHECK_STATUS(SDFIO_ERR_ARGUMENT, sdfio_data_row_size(header, NULL));
}

int main(void)
{
    check_sizes();
    return finish_tests();
}
