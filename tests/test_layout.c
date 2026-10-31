/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

#include "check.h"
#include <stdint.h>

static void check_init_and_packed(void)
{
    sdfio_layout layout;
    memset(&layout, 0xA5, sizeof layout);
    sdfio_layout_init(&layout);
    CHECK(layout.type == SDFIO_TYPE_FLOAT);
    CHECK(layout.element_stride == 0);
    CHECK(layout.row_stride == 0);

    const sdfio_data_type types[] = {SDFIO_TYPE_FLOAT, SDFIO_TYPE_INT8,
                                     SDFIO_TYPE_INT16, SDFIO_TYPE_INT32,
                                     SDFIO_TYPE_DOUBLE};
    for (size_t i = 0; i < sizeof types / sizeof types[0]; ++i)
    {
        const sdfio_layout *packed = sdfio_layout_packed(types[i]);
        CHECK(packed != NULL && packed->type == types[i]
              && packed->element_stride == 0 && packed->row_stride == 0);
        CHECK(sdfio_layout_packed(types[i]) == packed);
    }
    CHECK(sdfio_layout_packed((sdfio_data_type)99) == NULL);
}

int main(void)
{
    check_init_and_packed();
    return finish_tests();
}
