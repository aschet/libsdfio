# Design

This is the C library for the SDF format. The design follows what C libraries
for images and arrays do well, libpng, libtiff, libwebp and libheif, and
and keeps the interface small.

## Handles

A reader or writer is an opaque pointer. It is made empty (`sdfio_reader_new`)
and opened later, as libpng makes its struct before it reads, so that a failed
open still has a handle that can say why. There is one mechanism for errors:
the call returns a status, the handle keeps the message. A failure in the
middle of the data ends the reading; `sdfio_rewind` starts again for a source
that can seek.

The header of a file, `sdfio_header`, record 1 of the standard with its fields
and names, is a plain struct, because C programs want to fill one in.
`sdfio_header_init` sets it, as `WebPConfigInit` does, so a field that a later
version adds has a default. A change of the struct is a change of the major
version.

## Streams

Bytes come from and go to callbacks (`sdfio_stream`), as libtiff's
`TIFFClientOpen` has them. A stream without `seek` is read once from the front.
Files and memory are streams too. The reader holds a 16 KiB buffer; the heights
are never held.

## The records first

The data area, record 2, is read and written as the file codes it: the data
type of the file, in the byte order of the machine, with the number that marks
a missing point unchanged. Nothing about metres. A text file is parsed into the
type of the file, so a binary32 text file is rounded to single precision.

Metres are a separate step: `sdfio_decode_heights` turns the coded height
information into heights of any sample type, `sdfio_encode_heights` does the reverse, and both
work on a chunk of 128 values at a time. They need no handle, so they apply to
data from any source. With the layers split there is no `raw`/`metres` switch,
and so no combination of options that is an error
(a marker for missing points in the raw space).

The convenience calls `sdfio_read_heights` and `sdfio_write_heights` compose the two
layers for a matrix and take a row stride, which can be negative.

## Where the type is a double

Memory of the program has the type of its sample (`sdfio_sample`): `float` is as
good as `double`. A marker for missing points is a number of the program and is
taken in the precision of the sample type. Scalars that describe the file
(scales, resolution) are binary64 in the file and `double` in `sdfio_header`.
A `double` is only the intermediate of a chunk, which holds every integer
type of the format exactly.

## Names

Everything the library puts into a program starts with its name: `sdfio_` for
functions and types, `SDFIO_` for macros and enumerators, the headers in
`<sdfio/>`, the CMake package and target `sdfio`. The shorter `sdf_` and `SDF_`
are taken: the measurement library comedi has `SDF_READABLE`, `SDF_WRITABLE` and
a dozen more, and a program that talks to a scanner may include both. The
library `libsdf` on GitHub (signed distance functions) has the same file name as
a `sdf` library would. The files keep the extension `.sdf`. The internal
functions have the prefix too, and are hidden in the shared library.

## Memory

The library allocates no memory for the heights, but sizing it is where
programs go wrong: `columns * rows * size` overflows, and the element size is
that of the data type in one place and of the sample type in another.
`sdfio_alloc_data` and `sdfio_alloc_heights` do the arithmetic from the
description of the file, return zeroed memory, and refuse a size that cannot be
counted. `sdfio_free` pairs with them, so a shared library on Windows frees in the
runtime that allocated.

## Allocator and configuration

`sdfio_reader_new` and `sdfio_writer_new` take an `sdfio_config`, null for all
defaults. Its allocator is a pair of callbacks with a user pointer, as in libpng
and OpenEXR, and covers every allocation of a handle: the handle, the buffer
between it and its stream, the chunk in which values are converted, and the
strings of a text header and a trailer. The sizes of buffer and chunk are in
the configuration too, because they are the RAM of a small program. The
functions without a handle, `sdfio_decode_heights` and the others, use a small
chunk on the stack. Memory that the program asks for with `sdfio_alloc_data` is
its own, and is freed with `sdfio_free`.

Files are in `<sdfio/file.h>`. They are a layer over the stream callbacks that a
program with a file system wants and one without it does not: the core header
does not include `<stdio.h>`. A reader or a writer can own something that is
closed with it, which is how a file is closed.

## Trailer

The trailer, record 3, is free text that belongs to the measurement. ISO-2.0
asks for the tagged form of the header in it: lines of `field name = value`.
The library neither parses nor checks a trailer. Reading gives the application
the text, and writing puts down the text it gives; what the lines mean, and
whether they have the form, is the application's business. What the library
does is help to assemble the lines:

- **Assembly.** `sdfio_trailer` is a small struct over memory of the caller, and
  `sdfio_trailer_add_field` appends a line to it. Nothing is allocated, so a
  small device can use it. A name that is not a letter, a digit or `_`, a value
  that is not one line of 7-bit ASCII, or a line that does not fit is refused
  and the text is left as it was. This is the one place where the form is
  guaranteed, and it fails at once, before a scan has begun, not at the end of
  the file. The value is text: how a number is written is for the program, which
  knows its precision and its locale.
- **Line ends.** In memory the lines end with LF. Writing turns every line end,
  LF, CR or CRLF, into CRLF, as Annex A has it, in text and binary files alike,
  and closes the last line. Reading accepts all three, as the BCR text allows,
  and gives LF with the end of the last line dropped, so the trailer that is
  written is the trailer that is read, whatever the representation.

Because a trailer is not looked at, free text for ISO-1.0 and BCR-1.0 is written
as it is, and so is text in ISO-2.0 that is not in the tagged form. One thing
the application has to know: in a text file a line of only `*` ends the record,
so a trailer of its own must not have one. The assembled lines never do.

## Row order

BCR-1.0 stores the rows of the data area from the largest y down; ISO-1.0 and
ISO-2.0 store y = 0 first. The two kinds of calls treat this differently, on
purpose:

| Calls | Order | Who reverses |
| --- | --- | --- |
| `sdfio_read_heights`, `sdfio_write_heights` | y = 0 is the first row of your matrix, always | the library, by taking each file row from or to `dst + y * row_stride` |
| `sdfio_read_data`, `sdfio_write_data` | the order of the file | the caller, with `sdfio_row_y(header, n)` |

A whole-matrix call can reverse because the matrix is in memory and the stream
is still read or written in order. A call that moves rows cannot hide it, since
a device that holds one row at a time has to know which row to produce next.
The answer is `sdfio_row_y`, which is the index for the ISO revisions and
`rows - 1 - n` for BCR-1.0. A scanner that asks it for every row writes any
revision from one loop and holds one row of memory; the alternative, a whole
matrix in memory, is not available on a small device. Reading is the same:
the first row received is the largest y in a BCR file.

A consumer that needs the rows streamed with y ascending cannot get them from
a BCR file without seeking to the end and reading backwards, which is not done.
The orientation of the caller's own matrix, such as the rows of an image from
the top, is a separate matter that the row stride takes care of.

## Strides

The matrix of heights of the caller is described by a row stride and an
element stride in bytes, both of which may be negative, as GDAL has its line and
pixel spacing. A plain matrix needs neither. The element stride reads the `z` of
an array of points or one channel of an image in place, a negative row stride
turns an image the right way up, and both together take a column-major matrix.
The library accepts rows that lie beside each other or columns that do, and
refuses strides that make elements overlap.

The same strides serve memory that another library hands out: a device buffer
whose rows are padded to a pitch, mapped into the address space, or rows aligned
for vector loads. The call is made on the mapped pointer with the pitch as the
row stride, so the heights go straight into the buffer that the device reads,
and a result is written straight from a mapped buffer, with no array in
between. Most programs read into an image class of their own or into an OpenCV
`Mat` first and upload from there, which is as it should be; these calls are for
the ones that cannot spend the copy.

## Z-scale

An integer data type needs a z-scale. `sdfio_write_heights` chooses the best for
the heights it is given; the row call cannot, because it has not seen the
other rows, and says so instead of guessing. `sdfio_z_scale_for_range` and
`sdfio_suggest_z_scale` compute it for a program that knows its range or has the
matrix.

## Threads

There is no global state: no static buffers, no `strtok`, no `localeconv`
(which the C library shares and rewrites). The decimal point of the locale is
measured with a print on every call that needs it, and the date conversion uses
only `localtime_r`. Handles are independent, so threads can read and write at
the same time; the tests run that under ThreadSanitizer.

## Not done

- Random access to a row of a binary file.
- Compression and checksums of the data area, which the standard describes and
  the library does not support.
- Comments of BCR-1.0 files are dropped when a file is read.
