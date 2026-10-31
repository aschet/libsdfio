# SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
#
# SPDX-License-Identifier: MIT

find_package(Threads)

# A test links the objects of the library when it needs the functions that the
# library hides, and the library when it checks what a user can call.
function(sdfio_add_test name)
    cmake_parse_arguments(PARSE_ARGV 1 TEST "" "SOURCE;OBJECTS" "")
    if(NOT TEST_SOURCE)
        set(TEST_SOURCE ${name}.c)
    endif()
    if(NOT TEST_OBJECTS)
        set(TEST_OBJECTS sdfio_objects)
    endif()
    add_executable(${name} ${TEST_SOURCE})
    target_link_libraries(${name} PRIVATE ${TEST_OBJECTS} $<$<NOT:$<PLATFORM_ID:Windows>>:m>)
    target_include_directories(${name} PRIVATE "${PROJECT_SOURCE_DIR}/src")
    target_compile_options(${name} PRIVATE ${SDFIO_WARNINGS})
    # The tests link the objects of the library, also when it is shared: its
    # functions are not imported from the DLL.
    target_compile_definitions(${name} PRIVATE
        SDFIO_STATIC_DEFINE
        SDFIO_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
    )
    add_test(NAME ${name} COMMAND ${name})
endfunction()

# Heights of type float are calculated in float or in double, as the processor
# favours. The objects below choose the other way, so that the tests check both.
set(SDFIO_OTHER_SOURCES ${SDFIO_SOURCES})
list(TRANSFORM SDFIO_OTHER_SOURCES PREPEND "${PROJECT_SOURCE_DIR}/")
add_library(sdfio_objects_other OBJECT ${SDFIO_OTHER_SOURCES})
target_include_directories(sdfio_objects_other
    PUBLIC
        "${PROJECT_SOURCE_DIR}/include"
        "${SDFIO_GENERATED_INCLUDE_DIR}"
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src"
)
target_compile_features(sdfio_objects_other PUBLIC c_std_11)
target_compile_options(sdfio_objects_other PRIVATE ${SDFIO_WARNINGS})
target_compile_definitions(sdfio_objects_other
    PUBLIC SDFIO_STATIC_DEFINE
    PRIVATE
        SDFIO_FLOAT_HEIGHTS_OTHER
        sdfio_EXPORTS
        $<$<PLATFORM_ID:Windows>:_CRT_SECURE_NO_WARNINGS>
)
set_target_properties(sdfio_objects_other PROPERTIES
    C_VISIBILITY_PRESET hidden
    POSITION_INDEPENDENT_CODE ON
)

sdfio_add_test(test_samples)
sdfio_add_test(test_roundtrip)
sdfio_add_test(test_streams)
sdfio_add_test(test_numbers)
sdfio_add_test(test_heights)
sdfio_add_test(test_layout)
sdfio_add_test(test_heights_other SOURCE test_heights.c OBJECTS sdfio_objects_other)
sdfio_add_test(test_datetime)
sdfio_add_test(test_trailer)
sdfio_add_test(test_allocator)
sdfio_add_test(test_estimate)
sdfio_add_test(test_reset)
sdfio_add_test(test_header)
sdfio_add_test(test_states)
if(WIN32)
    sdfio_add_test(test_wide)
endif()

if(Threads_FOUND AND CMAKE_USE_PTHREADS_INIT)
    sdfio_add_test(test_threads)
    target_link_libraries(test_threads PRIVATE Threads::Threads)
endif()

# Links the library as a user does, not its objects: the test fails to link if a
# public function is not exported.
add_executable(test_exports test_exports.c)
target_link_libraries(test_exports PRIVATE sdfio)
target_compile_options(test_exports PRIVATE ${SDFIO_WARNINGS})
add_test(NAME test_exports COMMAND test_exports)
