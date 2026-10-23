# libsdfio

[![CI](https://github.com/aschet/libsdfio/actions/workflows/ci.yml/badge.svg)](https://github.com/aschet/libsdfio/actions/workflows/ci.yml)

A portable C library that reads and writes ISO 25178-71 SDF surface data
files: the revisions ISO-1.0 and ISO-2.0, in ASCII or binary format. It
requires a C99 compiler with IEEE 754 `float` and `double` and 8-bit bytes,
which a C11 compiler checks at compile time.

```c
sdfio_reader *reader = sdfio_reader_new(NULL);
if (sdfio_open_file(reader, "surface.sdf") == SDFIO_OK)
{
    const sdfio_header *header = sdfio_get_header(reader);
    size_t bytes = 0;
    sdfio_heights_size(header, SDFIO_FLOAT, &bytes);
    float *heights = malloc(bytes);
    sdfio_read_heights(reader, SDFIO_FLOAT, heights);
    free(heights);
}
sdfio_reader_free(reader);
```

See the [examples](examples).

## Building

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

`SDFIO_WITH_ASCII_FORMAT=OFF` removes ASCII support and reduces the size of
the library: ASCII files return `SDFIO_ERR_UNSUPPORTED`.
