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

## Checks

The continuous integration runs its jobs with `.github/ci/run.sh`, which runs
them on a developer machine in the same way. The first command lists the
jobs and the second runs one of them:

```sh
.github/ci/run.sh --list
.github/ci/run.sh gcc-x86-64-linux
```
