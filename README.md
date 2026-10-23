# libsdfio

[![CI](https://github.com/aschet/libsdfio/actions/workflows/ci.yml/badge.svg)](https://github.com/aschet/libsdfio/actions/workflows/ci.yml)

A bare-bones, portable C library that reads and writes ISO 25178-71 SDF
surface data files: the revisions ISO-1.0 and ISO-2.0, in ASCII or binary
format.

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

The jobs of the continuous integration are one script, which runs the same
way on a machine of a developer:

```sh
.github/ci/run.sh --list
.github/ci/run.sh gcc-x86-64-linux
```

`run.sh --apt <job>` lists the Debian and Ubuntu packages that a job needs,
for example the cross compiler and qemu of `gcc-arm-linux`.
