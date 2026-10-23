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

# Standard C for every target: no extensions of the compiler. The standard
# itself is the default of the compiler, but at least C11, which the library
# asks for with a compile feature.
set(CMAKE_C_EXTENSIONS OFF)

include(CMakePackageConfigHelpers)
include(CTest)
include(GenerateExportHeader)
include(GNUInstallDirs)

option(SDFIO_BUILD_EXAMPLES "Build the examples" ${PROJECT_IS_TOP_LEVEL})
option(SDFIO_BUILD_INTEGRATIONS "Build the examples with OpenCV and other libraries" OFF)
option(SDFIO_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(SDFIO_SANITIZE_THREAD "Build with ThreadSanitizer" OFF)
option(SDFIO_BUILD_FUZZERS "Build the fuzzers of libFuzzer, with Clang" OFF)
option(SDFIO_BUILD_DOCS "Build the API reference with Doxygen" OFF)

set(SDFIO_WARNINGS
    $<$<C_COMPILER_ID:MSVC>:/W4;/permissive-;/utf-8>
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:-Wall;-Wextra;-Wpedantic;-Wshadow;-Wconversion;-Wstrict-prototypes;-Wmissing-prototypes;-Wcast-qual;-Wformat=2;-Wundef>
)

set(SDFIO_SANITIZER_FLAGS "")
if(SDFIO_SANITIZE)
    list(APPEND SDFIO_SANITIZER_FLAGS -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer)
endif()
if(SDFIO_BUILD_FUZZERS)
    list(APPEND SDFIO_SANITIZER_FLAGS -fsanitize=address,undefined,fuzzer-no-link -fno-omit-frame-pointer)
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
    src/ascii.h
    src/ascii_reader.c
    src/ascii_writer.c
    src/binary.h
    src/binary_reader.c
    src/binary_writer.c
    src/byte_order.c
    src/byte_order.h
    src/data_type.c
    src/data_type.h
    src/dates.c
    src/dates.h
    src/decimal.c
    src/decimal.h
    src/dynstring.c
    src/dynstring.h
    src/fields.h
    src/file.c
    src/header.c
    src/header.h
    src/heights.c
    src/heights.h
    src/heights_real.h
    src/magic.c
    src/memory.c
    src/memory.h
    src/message.h
    src/owner.h
    src/read_header.c
    src/read_header.h
    src/reader.c
    src/reader_file.h
    src/reader_codec.h
    src/revision.c
    src/revision.h
    src/settings.c
    src/settings.h
    src/sink.c
    src/sink.h
    src/sizes.c
    src/source.c
    src/source.h
    src/status.c
    src/tagged.c
    src/tagged.h
    src/text.c
    src/text.h
    src/writer.c
    src/writer_file.h
    src/writer_codec.h
)

if(WIN32)
    list(APPEND SDFIO_SOURCES src/file_win32.c)
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
target_compile_features(sdfio_objects PUBLIC c_std_11)
target_compile_options(sdfio_objects PRIVATE ${SDFIO_WARNINGS} ${SDFIO_SANITIZER_FLAGS})
# The library is standard C11, with no feature macros. fopen is a function of
# the standard, and the file functions of Windows use it.
target_compile_definitions(sdfio_objects PRIVATE
    $<$<PLATFORM_ID:Windows>:_CRT_SECURE_NO_WARNINGS>
)
set_target_properties(sdfio_objects PROPERTIES
    C_VISIBILITY_PRESET hidden
    POSITION_INDEPENDENT_CODE ON
)

add_library(sdfio $<TARGET_OBJECTS:sdfio_objects>)
target_compile_features(sdfio PUBLIC c_std_11)
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

target_sources(sdfio
    PUBLIC
        FILE_SET HEADERS
            BASE_DIRS
                "${PROJECT_SOURCE_DIR}/include"
                "${SDFIO_GENERATED_INCLUDE_DIR}"
            FILES
                include/sdfio/file.h
                include/sdfio/sdfio.h
                "${SDFIO_GENERATED_INCLUDE_DIR}/sdfio/sdfio_export.h"
                "${SDFIO_GENERATED_INCLUDE_DIR}/sdfio/version.h"
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

if(SDFIO_BUILD_FUZZERS)
    add_subdirectory(tests/fuzz)
endif()

if(SDFIO_BUILD_DOCS)
    find_package(Doxygen REQUIRED)
    configure_file(cmake/Doxyfile.in "${PROJECT_BINARY_DIR}/Doxyfile" @ONLY)
    add_custom_target(docs
        COMMAND "${DOXYGEN_EXECUTABLE}" "${PROJECT_BINARY_DIR}/Doxyfile"
        WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
        COMMENT "Generating the API reference"
        VERBATIM
    )
endif()

if(PROJECT_IS_TOP_LEVEL AND BUILD_TESTING)
    add_subdirectory(tests)
endif()

set(sdfio_cmakedir "${CMAKE_INSTALL_LIBDIR}/cmake/sdfio")

install(TARGETS sdfio
    EXPORT sdfio-targets
    FILE_SET HEADERS
)

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

# For programs that find libraries with pkg-config. A static library needs the
# macro that tells the headers that it is not a DLL.
if(UNIX)
    set(SDFIO_PC_CFLAGS "")
    if(NOT BUILD_SHARED_LIBS)
        set(SDFIO_PC_CFLAGS " -DSDFIO_STATIC_DEFINE")
    endif()
    configure_file(cmake/sdfio.pc.in "${PROJECT_BINARY_DIR}/sdfio.pc" @ONLY)
    install(FILES "${PROJECT_BINARY_DIR}/sdfio.pc"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
endif()
