# libsdfio

[![CI](https://github.com/aschet/libsdfio/actions/workflows/ci.yml/badge.svg)](https://github.com/aschet/libsdfio/actions/workflows/ci.yml)

A portable C library that reads and writes ISO 25178-71 SDF surface data
files: the revisions ISO-1.0, ISO-2.0 and BCR-1.0, in ASCII or binary format. It
requires a C11 compiler with IEEE 754 `float` and `double` and 8-bit bytes.

```c
sdfio_reader *reader = sdfio_reader_new(NULL);
if (sdfio_open_file(reader, "surface.sdf") == SDFIO_OK)
{
    const sdfio_header *header = sdfio_get_header(reader);
    float *depth = sdfio_alloc_heights(header, SDFIO_FLOAT32, header->rows);
    sdfio_read_heights(reader, sdfio_metres(SDFIO_FLOAT32), depth,
                       (ptrdiff_t)(header->columns * sizeof *depth));
    sdfio_free(depth);
}
sdfio_reader_free(reader);
```

The [headers](include/sdfio) document the interface, and [docs/design.md](docs/design.md)
says why it is the way it is. See the [examples](examples).

## Building

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

`SDFIO_WITH_ASCII_FORMAT=OFF` removes ASCII support: ASCII files return
`SDFIO_ERR_UNSUPPORTED`. `SDFIO_WITH_TIME_ZONE=OFF` removes the time zone
conversion: ISO-1.0 and BCR-1.0 dates are treated as UTC. Both reduce the size
of the library.
