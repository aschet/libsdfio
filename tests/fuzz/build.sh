#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
#
# SPDX-License-Identifier: MIT

# Builds the fuzzers of libFuzzer with Clang into build/fuzzers. The library is
# instrumented, and each fuzzer is linked with it. Run one with a folder of
# inputs:
#   build/fuzzers/fuzz_reader corpus/ -max_total_time=60
set -euo pipefail

root=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
cd "$root"
dir=build/fuzzers
sanitizers=address,undefined

CC=clang cmake -S . -B "$dir" -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  "-DCMAKE_C_FLAGS=-fsanitize=$sanitizers,fuzzer-no-link" -DBUILD_TESTING=OFF \
  -DSDFIO_BUILD_EXAMPLES=OFF
cmake --build "$dir"
for fuzzer in fuzz_reader fuzz_writer; do
  clang "-fsanitize=$sanitizers,fuzzer" -DSDFIO_STATIC_DEFINE -Iinclude \
    "-I$dir/generated/include" "tests/fuzz/$fuzzer.c" "$dir/libsdfio.a" -lm \
    -o "$dir/$fuzzer"
done
