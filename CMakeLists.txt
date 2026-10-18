# SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
#
# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.28...4.2)

project(sdf
    VERSION 0.1.0
    DESCRIPTION "Reader and writer for ISO 25178-71 SDF surface data files in C"
    LANGUAGES C
)

include(CMakePackageConfigHelpers)
include(CTest)
include(GenerateExportHeader)
include(GNUInstallDirs)

option(SDF_BUILD_EXAMPLES "Build the examples" ${PROJECT_IS_TOP_LEVEL})
option(SDF_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(SDF_SANITIZE_THREAD "Build with ThreadSanitizer" OFF)

set(SDF_WARNINGS
    $<$<C_COMPILER_ID:MSVC>:/W4;/permissive-;/utf-8>
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:-Wall;-Wextra;-Wpedantic;-Wshadow;-Wconversion;-Wstrict-prototypes;-Wmissing-prototypes;-Wcast-qual;-Wformat=2;-Wundef>
)

set(SDF_SANITIZER_FLAGS "")
if(SDF_SANITIZE)
    list(APPEND SDF_SANITIZER_FLAGS -fsanitize=address,undefined -fno-omit-frame-pointer)
endif()
if(SDF_SANITIZE_THREAD)
    list(APPEND SDF_SANITIZER_FLAGS -fsanitize=thread -fno-omit-frame-pointer)
endif()

set(SDF_GENERATED_INCLUDE_DIR "${PROJECT_BINARY_DIR}/generated/include")

set(SDF_SOURCES
    src/ascii.c
    src/binary.c
    src/common.c
    src/dates.c
    src/header.c
    src/heights.c
    src/memory.c
    src/numbers.c
    src/reader.c
    src/stream.c
    src/tagged.c
    src/types.c
    src/values.c
    src/writer.c
)

# The code is compiled once. The tests link the objects, which have the
# internal functions that the library hides.
add_library(sdf_objects OBJECT ${SDF_SOURCES})
target_include_directories(sdf_objects
    PUBLIC
        "${PROJECT_SOURCE_DIR}/include"
        "${SDF_GENERATED_INCLUDE_DIR}"
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src"
)
target_compile_features(sdf_objects PUBLIC c_std_17)
target_compile_options(sdf_objects PRIVATE ${SDF_WARNINGS} ${SDF_SANITIZER_FLAGS})
# fseeko and localtime_r, and files above 2 GiB on 32-bit systems
target_compile_definitions(sdf_objects PRIVATE
    $<$<NOT:$<PLATFORM_ID:Windows>>:_POSIX_C_SOURCE=200809L>
    $<$<NOT:$<PLATFORM_ID:Windows>>:_FILE_OFFSET_BITS=64>
)
set_target_properties(sdf_objects PROPERTIES
    C_EXTENSIONS OFF
    C_VISIBILITY_PRESET hidden
    POSITION_INDEPENDENT_CODE ON
)

add_library(sdf $<TARGET_OBJECTS:sdf_objects>)
add_library(sdf::sdf ALIAS sdf)

generate_export_header(sdf
    BASE_NAME sdf
    EXPORT_MACRO_NAME SDF_API
    EXPORT_FILE_NAME "${SDF_GENERATED_INCLUDE_DIR}/sdf/sdf_export.h"
)
configure_file(include/sdf/version.h.in
    "${SDF_GENERATED_INCLUDE_DIR}/sdf/version.h" @ONLY
)

# The objects are compiled for the library, so they export what it exports.
target_compile_definitions(sdf_objects PRIVATE sdf_EXPORTS)
if(NOT BUILD_SHARED_LIBS)
    target_compile_definitions(sdf_objects PUBLIC SDF_STATIC_DEFINE)
    target_compile_definitions(sdf PUBLIC SDF_STATIC_DEFINE)
endif()

target_include_directories(sdf
    PUBLIC
        "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
        "$<BUILD_INTERFACE:${SDF_GENERATED_INCLUDE_DIR}>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)
target_compile_features(sdf PUBLIC c_std_11)
target_link_libraries(sdf PUBLIC $<$<NOT:$<PLATFORM_ID:Windows>>:m>)
target_link_options(sdf PUBLIC ${SDF_SANITIZER_FLAGS})

set_target_properties(sdf PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    EXPORT_NAME sdf
    C_EXTENSIONS OFF
    C_VISIBILITY_PRESET hidden
    POSITION_INDEPENDENT_CODE ON
)

if(SDF_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(PROJECT_IS_TOP_LEVEL AND BUILD_TESTING)
    add_subdirectory(tests)
endif()

set(sdf_cmakedir "${CMAKE_INSTALL_LIBDIR}/cmake/sdf")

install(TARGETS sdf
    EXPORT sdf-targets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)
install(DIRECTORY include/sdf DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    FILES_MATCHING PATTERN "*.h")
install(FILES
    "${SDF_GENERATED_INCLUDE_DIR}/sdf/sdf_export.h"
    "${SDF_GENERATED_INCLUDE_DIR}/sdf/version.h"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/sdf")

install(EXPORT sdf-targets
    FILE sdf-targets.cmake
    NAMESPACE sdf::
    DESTINATION "${sdf_cmakedir}"
)

write_basic_package_version_file("${PROJECT_BINARY_DIR}/sdf-config-version.cmake"
    COMPATIBILITY SameMajorVersion
)
configure_package_config_file(cmake/sdf-config.cmake.in
    "${PROJECT_BINARY_DIR}/sdf-config.cmake"
    INSTALL_DESTINATION "${sdf_cmakedir}"
)
install(FILES
    "${PROJECT_BINARY_DIR}/sdf-config.cmake"
    "${PROJECT_BINARY_DIR}/sdf-config-version.cmake"
    DESTINATION "${sdf_cmakedir}"
)
install(FILES LICENSE DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/licenses/sdf")
