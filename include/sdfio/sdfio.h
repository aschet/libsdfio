/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * Reading and writing of ISO 25178-71 SDF surface data files.
 *
 * The revisions ISO-1.0 and ISO-2.0 are supported, each in the ASCII and in the
 * binary representation. An SDF file consists of three records: the
 * header (record 1), the data area (record 2) and the trailer (record 3).
 *
 * @par Layers
 * The interface consists of four layers. A program uses the highest layer that
 * suffices.
 * 1. Handles and streams. A reader or a writer operates on a file, a block of
 *    memory or caller-supplied callbacks. It holds one buffer and one
 *    conversion chunk of the file, never the whole file.
 * 2. Records. sdfio_get_header() provides the header. sdfio_read_data() and
 *    sdfio_write_data() transfer the data area row by row in the coded form of
 *    the file. sdfio_read_trailer() and sdfio_write_trailer() transfer the
 *    trailer.
 * 3. Heights. sdfio_decode_heights() and sdfio_encode_heights() convert between
 *    the coded values of the data area and heights in metres. They require
 *    only a header, no handle.
 * 4. Images. sdfio_read_heights() and sdfio_write_heights() combine layers 2
 *    and 3 for a complete matrix, in a plain array or in a buffer that lies in
 *    memory as described by strides.
 *
 * @par Naming
 * Every identifier that the library provides begins with `sdfio_` (functions
 * and types) or `SDFIO_` (macros and enumerators). The headers reside in the
 * directory `sdfio/`.
 *
 * @par Files
 * sdfio_open_file() and sdfio_create_file() are declared in `<sdfio/file.h>`.
 * This header does not include `<stdio.h>`.
 *
 * @par Data and heights
 * The data area stores one value per point in the data type of the file, in
 * units of the z-scale of the header. These are the coded values, which
 * sdfio_read_data() and sdfio_write_data() transfer. A height is the same
 * point in metres, as a float or a double. A point that is not measured is NaN
 * or a marker that the program chooses.
 * sdfio_read_heights() and sdfio_write_heights() transfer heights, and
 * sdfio_decode_heights() and sdfio_encode_heights() convert between the two.
 *
 * @par Memory
 * The library allocates no memory for the data or the heights. The caller
 * allocates the buffers and obtains their size from sdfio_data_size(),
 * sdfio_data_row_size(), sdfio_heights_size() and sdfio_heights_row_size(),
 * with a check for arithmetic overflow. A reader or a writer obtains its own
 * memory through the allocator of its sdfio_config.
 *
 * @par Error handling
 * A function that can fail returns an sdfio_status, or a null pointer where it
 * returns a handle. sdfio_reader_error_message() and
 * sdfio_writer_error_message() describe the most recent failure of a handle.
 *
 * @par Points that are not measured
 * Heights in metres represent a point that is not measured by NaN or by a
 * marker chosen by the caller (see sdfio_format). The coded values of the data
 * area represent it by the bad value of the data type and revision, which
 * sdfio_bad_value() returns and sdfio_is_bad_value() tests.
 *
 * @par Locale
 * Numbers in ASCII files are read and written with the decimal point `.`,
 * independent of `LC_NUMERIC`.
 *
 * @par Versioning
 * Structures such as sdfio_header are plain structures that the caller
 * initializes. Changing one is an incompatible change and increments the major
 * version.
 *
 * @par Standard
 * The library follows ISO 25178-71:2026, and a reader checks what the standard
 * defines: the magic, the data types of the revision, a compression and a
 * checksum type of 0, the ranges of the counts and of the dates, a positive
 * z-scale, the line ends and the string `BAD`. It deviates in two places, which
 * files of MountainsMap need: the numbers of an ASCII file can be in any
 * decimal or scientific form, and a manufacturer identifier in a binary file
 * ends at its first 0 byte, where the standard fills it with spaces.
 *
 * @par Limitations
 * Rows of a file can be read only in order.
 *
 * @par Thread safety
 * The library has no global state. Distinct handles can be used concurrently by
 * distinct threads, including over the same memory when it is only read. A
 * single handle must not be used by more than one thread at a time. The locale
 * must not be changed while a call is in progress.
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
 * Returns the version of the library that is loaded, as the string
 * "major.minor.patch". With a shared library it can differ from
 * SDFIO_VERSION_STRING, which describes the headers that the program was
 * compiled with. The string is a constant.
 */
SDFIO_API const char *sdfio_version(void);

/** Result of a function call. */
typedef enum sdfio_status
{
    /** The call succeeded. */
    SDFIO_OK = 0,
    /**
     * An argument is invalid, for example a null pointer or a row count that
     * exceeds the rows remaining.
     */
    SDFIO_ERR_ARGUMENT,
    /**
     * The call is not valid in the current state of the handle, for example
     * opening a reader a second time.
     */
    SDFIO_ERR_STATE,
    /** The stream failed, or a file could not be opened. */
    SDFIO_ERR_IO,
    /** The data is not an SDF file, is damaged or is truncated. */
    SDFIO_ERR_FORMAT,
    /**
     * The request requires an unavailable feature: the ASCII representation in
     * a library built without it.
     */
    SDFIO_ERR_UNSUPPORTED,
    /** A number does not fit the type that has to hold it. */
    SDFIO_ERR_RANGE,
    /**
     * A height equals the bad value of the data type, so it would be read as a
     * point that is not measured.
     */
    SDFIO_ERR_BAD_VALUE,
    /** An allocation failed. */
    SDFIO_ERR_MEMORY
} sdfio_status;

/**
 * Returns a constant description of a status. The result is never null.
 *
 * @param[in] status The status to describe.
 */
SDFIO_API const char *sdfio_status_string(sdfio_status status);

/** Revision of the SDF format that a file follows. */
typedef enum sdfio_revision
{
    /** Revision ISO-1.0. */
    SDFIO_ISO_1_0,
    /** Revision ISO-2.0. */
    SDFIO_ISO_2_0
} sdfio_revision;

/** Representation of a file. */
typedef enum sdfio_representation
{
    /** Binary representation. */
    SDFIO_BINARY,
    /** ASCII representation. */
    SDFIO_ASCII
} sdfio_representation;

/**
 * Coding of a value in the data area. The enumerator values are the data type
 * codes of the standard.
 */
typedef enum sdfio_data_type
{
    /** IEEE 754 binary32 floating point number. */
    SDFIO_DATA_BINARY32 = 3,
    /** Signed 8-bit integer. */
    SDFIO_DATA_INT8 = 4,
    /** Signed 16-bit integer. */
    SDFIO_DATA_INT16 = 5,
    /** Signed 32-bit integer. */
    SDFIO_DATA_INT32 = 6,
    /** IEEE 754 binary64 floating point number. */
    SDFIO_DATA_BINARY64 = 7
} sdfio_data_type;

/** Element type of a buffer of heights of the caller. */
typedef enum sdfio_sample
{
    /** 32-bit floating point number, `float`. */
    SDFIO_FLOAT32,
    /** 64-bit floating point number, `double`. */
    SDFIO_FLOAT64
} sdfio_sample;

/**
 * Returns the name of a revision, for example "ISO-2.0", or an empty string
 * for an invalid value.
 *
 * @param[in] revision The revision.
 */
SDFIO_API const char *sdfio_revision_name(sdfio_revision revision);

/**
 * Returns the name of a data type, for example "binary32", or an empty string
 * for an invalid value.
 *
 * @param[in] type The data type.
 */
SDFIO_API const char *sdfio_data_type_name(sdfio_data_type type);

/**
 * Returns the size in bytes of one value of the data area in a data type, or 0
 * for an invalid value.
 *
 * @param[in] type The data type.
 */
SDFIO_API size_t sdfio_data_type_size(sdfio_data_type type);

/**
 * Returns the size in bytes of a sample, or 0 for an invalid value.
 *
 * @param[in] sample The sample type.
 */
SDFIO_API size_t sdfio_sample_size(sdfio_sample sample);

/**
 * Returns true if a revision permits a data type.
 *
 * @param[in] revision The revision.
 * @param[in] type The data type.
 */
SDFIO_API bool sdfio_revision_allows(sdfio_revision revision,
                                     sdfio_data_type type);

enum
{
    /**
     * Capacity of sdfio_header::manufacturer_id in bytes, including the
     * terminating null character.
     */
    SDFIO_MANUFACTURER_CAPACITY = 64
};

/** Value of a date that is not recorded. */
#define SDFIO_NO_TIME ((time_t)-1)

/**
 * Value of sdfio_header::z_scale that requests the writer to select the
 * z-scale.
 */
#define SDFIO_AUTO_Z_SCALE 0.0

/**
 * Value of sdfio_header::z_resolution for an unknown resolution. The standard
 * represents it as a negative number, -1 by default. A reader reports it for
 * every resolution that is not positive. A writer treats every value that is
 * not positive as unknown and writes -1.
 */
#define SDFIO_NO_RESOLUTION (-1.0)

/**
 * Header of a file (record 1), with the field names of the standard.
 *
 * A reader fills it in when it opens a file. A writer takes it as the
 * description of the file to create. The compression type and the checksum
 * type are not members: the standard allows neither.
 */
typedef struct sdfio_header
{
    /** Revision of the format. */
    sdfio_revision revision;
    /** Representation of the file. */
    sdfio_representation representation;
    /** Data type of the values in the data area. */
    sdfio_data_type data_type;
    /** Number of columns M of the data matrix: the points along x. */
    size_t columns;
    /** Number of rows N of the data matrix: 1 for a profile. */
    size_t rows;
    /** Sampling interval along x in metres (X-scale factor). */
    double x_scale;
    /**
     * Sampling interval along y in metres (Y-scale factor). It is meaningless
     * for a profile, and any finite value is written.
     */
    double y_scale;
    /**
     * Z-scale factor: the height of one count of the data area in metres.
     *
     * A reader always sets it. A writer treats SDFIO_AUTO_Z_SCALE as 1 for the
     * floating point data types. For the integer data types,
     * sdfio_write_heights() selects the z-scale from the heights, whereas
     * sdfio_write_data() requires it to be set.
     */
    double z_scale;
    /**
     * Z-resolution: the quantization step of the instrument in metres, or
     * SDFIO_NO_RESOLUTION. It is independent of the z-scale, because data can
     * be rescaled after the measurement.
     */
    double z_resolution;
    /**
     * Original creation date and time, with a resolution of one minute.
     *
     * A file that is read can have none (SDFIO_NO_TIME). A file that is written
     * requires one; sdfio_header_init() sets the current time.
     *
     * ISO-2.0 files store UTC. ISO-1.0 files store the local clock
     * of the writer, without a time zone. The library does not know the zone,
     * so it keeps such a clock reading as it is, as if it were UTC, unless
     * the configuration supplies an sdfio_time_zone.
     */
    time_t creation_date;
    /** Last modification date and time. The rules of creation_date apply. */
    time_t modification_date;
    /**
     * Manufacturer's identifier, 7-bit ASCII, terminated by a null character.
     * A binary file stores the first 10 characters.
     */
    char manufacturer_id[SDFIO_MANUFACTURER_CAPACITY];
} sdfio_header;

/**
 * Initializes a header with default values.
 *
 * The defaults are: revision ISO-2.0, binary representation, data type
 * binary32, x_scale and y_scale of 1, z_scale SDFIO_AUTO_Z_SCALE, z_resolution
 * SDFIO_NO_RESOLUTION, both dates set to the current time, manufacturer_id
 * "libsdfio", and zero columns and rows. The caller sets the number of columns
 * and rows and every other field that differs.
 *
 * @param[out] header The header to initialize.
 */
SDFIO_API void sdfio_header_init(sdfio_header *header);

/**
 * Returns true if a value of the data area is the bad value, which labels a
 * point that is not measured or spurious.
 *
 * @param type The data type of the value.
 * @param value The value of the data area.
 */
SDFIO_API bool sdfio_is_bad_value(sdfio_data_type type, double value);

/**
 * Returns the bad value of a data type: the number that labels a point as not
 * measured or spurious. It is the minimum of the type, which the standard
 * excludes for a valid height. ASCII files denote it by the string `BAD`.
 *
 * @param type The data type.
 */
SDFIO_API double sdfio_bad_value(sdfio_data_type type);

/* ---- Streams ------------------------------------------------------------ */

/** Return value of a read callback that failed. */
#define SDFIO_STREAM_ERROR ((size_t)-1)

/**
 * Callbacks that provide or accept the bytes of a file.
 *
 * A reader uses `read`. A writer uses `write`. The bytes are processed once and
 * in order, as for a pipe.
 */
typedef struct sdfio_stream
{
    /** Opaque pointer passed as the first argument of every callback. */
    void *user;
    /**
     * Reads up to `size` bytes into `data`. Returns the number of bytes read,
     * 0 at the end of the data, or SDFIO_STREAM_ERROR on failure. Returns fewer
     * than `size` bytes only at the end of the data.
     */
    size_t (*read)(void *user, void *data, size_t size);
    /**
     * Writes `size` bytes from `data`. Returns the number of bytes written; a
     * count smaller than `size` indicates a failure.
     */
    size_t (*write)(void *user, const void *data, size_t size);
} sdfio_stream;

/**
 * Memory management callbacks that replace malloc() and free(), for example
 * with a pool or a bump allocator.
 *
 * A handle calls them for all of its memory: when it is created, when a file is
 * created or opened, and when a trailer is read.
 */
typedef struct sdfio_allocator
{
    /** Opaque pointer passed as the first argument of both callbacks. */
    void *user;
    /**
     * Returns a block of at least `size` bytes, aligned for any object type,
     * or null on failure.
     */
    void *(*allocate)(void *user, size_t size);
    /** Releases a block returned by `allocate`. Never called with null. */
    void (*release)(void *user, void *memory);
} sdfio_allocator;

/**
 * Conversion of the dates of ISO-1.0 files between the local clock of the
 * writer and UTC.
 *
 * These files store the clock reading without a time zone, and only the
 * program can know the zone, including its rules for daylight saving time. The
 * callbacks receive and return times in the form of a time_t, with a resolution
 * of one minute. A clock reading is expressed as if it were UTC. A callback
 * returns SDFIO_NO_TIME if it cannot convert the time. A callback that is null
 * performs no conversion. ISO-2.0 files store UTC and are not affected. The
 * callbacks are called by the thread that uses the handle.
 */
typedef struct sdfio_time_zone
{
    /** Opaque pointer passed as the first argument of both callbacks. */
    void *user;
    /**
     * Converts the clock reading `local` of a file to the UTC time. Called when
     * a reader parses a date. If it returns SDFIO_NO_TIME, the date reads as
     * not recorded.
     */
    time_t (*local_to_utc)(void *user, time_t local);
    /**
     * Converts the UTC time `utc` to the clock reading to store. Called when a
     * writer formats a date. If it returns SDFIO_NO_TIME, the file cannot be
     * created.
     */
    time_t (*utc_to_local)(void *user, time_t utc);
} sdfio_time_zone;

/**
 * Configuration of a reader or a writer.
 *
 * A field that is zero selects its default. A null pointer in place of a
 * configuration selects all defaults.
 */
typedef struct sdfio_config
{
    /**
     * Allocator for all memory of the handle. The structure is copied, so it
     * need not outlive the call that receives it. Null selects malloc() and
     * free(). Both callbacks must be set.
     */
    const sdfio_allocator *allocator;
    /**
     * Size in bytes of the buffer between the handle and its stream. Zero
     * selects 16384. A value below 64 is raised to 64. A smaller buffer causes
     * more calls of the stream callbacks.
     */
    size_t buffer_size;
    /**
     * Number of values that are converted at a time. Zero selects 1024. The
     * chunk occupies up to 8 bytes per value; a writer of the ASCII
     * representation occupies more.
     */
    size_t chunk_values;
    /**
     * Conversion of the dates of ISO-1.0 files. The structure is
     * copied, so it need not outlive the call that receives it. Null selects no
     * conversion.
     */
    const sdfio_time_zone *time_zone;
} sdfio_config;

/* ---- Memory ------------------------------------------------------------- */

/**
 * Returns the size in bytes of a buffer for the whole data area: `header->rows`
 * times `header->columns` values of `header->data_type`. The size is checked
 * for arithmetic overflow. A buffer of this size is suitable for
 * sdfio_read_data() and sdfio_decode_heights(). The caller allocates it.
 *
 * @param[in] header The header of the file.
 * @param[out] bytes The size, which is 0 for an empty matrix.
 * @retval SDFIO_ERR_ARGUMENT A pointer is null or the data type is invalid.
 * @retval SDFIO_ERR_RANGE The size cannot be represented in a size_t.
 */
SDFIO_API sdfio_status sdfio_data_size(const sdfio_header *header,
                                       size_t *bytes);

/**
 * Returns the size in bytes of a buffer for one row of the data area, to read
 * or write the data row by row with sdfio_read_data() and sdfio_write_data().
 *
 * @param[in] header The header of the file.
 * @param[out] bytes The size, which is 0 for a row without columns.
 * @retval SDFIO_ERR_ARGUMENT A pointer is null or the data type is invalid.
 * @retval SDFIO_ERR_RANGE The size cannot be represented in a size_t.
 */
SDFIO_API sdfio_status sdfio_data_row_size(const sdfio_header *header,
                                           size_t *bytes);

/**
 * Returns the size in bytes of a buffer for the whole matrix of heights in a
 * sample type, for sdfio_read_heights() and sdfio_write_heights(). The caller
 * allocates it.
 *
 * @param[in] header The header of the file.
 * @param sample The sample type of the heights.
 * @param[out] bytes The size, which is 0 for an empty matrix.
 * @retval SDFIO_ERR_ARGUMENT A pointer is null or the sample type is invalid.
 * @retval SDFIO_ERR_RANGE The size cannot be represented in a size_t.
 */
SDFIO_API sdfio_status sdfio_heights_size(const sdfio_header *header,
                                          sdfio_sample sample, size_t *bytes);

/**
 * Returns the size in bytes of a buffer for one row of heights in a sample
 * type, for sdfio_decode_heights() and sdfio_encode_heights() of one row of
 * the data area.
 *
 * @param[in] header The header of the file.
 * @param sample The sample type of the heights.
 * @param[out] bytes The size, which is 0 for a row without columns.
 * @retval SDFIO_ERR_ARGUMENT A pointer is null or the sample type is invalid.
 * @retval SDFIO_ERR_RANGE The size cannot be represented in a size_t.
 */
SDFIO_API sdfio_status sdfio_heights_row_size(const sdfio_header *header,
                                              sdfio_sample sample,
                                              size_t *bytes);

/* ---- Reading ------------------------------------------------------------ */

/** Opaque handle of a reader. */
typedef struct sdfio_reader sdfio_reader;

/**
 * Creates a reader. The reader reads nothing until a source is opened. It is
 * created before the source is opened, so that a failed open can report its
 * reason.
 *
 * The handle, the buffer and the conversion chunk are allocated by this
 * function. Memory for a trailer is allocated when the trailer is read.
 * Release the reader with sdfio_reader_free().
 *
 * @param[in] config The configuration, or null for the defaults.
 * @return The reader, or null if an allocation fails or the allocator of the
 * configuration lacks a callback.
 */
SDFIO_API sdfio_reader *sdfio_reader_new(const sdfio_config *config);

/**
 * Releases a reader and closes the file that sdfio_open_file() opened, if any.
 * A null pointer is ignored.
 *
 * @param reader The reader.
 */
SDFIO_API void sdfio_reader_free(sdfio_reader *reader);

/**
 * Opens a block of memory as the source and reads the header. The memory is not
 * copied: it must remain valid and unchanged while the reader is in use.
 *
 * @param reader The reader.
 * @param[in] data The first byte of the file.
 * @param size The size of the file in bytes.
 * @retval SDFIO_ERR_ARGUMENT A pointer is null.
 * @retval SDFIO_ERR_STATE The reader is open already.
 * @retval SDFIO_ERR_FORMAT The data is not an SDF file or is truncated.
 * @retval SDFIO_ERR_UNSUPPORTED The file is ASCII and the library was built
 * without it.
 * @retval SDFIO_ERR_MEMORY An allocation failed.
 */
SDFIO_API sdfio_status sdfio_open_memory(sdfio_reader *reader, const void *data,
                                         size_t size);

/**
 * Opens a stream as the source and reads the header. The structure is copied,
 * and the callbacks and the object that `user` refers to must remain valid
 * while the reader is in use. The stream is read once, from the start.
 *
 * @param reader The reader.
 * @param[in] stream The stream; `read` is required.
 * @retval SDFIO_ERR_ARGUMENT A pointer is null or `read` is not set.
 * @retval SDFIO_ERR_STATE The reader is open already.
 * @retval SDFIO_ERR_IO The stream failed.
 * @retval SDFIO_ERR_FORMAT The data is not an SDF file or is truncated.
 * @retval SDFIO_ERR_UNSUPPORTED The file is ASCII and the library was built
 * without it.
 * @retval SDFIO_ERR_MEMORY An allocation failed.
 */
SDFIO_API sdfio_status sdfio_open_stream(sdfio_reader *reader,
                                         const sdfio_stream *stream);

/**
 * Returns the header of the opened file, or null if no file is open. The
 * pointer remains valid until the reader is freed.
 *
 * @param[in] reader The reader.
 */
SDFIO_API const sdfio_header *sdfio_get_header(const sdfio_reader *reader);

/**
 * Reads the next rows of the data area (record 2) as the file codes them.
 *
 * `dst` receives `rows` times `header->columns` values of `header->data_type`
 * in the byte order of the machine, in the order of the file. The bad value is
 * not changed. ASCII values are parsed into the data type of the file, so that
 * an ASCII file of binary32 values is rounded to single precision. Use
 * sdfio_decode_heights() to obtain heights in metres.
 *
 * The first row read is the first row stored in the file, the row with y = 0.
 *
 * After an error the reader rejects further reads.
 *
 * @param reader The reader.
 * @param[out] dst The buffer for the values.
 * @param rows The number of rows to read.
 * @retval SDFIO_ERR_ARGUMENT A pointer is null or `rows` exceeds the rows
 * remaining.
 * @retval SDFIO_ERR_STATE No file is open, or an earlier error ended the
 * reading.
 * @retval SDFIO_ERR_IO The stream failed.
 * @retval SDFIO_ERR_FORMAT The data area is damaged or truncated.
 */
SDFIO_API sdfio_status sdfio_read_data(sdfio_reader *reader, void *dst,
                                       size_t rows);

/**
 * Description of a buffer of heights of the caller: the type of its elements,
 * how a point that is not measured is marked, and how the matrix lies in
 * memory.
 *
 * The element (y, x) is at `data + y * row_stride + x * element_stride`. Both
 * strides can be negative. They must keep the elements apart: either the rows
 * or the columns must not overlap. This allows heights to be read or written in
 * place where they are not contiguous, for example the z member of an array of
 * points, one channel of an interleaved image or a column-major matrix.
 * sdfio_format_packed() describes a plain array.
 */
typedef struct sdfio_format
{
    /** Element type of the buffer. */
    sdfio_sample sample;
    /**
     * Marker for a point that is not measured. NaN selects no marker: such a
     * point is NaN. The marker is compared in the precision of the sample type,
     * so -9999.1 marks the points of a float buffer that hold the float nearest
     * to -9999.1.
     */
    double missing;
    /**
     * Distance in bytes between consecutive elements within a row. 0 selects
     * elements that follow each other directly.
     */
    ptrdiff_t element_stride;
    /**
     * Distance in bytes between consecutive rows. 0 selects rows that follow
     * each other, each holding the columns of the matrix at `element_stride`.
     * A column-major matrix needs an explicit value. sdfio_decode_heights() and
     * sdfio_encode_heights() convert one row and ignore it.
     */
    ptrdiff_t row_stride;
} sdfio_format;

/**
 * Returns the format of a plain array of heights: the elements follow each
 * other, the rows follow each other, and NaN marks a point that is not
 * measured.
 *
 * @param sample The sample type.
 */
static inline sdfio_format sdfio_format_packed(sdfio_sample sample)
{
    sdfio_format format = {sample, NAN, 0, 0};
    return format;
}

/**
 * Reads the remaining rows as heights in metres into a plain array of
 * `header->rows` rows of `header->columns` samples. A point that is not
 * measured is NaN. The row y = 0 is the first row.
 *
 * The function reads from the first row of the data area, so no rows may have
 * been read before. It is sdfio_read_heights_strided() with
 * sdfio_format_packed().
 *
 * @param reader The reader.
 * @param sample The sample type of the array.
 * @param[out] dst The array for the heights. sdfio_heights_size() gives its
 * size.
 * @retval SDFIO_ERR_ARGUMENT A pointer is null or the sample type is invalid.
 * @retval SDFIO_ERR_STATE No file is open, an earlier error ended the reading,
 * or rows were read already.
 * @retval SDFIO_ERR_RANGE A height does not fit the sample type.
 * @retval SDFIO_ERR_IO The stream failed.
 * @retval SDFIO_ERR_FORMAT The data area is damaged or truncated.
 */
SDFIO_API sdfio_status sdfio_read_heights(sdfio_reader *reader,
                                          sdfio_sample sample, void *dst);

/**
 * Reads the remaining rows as heights in metres into a buffer that lies in
 * memory as the format describes.
 *
 * The sample (y, x) is stored at `dst + y * format.row_stride + x *
 * format.element_stride`. The row y = 0 is the first row. A negative row
 * stride, with `dst` at the first sample of the row y = 0, which is the last
 * row in memory, stores the matrix upside down.
 *
 * The function reads from the first row of the data area, so no rows may have
 * been read before.
 *
 * @param reader The reader.
 * @param format The format of the buffer.
 * @param[out] dst The buffer for the heights.
 * @retval SDFIO_ERR_ARGUMENT A pointer is null or the strides are invalid.
 * @retval SDFIO_ERR_STATE No file is open, an earlier error ended the reading,
 * or rows were read already.
 * @retval SDFIO_ERR_RANGE A height does not fit the sample type.
 * @retval SDFIO_ERR_IO The stream failed.
 * @retval SDFIO_ERR_FORMAT The data area is damaged or truncated.
 */
SDFIO_API sdfio_status sdfio_read_heights_strided(sdfio_reader *reader,
                                                  sdfio_format format,
                                                  void *dst);

/**
 * Reads the trailer (record 3), the free text after the data area.
 *
 * Rows of the data area that have not been read are read and discarded.
 * `*trailer` receives a pointer to the text, or null if the file has no
 * trailer, and `*size` its length in bytes. The text is followed by a null
 * character, which the size does not count. It remains valid until the reader
 * is freed. A repeated call returns the same trailer.
 *
 * In memory every line end is LF, whichever of CRLF, CR or LF the file
 * contains, in ASCII and in binary files. The end of the last line is not part
 * of the text. In an ASCII file, white space around the trailer is removed.
 *
 * @param reader The reader.
 * @param[out] trailer The text of the trailer.
 * @param[out] size The length of the text in bytes.
 * @retval SDFIO_ERR_ARGUMENT A pointer is null.
 * @retval SDFIO_ERR_STATE No file is open, or an earlier error ended the
 * reading.
 * @retval SDFIO_ERR_IO The stream failed.
 * @retval SDFIO_ERR_FORMAT The file is damaged or truncated.
 * @retval SDFIO_ERR_MEMORY An allocation failed.
 */
SDFIO_API sdfio_status sdfio_read_trailer(sdfio_reader *reader,
                                          const char **trailer, size_t *size);

/**
 * Returns a description of the most recent failure of the reader, or an empty
 * string if none occurred. The result is never null.
 *
 * @param[in] reader The reader.
 */
SDFIO_API const char *sdfio_reader_error_message(const sdfio_reader *reader);

/* ---- Writing ------------------------------------------------------------ */

/** Opaque handle of a writer. */
typedef struct sdfio_writer sdfio_writer;

/**
 * Creates a writer. The writer writes nothing until a file is created. The
 * buffer is allocated by this function, the remaining memory when the file is
 * created. Release the writer with sdfio_writer_free().
 *
 * @param[in] config The configuration, or null for the defaults.
 * @return The writer, or null under the conditions of sdfio_reader_new().
 */
SDFIO_API sdfio_writer *sdfio_writer_new(const sdfio_config *config);

/**
 * Releases a writer. A file that sdfio_create_file() created and
 * sdfio_write_trailer() did not end is closed in its incomplete state. A null
 * pointer is ignored.
 *
 * @param writer The writer.
 */
SDFIO_API void sdfio_writer_free(sdfio_writer *writer);

/**
 * Starts a file that the header describes, including the numbers of columns
 * and rows. The header is written together with the first rows of the data
 * area. Both dates must be set; SDFIO_NO_TIME cannot be written.
 *
 * @param writer The writer.
 * @param[in] stream The stream; `write` is required. The structure is copied.
 * @param[in] header The description of the file.
 * @retval SDFIO_ERR_ARGUMENT A pointer is null, or the header cannot be
 * written: a data type that the revision does not permit, a scale that is not
 * positive, a date that is not set, or a count that the binary header cannot
 * hold.
 * @retval SDFIO_ERR_RANGE The time zone of the configuration cannot convert a
 * date. It is reported when the header is written, with the first rows.
 * @retval SDFIO_ERR_STATE The writer has created a file already.
 * @retval SDFIO_ERR_UNSUPPORTED The representation is ASCII and the library was
 * built without it.
 * @retval SDFIO_ERR_MEMORY An allocation failed.
 */
SDFIO_API sdfio_status sdfio_create_stream(sdfio_writer *writer,
                                           const sdfio_stream *stream,
                                           const sdfio_header *header);

/**
 * Writes the next rows of the data area.
 *
 * `src` holds `rows` times `header.columns` values of `header.data_type` in the
 * byte order of the machine, in the order of the file. The bad value is written
 * unchanged.
 *
 * The first row written is the first row stored in the file, the row with
 * y = 0.
 *
 * @param writer The writer.
 * @param[in] src The values to write.
 * @param rows The number of rows to write.
 * @retval SDFIO_ERR_ARGUMENT A pointer is null, `rows` exceeds the rows
 * remaining, or the data type is an integer type and the z-scale is not set.
 * @retval SDFIO_ERR_STATE No file is created, or an earlier error ended the
 * writing.
 * @retval SDFIO_ERR_IO The stream failed.
 */
SDFIO_API sdfio_status sdfio_write_data(sdfio_writer *writer, const void *src,
                                        size_t rows);

/**
 * Writes the whole data area from a plain array of heights in metres: the
 * `header->rows` rows of `header->columns` samples follow each other, and the
 * row y = 0 is the first. For an integer data type with SDFIO_AUTO_Z_SCALE the
 * z-scale is selected from the heights. NaN and infinity label points that are
 * not measured. Call sdfio_write_trailer() afterwards.
 *
 * It is sdfio_write_heights_strided() with sdfio_format_packed().
 *
 * @param writer The writer.
 * @param sample The sample type of the array.
 * @param[in] src The heights.
 * @retval SDFIO_ERR_ARGUMENT A pointer is null or the sample type is invalid.
 * @retval SDFIO_ERR_STATE No file is created, an earlier error ended the
 * writing, or rows were written already.
 * @retval SDFIO_ERR_RANGE A height does not fit the data type with the z-scale.
 * @retval SDFIO_ERR_BAD_VALUE A height would be read as a point that is not
 * measured.
 * @retval SDFIO_ERR_IO The stream failed.
 */
SDFIO_API sdfio_status sdfio_write_heights(sdfio_writer *writer,
                                           sdfio_sample sample,
                                           const void *src);

/**
 * Writes the whole data area from a matrix of heights in metres that lies in
 * memory as the format describes.
 *
 * The layout is that of sdfio_read_heights_strided(): the sample (y, x) is at
 * `src + y * format.row_stride + x * format.element_stride`, and the row y = 0
 * is the first row. For an integer data type with SDFIO_AUTO_Z_SCALE the
 * z-scale is selected from the heights. NaN, infinity and `format.missing`
 * label points that are not measured. Call sdfio_write_trailer() afterwards.
 *
 * @param writer The writer.
 * @param format The format of the buffer.
 * @param[in] src The heights.
 * @retval SDFIO_ERR_ARGUMENT A pointer is null or the strides are invalid.
 * @retval SDFIO_ERR_STATE No file is created, an earlier error ended the
 * writing, or rows were written already.
 * @retval SDFIO_ERR_RANGE A height does not fit the data type with the z-scale.
 * @retval SDFIO_ERR_BAD_VALUE A height would be read as a point that is not
 * measured.
 * @retval SDFIO_ERR_IO The stream failed.
 */
SDFIO_API sdfio_status sdfio_write_heights_strided(sdfio_writer *writer,
                                                   sdfio_format format,
                                                   const void *src);

/**
 * Writes the trailer (record 3) and ends the file. All rows of the data area
 * must have been written. A file that sdfio_create_file() created is closed.
 *
 * The text is not interpreted. ISO-2.0 expects lines of the form
 * `field name = value`, which sdfio_trailer_add_field() assembles. Every line
 * end of the text (LF, CR or CRLF) is written as CRLF, as the standard
 * specifies, in ASCII and in binary files, and the last line is terminated.
 * sdfio_read_trailer() returns the lines with LF. In an ASCII file a line that
 * consists only of `*` ends the record, so the text must not contain one;
 * lines assembled by sdfio_trailer_add_field() never do.
 *
 * @param writer The writer.
 * @param[in] trailer The text, or null for no trailer.
 * @param size The length of the text in bytes.
 * @retval SDFIO_ERR_ARGUMENT `trailer` is null and `size` is not 0.
 * @retval SDFIO_ERR_STATE No file is created, rows are missing, or an earlier
 * error ended the writing.
 * @retval SDFIO_ERR_IO The stream failed.
 */
SDFIO_API sdfio_status sdfio_write_trailer(sdfio_writer *writer,
                                           const char *trailer, size_t size);

/**
 * Returns a description of the most recent failure of the writer, or an empty
 * string if none occurred. The result is never null.
 *
 * @param[in] writer The writer.
 */
SDFIO_API const char *sdfio_writer_error_message(const sdfio_writer *writer);

/* ---- Heights ------------------------------------------------------------ */

/**
 * Converts values of the data area into heights in metres.
 *
 * `count` values of `header->data_type` are converted with `header->z_scale`.
 * The height i is stored at `heights + i * format.element_stride`. A point that
 * is not measured becomes `format.missing`. The function needs no handle, so it
 * works on data from any source.
 *
 * @param[in] header The header that describes the values.
 * @param format The format of the output buffer.
 * @param[in] data The values of the data area.
 * @param count The number of values.
 * @param[out] heights The output buffer.
 * @retval SDFIO_ERR_ARGUMENT A pointer is null or the description is invalid.
 * @retval SDFIO_ERR_RANGE A height does not fit the sample type.
 */
SDFIO_API sdfio_status sdfio_decode_heights(const sdfio_header *header,
                                            sdfio_format format,
                                            const void *data, size_t count,
                                            void *heights);

/**
 * Converts heights in metres into values of the data area. It is the inverse of
 * sdfio_decode_heights().
 *
 * The heights are divided by `header->z_scale`. NaN, infinity and
 * `format.missing` become the bad value. Integer results are rounded to the
 * nearest value.
 *
 * @param[in] header The header that describes the values.
 * @param format The format of the input buffer.
 * @param[in] heights The heights.
 * @param count The number of heights.
 * @param[out] data The values of the data area.
 * @retval SDFIO_ERR_ARGUMENT A pointer is null, the z-scale is not set, or the
 * description is invalid.
 * @retval SDFIO_ERR_RANGE A height does not fit the data type.
 * @retval SDFIO_ERR_BAD_VALUE A height would be read as a point that is not
 * measured.
 */
SDFIO_API sdfio_status sdfio_encode_heights(const sdfio_header *header,
                                            sdfio_format format,
                                            const void *heights, size_t count,
                                            void *data);

/**
 * Returns the z-scale that uses the range of an integer data type best for
 * heights between `lowest` and `highest` metres, or 1 for a floating point
 * data type.
 *
 * @param type The data type.
 * @param lowest The lowest height in metres.
 * @param highest The highest height in metres.
 */
SDFIO_API double sdfio_z_scale_for_range(sdfio_data_type type, double lowest,
                                         double highest);

/**
 * Determines the z-scale for a matrix of heights in metres, as
 * sdfio_write_heights() does: the range of the finite heights, excluding the
 * points that `format.missing` marks, is determined and passed to
 * sdfio_z_scale_for_range(). It is intended for sdfio_write_data() with an
 * integer data type.
 *
 * @param type The data type.
 * @param format The format of the buffer.
 * @param[in] heights The heights.
 * @param rows The number of rows.
 * @param columns The number of columns.
 * @param[out] z_scale The z-scale.
 * @retval SDFIO_ERR_ARGUMENT A pointer is null or the strides are invalid.
 */
SDFIO_API sdfio_status sdfio_suggest_z_scale(sdfio_data_type type,
                                             sdfio_format format,
                                             const void *heights, size_t rows,
                                             size_t columns, double *z_scale);

/* ---- Trailer ------------------------------------------------------------ */

/**
 * A trailer (record 3) under assembly, in memory of the caller.
 *
 * The trailer consists of lines of the form `field name = value`, which ISO-2.0
 * expects. In memory the lines end with LF; sdfio_write_trailer() writes them
 * as the standard specifies. Pass `text` and `size` to sdfio_write_trailer().
 * The library does not interpret a trailer; reading one returns the text.
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
    /** The lines so far, terminated by a null character. */
    char *text;
    /** The length of the text in bytes, not counting the null character. */
    size_t size;
    /** The capacity of the memory in bytes. */
    size_t capacity;
} sdfio_trailer;

/**
 * Initializes an empty trailer that stores its text in a block of memory.
 * Nothing is allocated.
 *
 * @param[out] trailer The trailer.
 * @param memory The memory for the text.
 * @param capacity The size of the memory in bytes.
 */
SDFIO_API void sdfio_trailer_init(sdfio_trailer *trailer, char *memory,
                                  size_t capacity);

/**
 * Appends the line `field = value`.
 *
 * The value is text. The caller formats numbers, for instance with snprintf()
 * in the C locale, because a locale with a decimal comma would produce `20,5`.
 * White space around a value is lost when the line is read.
 *
 * @param trailer The trailer.
 * @param[in] field The field name: letters, digits and `_`.
 * @param[in] value The value: one line of 7-bit ASCII.
 * @retval SDFIO_ERR_ARGUMENT The field name is empty or contains another
 * character, or the value is not one line of 7-bit ASCII.
 * @retval SDFIO_ERR_RANGE The line and the null character do not fit. The
 * trailer is unchanged.
 */
SDFIO_API sdfio_status sdfio_trailer_add_field(sdfio_trailer *trailer,
                                               const char *field,
                                               const char *value);

#ifdef __cplusplus
}
#endif

#endif /* SDFIO_SDFIO_H */
