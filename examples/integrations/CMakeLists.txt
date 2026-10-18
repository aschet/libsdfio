# SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
#
# SPDX-License-Identifier: 0BSD
#
# Examples with the libraries that process images and surfaces, in C++ over the
# C interface. Each one is built if its library is found and skipped with a
# note if not.

function(sdfio_integration name)
    cmake_parse_arguments(ARG "" "" "LIBS" ${ARGN})
    add_executable(integration_${name} ${name}.cpp)
    target_link_libraries(integration_${name} PRIVATE sdfio ${ARG_LIBS})
    target_compile_definitions(integration_${name} PRIVATE
        SDFIO_EXAMPLE_FILE="${PROJECT_SOURCE_DIR}/tests/data/samples/iso25178-71-2026/iso2_surface_binary.sdf")
    set_target_properties(integration_${name} PROPERTIES CXX_EXTENSIONS OFF)
    target_compile_features(integration_${name} PRIVATE cxx_std_17)
    set(work "${CMAKE_CURRENT_BINARY_DIR}/run_${name}")
    file(MAKE_DIRECTORY "${work}")
    add_test(NAME integration_${name} COMMAND integration_${name}
        WORKING_DIRECTORY "${work}")
    set_tests_properties(integration_${name} PROPERTIES LABELS integration)
endfunction()

find_package(OpenCV QUIET COMPONENTS core imgproc imgcodecs)
if(OpenCV_FOUND)
    sdfio_integration(opencv LIBS ${OpenCV_LIBS})
    target_include_directories(integration_opencv SYSTEM PRIVATE ${OpenCV_INCLUDE_DIRS})
else()
    message(STATUS "Integration example for OpenCV: not found, skipped")
endif()
