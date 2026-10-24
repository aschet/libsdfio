# SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
#
# SPDX-License-Identifier: MIT

foreach(example writer reader streams excerpt coded_values time_zone)
    add_executable(${example} ${example}.c)
    target_link_libraries(${example} PRIVATE sdfio)
    target_compile_options(${example} PRIVATE ${SDFIO_WARNINGS})
    target_compile_definitions(${example} PRIVATE
        $<$<PLATFORM_ID:Windows>:_CRT_SECURE_NO_WARNINGS>
    )
endforeach()

if(BUILD_TESTING AND PROJECT_IS_TOP_LEVEL)
    set(work "${CMAKE_CURRENT_BINARY_DIR}/run")
    file(MAKE_DIRECTORY "${work}")
    add_test(NAME example_writer COMMAND writer WORKING_DIRECTORY "${work}")
    add_test(NAME example_reader COMMAND reader integer.sdf WORKING_DIRECTORY "${work}")
    add_test(NAME example_coded_values COMMAND coded_values WORKING_DIRECTORY "${work}")
    add_test(NAME example_excerpt COMMAND excerpt coded_values.sdf excerpt.sdf WORKING_DIRECTORY "${work}")
    add_test(NAME example_time_zone COMMAND time_zone WORKING_DIRECTORY "${work}")
    set_tests_properties(example_reader PROPERTIES DEPENDS example_writer)
    set_tests_properties(example_excerpt PROPERTIES DEPENDS example_coded_values)
    # A pipe through a shell: not for a program that needs an emulator
    if(UNIX AND NOT CMAKE_CROSSCOMPILING)
        add_test(NAME example_streams
            COMMAND sh -c "cat coded_values.sdf | $<TARGET_FILE:streams>"
            WORKING_DIRECTORY "${work}")
        set_tests_properties(example_streams PROPERTIES DEPENDS example_coded_values)
    endif()
endif()
