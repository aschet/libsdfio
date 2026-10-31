# SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
#
# SPDX-License-Identifier: MIT

find_package(Threads)

# A test links the library, as a user does. A test of an internal function sets
# INTERNAL and links the objects of the library, which have the functions that
# the library hides.
function(sdfio_add_test name)
    cmake_parse_arguments(PARSE_ARGV 1 TEST "INTERNAL" "" "")
    add_executable(${name} ${name}.c)
    target_link_libraries(${name} PRIVATE
        $<IF:$<BOOL:${TEST_INTERNAL}>,sdfio_objects,sdfio>
        $<$<NOT:$<PLATFORM_ID:Windows>>:m>
    )
    target_compile_options(${name} PRIVATE ${SDFIO_WARNINGS})
    target_compile_definitions(${name} PRIVATE
        SDFIO_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
    )
    if(TEST_INTERNAL)
        # The objects are linked also when the library is shared: their
        # functions are not imported from the shared library.
        target_include_directories(${name} PRIVATE "${PROJECT_SOURCE_DIR}/src")
        target_compile_definitions(${name} PRIVATE SDFIO_STATIC_DEFINE)
    endif()
    add_test(NAME ${name} COMMAND ${name})
endfunction()

sdfio_add_test(test_samples)
sdfio_add_test(test_roundtrip)
sdfio_add_test(test_file)
sdfio_add_test(test_reader)
sdfio_add_test(test_writer)
sdfio_add_test(test_malformed)
sdfio_add_test(test_internal_number_text INTERNAL)
sdfio_add_test(test_internal_datetime INTERNAL)
sdfio_add_test(test_internal_layout INTERNAL)
sdfio_add_test(test_internal_formats INTERNAL)
sdfio_add_test(test_heights)
sdfio_add_test(test_layout)
sdfio_add_test(test_types)
sdfio_add_test(test_sizes)
sdfio_add_test(test_trailer)
sdfio_add_test(test_reset)
sdfio_add_test(test_header)

if(Threads_FOUND AND CMAKE_USE_PTHREADS_INIT)
    sdfio_add_test(test_threads)
    target_link_libraries(test_threads PRIVATE Threads::Threads)
endif()
