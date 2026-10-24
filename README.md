# libsdfio

[![CI](https://github.com/aschet/libsdfio/actions/workflows/ci.yml/badge.svg)](https://github.com/aschet/libsdfio/actions/workflows/ci.yml)

A portable, embedded-friendly and standard-conformant C library that reads and
writes ISO 25178-71 SDF surface data files: the revisions ISO-1.0 and ISO-2.0,
in ASCII or binary format.

The [API reference](https://aschet.github.io/libsdfio/) documents the
interface, and the [examples](examples) show how to use it.

## Building

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build
cmake --install build --prefix /path/to/prefix
```

An installed library is used with `find_package(sdfio REQUIRED)` and the target
`sdfio::sdfio`, or with pkg-config.

With Visual Studio, configure with `-A x64` (or `Win32`, `ARM64`) and name the
configuration in the other commands: `--config Release`, and `-C Release` for
`ctest`.

### Dates and the Width of time_t

The dates of the interface are `time_t` values, so the library and the program
that calls it have to be built with the same width of `time_t`. The build of the
library does not select it. A date that a 32-bit `time_t` cannot hold reads as
`SDFIO_NO_TIME`.

| Target | Width of `time_t` | Selection |
| --- | --- | --- |
| 64-bit targets | 64 bits | none needed |
| Visual C++, 32-bit and 64-bit | 64 bits | `_USE_32BIT_TIME_T` selects 32 bits |
| MinGW-w64 with the UCRT | 64 bits | `_USE_32BIT_TIME_T` selects 32 bits |
| MinGW-w64, 32-bit, with the msvcrt runtime | 32 bits | `_TIME_BITS=64` selects 64 bits |
| glibc, 32-bit | 32 bits, unless the toolchain defaults to 64 bits | `_TIME_BITS=64` and `_FILE_OFFSET_BITS=64` select 64 bits |

The flags go to the compiler of the program and to the compiler of the library,
for example when the library is built from source:

```sh
cmake -S . -B build -DCMAKE_C_FLAGS="-D_TIME_BITS=64 -D_FILE_OFFSET_BITS=64"
```

## Checks

The continuous integration runs its jobs with `.github/ci/run.sh`, which runs
them on a developer machine in the same way. The first command lists the
jobs and the second runs one of them:

```sh
.github/ci/run.sh --list
.github/ci/run.sh gcc-x86-64-linux
```
