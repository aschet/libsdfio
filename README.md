# libsdfio

[![CI](https://github.com/aschet/libsdfio/actions/workflows/ci.yml/badge.svg)](https://github.com/aschet/libsdfio/actions/workflows/ci.yml)

A bare-bones, portable C library that reads and writes ISO 25178-71 SDF
surface data files: the revisions ISO-1.0 and ISO-2.0, in ASCII or binary
format.

See the [examples](examples) and the
[API reference](https://aschet.github.io/libsdfio/).

## Building

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

To install the library:

```sh
cmake --install build --prefix /path/to/prefix
```

A project then uses it with `find_package(sdfio REQUIRED)` and links the
target `sdfio::sdfio`. On Linux and macOS the installation also has a
pkg-config file: `pkg-config --cflags --libs sdfio`, with `--static` for the
static library.

With Visual Studio, in PowerShell, the generator builds every configuration, so
the commands name the one to build, test and install. `-A` selects `x64`,
`Win32` or `ARM64`:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release
cmake --install build --config Release --prefix C:\sdfio
```

## Checks

The jobs of the continuous integration are one script, which runs the same
way on a machine of a developer:

```sh
.github/ci/run.sh --list
.github/ci/run.sh gcc-x86-64-linux
```

`run.sh --apt <job>` lists the Debian and Ubuntu packages that a job needs,
for example the cross compiler and qemu of `gcc-arm-linux`.
