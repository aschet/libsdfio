# sdfio for C

A C library that reads and writes ISO 25178-71 SDF surface data files: the
revisions ISO-1.0, ISO-2.0 and BCR-1.0, as text or binary. It is the C sibling
of the C++ library [sdfio](../sdfkit), an experiment in what the same job looks
like with the design of libpng and libtiff instead of C++ templates. The files
are the same; the interface is not.

```c
sdfio_reader *reader = sdfio_reader_new();
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

The library allocates no memory for the heights, holds no more of a file than
a chunk of 1024 values, and reads from files, memory and callbacks of your own,
also from a pipe that cannot seek.

## Names

Everything the library puts into your program starts with the name of the
library, so it can sit next to others: functions and types with `sdfio_`,
macros and enumerators with `SDFIO_`, the headers in `<sdfio/>`, the CMake
package and target `sdfio`. The shorter `sdf_` and `SDF_` are taken: the
measurement library comedi has `SDF_READABLE`, `SDF_WRITABLE` and a dozen more,
and a program that talks to a scanner may include both. The library `libsdf`
on GitHub (signed distance functions) has the same file name as a `sdf`
library would. The files themselves keep the extension `.sdf`. The
internals use `sdfi_`, hidden in the shared library.

The C++ library has the same name and the same CMake package, so install the two
into different prefixes.

## Layers

Use the highest one that does the job.

| Layer | Calls | What it is |
| --- | --- | --- |
| Handles | `sdfio_open_file`, `sdfio_open_memory`, `sdfio_open_stream`, `sdfio_create_file`, `sdfio_create_stream` | A reader or writer over a file, memory or `sdfio_stream` callbacks |
| Records | `sdfio_get_header`, `sdfio_read_data`, `sdfio_write_data`, `sdfio_read_trailer`, `sdfio_write_trailer` | The three records of the standard: the header (record 1), the data area (record 2) as the file codes it, in the data type of the file, host byte order, row by row, in file order, and the trailer (record 3) |
| Heights | `sdfio_decode_heights`, `sdfio_encode_heights`, `sdfio_z_scale_for_range` | The coded height information to metres and back, without a handle |
| Images | `sdfio_read_heights`, `sdfio_write_heights` | A whole matrix of heights in metres of any sample type, any row stride |

The records know nothing of metres, z-scales or missing points: they
move what the file holds. The conversion is a step of its own, which also works
on data from any other source.

## Using it

- **One header**: `#include <sdfio/sdfio.h>`, link `sdf::sdf` with CMake.
- **Memory**: `sdfio_alloc_heights` and `sdfio_alloc_data` size a buffer from the
  description of a file, with the overflow checked, and `sdfio_free` releases it.
  Your own `malloc` works as well; the library never allocates the heights.
- **Errors**: every call returns an `sdfio_status`; `sdfio_reader_error_message()` and
  `sdfio_writer_error_message()` give the text of the last failure.
- **Strides**: `sdfio_format.element_stride` and the row stride place the heights
  in memory that is not a plain matrix: the `z` of an array of points, one
  channel of an interleaved image, a column-major matrix, a mirrored row. Both
  strides may be negative.
- **Sample types**: heights can be `int8` to `uint32`, `float` or `double`; the
  program does not have to widen its memory to `double`.
- **Missing points** are NaN, or the marker of your choice, which is taken in
  the precision of the sample type. In the coded values of the data area, as
  `sdfio_read_data` returns them, a point that was not measured has the bad
  value that `sdfio_bad_value(type, revision)` gives: test a value with
  `sdfio_is_bad_value(type, revision, value)`, and write that value for a point
  that was not measured.
- **Row order**: `sdfio_read_heights` and `sdfio_write_heights` always put
  `y = 0` first. `sdfio_read_data` and `sdfio_write_data` keep the order of the
  file, which BCR-1.0 stores from the largest y down, so a program that handles
  rows one at a time asks `sdfio_row_y(header, n)` for the y of the n-th row,
  and is then right for every revision (see [docs/design.md](docs/design.md)
  and `examples/counts.c`).
- **Dates** are UTC `time_t` in the program and the clock that the revision
  requires in the file.
- **Threads**: there is no global state. Handles of different threads are
  independent, also over the same bytes; one handle belongs to one thread at a
  time. Do not change the locale or the time zone while a call is running.
- **Locale**: numbers in text files always have the dot, whatever `LC_NUMERIC`
  says.

The [examples](examples) are short programs for reading, writing, streaming
from a pipe and writing integer counts of a scanner. [docs/design.md](docs/design.md)
says why the interface is the way it is.

## Building

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

The options are `BUILD_SHARED_LIBS`, `SDFIO_BUILD_EXAMPLES`, `SDFIO_SANITIZE`
(AddressSanitizer and UndefinedBehaviorSanitizer) and `SDFIO_SANITIZE_THREAD`.
The library needs a C11 compiler with IEEE 754 `float` and `double` and 8-bit
bytes, and builds with GCC, Clang and MSVC. The version and the export macro
are generated by CMake.

## Tests

The tests compare against `tests/data/expected.txt`, what the Python package
`sdfio` reads from the sample files; round-trip every revision, representation
and data type; read from sources that cannot seek and give one byte at a time;
read every prefix and thousands of damaged copies of a file under the
sanitizers; run threads with handles of their own; and use a locale with a
decimal comma and several time zones.

## License

MIT; the examples are 0BSD. See [LICENSES](LICENSES).
