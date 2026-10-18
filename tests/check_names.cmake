# SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
#
# SPDX-License-Identifier: MIT
#
# Run as: cmake -DLIBRARY=<file> -DHEADER=<sdfio.h> -P check_names.cmake
# Fails if the library defines a symbol or the public header declares a name
# that does not start with the name of the library.

execute_process(COMMAND nm --extern-only --defined-only "${LIBRARY}"
    OUTPUT_VARIABLE symbols RESULT_VARIABLE status)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "nm failed on ${LIBRARY}")
endif()
string(REPLACE "\n" ";" lines "${symbols}")
foreach(line IN LISTS lines)
    if(line MATCHES "^[0-9a-fA-F]+ [TDBRCSVW] (.+)$")
        set(name "${CMAKE_MATCH_1}")
        if(NOT name MATCHES "^(sdfio_|sdfi_)" AND NOT name MATCHES "^_")
            message(SEND_ERROR "the library defines ${name}")
        endif()
    endif()
endforeach()

file(READ "${HEADER}" header)
# Lists of lines: the characters that CMake takes for list syntax are hidden.
string(REPLACE ";" "<semicolon>" header "${header}")
string(REPLACE "[" "<open>" header "${header}")
string(REPLACE "]" "<close>" header "${header}")
string(REPLACE "\n" ";" lines "${header}")

foreach(line IN LISTS lines)
    set(name "")
    if(line MATCHES "^#define +([A-Za-z_][A-Za-z0-9_]*)")
        set(name "${CMAKE_MATCH_1}")
    elseif(line MATCHES "^} +([a-z_]+)<semicolon>$")
        set(name "${CMAKE_MATCH_1}")
    elseif(line MATCHES "^(SDFIO_API|static inline) .*[ *]([a-z_0-9]+)\\(")
        set(name "${CMAKE_MATCH_2}")
    elseif(line MATCHES "^    ([A-Z][A-Z0-9_]*)( =.*)?,?$")
        set(name "${CMAKE_MATCH_1}")
    endif()
    if(NOT name STREQUAL "" AND NOT name MATCHES "^(sdfio_|SDFIO_)")
        message(SEND_ERROR "the header declares ${name}")
    endif()
endforeach()
