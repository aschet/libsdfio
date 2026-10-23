#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
#
# SPDX-License-Identifier: MIT

# Runs a job of the continuous integration, on GitHub and on a machine of a
# developer alike:
#
#   run.sh <job>          runs the job in build/<job>
#   run.sh --list         prints the names of the jobs
#   run.sh --apt <job>    prints the packages of Debian and Ubuntu that it needs
#   run.sh --pipx <job>   prints the Python packages that it needs
#
# The build jobs build the library as a static and as a shared library, test
# it, install it, and build and run a project against the installation. A
# job for a processor that the machine does not have needs the cross compiler
# and qemu, which --apt lists.
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
)

# What a job is and needs. kind is build, firmware or check. A build job sets
# generator (empty for the one of the platform), toolchain (options for CMake
# of the library and of the project against it), configure (options of the
# library only), emulator (the command that runs a program of another
# processor) and machine (the processor that a job needs natively). A firmware
# job sets prefix, processor and flags.
kind=
generator=
toolchain=
configure=
emulator=
machine=
apt=
pipx=
prefix=
processor=
flags=

describe() {
  case "$1" in
    # The strict one: exactly C11, without the extensions of the compiler and
    # without a diagnostic that a flag could silence.
    gcc-x86-64-linux)
      kind=build generator=Ninja machine=x86_64 apt="ninja-build gcc"
      toolchain="-DCMAKE_C_COMPILER=gcc"
      configure="-DCMAKE_C_STANDARD=11 -DCMAKE_C_STANDARD_REQUIRED=ON -DCMAKE_C_FLAGS=-pedantic-errors"
      ;;
    gcc-x86-linux)
      kind=build generator=Ninja apt="ninja-build gcc gcc-multilib"
      toolchain="-DCMAKE_C_COMPILER=gcc -DCMAKE_C_FLAGS=-m32 -DCMAKE_EXE_LINKER_FLAGS=-m32 -DCMAKE_SHARED_LINKER_FLAGS=-m32"
      ;;
    gcc-aarch64-linux)
      kind=build generator=Ninja machine=aarch64 apt="ninja-build gcc"
      toolchain="-DCMAKE_C_COMPILER=gcc"
      ;;
    gcc-arm-linux)
      kind=build generator=Ninja
      apt="ninja-build qemu-user gcc-arm-linux-gnueabihf libc6-dev-armhf-cross"
      toolchain="-DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR=arm -DCMAKE_C_COMPILER=arm-linux-gnueabihf-gcc -DCMAKE_CROSSCOMPILING_EMULATOR=qemu-arm;-L;/usr/arm-linux-gnueabihf"
      emulator="qemu-arm -L /usr/arm-linux-gnueabihf"
      ;;
    gcc-z-architecture-linux)
      kind=build generator=Ninja
      apt="ninja-build qemu-user gcc-s390x-linux-gnu libc6-dev-s390x-cross"
      toolchain="-DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR=s390x -DCMAKE_C_COMPILER=s390x-linux-gnu-gcc -DCMAKE_CROSSCOMPILING_EMULATOR=qemu-s390x;-L;/usr/s390x-linux-gnu"
      emulator="qemu-s390x -L /usr/s390x-linux-gnu"
      ;;
    gcc-risc-v-linux)
      kind=build generator=Ninja
      apt="ninja-build qemu-user gcc-riscv64-linux-gnu libc6-dev-riscv64-cross"
      toolchain="-DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR=riscv64 -DCMAKE_C_COMPILER=riscv64-linux-gnu-gcc -DCMAKE_CROSSCOMPILING_EMULATOR=qemu-riscv64;-L;/usr/riscv64-linux-gnu"
      emulator="qemu-riscv64 -L /usr/riscv64-linux-gnu"
      ;;
    clang-x86-64-linux)
      kind=build generator=Ninja machine=x86_64 apt="ninja-build clang"
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
    # A Cortex-M4 with a floating point unit.
    gcc-arm-bare-metal)
      kind=firmware prefix=arm-none-eabi processor=arm
      apt="ninja-build gcc-arm-none-eabi libnewlib-arm-none-eabi"
      flags="-mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 -ffunction-sections -fdata-sections --specs=nano.specs --specs=nosys.specs"
      ;;
    # RV32IMAC.
    gcc-risc-v-bare-metal)
      kind=firmware prefix=riscv64-unknown-elf processor=riscv32
      apt="ninja-build gcc-riscv64-unknown-elf picolibc-riscv64-unknown-elf"
      flags="-march=rv32imac -mabi=ilp32 -ffunction-sections -fdata-sections --specs=picolibc.specs"
      ;;
    asan-ubsan | tsan)
      kind=check apt="ninja-build clang"
      ;;
    static-analysis)
      kind=check apt="ninja-build" pipx="clang-tidy==21.1.6"
      ;;
    format)
      kind=check pipx="clang-format==21.1.6"
      ;;
    documentation)
      kind=check apt="ninja-build doxygen"
      ;;
    licenses)
      kind=check pipx="reuse"
      ;;
    *)
      echo "Unknown job: $1 (run.sh --list names them)" >&2
      exit 2
      ;;
  esac
}

# Makes the tools that pipx installed findable.
use_pipx() {
  if command -v pipx > /dev/null; then
    PATH="$(pipx environment --value PIPX_BIN_DIR):$PATH"
  fi
}

# The processor of the machine, as uname -m names it.
machine_is() {
  case "$(uname -m)" in
    arm64 | aarch64) [ "$1" = aarch64 ] ;;
    x86_64 | amd64) [ "$1" = x86_64 ] ;;
    *) false ;;
  esac
}

run_build() {
  if [ -n "$machine" ] && ! machine_is "$machine"; then
    echo "This job needs a machine of the architecture $machine." >&2
    exit 1
  fi
  GENERATOR=$generator TOOLCHAIN_OPTIONS=$toolchain \
    CONFIGURE_OPTIONS=$configure EMULATOR=$emulator WORKDIR="$dir" \
    "$here/install-and-use.sh"
}

# The library as a static library, and the image of a program that writes a
# file into memory. The image has to contain no code to read files or to
# handle ASCII.
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
    -Wl,--entry=main -Iinclude -I"$dir/generated/include" \
    -DSDFIO_STATIC_DEFINE tests/embedded/main.c "$dir/libsdfio.a" \
    -o "$dir/firmware"
  "$prefix-size" "$dir/firmware"
  if "$prefix-nm" "$dir/firmware" \
    | grep -E "sdfio_(ascii|parse_number|format_scientific)|_reader_codec"; then
    echo "The image contains code that the program does not name." >&2
    exit 1
  fi
}

# The thread sanitizer cannot be combined with the address sanitizer, so the
# builds are two.
run_sanitizer() {
  if [ -n "${CI:-}" ]; then
    # The sanitizers map their memory
    sudo sysctl vm.mmap_rnd_bits=28 || true
  fi
  CC=clang cmake -S . -B "$dir" -G Ninja -DCMAKE_BUILD_TYPE=Debug \
    "$1" -DCMAKE_COMPILE_WARNING_AS_ERROR=ON
  cmake --build "$dir"
  ctest --test-dir "$dir" --output-on-failure --no-tests=error
}

run_static_analysis() {
  use_pipx
  cmake -S . -B "$dir" -G Ninja -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DSDFIO_BUILD_EXAMPLES=ON
  # The files for Windows cannot be analyzed here
  # shellcheck disable=SC2046
  clang-tidy -p "$dir" --warnings-as-errors='*' \
    $(ls src/*.c tests/*.c examples/*.c | grep -v -e file_win32 -e test_wide)
}

run_format() {
  use_pipx
  clang-format --dry-run -Werror src/*.c src/*.h include/sdfio/*.h \
    tests/*.c tests/fuzz/*.c tests/embedded/*.c test_package/*.c \
    examples/*.c examples/integrations/*.cpp
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
  use_pipx
  reuse lint
}

main() {
  case "${1:-}" in
    --list)
      printf '%s\n' "${jobs[@]}"
      ;;
    --apt)
      describe "${2:?a job}"
      echo "$apt"
      ;;
    --pipx)
      describe "${2:?a job}"
      echo "$pipx"
      ;;
    "" | -*)
      echo "Usage: run.sh <job> | --list | --apt <job> | --pipx <job>" >&2
      exit 2
      ;;
    *)
      describe "$1"
      cd "$root"
      dir="$root/build/$1"
      case "$1" in
        asan-ubsan) run_sanitizer -DSDFIO_SANITIZE=ON ;;
        tsan) run_sanitizer -DSDFIO_SANITIZE_THREAD=ON ;;
        static-analysis) run_static_analysis ;;
        format) run_format ;;
        documentation) run_documentation ;;
        licenses) run_licenses ;;
        *) "run_$kind" ;;
      esac
      ;;
  esac
}

main "$@"
