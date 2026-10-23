# SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
#
# SPDX-License-Identifier: MIT

foreach(example writing reading streaming coded_values time_zone)
    add_executable(${example} ${example}.c)
    target_link_libraries(${example} PRIVATE sdfio)
    target_compile_options(${example} PRIVATE ${SDFIO_WARNINGS} ${SDFIO_SANITIZER_FLAGS})
    target_compile_definitions(${example} PRIVATE
        $<$<PLATFORM_ID:Windows>:_CRT_SECURE_NO_WARNINGS>
    )
endforeach()

if(BUILD_TESTING AND PROJECT_IS_TOP_LEVEL)
    set(work "${CMAKE_CURRENT_BINARY_DIR}/run")
    file(MAKE_DIRECTORY "${work}")
    add_test(NAME example_writing COMMAND writing WORKING_DIRECTORY "${work}")
    add_test(NAME example_reading COMMAND reading integer.sdf WORKING_DIRECTORY "${work}")
    add_test(NAME example_coded_values COMMAND coded_values WORKING_DIRECTORY "${work}")
    add_test(NAME example_time_zone COMMAND time_zone WORKING_DIRECTORY "${work}")
    set_tests_properties(example_reading PROPERTIES DEPENDS example_writing)
    # A pipe through a shell: not for a program that needs an emulator
    if(UNIX AND NOT CMAKE_CROSSCOMPILING)
        add_test(NAME example_streaming
            COMMAND sh -c "cat coded_values.sdf | $<TARGET_FILE:streaming>"
            WORKING_DIRECTORY "${work}")
        set_tests_properties(example_streaming PROPERTIES DEPENDS example_coded_values)
    endif()
endif()
