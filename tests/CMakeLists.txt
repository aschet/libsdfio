# SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
#
# SPDX-License-Identifier: MIT

find_package(Threads)

# A test links the objects of the library when it needs the functions that the
# library hides, and the library when it checks what a user can call.
function(sdfio_add_test name)
    add_executable(${name} ${name}.c)
    target_link_libraries(${name} PRIVATE sdfio_objects $<$<NOT:$<PLATFORM_ID:Windows>>:m>)
    target_include_directories(${name} PRIVATE "${PROJECT_SOURCE_DIR}/src")
    target_compile_options(${name} PRIVATE ${SDFIO_WARNINGS} ${SDFIO_SANITIZER_FLAGS})
    target_link_options(${name} PRIVATE ${SDFIO_SANITIZER_FLAGS})
    # The tests link the objects of the library, also when it is shared: its
    # functions are not imported from the DLL.
    target_compile_definitions(${name} PRIVATE
        SDFIO_STATIC_DEFINE
        SDFIO_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
    )
    add_test(NAME ${name} COMMAND ${name})
endfunction()

# The tests of the whole library need all of it. A build without text
# or the local clock runs the tests of what is left.
if(SDFIO_WITH_ASCII_FORMAT AND SDFIO_WITH_TIME_ZONE)
    sdfio_add_test(test_samples)
    sdfio_add_test(test_roundtrip)
    sdfio_add_test(test_streams)
    sdfio_add_test(test_numbers)
    sdfio_add_test(test_heights)
    sdfio_add_test(test_dates)
    sdfio_add_test(test_trailer)
endif()
sdfio_add_test(test_allocator)
if(WIN32)
    sdfio_add_test(test_wide)
endif()
target_compile_definitions(test_allocator PRIVATE
    SDFIO_TEST_TEXT=$<BOOL:${SDFIO_WITH_ASCII_FORMAT}>
)

if(Threads_FOUND AND CMAKE_USE_PTHREADS_INIT AND SDFIO_WITH_ASCII_FORMAT)
    sdfio_add_test(test_threads)
    target_link_libraries(test_threads PRIVATE Threads::Threads)
endif()

# The public interface only, through the library that is installed.
add_executable(test_public test_public.c)
target_link_libraries(test_public PRIVATE sdfio)
target_compile_options(test_public PRIVATE ${SDFIO_WARNINGS} ${SDFIO_SANITIZER_FLAGS})
add_test(NAME test_public COMMAND test_public)
