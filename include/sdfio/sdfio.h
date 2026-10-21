/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * Reading and writing ISO 25178-71 SDF surface data files, in the revisions
 * ISO-1.0, ISO-2.0 and BCR-1.0, as text or binary.
 *
 * A file has three records, as the standard names them: the header (record
 * 1), the data area (record 2) and the trailer (record 3). The library is made
 * of layers; use the highest one that does the job.
 *
 *  1. Handles and streams: a reader or a writer works on a file, on memory or
 *     on callbacks of your own, and holds no more of the file than a chunk.
 *  2. Records: sdfio_get_header() has the header. sdfio_read_data() and
 *     sdfio_write_data() move the data area as the file codes it, in the data
 *     type of the file, row by row into and out of memory that you own.
 *     sdfio_read_trailer() and sdfio_write_trailer() move the trailer.
 *  3. Heights: sdfio_decode_heights() and sdfio_encode_heights() turn the coded
 *     height information of the data area into metres and back. They need no
 *     handle, only the header, so they work on data from any source.
 *  4. Images: sdfio_read_heights() and sdfio_write_heights() do layers 2 and 3
 *     for a whole matrix with any row stride.
 *
 * Names: everything that the library puts into a program starts with `sdfio_`
 * (functions and types) or `SDFIO_` (macros and enumerators), and the headers
 * are in `<sdfio/>`.
 *
 * Files: sdfio_open_file() and sdfio_create_file() are in `<sdfio/file.h>`, so
 * that a program without a file system does not need `<stdio.h>`.
 *
 * Memory: the library allocates no memory for heights. sdfio_alloc_data() and
 * sdfio_alloc_heights() size a buffer from the description of a file, with the
 * overflow checked, and sdfio_free() releases it; a buffer of your own works as
 * well. A reader or a writer takes its memory from the allocator of an
 * sdfio_config.
 *
 * Errors: every call returns an sdfio_status. The text of the last failure of a
 * handle is sdfio_reader_error_message() or sdfio_writer_error_message().
 *
 * Missing points: heights in metres mark them with NaN, or with the marker of
 * your choice. The coded values of the data area mark them with the bad value
 * of the data type and revision, sdfio_bad_value(), which sdfio_is_bad_value()
 * tests.
 *
 * Locale: numbers in text files always have the dot, whatever LC_NUMERIC says.
 *
 * Threads: there is no global state. Handles are independent of each other, so
 * one thread can read a file while another writes one, also over the same
 * memory that is only read. A single handle is used by one thread at a time.
 * Do not change the locale or the time zone while a call is running.
 */

#ifndef SDFIO_SDFIO_H
#define SDFIO_SDFIO_H

#include <sdfio/sdfio_export.h>
#include <sdfio/version.h>

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * The version of the library that is loaded, as "major.minor.patch". With a
 * shared library this can differ from SDFIO_VERSION_STRING, which describes
 * the headers that were compiled against.
 */
SDFIO_API const char *sdfio_version(void);

/** What a call returns: SDFIO_OK, or the kind of the failure. */
typedef enum sdfio_status
{
    SDFIO_OK = 0,
    /** A parameter is invalid, such as a null pointer or a row count. */
    SDFIO_ERR_ARGUMENT,
    /** The call does not fit the state of the handle, such as a second
       open. */
    SDFIO_ERR_STATE,
    /** The stream failed, or the file could not be opened. */
    SDFIO_ERR_IO,
    /** The data is not an SDF file, or is damaged or cut off. */
    SDFIO_ERR_FORMAT,
    /** A valid file with a feature that is not supported: compression. */
    SDFIO_ERR_UNSUPPORTED,
    /** A number does not fit the type that has to hold it. */
    SDFIO_ERR_RANGE,
    /** A height equals the bad value, which labels a point as not measured. */
    SDFIO_ERR_BAD_VALUE,
    SDFIO_ERR_MEMORY
} sdfio_status;

/** A short fixed text for a status, never null. */
SDFIO_API const char *sdfio_status_string(sdfio_status status);

/** The version number of the format that a file follows. */
typedef enum sdfio_revision
{
    SDFIO_ISO_1_0,
    SDFIO_ISO_2_0,
    SDFIO_BCR_1_0
} sdfio_revision;

/** The representation of a file, as the standard calls them: text or binary. */
typedef enum sdfio_representation
{
    SDFIO_BINARY,
    SDFIO_ASCII
} sdfio_representation;

/** How a value of the data area is coded. The numbers are those of the
 * standard. */
typedef enum sdfio_data_type
{
    SDFIO_DATA_UINT8 = 0,
    SDFIO_DATA_UINT16 = 1,
    SDFIO_DATA_UINT32 = 2,
    SDFIO_DATA_BINARY32 = 3,
    SDFIO_DATA_INT8 = 4,
    SDFIO_DATA_INT16 = 5,
    SDFIO_DATA_INT32 = 6,
    SDFIO_DATA_BINARY64 = 7
} sdfio_data_type;

/** The element type of a buffer of the caller. */
typedef enum sdfio_sample
{
    SDFIO_INT8,
    SDFIO_UINT8,
    SDFIO_INT16,
    SDFIO_UINT16,
    SDFIO_INT32,
    SDFIO_UINT32,
    SDFIO_FLOAT32,
    SDFIO_FLOAT64
} sdfio_sample;

SDFIO_API const char *sdfio_revision_name(sdfio_revision revision);
SDFIO_API const char *sdfio_data_type_name(sdfio_data_type type);

/** The size in bytes of a value of the data area, and of a sample. */
SDFIO_API size_t sdfio_data_type_size(sdfio_data_type type);
SDFIO_API size_t sdfio_sample_size(sdfio_sample sample);

/** The sample type that holds the values of a data type as they are. */
SDFIO_API sdfio_sample sdfio_sample_of(sdfio_data_type type);

/** Whether a revision allows a data type. */
SDFIO_API bool sdfio_revision_allows(sdfio_revision revision,
                                     sdfio_data_type type);

enum
{
    /** The room for the manufacturer's identifier, with its terminating 0.
     */
    SDFIO_MANUFACTURER_CAPACITY = 64
};

/** A date and time that is not recorded. */
#define SDFIO_NO_TIME ((time_t)-1)

/** A z-scale that the writer chooses. */
#define SDFIO_AUTO_Z_SCALE 0.0

/**
 * A z-resolution that is not known, as the standard has it: a negative number,
 * -1 by default. A reader reports it for every resolution that is not
 * positive. A writer takes any value that is not positive, such as 0 or a
 * negative number, as not known and writes -1.
 */
#define SDFIO_NO_RESOLUTION (-1.0)

/**
 * The header of a file, record 1, with the names of the standard. It is a plain
 * value that you fill in to write a file and that the reader fills in when it
 * opens one. The compression type and the checksum type are not fields: a file
 * has none of either.
 */
typedef struct sdfio_header
{
    sdfio_revision revision;
    sdfio_representation representation;
    sdfio_data_type data_type;
    /** Number of columns in the data matrix, M: the points along x. */
    size_t columns;
    /** Number of rows in the data matrix, N: 1 for a profile. */
    size_t rows;
    /** X-scale and Y-scale factor: the sampling intervals in metres. */
    double x_scale;
    double y_scale;
    /**
     * Z-scale factor: the height of one count of the data area in metres. A
     * reader always sets it. A writer given SDFIO_AUTO_Z_SCALE takes 1 for the
     * floating point types and chooses the best for the heights in
     * sdfio_write_heights(); the integer types need one for sdfio_write_data().
     */
    double z_scale;
    /**
     * Z-resolution: the quantization step of the instrument in metres,
     * SDFIO_NO_RESOLUTION if it is not known. It is not the z-scale: data can
     * be rescaled after the measurement.
     */
    double z_resolution;
    /**
     * Original creation date and time, and last modification date and time,
     * in UTC to the minute. A file that is read can have none, SDFIO_NO_TIME;
     * one that is written needs both, which sdfio_header_init() sets to now.
     * The files of ISO-1.0 and BCR-1.0 hold the local clock of the writer, and
     * the library converts, so a date that is read back equals the one that
     * was written.
     */
    time_t creation_date;
    time_t modification_date;
    /** Manufacturer's ID, 7-bit ASCII; a binary file keeps 10 characters. */
    char manufacturer_id[SDFIO_MANUFACTURER_CAPACITY];
} sdfio_header;

/**
 * Sets the header of a binary ISO-2.0 file of binary32 values with sampling
 * intervals of 1 m, both dates now and the manufacturer "libsdfio". Set the
 * fields you need, and the number of columns and rows, afterwards.
 */
SDFIO_API void sdfio_header_init(sdfio_header *header);

/**
 * Whether the first row in a file of the revision is the one with the
 * largest y, which is so in BCR-1.0 and not in ISO-1.0 and ISO-2.0.
 *
 * The functions that move a whole matrix of heights, sdfio_read_heights() and
 * sdfio_write_heights(), always have y = 0 first and reverse as needed. The
 * functions that move rows of the data area, sdfio_read_data() and
 * sdfio_write_data(), work in the order of the file and leave the reversal to
 * the caller, with sdfio_row_y().
 */
SDFIO_API bool sdfio_rows_reversed(sdfio_revision revision);

/**
 * The y of the n-th row of the file, counting from 0: n for ISO-1.0 and
 * ISO-2.0, and rows - 1 - n for BCR-1.0. A program that handles rows one at a
 * time asks it for every row, and is then right for every revision:
 *
 * ~~~
 * for (size_t n = 0; n < header.rows; ++n)
 * {
 *     scan_row(sdfio_row_y(&header, n), row);
 *     sdfio_write_data(writer, row, 1);
 * }
 * ~~~
 */
SDFIO_API size_t sdfio_row_y(const sdfio_header *header, size_t file_row);

/**
 * Whether a value of the data area is the bad value, which labels a point that
 * was not measured or is spurious.
 */
SDFIO_API bool sdfio_is_bad_value(sdfio_data_type type, sdfio_revision revision,
                                  double value);

/**
 * The bad value of a data type in a revision: the number that labels a point
 * as not measured or spurious. It is the minimum of the type in ISO-1.0 and
 * ISO-2.0, which the standard does not permit for a valid height, and the
 * maximum in BCR-1.0. A text file writes the string BAD.
 */
SDFIO_API double sdfio_bad_value(sdfio_data_type type, sdfio_revision revision);

/* ---- Streams ------------------------------------------------------------ */

/** The result of a read callback that failed. */
#define SDFIO_STREAM_ERROR ((size_t)-1)

/**
 * Where the bytes of a file come from and go to, as callbacks of your own.
 * A reader uses `read` and `seek`, a writer `write`. Without `seek` the
 * bytes are handled in order and once, which is enough for a pipe.
 */
typedef struct sdfio_stream
{
    void *user;
    /**
     * Reads up to `size` bytes and returns how many, 0 at the end of the
     * data, SDFIO_STREAM_ERROR on failure. It returns fewer than `size` bytes
     * only at the end.
     */
    size_t (*read)(void *user, void *data, size_t size);
    /** Writes `size` bytes and returns how many were written. */
    size_t (*write)(void *user, const void *data, size_t size);
    /**
     * Moves like `lseek`: `whence` is SEEK_SET, SEEK_CUR or SEEK_END, the
     * result is the new position from the start, or -1 on failure.
     */
    int64_t (*seek)(void *user, int64_t offset, int whence);
} sdfio_stream;

/**
 * How memory is got and given back, if not by malloc and free: a callback of
 * your own, for a program that has a pool or a bump allocator. `user` is
 * passed to both. `allocate` returns memory suitably aligned for any type, or
 * null; `release` takes a pointer `allocate` returned. A handle calls them for
 * all it needs, during its creation and when a file is created or read.
 */
typedef struct sdfio_allocator
{
    void *user;
    void *(*allocate)(void *user, size_t size);
    void (*release)(void *user, void *memory);
} sdfio_allocator;

/**
 * What a reader or a writer is made with. Every field may be left zero for
 * its default, and a null configuration is all defaults.
 */
typedef struct sdfio_config
{
    /** Copied, so it need not outlive the call. Null is malloc and free. */
    const sdfio_allocator *allocator;
    /**
     * The size of the buffer between the handle and its stream, in bytes: 0
     * is 16384, and less than 64 is 64. A smaller buffer means more calls of
     * the stream.
     */
    size_t buffer_size;
    /**
     * How many values are converted at a time, which sets the memory of the
     * handle apart from the buffer: 0 is 1024. A chunk of values costs up to
     * 8 bytes for each of them, and in a writer of text some more.
     */
    size_t chunk_values;
} sdfio_config;

/* ---- Memory ------------------------------------------------------------- */

/**
 * Memory for `rows` rows of the data area of the file that `header`
 * describes, zeroed, to read into with sdfio_read_data() or to hand to
 * sdfio_decode_heights(). The size is checked for an overflow. Returns null if
 * the memory is short or `header` is not valid. Free it with sdfio_free().
 */
SDFIO_API void *sdfio_alloc_data(const sdfio_header *header, size_t rows);

/**
 * Memory for `rows` rows of the file as heights of the sample type, zeroed:
 * the matrix for sdfio_read_heights() and sdfio_write_heights() when `rows` is
 * `header->rows`. Returns null as sdfio_alloc_data() does.
 */
SDFIO_API void *sdfio_alloc_heights(const sdfio_header *header,
                                    sdfio_sample sample, size_t rows);

/**
 * Frees memory of sdfio_alloc_data() and sdfio_alloc_heights(), in the runtime
 * that allocated it, which with a shared library on Windows is not that of the
 * program. Null is ignored.
 */
SDFIO_API void sdfio_free(void *memory);

/* ---- Reading ------------------------------------------------------------ */

typedef struct sdfio_reader sdfio_reader;

/**
 * Makes a reader, which reads nothing yet; open a source next. A reader is
 * made first so that a failed open can still tell what went wrong. All the
 * memory it needs, the buffer included, is got here and by reading a trailer.
 * Returns null if memory is short, or if the allocator of the configuration
 * lacks a callback.
 */
SDFIO_API sdfio_reader *sdfio_reader_new(const sdfio_config *config);

/** Frees a reader and closes the file it opened, if any. Null is ignored. */
SDFIO_API void sdfio_reader_free(sdfio_reader *reader);

/**
 * Opens a source and reads its header. A source that cannot seek can be
 * read once, from the front. The memory and the stream stay yours: keep
 * them alive and unchanged while the reader is used.
 *
 * @return SDFIO_ERR_FORMAT if it is not an SDF file or is cut off,
 * SDFIO_ERR_UNSUPPORTED for a compressed or checksummed data area.
 */
SDFIO_API sdfio_status sdfio_open_memory(sdfio_reader *reader, const void *data,
                                         size_t size);
SDFIO_API sdfio_status sdfio_open_stream(sdfio_reader *reader,
                                         const sdfio_stream *stream);

/** The header of the opened file, null before it is open. */
SDFIO_API const sdfio_header *sdfio_get_header(const sdfio_reader *reader);

/**
 * Reads the next `rows` rows of the data area, record 2, into `dst`, which
 * holds rows * columns values of header.data_type in the byte order of the
 * machine, as the file codes them: in the order of the file, with the number
 * that marks a point that is not measured unchanged. Text is parsed to the
 * data type of the file, so a binary32 text file is rounded to single
 * precision. To get heights, decode the values, see sdfio_decode_heights().
 *
 * The first row that is read is the first row of the file. In BCR-1.0 that is
 * the row with the largest y: sdfio_row_y() gives the y of every row that is
 * read, in every revision.
 *
 * After an error the reader cannot go on; rewind it if it can seek.
 */
SDFIO_API sdfio_status sdfio_read_data(sdfio_reader *reader, void *dst,
                                       size_t rows);

/**
 * How a buffer of heights of the caller looks: its element type, and the
 * number that stands in it for a point that was not measured. NaN means
 * none: a point that was not measured is NaN, or 0 in an integer type,
 * which has no NaN. A marker is taken in the precision of the sample type,
 * so -9999.1 marks the points of a float buffer that hold the float
 * -9999.1.
 */
typedef struct sdfio_format
{
    sdfio_sample sample;
    double missing;
    /**
     * The distance in bytes between a sample and the next one in a row of the
     * heights, 0 for samples that follow each other. With it the heights are
     * read and written in place where they are not contiguous: the z of an
     * array of points, one channel of an interleaved image, the rows of a
     * column-major matrix. A negative stride runs the row backwards. The
     * element (y, x) is at `data + y * row_stride + x * element_stride`, and
     * the strides have to keep the elements apart: rows that lie beside each
     * other, or columns that do.
     */
    ptrdiff_t element_stride;
} sdfio_format;

/** Heights in metres of the sample type, a point not measured being NaN. */
static inline sdfio_format sdfio_metres(sdfio_sample sample)
{
    sdfio_format format = {sample, NAN, 0};
    return format;
}

/**
 * Reads the rest of the file as heights in metres into a matrix of `rows`
 * rows of `columns` samples: the sample (y, x) is at
 * `dst + y * row_stride + x * format.element_stride`, and y = 0 is the first
 * row in every revision. A negative row stride, with `dst` at the first sample
 * of the row y = 0, which is the last row in memory, turns the matrix upside
 * down.
 *
 * Start from the first row: after other reads, rewind first.
 */
SDFIO_API sdfio_status sdfio_read_heights(sdfio_reader *reader,
                                          sdfio_format format, void *dst,
                                          ptrdiff_t row_stride);

/**
 * Reads the trailer, record 3, the free text after the data area, and returns
 * it with its size, valid until the reader is freed. Nothing is returned as a
 * null pointer. Rows of the data area that were not read are passed over,
 * which a stream that cannot seek has to do to get there. A second call
 * returns the same trailer. It has a 0 after its last byte, which the size
 * does not count.
 *
 * In memory the lines of the trailer end with LF, whatever the file has: a
 * line end of the file, CRLF, CR or LF, in a text file or a binary file, is
 * read as LF, and the end of the last line is not part of the text. A text file
 * also drops the white space around the trailer.
 *
 * Text files give the line ends of the trailer as LF.
 */
SDFIO_API sdfio_status sdfio_read_trailer(sdfio_reader *reader,
                                          const char **trailer, size_t *size);

/** Goes back to the first row, for a source that can seek. */
SDFIO_API sdfio_status sdfio_rewind(sdfio_reader *reader);

/** The text of the last error of the reader, empty if there was none. */
SDFIO_API const char *sdfio_reader_error_message(const sdfio_reader *reader);

/* ---- Writing ------------------------------------------------------------ */

typedef struct sdfio_writer sdfio_writer;

/**
 * Makes a writer, which writes nothing yet; create a file next. The buffer is
 * got here, the rest when the file is created. Returns null as
 * sdfio_reader_new(NULL) does.
 */
SDFIO_API sdfio_writer *sdfio_writer_new(const sdfio_config *config);

/**
 * Frees a writer. A file that it created with sdfio_create_file() and that was
 * not ended is closed as it is, incomplete.
 */
SDFIO_API void sdfio_writer_free(sdfio_writer *writer);

/**
 * Starts a file whose header is `header`, the number of columns and rows
 * included. The header is written with the first rows of the data area. The
 * dates of the header must be set: SDFIO_NO_TIME cannot be written.
 *
 * @return SDFIO_ERR_ARGUMENT if the description cannot be written: a data
 * type the revision does not have, a scale that is not positive, a count
 * the binary header cannot hold.
 */
SDFIO_API sdfio_status sdfio_create_stream(sdfio_writer *writer,
                                           const sdfio_stream *stream,
                                           const sdfio_header *header);

/**
 * Writes the next `rows` rows of the data area from `src`, which holds
 * rows * columns values of header.data_type in the byte order of the machine,
 * in the order of the file. The number that marks a point as not measured is
 * written as it is.
 *
 * The first row that is written is the first row of the file. In BCR-1.0 that
 * is the row with the largest y: ask sdfio_row_y() which y belongs to the next
 * row, and a program that scans a row at a time is right for every revision.
 *
 * @return SDFIO_ERR_ARGUMENT if the data type is an integer type and the
 * z-scale is not set: the writer cannot know what a count means.
 */
SDFIO_API sdfio_status sdfio_write_data(sdfio_writer *writer, const void *src,
                                        size_t rows);

/**
 * Writes the whole file from a matrix of heights in metres, `rows` rows of
 * `columns` samples laid out as for sdfio_read_heights(): the sample (y, x) is
 * at `src + y * row_stride + x * format.element_stride`, row 0 being y = 0.
 * With SDFIO_AUTO_Z_SCALE and an integer data type the best z-scale for the
 * heights is chosen. NaN, infinity and `format.missing` mark points that are
 * not measured. Write the trailer with sdfio_write_trailer() after it.
 *
 * @return SDFIO_ERR_RANGE if a height does not fit the data type with the
 * z-scale, SDFIO_ERR_BAD_VALUE if one would be read as a point not measured.
 */
SDFIO_API sdfio_status sdfio_write_heights(sdfio_writer *writer,
                                           sdfio_format format, const void *src,
                                           ptrdiff_t row_stride);

/**
 * Writes the trailer, record 3, and ends the file: the text as it is given, or
 * null for none. It is not looked at: what the text is, in ISO-2.0 lines of the
 * form `field name = value`, is for the program, which is helped by
 * sdfio_trailer_add_field(). All rows of the data area must
 * have been written. A file of sdfio_create_file() is closed.
 *
 * Every line end of the text, LF, CR or CRLF, is written as CRLF, as the
 * standard has it, in a text file and in a binary file, and the last line is
 * closed by one. Keep the lines with LF in memory; sdfio_read_trailer() gives
 * them back that way. In a text file a line of only `*` ends the record, so
 * the text of a program must not have one; the assembled lines never do.
 *
 * @return SDFIO_ERR_ARGUMENT if the text is null with a size.
 */
SDFIO_API sdfio_status sdfio_write_trailer(sdfio_writer *writer,
                                           const char *trailer, size_t size);

/** The text of the last error of the writer, empty if there was none. */
SDFIO_API const char *sdfio_writer_error_message(const sdfio_writer *writer);

/* ---- Heights ------------------------------------------------------------ */

/**
 * Turns `count` values of the data area (header->data_type), the coded height
 * information, into heights in metres of the format, `format.element_stride`
 * bytes apart. A point that is not measured becomes format.missing. Needs no
 * handle, so it works on data from any source.
 *
 * @return SDFIO_ERR_RANGE if a height does not fit the sample type.
 */
SDFIO_API sdfio_status sdfio_decode_heights(const sdfio_header *header,
                                            sdfio_format format,
                                            const void *data, size_t count,
                                            void *heights);

/**
 * The reverse: heights in metres of the format into values of the data area
 * (header->data_type), with header->z_scale. NaN, infinity and
 * `format.missing` become the number that marks a point not measured.
 * Integers are rounded.
 *
 * @return SDFIO_ERR_ARGUMENT if the z-scale is not set, SDFIO_ERR_RANGE if a
 * height does not fit the data type, SDFIO_ERR_BAD_VALUE if one would be read
 * as a point not measured.
 */
SDFIO_API sdfio_status sdfio_encode_heights(const sdfio_header *header,
                                            sdfio_format format,
                                            const void *heights, size_t count,
                                            void *data);

/**
 * The z-scale for heights between `lowest` and `highest` metres that uses
 * the range of an integer data type best; 1 for a floating point type.
 */
SDFIO_API double sdfio_z_scale_for_range(sdfio_data_type type,
                                         sdfio_revision revision, double lowest,
                                         double highest);

/**
 * The best z-scale for a matrix of heights in metres of any sample type, as
 * sdfio_write_heights() chooses it: the range of the finite heights, leaving
 * out the points that format.missing marks, is looked up and handed to
 * sdfio_z_scale_for_range(). For the use of sdfio_write_data() with an integer
 * data type.
 */
SDFIO_API sdfio_status
sdfio_suggest_z_scale(sdfio_data_type type, sdfio_revision revision,
                      sdfio_format format, const void *heights, size_t rows,
                      size_t columns, ptrdiff_t row_stride, double *z_scale);

/* ---- Trailer ------------------------------------------------------------ */

/**
 * A trailer, record 3, in the making: lines of the form `field name = value`,
 * which ISO-2.0 asks for in its trailer, in memory of the caller. The lines end
 * with LF in memory; sdfio_write_trailer() writes them as the standard has
 * them. Hand `text` and `size` to sdfio_write_trailer(). The library does not
 * interpret a trailer: reading one gives the text, which is the program's.
 *
 * ~~~
 * char memory[512];
 * sdfio_trailer trailer;
 * sdfio_trailer_init(&trailer, memory, sizeof memory);
 * sdfio_trailer_add_field(&trailer, "OperatorName", "Jane Doe");
 * sdfio_trailer_add_field(&trailer, "Magnification", "20.5");
 * ...
 * sdfio_write_trailer(writer, trailer.text, trailer.size);
 * ~~~
 */
typedef struct sdfio_trailer
{
    /** The lines so far, 0-terminated, in the memory given to init. */
    char *text;
    /** The bytes of the text, not counting the 0. */
    size_t size;
    size_t capacity;
} sdfio_trailer;

/** Starts an empty trailer in `memory` of `capacity` bytes. Nothing is
 * allocated. */
SDFIO_API void sdfio_trailer_init(sdfio_trailer *trailer, char *memory,
                                  size_t capacity);

/**
 * Appends one line `field = value`. The value is text: a number is written by
 * the program, as it wants it, for instance with snprintf in the C locale, as a
 * locale with a decimal comma would write `20,5`. White space around a value is
 * lost when the line is read.
 *
 * @return SDFIO_ERR_ARGUMENT if the field name is empty or has a character
 * other than a letter, a digit or `_`, or if the value is not one line of 7-bit
 * ASCII. SDFIO_ERR_RANGE if the line and the 0 after the text do not fit. The
 * trailer is left as it was.
 */
SDFIO_API sdfio_status sdfio_trailer_add_field(sdfio_trailer *trailer,
                                               const char *field,
                                               const char *value);

#ifdef __cplusplus
}
#endif

#endif /* SDFIO_SDFIO_H */
