# SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
#
# SPDX-License-Identifier: 0BSD

foreach(example writing reading streaming counts)
    add_executable(${example} ${example}.c)
    target_link_libraries(${example} PRIVATE sdfio)
    target_compile_options(${example} PRIVATE ${SDFIO_WARNINGS} ${SDFIO_SANITIZER_FLAGS})
endforeach()

if(BUILD_TESTING AND PROJECT_IS_TOP_LEVEL)
    set(work "${CMAKE_CURRENT_BINARY_DIR}/run")
    file(MAKE_DIRECTORY "${work}")
    add_test(NAME example_writing COMMAND writing surface.sdf WORKING_DIRECTORY "${work}")
    add_test(NAME example_reading COMMAND reading surface.sdf WORKING_DIRECTORY "${work}")
    add_test(NAME example_counts COMMAND counts counts.sdf WORKING_DIRECTORY "${work}")
    set_tests_properties(example_reading PROPERTIES DEPENDS example_writing)
    if(UNIX)
        add_test(NAME example_streaming
            COMMAND sh -c "cat counts.sdf | $<TARGET_FILE:streaming>"
            WORKING_DIRECTORY "${work}")
        set_tests_properties(example_streaming PROPERTIES DEPENDS example_counts)
    endif()
endif()
