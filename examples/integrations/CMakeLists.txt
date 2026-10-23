# SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
#
# SPDX-License-Identifier: MIT
#
# Examples that use the library together with OpenCV, VTK and Eigen, written in
# C++ over the C interface. They are built with -DSDFIO_BUILD_INTEGRATIONS=ON,
# each one if its library is found and skipped with a note if not.

function(sdfio_integration name)
    cmake_parse_arguments(ARG "" "SOURCE" "LIBS" ${ARGN})
    add_executable(integration_${name} ${ARG_SOURCE})
    target_link_libraries(integration_${name} PRIVATE sdfio ${ARG_LIBS})
    target_compile_features(integration_${name} PRIVATE cxx_std_17)
    set_target_properties(integration_${name} PROPERTIES CXX_EXTENSIONS OFF)
endfunction()

find_package(OpenCV QUIET COMPONENTS core imgproc)
if(OpenCV_FOUND)
    sdfio_integration(opencv SOURCE opencv.cpp LIBS opencv_core opencv_imgproc)
else()
    message(STATUS "Integration example for OpenCV: not found, skipped")
endif()

set(vtk_components CommonCore CommonDataModel FiltersCore FiltersGeneral
    FiltersGeometry InteractionStyle RenderingCore RenderingOpenGL2)
find_package(VTK QUIET COMPONENTS ${vtk_components})
if(VTK_FOUND)
    set(vtk_modules ${vtk_components})
    list(TRANSFORM vtk_modules PREPEND VTK::)
    sdfio_integration(vtk SOURCE vtk.cpp LIBS ${vtk_modules})
    vtk_module_autoinit(TARGETS integration_vtk MODULES ${vtk_modules})
else()
    message(STATUS "Integration example for VTK: not found, skipped")
endif()

find_package(Eigen3 3.3 QUIET NO_MODULE)
if(Eigen3_FOUND)
    sdfio_integration(eigen SOURCE eigen.cpp LIBS Eigen3::Eigen)
else()
    message(STATUS "Integration example for Eigen: not found, skipped")
endif()
