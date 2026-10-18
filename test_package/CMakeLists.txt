# SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
#
# SPDX-License-Identifier: MIT

# A program outside the project that uses the installed library, as a user's
# project does: find_package, the target sdfio::sdfio, the headers.
cmake_minimum_required(VERSION 3.28)
project(sdfio_test_package LANGUAGES C)

find_package(sdfio REQUIRED)

add_executable(test_package main.c)
target_link_libraries(test_package PRIVATE sdfio::sdfio)
