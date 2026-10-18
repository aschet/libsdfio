# Design

This is the C library for the SDF format. The design follows what C libraries
for images and arrays do well, libpng, libtiff, libwebp and libheif, and
leaves out what the C++ library needs templates for.

## Handles

A reader or writer is an opaque pointer. It is made empty (`sdf_reader_new`)
and opened later, as libpng makes its struct before it reads, so that a failed
open still has a handle that can say why. There is one mechanism for errors:
the call returns a status, the handle keeps the message. A failure in the
middle of the data ends the reading; `sdf_rewind` starts again for a source
that can seek.

The description of a file, `sdf_info`, is a plain struct, because C programs
want to fill one in. `sdf_info_init` sets it, as `WebPConfigInit` does, so a
field that a later version adds has a default. A change of the struct is a
change of the major version.

## Streams

Bytes come from and go to callbacks (`sdf_stream`), as libtiff's
`TIFFClientOpen` has them. A stream without `seek` is read once from the front.
Files and memory are streams too. The reader holds a 16 KiB buffer; the heights
are never held. A file that is written goes to a name beside its place and is
renamed when it is whole.

## Stored values first

The data area is read and written as the file stores it: the data type of the
file, in the byte order of the machine, with the number that marks a missing
point unchanged. Nothing about metres. A text file is parsed into the type of
the file, so a binary32 text file is rounded to single precision.

Metres are a separate step: `sdf_decode_heights` turns stored values into
heights of any sample type, `sdf_encode_heights` does the reverse, and both
work on a chunk of 1024 values at a time. They need no handle, so they apply to
data from any source. Splitting the layers took out the `raw`/`metres` switch of
the C++ library, and with it the combination of options that is an error
(a marker for missing points in the raw space).

The convenience calls `sdf_read_heights` and `sdf_write_heights` compose the two
layers for a matrix and take a row stride, which can be negative.

## Where the type is a double

Memory of the program has the type of its sample (`sdf_sample`): `float` is as
good as `double`. A marker for missing points is a number of the program and is
taken in the precision of the sample type. Scalars that describe the file
(scales, resolution) are binary64 in the file and `double` in `sdf_info`.
A `double` is only the intermediate of a chunk, which holds every integer
type of the format exactly.

## Z-scale

An integer data type needs a z-scale. `sdf_write_heights` chooses the best for
the heights it is given; the row call cannot, because it has not seen the
other rows, and says so instead of guessing. `sdf_z_scale_for_range` and
`sdf_suggest_z_scale` compute it for a program that knows its range or has the
matrix.

## Threads

There is no global state: no static buffers, no `strtok`, no `localeconv`
(which the C library shares and rewrites). The decimal point of the locale is
measured with a print on every call that needs it, and the date conversion uses
only `localtime_r`. Handles are independent, so threads can read and write at
the same time; the tests run that under ThreadSanitizer.

## Not done

- A hook for the allocator, as libpng and OpenEXR have.
- Random access to a row of a binary file.
- Compression and checksums of the data area, which the standard describes and
  the C++ library does not support either.
- Comments of BCR-1.0 files are dropped when a file is read.
