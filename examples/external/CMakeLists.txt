# SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
#
# SPDX-License-Identifier: MIT
#
# Examples that use the library together with external libraries, written in
# C++ over the C interface. They are built with
# -DSDFIO_BUILD_EXTERNAL_EXAMPLES=ON, each one if its library is found and
# skipped with a note if not.

function(sdfio_external_example name)
    cmake_parse_arguments(ARG "" "SOURCE" "LIBS" ${ARGN})
    add_executable(example_${name} ${ARG_SOURCE})
    target_link_libraries(example_${name} PRIVATE sdfio ${ARG_LIBS})
    target_compile_features(example_${name} PRIVATE cxx_std_17)
    set_target_properties(example_${name} PROPERTIES CXX_EXTENSIONS OFF)
endfunction()

find_package(OpenCV QUIET COMPONENTS core imgproc)
if(OpenCV_FOUND)
    sdfio_external_example(opencv SOURCE opencv.cpp LIBS opencv_core opencv_imgproc)
else()
    message(STATUS "Example with OpenCV: not found, skipped")
endif()

set(vtk_components CommonCore CommonDataModel FiltersCore FiltersGeneral
    FiltersGeometry InteractionStyle RenderingCore RenderingOpenGL2)
find_package(VTK QUIET COMPONENTS ${vtk_components})
if(VTK_FOUND)
    set(vtk_modules ${vtk_components})
    list(TRANSFORM vtk_modules PREPEND VTK::)
    sdfio_external_example(vtk SOURCE vtk.cpp LIBS ${vtk_modules})
    vtk_module_autoinit(TARGETS example_vtk MODULES ${vtk_modules})
else()
    message(STATUS "Example with VTK: not found, skipped")
endif()

find_package(Eigen3 3.3 QUIET NO_MODULE)
if(Eigen3_FOUND)
    sdfio_external_example(eigen SOURCE eigen.cpp LIBS Eigen3::Eigen)
else()
    message(STATUS "Example with Eigen: not found, skipped")
endif()
