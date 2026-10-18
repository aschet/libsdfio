# SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
#
# SPDX-License-Identifier: 0BSD
#
# Examples with the libraries that process images and surfaces, in C++ over the
# C interface. Each one is built if its library is found and skipped with a
# note if not.

function(sdfio_integration name)
    cmake_parse_arguments(ARG "" "SOURCE" "LIBS;DEFINITIONS" ${ARGN})
    add_executable(integration_${name} ${ARG_SOURCE})
    target_link_libraries(integration_${name} PRIVATE sdfio ${ARG_LIBS})
    target_compile_definitions(integration_${name} PRIVATE ${ARG_DEFINITIONS}
        SDFIO_EXAMPLE_FILE="${PROJECT_SOURCE_DIR}/tests/data/samples/iso25178-71-2026/iso2_surface_binary.sdf")
    if(ARG_SOURCE MATCHES "\\.cpp$")
        target_compile_features(integration_${name} PRIVATE cxx_std_17)
        set_target_properties(integration_${name} PROPERTIES CXX_EXTENSIONS OFF)
    else()
        target_compile_features(integration_${name} PRIVATE c_std_11)
        set_target_properties(integration_${name} PROPERTIES C_EXTENSIONS OFF)
    endif()
    set(work "${CMAKE_CURRENT_BINARY_DIR}/run_${name}")
    file(MAKE_DIRECTORY "${work}")
    add_test(NAME integration_${name} COMMAND integration_${name}
        WORKING_DIRECTORY "${work}")
    set_tests_properties(integration_${name} PROPERTIES
        LABELS integration SKIP_RETURN_CODE 77)
endfunction()

find_package(OpenCV QUIET COMPONENTS core imgproc imgcodecs)
if(OpenCV_FOUND)
    sdfio_integration(opencv SOURCE opencv.cpp LIBS ${OpenCV_LIBS})
    target_include_directories(integration_opencv SYSTEM PRIVATE ${OpenCV_INCLUDE_DIRS})
else()
    message(STATUS "Integration example for OpenCV: not found, skipped")
endif()

find_package(OpenCL QUIET)
if(OpenCL_FOUND)
    sdfio_integration(opencl SOURCE opencl.c LIBS OpenCL::OpenCL)
    target_link_libraries(integration_opencl PRIVATE $<$<NOT:$<PLATFORM_ID:Windows>>:m>)
else()
    message(STATUS "Integration example for OpenCL: not found, skipped")
endif()
