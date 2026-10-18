# SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
#
# SPDX-License-Identifier: MIT

# Fuzzers of libFuzzer, built with Clang. Run one with a folder of inputs:
#   ./fuzz_reader corpus/ -max_total_time=60
foreach(fuzzer fuzz_reader fuzz_writer)
    add_executable(${fuzzer} ${fuzzer}.c)
    target_link_libraries(${fuzzer} PRIVATE sdfio_objects $<$<NOT:$<PLATFORM_ID:Windows>>:m>)
    target_compile_definitions(${fuzzer} PRIVATE SDFIO_STATIC_DEFINE)
    target_compile_options(${fuzzer} PRIVATE ${SDFIO_SANITIZER_FLAGS})
    target_link_options(${fuzzer} PRIVATE -fsanitize=fuzzer ${SDFIO_SANITIZER_FLAGS})
endforeach()
