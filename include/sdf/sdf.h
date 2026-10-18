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
 * The library is made of layers; use the highest one that does the job.
 *
 *  1. Handles and streams: a reader or a writer works on a file, on memory or
 *     on callbacks of your own, and holds no more of the data than a chunk.
 *  2. Stored values: sdf_read_rows() and sdf_write_rows() move the data area
 *     as the file stores it, in the data type of the file, row by row into and
 *     out of memory that you own.
 *  3. Heights: sdf_decode_heights() and sdf_encode_heights() turn stored
 *     values into metres and back. They need no handle, only the description
 *     of a file, so they work on data from any source.
 *  4. Images: sdf_read_heights() and sdf_write_heights() do layers 2 and 3
 *     for a whole matrix with any row stride.
 *
 * Threads: there is no global state. Handles are independent of each other, so
 * one thread can read a file while another writes one, also over the same
 * memory that is only read. A single handle is used by one thread at a time.
 * Do not change the locale or the time zone while a call is running.
 */

#ifndef SDF_SDF_H
#define SDF_SDF_H

#include <sdf/sdf_export.h>
#include <sdf/version.h>

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * The version of the library that is loaded, as "major.minor.patch". With a
 * shared library this can differ from SDF_VERSION_STRING, which describes
 * the headers that were compiled against.
 */
SDF_API const char *sdf_version(void);

/** What a call returns: SDF_OK, or the kind of the failure. */
typedef enum sdf_status
{
    SDF_OK = 0,
    /** A parameter is invalid, such as a null pointer or a row count. */
    SDF_ERR_ARGUMENT,
    /** The call does not fit the state of the handle, such as a second
       open. */
    SDF_ERR_STATE,
    /** The stream failed, or the file could not be opened. */
    SDF_ERR_IO,
    /** The data is not an SDF file, or is damaged or cut off. */
    SDF_ERR_FORMAT,
    /** A valid file with a feature that is not supported: compression. */
    SDF_ERR_UNSUPPORTED,
    /** A number does not fit the type that has to hold it. */
    SDF_ERR_RANGE,
    /** A height is the number that marks a point as not measured. */
    SDF_ERR_SENTINEL,
    SDF_ERR_MEMORY
} sdf_status;

/** A short fixed text for a status, never null. */
SDF_API const char *sdf_status_string(sdf_status status);

/** The revision of the standard that a file follows. */
typedef enum sdf_revision
{
    SDF_ISO_1_0,
    SDF_ISO_2_0,
    SDF_BCR_1_0
} sdf_revision;

typedef enum sdf_storage
{
    SDF_BINARY,
    SDF_ASCII
} sdf_storage;

/**
 * How a value is stored in the data area. The numbers are the codes of the
 * standard.
 */
typedef enum sdf_data_type
{
    SDF_DATA_UINT8 = 0,
    SDF_DATA_UINT16 = 1,
    SDF_DATA_UINT32 = 2,
    SDF_DATA_BINARY32 = 3,
    SDF_DATA_INT8 = 4,
    SDF_DATA_INT16 = 5,
    SDF_DATA_INT32 = 6,
    SDF_DATA_BINARY64 = 7
} sdf_data_type;

/** The element type of a buffer of the caller. */
typedef enum sdf_sample
{
    SDF_INT8,
    SDF_UINT8,
    SDF_INT16,
    SDF_UINT16,
    SDF_INT32,
    SDF_UINT32,
    SDF_FLOAT32,
    SDF_FLOAT64
} sdf_sample;

SDF_API const char *sdf_revision_name(sdf_revision revision);
SDF_API const char *sdf_data_type_name(sdf_data_type type);

/** The size in bytes of a stored value, and of a sample. */
SDF_API size_t sdf_data_type_size(sdf_data_type type);
SDF_API size_t sdf_sample_size(sdf_sample sample);

/** The sample type that holds the values of a data type as they are. */
SDF_API sdf_sample sdf_sample_of(sdf_data_type type);

/** Whether a revision allows a data type. */
SDF_API bool sdf_revision_allows(sdf_revision revision, sdf_data_type type);

enum
{
    /** The room for the manufacturer's identifier, with its terminating 0.
     */
    SDF_MANUFACTURER_CAPACITY = 64
};

/** A date and time that is not recorded. */
#define SDF_NO_TIME ((time_t)-1)

/** A z-scale that the writer chooses, and a resolution that is not known. */
#define SDF_AUTO_Z_SCALE 0.0
#define SDF_NO_RESOLUTION 0.0

/**
 * What the header of a file says. It is a plain value that you fill in to
 * write a file and that the reader fills in when it opens one. The size of
 * the grid belongs to it: the writer writes the file that this describes.
 */
typedef struct sdf_info
{
    sdf_revision revision;
    sdf_storage storage;
    sdf_data_type data_type;
    /** The points along x, the columns of the matrix. */
    size_t columns;
    /** The profiles along y, the rows of the matrix: 1 for a profile. */
    size_t rows;
    /** The sampling intervals in metres. */
    double x_scale;
    double y_scale;
    /**
     * The height of one stored count in metres. A reader always sets it. A
     * writer given SDF_AUTO_Z_SCALE takes 1 for the floating point types
     * and chooses the best for the heights in sdf_write_heights(); the
     * integer types need one for sdf_write_rows().
     */
    double z_scale;
    /**
     * The step of the instrument in metres, SDF_NO_RESOLUTION if it is not
     * known. It is not the z-scale: data can be rescaled after the
     * measurement.
     */
    double z_resolution;
    /**
     * When the data was made and last changed, in UTC to the minute, or
     * SDF_NO_TIME. The files of ISO-1.0 and BCR-1.0 hold the local clock of
     * the writer, and the library converts, so a date that is read back
     * equals the one that was written.
     */
    time_t create_date;
    time_t mod_date;
    /** 7-bit ASCII; a binary file keeps the first 10 characters. */
    char manufacturer_id[SDF_MANUFACTURER_CAPACITY];
} sdf_info;

/**
 * Sets a binary ISO-2.0 file of binary64 values with sampling intervals of
 * 1 m, both dates now and the manufacturer "libsdf". Set the fields you
 * need, and the size of the grid, afterwards.
 */
SDF_API void sdf_info_init(sdf_info *info);

/**
 * Whether the first row in a file of the revision is the one with the
 * largest y (BCR-1.0). The row functions work in the order of the file; the
 * image functions always put y = 0 first.
 */
SDF_API bool sdf_rows_reversed(sdf_revision revision);

/** The y of the n-th row of the file, counting from 0. */
SDF_API size_t sdf_row_y(const sdf_info *info, size_t file_row);

/** Whether a stored value marks a point that was not measured. */
SDF_API bool sdf_is_sentinel(sdf_data_type type, sdf_revision revision,
                             double stored);

/** The stored value that marks a point that was not measured. */
SDF_API double sdf_sentinel(sdf_data_type type, sdf_revision revision);

/* ---- Streams ------------------------------------------------------------ */

/** The result of a read callback that failed. */
#define SDF_STREAM_ERROR ((size_t)-1)

/**
 * Where the bytes of a file come from and go to, as callbacks of your own.
 * A reader uses `read` and `seek`, a writer `write`. Without `seek` the
 * bytes are handled in order and once, which is enough for a pipe.
 */
typedef struct sdf_stream
{
    void *user;
    /**
     * Reads up to `size` bytes and returns how many, 0 at the end of the
     * data, SDF_STREAM_ERROR on failure. It returns fewer than `size` bytes
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
} sdf_stream;

/** A stream over a C file, which stays yours and is not closed. */
SDF_API sdf_stream sdf_stdio_stream(FILE *file);

/* ---- Memory ------------------------------------------------------------- */

/**
 * Memory for the stored values of `rows` rows of the file that `info`
 * describes, zeroed, to read into with sdf_read_rows() or to hand to
 * sdf_decode_heights(). The size is checked for an overflow. Returns null if
 * the memory is short or `info` is not valid. Free it with sdf_free().
 */
SDF_API void *sdf_alloc_stored(const sdf_info *info, size_t rows);

/**
 * Memory for `rows` rows of the file as heights of the sample type, zeroed:
 * the matrix for sdf_read_heights() and sdf_write_heights() when `rows` is
 * `info->rows`. Returns null as sdf_alloc_stored() does.
 */
SDF_API void *sdf_alloc_heights(const sdf_info *info, sdf_sample sample,
                                size_t rows);

/**
 * Frees memory of sdf_alloc_stored() and sdf_alloc_heights(), in the runtime
 * that allocated it, which with a shared library on Windows is not that of the
 * program. Null is ignored.
 */
SDF_API void sdf_free(void *memory);

/* ---- Reading ------------------------------------------------------------ */

typedef struct sdf_reader sdf_reader;

/**
 * Makes a reader, which reads nothing yet; open a source next. A reader is
 * made first so that a failed open can still tell what went wrong.
 * Returns null if memory is short.
 */
SDF_API sdf_reader *sdf_reader_new(void);

/** Frees a reader and closes the file it opened. Null is ignored. */
SDF_API void sdf_reader_free(sdf_reader *reader);

/**
 * Opens a source and reads its header. A source that cannot seek can be
 * read once, from the front. The memory and the stream stay yours: keep
 * them alive and unchanged while the reader is used.
 *
 * @return SDF_ERR_IO if the file cannot be opened, SDF_ERR_FORMAT if it is
 * not an SDF file or is cut off, SDF_ERR_UNSUPPORTED for a compressed or
 * checksummed data area.
 */
SDF_API sdf_status sdf_open_file(sdf_reader *reader, const char *path);
SDF_API sdf_status sdf_open_memory(sdf_reader *reader, const void *data,
                                   size_t size);
SDF_API sdf_status sdf_open_stream(sdf_reader *reader,
                                   const sdf_stream *stream);

/** The header of the opened file, null before it is open. */
SDF_API const sdf_info *sdf_get_info(const sdf_reader *reader);

/**
 * Reads the next `rows` rows of the data area into `dst`, which holds
 * rows * columns values of info.data_type in the byte order of the machine,
 * as the file stores them: in the order of the file, with the number that
 * marks a point that is not measured unchanged. Text is parsed to the data
 * type of the file, so a binary32 text file is rounded to single precision.
 * To get heights, decode the values, see sdf_decode_heights().
 *
 * After an error the reader cannot go on; rewind it if it can seek.
 */
SDF_API sdf_status sdf_read_rows(sdf_reader *reader, void *dst, size_t rows);

/**
 * How a buffer of heights of the caller looks: its element type, and the
 * number that stands in it for a point that was not measured. NaN means
 * none: a point that was not measured is NaN, or 0 in an integer type,
 * which has no NaN. A marker is taken in the precision of the sample type,
 * so -9999.1 marks the points of a float buffer that hold the float
 * -9999.1.
 */
typedef struct sdf_format
{
    sdf_sample sample;
    double missing;
} sdf_format;

/** Heights in metres of the sample type, a point not measured being NaN. */
static inline sdf_format sdf_metres(sdf_sample sample)
{
    sdf_format format = {sample, NAN};
    return format;
}

/**
 * Reads the rest of the file as heights in metres into a matrix of `rows`
 * rows of `columns` samples whose rows are `row_stride` bytes apart. Row 0
 * is y = 0 in every revision. A stride that is negative, with `dst` at the
 * last row, turns the matrix upside down.
 *
 * Start from the first row: after other reads, rewind first.
 */
SDF_API sdf_status sdf_read_heights(sdf_reader *reader, sdf_format format,
                                    void *dst, ptrdiff_t row_stride);

/**
 * Reads the trailer, the free text after the data, and returns it with its
 * size, valid until the next call on the reader or its end. Nothing is
 * returned as a null pointer. The trailer is not null-terminated in a
 * binary file, but it has a 0 after its last byte. Rows that were not read
 * are passed over, which a stream that cannot seek has to do to get there.
 * A second call returns the same trailer.
 *
 * Text files give the line ends of the trailer as LF.
 */
SDF_API sdf_status sdf_read_trailer(sdf_reader *reader, const char **trailer,
                                    size_t *size);

/** Goes back to the first row, for a source that can seek. */
SDF_API sdf_status sdf_rewind(sdf_reader *reader);

/** The text of the last failure of the reader, empty if none. */
SDF_API const char *sdf_reader_message(const sdf_reader *reader);

/* ---- Writing ------------------------------------------------------------ */

typedef struct sdf_writer sdf_writer;

SDF_API sdf_writer *sdf_writer_new(void);

/**
 * Frees a writer. One that was not ended is abandoned: a file it created is
 * removed.
 */
SDF_API void sdf_writer_free(sdf_writer *writer);

/**
 * Starts a file that is described by `info` (the grid included). The header
 * is written with the first rows. A file is written beside its place and
 * moved there by sdf_write_end(), so an error never leaves a half-written
 * file.
 *
 * @return SDF_ERR_ARGUMENT if the description cannot be written: a data
 * type the revision does not have, a scale that is not positive, a count
 * the binary header cannot hold.
 */
SDF_API sdf_status sdf_create_file(sdf_writer *writer, const char *path,
                                   const sdf_info *info);
SDF_API sdf_status sdf_create_stream(sdf_writer *writer,
                                     const sdf_stream *stream,
                                     const sdf_info *info);

/**
 * Writes the next `rows` rows from `src`, which holds rows * columns values
 * of info.data_type in the byte order of the machine, in the order of the
 * file. The number that marks a point as not measured is written as it is.
 *
 * @return SDF_ERR_ARGUMENT if the data type is an integer type and the
 * z-scale is not set: the writer cannot know what a count means.
 */
SDF_API sdf_status sdf_write_rows(sdf_writer *writer, const void *src,
                                  size_t rows);

/**
 * Writes the whole file from a matrix of heights in metres, `rows` rows of
 * `columns` samples whose rows are `row_stride` bytes apart, row 0 being
 * y = 0. With SDF_AUTO_Z_SCALE and an integer data type the best z-scale
 * for the heights is chosen. NaN, infinity and `format.missing` mark points
 * that are not measured. Write the trailer with sdf_write_end() after it.
 *
 * @return SDF_ERR_RANGE if a height does not fit the data type with the
 * z-scale, SDF_ERR_SENTINEL if one would be read as a point not measured.
 */
SDF_API sdf_status sdf_write_heights(sdf_writer *writer, sdf_format format,
                                     const void *src, ptrdiff_t row_stride);

/**
 * Ends the file with its trailer, 7-bit ASCII text, or null for none; in
 * ISO-2.0 every line must be of the form `Name = Value`. All rows must have
 * been written. A file is moved to its place.
 */
SDF_API sdf_status sdf_write_end(sdf_writer *writer, const char *trailer,
                                 size_t size);

SDF_API const char *sdf_writer_message(const sdf_writer *writer);

/* ---- Heights ------------------------------------------------------------ */

/**
 * Turns `count` stored values (info->data_type) into heights in metres of
 * the format. A point that is not measured becomes format.missing. Needs no
 * handle, so it works on data from any source.
 *
 * @return SDF_ERR_RANGE if a height does not fit the sample type.
 */
SDF_API sdf_status sdf_decode_heights(const sdf_info *info, sdf_format format,
                                      const void *stored, size_t count,
                                      void *heights);

/**
 * The reverse: heights in metres of the format into stored values
 * (info->data_type), with info->z_scale. NaN, infinity and `format.missing`
 * become the number that marks a point not measured. Integers are rounded.
 *
 * @return SDF_ERR_ARGUMENT if the z-scale is not set, SDF_ERR_RANGE if a
 * height does not fit the data type, SDF_ERR_SENTINEL if one would be read
 * as a point not measured.
 */
SDF_API sdf_status sdf_encode_heights(const sdf_info *info, sdf_format format,
                                      const void *heights, size_t count,
                                      void *stored);

/**
 * The z-scale for heights between `lowest` and `highest` metres that uses
 * the range of an integer data type best; 1 for a floating point type.
 */
SDF_API double sdf_z_scale_for_range(sdf_data_type type, sdf_revision revision,
                                     double lowest, double highest);

/**
 * The best z-scale for a matrix of heights in metres of any sample type, as
 * sdf_write_heights() chooses it: the range of the finite heights, leaving
 * out the points that format.missing marks, is looked up and handed to
 * sdf_z_scale_for_range(). For the use of sdf_write_rows() with an integer
 * data type.
 */
SDF_API sdf_status sdf_suggest_z_scale(sdf_data_type type,
                                       sdf_revision revision, sdf_format format,
                                       const void *heights, size_t rows,
                                       size_t columns, ptrdiff_t row_stride,
                                       double *z_scale);

/* ---- Trailer ------------------------------------------------------------ */

/**
 * Looks for the line `Name = Value` in a trailer, with the name compared
 * without regard to the case; the last such line wins. On success `value`
 * points into the trailer, without the white space around it.
 */
SDF_API bool sdf_trailer_find(const char *trailer, size_t size,
                              const char *name, const char **value,
                              size_t *value_size);

#ifdef __cplusplus
}
#endif

#endif /* SDF_SDF_H */
