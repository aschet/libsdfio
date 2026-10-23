#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
#
# SPDX-License-Identifier: MIT

# Builds the library, tests it, installs it, and builds and runs a project of
# its own against the installation, as a shared and as a static library.
# GENERATOR picks a generator (a single-configuration one is built as Release).
# TOOLCHAIN_OPTIONS are options for CMake that the library and the project of
# its own share: the compiler, the architecture. CONFIGURE_OPTIONS are options
# for the library only. EMULATOR is the command that runs a program of another
# architecture.
set -e

generator=()
if [ -n "$GENERATOR" ]; then
  generator=(-G "$GENERATOR" -DCMAKE_BUILD_TYPE=Release)
fi

for linkage in ON OFF; do
  echo "== BUILD_SHARED_LIBS=$linkage"
  build="build-$linkage"
  stage="$PWD/stage-$linkage"
  # shellcheck disable=SC2086
  cmake -S . -B "$build" "${generator[@]}" $TOOLCHAIN_OPTIONS $CONFIGURE_OPTIONS \
    -DBUILD_SHARED_LIBS=$linkage \
    -DCMAKE_COMPILE_WARNING_AS_ERROR=ON \
    -DCMAKE_INSTALL_PREFIX="$stage"
  cmake --build "$build" --config Release
  ctest --test-dir "$build" -C Release --output-on-failure --no-tests=error
  cmake --install "$build" --config Release
  # shellcheck disable=SC2086
  cmake -S test_package -B "$build/consumer" "${generator[@]}" \
    $TOOLCHAIN_OPTIONS -DCMAKE_PREFIX_PATH="$stage"
  cmake --build "$build/consumer" --config Release
  consumer=$(find "$build/consumer" \
    \( -name test_package -o -name test_package.exe \) -type f | head -1)
  # shellcheck disable=SC2086
  PATH="$stage/bin:$PATH" LD_LIBRARY_PATH="$stage/lib" \
    DYLD_LIBRARY_PATH="$stage/lib" $EMULATOR "$consumer"
done
