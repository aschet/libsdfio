# SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
#
# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.28...4.2)

project(sdfio
    VERSION 0.1.0
    DESCRIPTION "Reader and writer for ISO 25178-71 SDF surface data files in C"
    HOMEPAGE_URL "https://github.com/aschet/libsdfio"
    LANGUAGES C
)

include(CMakePackageConfigHelpers)
include(CTest)
include(GenerateExportHeader)
include(GNUInstallDirs)

option(SDFIO_BUILD_EXAMPLES "Build the examples" ${PROJECT_IS_TOP_LEVEL})
option(SDFIO_BUILD_INTEGRATIONS "Build the examples with OpenCV and other libraries" OFF)
option(SDFIO_BUILD_FIRMWARE "Build the firmware that links the library for a target without an operating system" OFF)
option(SDFIO_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
# For a small program: a build can leave out what it does not use. Each option
# swaps one file for a stub with the same functions, so that nothing is
# conditional in the code.
option(SDFIO_TEXT "Read and write text files" ON)
option(SDFIO_LOCAL_TIME "Convert the dates of ISO-1.0 and BCR-1.0 to the local clock" ON)
option(SDFIO_SANITIZE_THREAD "Build with ThreadSanitizer" OFF)

set(SDFIO_WARNINGS
    $<$<C_COMPILER_ID:MSVC>:/W4;/permissive-;/utf-8>
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:-Wall;-Wextra;-Wpedantic;-Wshadow;-Wconversion;-Wstrict-prototypes;-Wmissing-prototypes;-Wcast-qual;-Wformat=2;-Wundef>
)

set(SDFIO_SANITIZER_FLAGS "")
if(SDFIO_SANITIZE)
    list(APPEND SDFIO_SANITIZER_FLAGS -fsanitize=address,undefined -fno-omit-frame-pointer)
endif()
if(SDFIO_SANITIZE_THREAD)
    list(APPEND SDFIO_SANITIZER_FLAGS -fsanitize=thread -fno-omit-frame-pointer)
endif()

# A Windows program finds the DLL beside itself: the tests, the examples and the
# library go to one folder.
if(WIN32 AND NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
endif()

set(SDFIO_GENERATED_INCLUDE_DIR "${PROJECT_BINARY_DIR}/generated/include")

set(SDFIO_SOURCES
    src/binary.c
    src/common.c
    src/dates.c
    src/file.c
    src/header.c
    src/heights.c
    src/memory.c
    src/numbers.c
    src/reader.c
    src/stream.c
    src/tagged.c
    src/trailer.c
    src/types.c
    src/values.c
    src/writer.c
)

if(SDFIO_TEXT)
    list(APPEND SDFIO_SOURCES src/ascii.c src/decimal.c)
else()
    list(APPEND SDFIO_SOURCES src/ascii_stub.c)
endif()
if(SDFIO_LOCAL_TIME)
    list(APPEND SDFIO_SOURCES src/localtime.c)
else()
    list(APPEND SDFIO_SOURCES src/localtime_stub.c)
endif()

# The code is compiled once. The tests link the objects, which have the
# internal functions that the library hides.
add_library(sdfio_objects OBJECT ${SDFIO_SOURCES})
target_include_directories(sdfio_objects
    PUBLIC
        "${PROJECT_SOURCE_DIR}/include"
        "${SDFIO_GENERATED_INCLUDE_DIR}"
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src"
)
target_compile_features(sdfio_objects PUBLIC c_std_99)
target_compile_options(sdfio_objects PRIVATE ${SDFIO_WARNINGS} ${SDFIO_SANITIZER_FLAGS})
# fseeko and localtime_r, and files above 2 GiB on 32-bit systems
target_compile_definitions(sdfio_objects PRIVATE
    $<$<NOT:$<PLATFORM_ID:Windows>>:_POSIX_C_SOURCE=200809L>
    $<$<NOT:$<PLATFORM_ID:Windows>>:_FILE_OFFSET_BITS=64>
)
set_target_properties(sdfio_objects PROPERTIES
    C_EXTENSIONS OFF
    C_VISIBILITY_PRESET hidden
    POSITION_INDEPENDENT_CODE ON
)

add_library(sdfio $<TARGET_OBJECTS:sdfio_objects>)
add_library(sdfio::sdfio ALIAS sdfio)

generate_export_header(sdfio
    BASE_NAME sdfio
    EXPORT_MACRO_NAME SDFIO_API
    EXPORT_FILE_NAME "${SDFIO_GENERATED_INCLUDE_DIR}/sdfio/sdfio_export.h"
)
configure_file(include/sdfio/version.h.in
    "${SDFIO_GENERATED_INCLUDE_DIR}/sdfio/version.h" @ONLY
)

# The objects are compiled for the library, so they export what it exports.
target_compile_definitions(sdfio_objects PRIVATE sdfio_EXPORTS)
if(NOT BUILD_SHARED_LIBS)
    target_compile_definitions(sdfio_objects PUBLIC SDFIO_STATIC_DEFINE)
    target_compile_definitions(sdfio PUBLIC SDFIO_STATIC_DEFINE)
endif()

target_include_directories(sdfio
    PUBLIC
        "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
        "$<BUILD_INTERFACE:${SDFIO_GENERATED_INCLUDE_DIR}>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)
target_compile_features(sdfio PUBLIC c_std_99)
target_link_libraries(sdfio PUBLIC $<$<NOT:$<PLATFORM_ID:Windows>>:m>)
target_link_options(sdfio PUBLIC ${SDFIO_SANITIZER_FLAGS})

set_target_properties(sdfio PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    EXPORT_NAME sdfio
    C_EXTENSIONS OFF
    C_VISIBILITY_PRESET hidden
    POSITION_INDEPENDENT_CODE ON
)

if(SDFIO_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(SDFIO_BUILD_INTEGRATIONS)
    enable_language(CXX)
    add_subdirectory(examples/integrations)
endif()

if(SDFIO_BUILD_FIRMWARE)
    add_subdirectory(tests/embedded)
endif()

if(PROJECT_IS_TOP_LEVEL AND BUILD_TESTING)
    add_subdirectory(tests)
endif()

set(sdfio_cmakedir "${CMAKE_INSTALL_LIBDIR}/cmake/sdfio")

install(TARGETS sdfio
    EXPORT sdfio-targets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)
install(DIRECTORY include/sdfio DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    FILES_MATCHING PATTERN "*.h")
install(FILES
    "${SDFIO_GENERATED_INCLUDE_DIR}/sdfio/sdfio_export.h"
    "${SDFIO_GENERATED_INCLUDE_DIR}/sdfio/version.h"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/sdfio")

install(EXPORT sdfio-targets
    FILE sdfio-targets.cmake
    NAMESPACE sdfio::
    DESTINATION "${sdfio_cmakedir}"
)

write_basic_package_version_file("${PROJECT_BINARY_DIR}/sdfio-config-version.cmake"
    COMPATIBILITY SameMajorVersion
)
configure_package_config_file(cmake/sdfio-config.cmake.in
    "${PROJECT_BINARY_DIR}/sdfio-config.cmake"
    INSTALL_DESTINATION "${sdfio_cmakedir}"
)
install(FILES
    "${PROJECT_BINARY_DIR}/sdfio-config.cmake"
    "${PROJECT_BINARY_DIR}/sdfio-config-version.cmake"
    DESTINATION "${sdfio_cmakedir}"
)
install(FILES LICENSE DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/licenses/sdfio")
