# SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
#
# SPDX-License-Identifier: MIT

foreach(example write_heights read_heights read_stream write_coded read_coded
        allocators time_zone)
    add_executable(${example} ${example}.c)
    target_link_libraries(${example} PRIVATE sdfio)
    target_compile_options(${example} PRIVATE ${SDFIO_WARNINGS})
    target_compile_definitions(${example} PRIVATE
        $<$<PLATFORM_ID:Windows>:_CRT_SECURE_NO_WARNINGS>
    )
endforeach()
