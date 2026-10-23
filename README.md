# libsdfio

[![CI](https://github.com/aschet/libsdfio/actions/workflows/ci.yml/badge.svg)](https://github.com/aschet/libsdfio/actions/workflows/ci.yml)

A bare-bones, portable C library that reads and writes ISO 25178-71 SDF
surface data files: the revisions ISO-1.0 and ISO-2.0, in ASCII or binary
format. It requires a C11 compiler with IEEE 754 `float` and `double` and 8-bit
bytes, which the compiler checks at compile time.

```c
const sdfio_config config = {
    .reader_codecs = {&sdfio_binary_reader_codec, &sdfio_ascii_reader_codec}};
sdfio_reader *reader = sdfio_reader_new(&config);
if (sdfio_open_file(reader, "surface.sdf") == SDFIO_OK)
{
    const sdfio_header *header = sdfio_get_header(reader);
    size_t bytes = 0;
    sdfio_heights_size(header, SDFIO_TYPE_FLOAT, &bytes);
    float *heights = malloc(bytes);
    sdfio_read_heights(reader, SDFIO_TYPE_FLOAT, heights);
    free(heights);
}
sdfio_reader_free(reader);
```

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
