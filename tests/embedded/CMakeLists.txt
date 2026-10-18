# SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
#
# SPDX-License-Identifier: MIT

# Firmware for a target without an operating system, to see that the library
# links without a file system and with little of the C library. It has no start
# file and is not run.
add_executable(firmware main.c)
target_link_libraries(firmware PRIVATE sdfio)
target_compile_options(firmware PRIVATE ${SDFIO_WARNINGS})
target_link_options(firmware PRIVATE -nostartfiles -Wl,--gc-sections -Wl,--entry=main)
