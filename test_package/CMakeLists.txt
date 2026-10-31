# SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
#
# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.28)
project(sdfio_test_package LANGUAGES C)

set(CMAKE_C_EXTENSIONS OFF)

find_package(sdfio REQUIRED)

add_executable(test_package main.c)
target_link_libraries(test_package PRIVATE sdfio::sdfio)
