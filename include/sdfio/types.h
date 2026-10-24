/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * The status codes, the enumerations, the allocator, the stream and the time
 * zone.
 */

#ifndef SDFIO_TYPES_H
#define SDFIO_TYPES_H

#include <sdfio/sdfio_export.h>

#include <float.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C"
{
#endif

/** Returns the version of the library that is loaded, as "major.minor.patch".
 */
SDFIO_API const char *sdfio_version(void);

/** Result of a function call. The values are part of the interface. */
typedef enum sdfio_status
{
    /** The call succeeded. */
    SDFIO_OK = 0,
    /**
     * An argument is invalid, for example a null pointer or a row count that
     * exceeds the rows remaining.
     */
    SDFIO_ERR_ARGUMENT = 1,
    /**
     * The call is not valid in the current state of the handle, for example
     * opening a reader a second time.
     */
    SDFIO_ERR_STATE = 2,
    /** The stream failed, or a file could not be opened. */
    SDFIO_ERR_IO = 3,
    /** The data is not an SDF file, is damaged or is truncated. */
    SDFIO_ERR_FORMAT = 4,
    /**
     * The file needs a format that has not been added to the handle.
     */
    SDFIO_ERR_UNSUPPORTED = 5,
    /** A number does not fit the type that has to hold it. */
    SDFIO_ERR_RANGE = 6,
    /**
     * A height equals the bad value of the data type, so it would be read as a
     * point that is not measured.
     */
    SDFIO_ERR_BAD_VALUE = 7,
    /** An allocation failed. */
    SDFIO_ERR_MEMORY = 8
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
    SDFIO_ISO_1_0 = 0,
    /** Revision ISO-2.0. */
    SDFIO_ISO_2_0 = 1
} sdfio_revision;

/** Format of a file. */
typedef enum sdfio_format
{
    /** Binary format. */
    SDFIO_BINARY = 0,
    /** ASCII format. */
    SDFIO_ASCII = 1
} sdfio_format;

/**
 * The type of the coded values of the data area of a file, and of the elements
 * of a buffer of heights.
 *
 * Each enumerator is named after the C type of the values. The enumerator
 * values are the data type codes of the standard, which calls SDFIO_TYPE_FLOAT
 * "binary32" and SDFIO_TYPE_DOUBLE "binary64".
 *
 * | Enumerator         | C type    | Revisions | A buffer of heights |
 * | ------------------ | --------- | --------- | ------------------- |
 * | SDFIO_TYPE_FLOAT   | `float`   | ISO-2.0   | yes                 |
 * | SDFIO_TYPE_DOUBLE  | `double`  | both      | yes                 |
 * | SDFIO_TYPE_INT8    | `int8_t`  | ISO-2.0   | no                  |
 * | SDFIO_TYPE_INT16   | `int16_t` | both      | no                  |
 * | SDFIO_TYPE_INT32   | `int32_t` | both      | no                  |
 *
 * A file of floating point values holds the heights in metres and has the
 * z-scale 1. A file of integers holds the coded height information, which the
 * z-scale multiplies to the heights in metres. sdfio_read_data() and
 * sdfio_write_data() transfer the values as they are in the file, in the C type
 * of the header's data type. sdfio_read_heights() and sdfio_write_heights()
 * transfer heights, in a buffer of `float` or `double`.
 */
typedef enum sdfio_data_type
{
    /** `float`: IEEE 754 binary32. */
    SDFIO_TYPE_FLOAT = 3,
    /** `int8_t`. */
    SDFIO_TYPE_INT8 = 4,
    /** `int16_t`. */
    SDFIO_TYPE_INT16 = 5,
    /** `int32_t`. */
    SDFIO_TYPE_INT32 = 6,
    /** `double`: IEEE 754 binary64. */
    SDFIO_TYPE_DOUBLE = 7
} sdfio_data_type;

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
     * Maximum length of the manufacturer's identifier in characters: the size
     * of the field of the standard.
     */
    SDFIO_MANUFACTURER_ID_MAX = 10
};

/** Value of a date that is not recorded. */
#define SDFIO_NO_TIME ((time_t)-1)

/**
 * Value of the z-scale of a header that requests the writer to select the
 * z-scale.
 */
#define SDFIO_AUTO_Z_SCALE 0.0

/**
 * Value of the z-resolution of a header for an unknown resolution. The standard
 * represents it as a negative number, -1 by default. A reader reports it for
 * every resolution that is not positive. A writer treats every value that is
 * not positive as unknown and writes -1.
 */
#define SDFIO_NO_RESOLUTION (-1.0)

/**
 * Memory management callbacks that replace malloc() and free(), for example
 * with a pool or a bump allocator.
 *
 * A header calls them when it is created. A handle calls them for all of its
 * memory, and only when it is created and when a file is created or opened.
 * Reading and writing allocate nothing, so a pool can be sized in advance.
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
 * Returns the allocator that uses malloc() and free().
 *
 * @return A pointer to a constant allocator, valid for the life of the program.
 */
SDFIO_API const sdfio_allocator *sdfio_default_allocator(void);

/**
 * The bad value of each data type: the number that labels a point that is not
 * measured or spurious. It is the minimum of the type, which the standard
 * excludes for a valid height.
 *
 * @{
 */
/** The bad value of SDFIO_TYPE_INT8. */
#define SDFIO_BAD_INT8 INT8_MIN
/** The bad value of SDFIO_TYPE_INT16. */
#define SDFIO_BAD_INT16 INT16_MIN
/** The bad value of SDFIO_TYPE_INT32. */
#define SDFIO_BAD_INT32 INT32_MIN
/** The bad value of SDFIO_TYPE_FLOAT. */
#define SDFIO_BAD_FLOAT (-FLT_MAX)
/** The bad value of SDFIO_TYPE_DOUBLE. */
#define SDFIO_BAD_DOUBLE (-DBL_MAX)
/** @} */

/** The size of the version number that starts every file, in bytes. */
#define SDFIO_VERSION_NUMBER_SIZE 8

/** Return value of a read callback that failed. */
#define SDFIO_STREAM_ERROR ((size_t)-1)

/**
 * Callbacks that provide or accept the bytes of a file.
 *
 * A reader uses `read`. A writer uses `write`. The bytes are processed once and
 * in order, as for a pipe. A program that does not use an initializer calls
 * sdfio_stream_init() before it assigns fields.
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
 * Initializes a stream: every field is null, so the stream has no callback.
 * The caller sets `user` and the callback that its handle uses.
 *
 * @param[out] stream The stream to initialize.
 */
SDFIO_API void sdfio_stream_init(sdfio_stream *stream);

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
 * callbacks are called by the thread that uses the handle. A program that does
 * not use an initializer calls sdfio_time_zone_init() before it assigns
 * fields.
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
 * Initializes a time zone: every field is null, so it performs no conversion.
 * The caller sets `user` and the callbacks.
 *
 * @param[out] time_zone The time zone to initialize.
 */
SDFIO_API void sdfio_time_zone_init(sdfio_time_zone *time_zone);

#ifdef __cplusplus
}
#endif

#endif /* SDFIO_TYPES_H */
