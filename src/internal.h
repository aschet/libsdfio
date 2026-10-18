/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* What the parts of the library share and the public header does not offer. */

#ifndef SDFIO_SRC_INTERNAL_H
#define SDFIO_SRC_INTERNAL_H

#include <sdfio/sdfio.h>

#include <float.h>
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

/* What the format needs of the platform, checked here and nowhere else: bytes
 * of 8 bits, and IEEE 754 binary32 and binary64 for float and double. */
_Static_assert(CHAR_BIT == 8, "the format counts in 8-bit bytes");
_Static_assert(sizeof(float) == 4 && FLT_RADIX == 2 && FLT_MANT_DIG == 24,
               "float has to be IEEE 754 binary32");
_Static_assert(sizeof(double) == 8 && DBL_MANT_DIG == 53,
               "double has to be IEEE 754 binary64");

enum
{
    /* The magic that starts a file, as in "aISO-2.0", and where its parts
     * are. */
    SDFI_MAGIC_SIZE = 8,
    SDFI_MAGIC_NAME_AT = 1,
    SDFI_MAGIC_NAME_SIZE = 3,
    SDFI_MAGIC_DASH_AT = 4,
    SDFI_MAGIC_MAJOR_AT = 5,
    SDFI_MAGIC_DOT_AT = 6,
    SDFI_MAGIC_MINOR_AT = 7,
    /* The fields of the header of a binary file. */
    SDFI_ID_SIZE = 10,
    SDFI_DATE_SIZE = 12,
    SDFI_HEADER_DATES = 2,
    SDFI_HEADER_COUNTS = 2,
    SDFI_HEADER_DOUBLES = 4,
    SDFI_HEADER_CODES = 3,
    SDFI_BINARY_HEADER_MAX = 85,
    /* The code of "no compression" and "no checksum". */
    SDFI_NONE = 0,
    /* How many values are converted at a time, so that no memory is needed
     * for a row. */
    SDFI_CHUNK = 1024,
    SDFI_LARGEST_SAMPLE = 8,
    /* The text of one value in an ASCII data area, with its separator. */
    SDFI_TEXT_PER_VALUE = 32,
    SDFI_SOURCE_BUFFER = 16384,
    SDFI_SINK_BUFFER = 16384,
    /* The longest number or word of an ASCII data area. */
    SDFI_TOKEN_CAPACITY = 64,
    SDFI_MESSAGE_CAPACITY = 256,
    SDFI_NUMBER_CAPACITY = 64,
    SDFI_POINT_CAPACITY = 8,
    /* A header of text that is longer than this is not one. */
    SDFI_MAX_HEADER_TEXT = 1 << 20,
    SDFI_MAX_TRAILER = 1 << 28,
    SDFI_FIRST_YEAR = 1,
    SDFI_LAST_YEAR = 9999,
    /* The first number that is not a character of ASCII. */
    SDFI_FIRST_NON_ASCII = 0x80,
    /* The room a text starts with. */
    SDFI_STRING_MINIMUM = 64
};

/* What a file holds for a resolution that is not known. */
#define SDFI_FILE_NO_RESOLUTION (-1.0)

/* The digits that numbers are written with: after the point, and in the
 * exponent. */
enum
{
    SDFI_FLOAT_DIGITS = 6,
    SDFI_FLOAT_EXPONENT = 2,
    SDFI_DOUBLE_DIGITS = 14,
    SDFI_DOUBLE_EXPONENT = 3
};

#if defined(__GNUC__)
#define SDFI_PRINTF(text_index, first_index)                                   \
    __attribute__((format(printf, text_index, first_index)))
#else
#define SDFI_PRINTF(text_index, first_index)
#endif

/* The text of the last failure of a handle. */
typedef struct sdfi_message
{
    char text[SDFI_MESSAGE_CAPACITY];
} sdfi_message;

sdfio_status sdfi_fail(sdfi_message *message, sdfio_status code,
                       const char *format, ...) SDFI_PRINTF(3, 4);

/* A growable text, always 0-terminated once something is in it. */
typedef struct sdfi_string
{
    char *data;
    size_t size;
    size_t capacity;
} sdfi_string;

bool sdfi_string_append(sdfi_string *string, const char *text, size_t size);
void sdfi_string_clear(sdfi_string *string);
void sdfi_string_free(sdfi_string *string);
const char *sdfi_string_text(const sdfi_string *string);

/* ---- The tables of the types -------------------------------------------- */

/* The scalar types of the format and of the buffers: the sample, the data
 * type that stores it, its name, its C type, an unsigned type of the same
 * size, its lowest and highest value, and whether it is a floating point
 * type. */
#define SDFI_SCALARS(X)                                                        \
    X(SDFIO_INT8, SDFIO_DATA_INT8, "int8", int8_t, uint8_t, INT8_MIN,          \
      INT8_MAX, false)                                                         \
    X(SDFIO_UINT8, SDFIO_DATA_UINT8, "uint8", uint8_t, uint8_t, 0, UINT8_MAX,  \
      false)                                                                   \
    X(SDFIO_INT16, SDFIO_DATA_INT16, "int16", int16_t, uint16_t, INT16_MIN,    \
      INT16_MAX, false)                                                        \
    X(SDFIO_UINT16, SDFIO_DATA_UINT16, "uint16", uint16_t, uint16_t, 0,        \
      UINT16_MAX, false)                                                       \
    X(SDFIO_INT32, SDFIO_DATA_INT32, "int32", int32_t, uint32_t, INT32_MIN,    \
      INT32_MAX, false)                                                        \
    X(SDFIO_UINT32, SDFIO_DATA_UINT32, "uint32", uint32_t, uint32_t, 0,        \
      UINT32_MAX, false)                                                       \
    X(SDFIO_FLOAT32, SDFIO_DATA_BINARY32, "binary32", float, uint32_t,         \
      -FLT_MAX, FLT_MAX, true)                                                 \
    X(SDFIO_FLOAT64, SDFIO_DATA_BINARY64, "binary64", double, uint64_t,        \
      -DBL_MAX, DBL_MAX, true)

bool sdfi_valid_sample(sdfio_sample sample);
bool sdfi_valid_data_type(sdfio_data_type type);
bool sdfi_is_floating(sdfio_data_type type);
double sdfi_lowest(sdfio_data_type type);
double sdfi_highest(sdfio_data_type type);
bool sdfi_revision_from_text(const char *text, size_t size,
                             sdfio_revision *revision);

/* The rules of the revisions. */
static inline bool sdfi_requires_utc(sdfio_revision revision)
{
    return revision == SDFIO_ISO_2_0;
}

static inline bool sdfi_requires_tagged_trailer(sdfio_revision revision)
{
    return revision == SDFIO_ISO_2_0;
}

static inline bool sdfi_uses_max_sentinel(sdfio_revision revision)
{
    return revision == SDFIO_BCR_1_0;
}

static inline bool sdfi_allows_comments(sdfio_revision revision)
{
    return revision == SDFIO_BCR_1_0;
}

static inline bool sdfi_allows_symbols(sdfio_revision revision)
{
    return revision == SDFIO_BCR_1_0;
}

static inline bool sdfi_has_bad_token(sdfio_revision revision)
{
    return revision != SDFIO_BCR_1_0;
}

/* The largest number of columns or rows of a binary file. */
size_t sdfi_max_binary_count(sdfio_revision revision);
size_t sdfi_binary_header_size(sdfio_revision revision);

/* ---- Values -------------------------------------------------------------- */

/* A test for the number that marks a point as not measured. BCR-1.0 treats
 * every floating point number from a threshold up as one. */
typedef struct sdfi_sentinel_test
{
    bool at_least;
    double value;
} sdfi_sentinel_test;

sdfi_sentinel_test sdfi_sentinel_test_for(sdfio_data_type type,
                                          sdfio_revision revision);
bool sdfi_matches(sdfi_sentinel_test test, double coded);
/* The number a text file writes for a point not measured, in BCR-1.0. */
double sdfi_ascii_sentinel(sdfio_data_type type, sdfio_revision revision);

bool sdfi_host_is_little_endian(void);
/* Puts the values of `bytes` in file order (little endian) or back, in
 * place. */
void sdfi_fix_endianness(void *values, size_t count, size_t size);

/* Samples of a buffer of the caller to doubles and back; the integers are
 * rounded, a number that does not fit is SDFIO_ERR_RANGE, and NaN becomes 0 in
 * an integer type. */
void sdfi_load_samples(sdfio_sample sample, const void *src, size_t count,
                       double *out);
sdfio_status sdfi_store_samples(sdfio_sample sample, void *dst,
                                const double *values, size_t count);

/* The values of the data area in memory, in the byte order of the host, as
 * doubles and back. */
void sdfi_load_coded(sdfio_data_type type, const void *src, size_t count,
                     double *out);
sdfio_status sdfi_store_coded(sdfio_data_type type, void *dst,
                              const double *values, size_t count);

/* One value of the data area, put at an index. */
sdfio_status sdfi_put_real(sdfio_data_type type, void *dst, size_t index,
                           double value);
sdfio_status sdfi_put_integer(sdfio_data_type type, void *dst, size_t index,
                              long long value);
double sdfi_get_real(sdfio_data_type type, const void *src, size_t index);

/* ---- Numbers ------------------------------------------------------------- */

/* The character that the locale uses for the decimal point. Measured with a
 * print and not read from a table that the C library shares between threads. */
typedef struct sdfi_point
{
    char text[SDFI_POINT_CAPACITY];
} sdfi_point;

sdfi_point sdfi_point_now(void);

bool sdfi_is_space(char c);
bool sdfi_is_digit(char c);
char sdfi_lower(char c);
bool sdfi_equals_nocase(const char *text, size_t size, const char *lower);
void sdfi_trim(const char **text, size_t *size);

bool sdfi_parse_number(const sdfi_point *point, const char *text, size_t size,
                       double *out);
bool sdfi_parse_integer(const char *text, size_t size, long long *out);
/* Writes a number as 1.5e+000 into `out`, and returns its length. */
size_t sdfi_format_scientific(const sdfi_point *point, char *out,
                              size_t capacity, double value, int digits,
                              int exponent_width);

/* ---- Dates ----------------------------------------------------------------
 */

time_t sdfi_now(void);
void sdfi_date_format(time_t time, sdfio_revision revision,
                      char out[SDFI_DATE_SIZE]);
/* Whether `year` is a year the field can hold. */
bool sdfi_date_representable(time_t time);
time_t sdfi_date_parse(const char *text, size_t size, sdfio_revision revision);

/* ---- Tagged fields: the lines `Name = Value` ------------------------------
 */

/* The next line of a text, ended by LF, CR or CRLF. */
bool sdfi_next_line(const char *text, size_t size, size_t *position,
                    const char **line, size_t *line_size);
/* Whether every line is blank or of the form `Name = Value`. */
bool sdfi_tagged_valid(const char *text, size_t size);
bool sdfi_tagged_find(const char *text, size_t size, const char *name,
                      const char **value, size_t *value_size);

/* ---- Sources and sinks --------------------------------------------------- */

typedef struct sdfi_source
{
    sdfio_stream io;
    unsigned char buffer[SDFI_SOURCE_BUFFER];
    size_t position;
    size_t end;
    /* Where buffer[0] is in the stream, if the stream can seek. */
    int64_t base;
    bool seekable;
    bool at_end;
    bool failed;
    /* A CR was the last character of a line: an LF that follows is part of
     * its end. */
    bool skip_lf;
} sdfi_source;

void sdfi_source_init(sdfi_source *source, const sdfio_stream *io);
/* Makes sure `want` bytes are buffered if the stream has them, and returns how
 * many are. */
size_t sdfi_source_fill(sdfi_source *source, size_t want);
int sdfi_source_peek(sdfi_source *source);
int sdfi_source_getc(sdfi_source *source);
size_t sdfi_source_read(sdfi_source *source, void *dst, size_t size);
int64_t sdfi_source_tell(const sdfi_source *source);
bool sdfi_source_seek(sdfi_source *source, int64_t position);
int64_t sdfi_source_size(sdfi_source *source);

typedef struct sdfi_sink
{
    sdfio_stream io;
    unsigned char buffer[SDFI_SINK_BUFFER];
    size_t used;
    bool failed;
} sdfi_sink;

void sdfi_sink_init(sdfi_sink *sink, const sdfio_stream *io);
bool sdfi_sink_write(sdfi_sink *sink, const void *data, size_t size);
bool sdfi_sink_flush(sdfi_sink *sink);

/* ---- The formats ----------------------------------------------------------
 */

/* Reads the header at the start of the source, ASCII or binary, and leaves
 * the source at the data area. */
sdfio_status sdfi_read_header(sdfi_source *source, sdfio_header *header,
                              const sdfi_point *point, sdfi_message *message);

/* The binary header: the revision of a magic, and the fields. */
sdfio_status sdfi_binary_revision(const unsigned char *magic,
                                  sdfio_revision *revision,
                                  sdfi_message *message);
sdfio_status sdfi_binary_parse(const unsigned char *bytes, sdfio_header *header,
                               sdfi_message *message);
sdfio_status sdfi_binary_header(const sdfio_header *header, unsigned char *out,
                                size_t *size, sdfi_message *message);
/* The rest of the source, nothing if it is empty. */
sdfio_status sdfi_binary_trailer(sdfi_source *source, sdfi_string *out,
                                 bool *present, sdfi_message *message);

/* The text header, and the lines of the text data area. */
sdfio_status sdfi_ascii_parse_header(sdfi_source *source, sdfio_header *header,
                                     const sdfi_point *point,
                                     sdfi_message *message);
/* One value of the data type is parsed into `dst` at `index`; `position` is
 * its number in the data area, for messages. */
sdfio_status sdfi_ascii_value(sdfi_source *source, const sdfio_header *header,
                              const sdfi_point *point, void *dst, size_t index,
                              size_t position, sdfi_message *message);
/* Reads the closing "*" of a data area that all its values were read from. */
sdfio_status sdfi_ascii_finish(sdfi_source *source, const sdfio_header *header,
                               sdfi_message *message);
/* Passes the values up to the closing "*". */
sdfio_status sdfi_ascii_skip(sdfi_source *source, const sdfio_header *header,
                             sdfi_message *message);
/* Reads the trailer record of a text file; `present` is false if the file
 * ends with the data. */
sdfio_status sdfi_ascii_trailer(sdfi_source *source, const sdfio_header *header,
                                sdfi_string *out, bool *present,
                                sdfi_message *message);

sdfio_status sdfi_ascii_header(const sdfio_header *header,
                               const sdfi_point *point, sdfi_string *out,
                               sdfi_message *message);
/* The text of the value at `index`: a number, or BAD. Returns its length. */
size_t sdfi_ascii_format(const sdfio_header *header, const sdfi_point *point,
                         const void *coded, size_t index, char *out,
                         size_t capacity);
/* The closing "*" of the data area, the trailer record and its "*". */
sdfio_status sdfi_ascii_tail(const char *trailer, size_t size, sdfi_string *out,
                             sdfi_message *message);

/* Checks what makes a file that cannot be written. */
sdfio_status sdfi_validate(const sdfio_header *header, sdfi_message *message);
/* Counts with an overflow check. */
bool sdfi_count(const sdfio_header *header, size_t *count);

#endif /* SDFIO_SRC_INTERNAL_H */
