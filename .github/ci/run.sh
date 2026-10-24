#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
#
# SPDX-License-Identifier: MIT

# Runs a job of the continuous integration. GitHub and a developer machine run
# the jobs with this same script.
#
#   run.sh <job>          runs the job in build/<job>
#   run.sh --list         prints the names of the jobs
#
# A build job builds the library as a static and as a shared library, runs the
# tests, installs the library, and builds and runs a consumer project against
# the installation. A job for a processor architecture that differs from that of
# the machine uses a cross compiler and runs the programs with qemu; it needs
# no emulator on a machine of the architecture of the job.
set -euo pipefail

here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
root=$(cd "$here/../.." && pwd)

jobs=(
  gcc-x86-64-linux
  gcc-x86-linux
  gcc-aarch64-linux
  gcc-arm-linux
  gcc-z-architecture-linux
  gcc-risc-v-linux
  clang-x86-64-linux
  clang-aarch64-macos
  msvc-x86-64-windows
  msvc-x86-windows
  msvc-aarch64-windows
  gcc-arm-bare-metal
  gcc-risc-v-bare-metal
  asan-ubsan
  tsan
  static-analysis
  format
  documentation
  licenses
  coverage
)

# Describes a job by setting variables. kind is build, firmware or check.
# A build job sets:
#   generator  the CMake generator; empty selects the default of the platform
#   toolchain  CMake options shared by the library and the consumer project
#   configure  CMake options for the library only
#   arch       the architecture of GCC jobs, which sets toolchain and emulator:
#              native GCC on a machine of this architecture, else a cross GCC
#              and qemu
#   emulator   the command that runs a program built for another architecture
#   machine    the architecture that the job requires natively
# A firmware job sets prefix (the prefix of the toolchain executables),
# processor (CMAKE_SYSTEM_PROCESSOR) and flags (the compiler flags).
kind=
generator=
toolchain=
configure=
arch=
emulator=
machine=
prefix=
processor=
flags=

describe() {
  case "$1" in
    # Strict conformance: ISO C11 without compiler extensions, with every
    # diagnostic that -pedantic-errors reports treated as an error.
    gcc-x86-64-linux)
      kind=build generator=Ninja arch=x86_64
      configure="-DCMAKE_C_STANDARD=11 -DCMAKE_C_STANDARD_REQUIRED=ON -DCMAKE_C_FLAGS=-pedantic-errors"
      ;;
    gcc-x86-linux)
      kind=build generator=Ninja
      toolchain="-DCMAKE_C_COMPILER=gcc -DCMAKE_C_FLAGS=-m32 -DCMAKE_EXE_LINKER_FLAGS=-m32 -DCMAKE_SHARED_LINKER_FLAGS=-m32"
      ;;
    gcc-aarch64-linux)
      kind=build generator=Ninja arch=aarch64
      ;;
    gcc-arm-linux)
      kind=build generator=Ninja arch=arm
      ;;
    gcc-z-architecture-linux)
      kind=build generator=Ninja arch=s390x
      ;;
    gcc-risc-v-linux)
      kind=build generator=Ninja arch=riscv64
      ;;
    clang-x86-64-linux)
      kind=build generator=Ninja machine=x86_64
      toolchain="-DCMAKE_C_COMPILER=clang"
      ;;
    clang-aarch64-macos)
      kind=build generator=Ninja toolchain="-DCMAKE_C_COMPILER=clang"
      ;;
    msvc-x86-64-windows)
      kind=build toolchain="-A x64"
      ;;
    msvc-x86-windows)
      kind=build toolchain="-A Win32"
      ;;
    msvc-aarch64-windows)
      kind=build toolchain="-A ARM64"
      ;;
    # Arm Cortex-M4 with a single-precision floating-point unit.
    gcc-arm-bare-metal)
      kind=firmware prefix=arm-none-eabi processor=arm
      flags="-mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 -ffunction-sections -fdata-sections --specs=nano.specs --specs=nosys.specs"
      ;;
    # RV32IMAC: 32-bit RISC-V with the integer, multiplication, atomic and
    # compressed instruction extensions.
    gcc-risc-v-bare-metal)
      kind=firmware prefix=riscv64-unknown-elf processor=riscv32
      flags="-march=rv32imac -mabi=ilp32 -ffunction-sections -fdata-sections --specs=picolibc.specs"
      ;;
    asan-ubsan | tsan | static-analysis | format | documentation | licenses \
      | coverage)
      kind=check
      ;;
    *)
      echo "Unknown job: $1 (run.sh --list names them)" >&2
      exit 2
      ;;
  esac
}

# Succeeds if the architecture of the host, as reported by uname -m, is the
# one given as the argument: x86_64, aarch64, arm, riscv64 or s390x.
machine_is() {
  case "$(uname -m)" in
    arm64 | aarch64) [ "$1" = aarch64 ] ;;
    x86_64 | amd64) [ "$1" = x86_64 ] ;;
    armv7* | armv8l) [ "$1" = arm ] ;;
    *) [ "$(uname -m)" = "$1" ] ;;
  esac
}

# Sets the toolchain and the emulator of a GCC job for an architecture: the GCC
# of the machine if the machine has the architecture, otherwise the cross GCC
# of the GNU triplet and the qemu user-mode emulator for that architecture.
select_gcc() {
  local triplet
  case "$1" in
    arm) triplet=arm-linux-gnueabihf ;;
    *) triplet=$1-linux-gnu ;;
  esac
  if machine_is "$1"; then
    toolchain="-DCMAKE_C_COMPILER=gcc"
  else
    toolchain="-DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR=$1 -DCMAKE_C_COMPILER=$triplet-gcc -DCMAKE_CROSSCOMPILING_EMULATOR=qemu-$1;-L;/usr/$triplet"
    emulator="qemu-$1 -L /usr/$triplet"
  fi
}

run_build() {
  if [ -n "$arch" ]; then
    select_gcc "$arch"
  fi
  if [ -n "$machine" ] && ! machine_is "$machine"; then
    echo "This job needs a machine of the architecture $machine." >&2
    exit 1
  fi
  GENERATOR=$generator TOOLCHAIN_OPTIONS=$toolchain \
    CONFIGURE_OPTIONS=$configure EMULATOR=$emulator WORKDIR="$dir" \
    "$here/install-and-use.sh"
}

# Builds the library as a static library and links the firmware image of the
# allocators example, which writes a file to memory and reads it back. The image
# must contain neither code that reads files, nor code that handles the ASCII
# representation, nor the heap of the C library. The job verifies this by
# searching the symbols of the image.
run_firmware() {
  cmake -S . -B "$dir" -G Ninja \
    -DCMAKE_SYSTEM_NAME=Generic -DCMAKE_SYSTEM_PROCESSOR="$processor" \
    -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
    -DCMAKE_C_COMPILER="$prefix-gcc" "-DCMAKE_C_FLAGS=$flags" \
    -DCMAKE_BUILD_TYPE=MinSizeRel -DCMAKE_COMPILE_WARNING_AS_ERROR=ON \
    -DBUILD_TESTING=OFF -DSDFIO_BUILD_EXAMPLES=OFF
  cmake --build "$dir"
  # shellcheck disable=SC2086
  "$prefix-gcc" -std=c11 $flags -Os -Wl,--gc-sections -nostartfiles \
    -Wl,--entry=allocators -Iinclude -I"$dir/generated/include" \
    -DSDFIO_STATIC_DEFINE examples/allocators.c tests/embedded/clock.c \
    "$dir/libsdfio.a" \
    -o "$dir/firmware"
  "$prefix-size" "$dir/firmware"
  if "$prefix-nm" "$dir/firmware" \
    | grep -E "sdfio_(ascii|parse_number|format_scientific)|add_ascii_format|[[:space:]](malloc|free)$"; then
    echo "The image contains code that the program does not name." >&2
    exit 1
  fi
}

# ThreadSanitizer cannot be combined with AddressSanitizer, so each runs in a
# build of its own. The argument holds the flags of the compiler and the linker.
run_sanitizer() {
  if [ -n "${CI:-}" ]; then
    # Lowers the entropy of address-space layout randomization. At the default
    # setting of the runners, the sanitizers cannot map their shadow memory.
    sudo sysctl vm.mmap_rnd_bits=28 || true
  fi
  CC=clang cmake -S . -B "$dir" -G Ninja -DCMAKE_BUILD_TYPE=Debug \
    "-DCMAKE_C_FLAGS=$1" "-DCMAKE_EXE_LINKER_FLAGS=$1" \
    "-DCMAKE_SHARED_LINKER_FLAGS=$1" -DCMAKE_COMPILE_WARNING_AS_ERROR=ON
  cmake --build "$dir"
  ctest --test-dir "$dir" --output-on-failure --no-tests=error
}

run_static_analysis() {
  cmake -S . -B "$dir" -G Ninja -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DSDFIO_BUILD_EXAMPLES=ON
  # The sources specific to Windows cannot be analyzed on this platform and
  # are excluded.
  # shellcheck disable=SC2046
  clang-tidy -p "$dir" --warnings-as-errors='*' \
    $(ls src/*.c tests/*.c examples/*.c | grep -v -e file_win32 -e test_wide)
}

run_format() {
  clang-format --dry-run -Werror src/*.c src/*.h include/sdfio/*.h \
    tests/*.c tests/fuzz/*.c tests/embedded/*.c test_package/*.c \
    examples/*.c examples/external/*.cpp
}

run_documentation() {
  cmake -S . -B "$dir" -G Ninja -DSDFIO_BUILD_DOCS=ON -DBUILD_TESTING=OFF
  mkdir -p "$dir"
  cmake --build "$dir" --target docs 2>&1 | tee "$dir/docs.log"
  if grep -i "warning" "$dir/docs.log"; then
    exit 1
  fi
}

run_licenses() {
  reuse lint
}

# Measures the coverage of the library sources by the tests, excluding the
# tests and the examples. The tests use threads, so the profiling counters are
# updated atomically. Some tests run a second build of the library, so a
# function can have two positions, and the first one counts. The job fails if
# fewer than 75 percent of the lines are executed, the threshold of related
# projects.
run_coverage() {
  cmake -S . -B "$dir" -G Ninja -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_C_COMPILER=gcc "-DCMAKE_C_FLAGS=--coverage -fprofile-update=atomic"
  cmake --build "$dir"
  ctest --test-dir "$dir" --output-on-failure --no-tests=error
  gcovr --root . --filter 'src/' --exclude-throw-branches \
    --merge-mode-functions=merge-use-line-min --fail-under-line 75 \
    --print-summary --txt "$dir/coverage.txt" "$dir"
}

main() {
  case "${1:-}" in
    --list)
      printf '%s\n' "${jobs[@]}"
      ;;
    "" | -*)
      echo "Usage: run.sh <job> | --list" >&2
      exit 2
      ;;
    *)
      describe "$1"
      cd "$root"
      dir="$root/build/$1"
      case "$1" in
        asan-ubsan)
          run_sanitizer "-fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer"
          ;;
        tsan) run_sanitizer "-fsanitize=thread -fno-omit-frame-pointer" ;;
        static-analysis) run_static_analysis ;;
        format) run_format ;;
        documentation) run_documentation ;;
        licenses) run_licenses ;;
        coverage) run_coverage ;;
        *) "run_$kind" ;;
      esac
      ;;
  esac
}

main "$@"
