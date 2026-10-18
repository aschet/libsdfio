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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* A check at compile time that C99 has: a type with a negative size does not
 * compile. The name says what failed. */
#define SDFIO_REQUIRE(name, condition)                                         \
    typedef char sdfio_requirement_##name[(condition) ? 1 : -1]

/* What the format needs of the platform, checked here and nowhere else: bytes
 * of 8 bits, and IEEE 754 binary32 and binary64 for float and double. */
SDFIO_REQUIRE(bytes_of_8_bits, CHAR_BIT == 8);
SDFIO_REQUIRE(float_is_binary32,
              sizeof(float) == 4 && FLT_RADIX == 2 && FLT_MANT_DIG == 24);
SDFIO_REQUIRE(double_is_binary64, sizeof(double) == 8 && DBL_MANT_DIG == 53);

enum
{
    /* The magic that starts a file, as in "aISO-2.0", and where its parts
     * are. */
    SDFIO_MAGIC_SIZE = 8,
    SDFIO_MAGIC_NAME_AT = 1,
    SDFIO_MAGIC_NAME_SIZE = 3,
    SDFIO_MAGIC_DASH_AT = 4,
    SDFIO_MAGIC_MAJOR_AT = 5,
    SDFIO_MAGIC_DOT_AT = 6,
    SDFIO_MAGIC_MINOR_AT = 7,
    /* The fields of the header of a binary file. */
    SDFIO_ID_SIZE = 10,
    SDFIO_DATE_SIZE = 12,
    SDFIO_HEADER_DATES = 2,
    SDFIO_HEADER_COUNTS = 2,
    SDFIO_HEADER_DOUBLES = 4,
    SDFIO_HEADER_CODES = 3,
    SDFIO_BINARY_HEADER_MAX = 85,
    /* The code of "no compression" and "no checksum". */
    SDFIO_NONE = 0,
    /* How many values the functions without a handle convert at a time on the
     * stack, and the defaults and limits of a reader and a writer. */
    SDFIO_STACK_CHUNK = 128,
    SDFIO_DEFAULT_CHUNK = 1024,
    SDFIO_DEFAULT_BUFFER = 16384,
    SDFIO_MINIMUM_BUFFER = 64,
    SDFIO_TRAILER_PIECE = 256,
    SDFIO_LARGEST_SAMPLE = 8,
    /* The text of one value in an ASCII data area, with its separator. */
    SDFIO_TEXT_PER_VALUE = 32,
    /* The longest number or word of an ASCII data area. */
    SDFIO_TOKEN_CAPACITY = 64,
    SDFIO_NUMBER_CAPACITY = 64,
    SDFIO_POINT_CAPACITY = 8,
    /* A header of text that is longer than this is not one. */
    SDFIO_MAX_HEADER_TEXT = 1 << 20,
    SDFIO_MAX_TRAILER = 1 << 28,
    SDFIO_FIRST_YEAR = 1,
    SDFIO_LAST_YEAR = 9999,
    /* The first number that is not a character of ASCII. */
    SDFIO_FIRST_NON_ASCII = 0x80,
    /* The room a text starts with. */
    SDFIO_STRING_MINIMUM = 64
};

/* The digits that numbers are written with: after the point, and in the
 * exponent. */
enum
{
    SDFIO_FLOAT_DIGITS = 6,
    SDFIO_FLOAT_EXPONENT = 2,
    SDFIO_DOUBLE_DIGITS = 14,
    SDFIO_DOUBLE_EXPONENT = 3
};

/* The text of the last failure of a handle: a string of the library, which
 * stays valid, so a handle holds no memory for it. */
typedef struct sdfio_message
{
    const char *text;
} sdfio_message;

static inline sdfio_status sdfio_fail(sdfio_message *message, sdfio_status code,
                                      const char *text)
{
    if (message != NULL)
    {
        message->text = text;
    }
    return code;
}

/* A growable text, always 0-terminated once something is in it. */
typedef struct sdfio_string
{
    char *data;
    size_t size;
    size_t capacity;
    /* Where the memory comes from: null is malloc and free. */
    const sdfio_allocator *allocator;
} sdfio_string;

/* Memory through the allocator of a configuration, malloc and free if it has
 * none. */
void *sdfio_allocate(const sdfio_allocator *allocator, size_t size);
void sdfio_release(const sdfio_allocator *allocator, void *memory);
/* The allocator, the size of the buffer and of a chunk that a configuration
 * means; false if the allocator is not complete. */
bool sdfio_config_resolve(const sdfio_config *config,
                          sdfio_allocator *allocator, size_t *buffer,
                          size_t *chunk);

bool sdfio_string_append(sdfio_string *string, const char *text, size_t size);
void sdfio_string_clear(sdfio_string *string);
void sdfio_string_free(sdfio_string *string);
const char *sdfio_string_text(const sdfio_string *string);

/* ---- The tables of the types -------------------------------------------- */

/* The scalar types of the format and of the buffers: the sample, the data
 * type that stores it, its name, its C type, an unsigned type of the same
 * size, its lowest and highest value, and whether it is a floating point
 * type. */
#define SDFIO_SCALARS(X)                                                       \
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

bool sdfio_valid_sample(sdfio_sample sample);
bool sdfio_valid_data_type(sdfio_data_type type);
bool sdfio_is_floating(sdfio_data_type type);
double sdfio_lowest(sdfio_data_type type);
double sdfio_highest(sdfio_data_type type);
bool sdfio_revision_from_text(const char *text, size_t size,
                              sdfio_revision *revision);

/* The rules of the revisions. */
static inline bool sdfio_requires_utc(sdfio_revision revision)
{
    return revision == SDFIO_ISO_2_0;
}

static inline bool sdfio_bad_is_maximum(sdfio_revision revision)
{
    return revision == SDFIO_BCR_1_0;
}

static inline bool sdfio_allows_comments(sdfio_revision revision)
{
    return revision == SDFIO_BCR_1_0;
}

static inline bool sdfio_allows_symbols(sdfio_revision revision)
{
    return revision == SDFIO_BCR_1_0;
}

static inline bool sdfio_has_bad_token(sdfio_revision revision)
{
    return revision != SDFIO_BCR_1_0;
}

/* The largest number of columns or rows of a binary file. */
size_t sdfio_max_binary_count(sdfio_revision revision);
size_t sdfio_binary_header_size(sdfio_revision revision);

/* ---- Values -------------------------------------------------------------- */

/* A test for the number that marks a point as not measured. BCR-1.0 treats
 * every floating point number from a threshold up as one. */
typedef struct sdfio_bad_test
{
    bool at_least;
    double value;
} sdfio_bad_test;

sdfio_bad_test sdfio_bad_test_for(sdfio_data_type type,
                                  sdfio_revision revision);
bool sdfio_matches(sdfio_bad_test test, double coded);
/* The number a text file writes for a point not measured, in BCR-1.0. */
double sdfio_ascii_bad_value(sdfio_data_type type, sdfio_revision revision);

bool sdfio_host_is_little_endian(void);
/* Puts the values of `bytes` in file order (little endian) or back, in
 * place. */
void sdfio_fix_endianness(void *values, size_t count, size_t size);

/* Samples of a buffer of the caller to doubles and back; the integers are
 * rounded, a number that does not fit is SDFIO_ERR_RANGE, and NaN becomes 0 in
 * an integer type. */
void sdfio_load_samples(sdfio_sample sample, const void *src, size_t count,
                        double *out);
sdfio_status sdfio_store_samples(sdfio_sample sample, void *dst,
                                 const double *values, size_t count);
/* The same for samples that are `stride` bytes apart, which may be negative. */
void sdfio_load_strided(sdfio_sample sample, const void *src, ptrdiff_t stride,
                        size_t count, double *out);
sdfio_status sdfio_store_strided(sdfio_sample sample, void *dst,
                                 ptrdiff_t stride, const double *values,
                                 size_t count);

/* The distance in bytes between the samples of a row of the heights of the
 * caller: the element stride of the format, the size of a sample if it is 0. */
ptrdiff_t sdfio_element_stride(sdfio_format format);
/* Checks that the strides of a matrix of heights keep its elements apart:
 * rows that lie beside each other, or columns that do (a column-major matrix),
 * with samples that do not overlap. */
sdfio_status sdfio_check_layout(sdfio_format format, size_t rows,
                                size_t columns, ptrdiff_t row_stride,
                                sdfio_message *message);

/* The values of the data area in memory, in the byte order of the host, as
 * doubles and back. */
void sdfio_load_coded(sdfio_data_type type, const void *src, size_t count,
                      double *out);
sdfio_status sdfio_store_coded(sdfio_data_type type, void *dst,
                               const double *values, size_t count);

/* One value of the data area, put at an index. */
sdfio_status sdfio_put_real(sdfio_data_type type, void *dst, size_t index,
                            double value);
sdfio_status sdfio_put_integer(sdfio_data_type type, void *dst, size_t index,
                               long long value);
double sdfio_get_real(sdfio_data_type type, const void *src, size_t index);

/* ---- Numbers ------------------------------------------------------------- */

/* The character that the locale uses for the decimal point. Measured with a
 * print and not read from a table that the C library shares between threads. */
typedef struct sdfio_point
{
    char text[SDFIO_POINT_CAPACITY];
} sdfio_point;

sdfio_point sdfio_point_now(void);

bool sdfio_is_space(char c);
bool sdfio_is_digit(char c);
char sdfio_lower(char c);
bool sdfio_equals_nocase(const char *text, size_t size, const char *lower);
void sdfio_trim(const char **text, size_t *size);

bool sdfio_parse_number(const sdfio_point *point, const char *text, size_t size,
                        double *out);
bool sdfio_parse_integer(const char *text, size_t size, long long *out);
/* Writes a number as 1.5e+000 into `out`, and returns its length. */
size_t sdfio_format_scientific(const sdfio_point *point, char *out,
                               size_t capacity, double value, int digits,
                               int exponent_width);

/* ---- Dates ----------------------------------------------------------------
 */

time_t sdfio_now(void);
void sdfio_date_format(time_t time, sdfio_revision revision,
                       char out[SDFIO_DATE_SIZE]);
/* Whether `year` is a year the field can hold. */
bool sdfio_date_representable(time_t time);
time_t sdfio_date_parse(const char *text, size_t size, sdfio_revision revision);

/* ---- Tagged fields: the lines `Name = Value` ------------------------------
 */

/* A trailer from the file: every line end LF, and the end of the last line
 * dropped. */
void sdfio_lines_to_lf(sdfio_string *text);

/* The next line of a text, ended by LF, CR or CRLF. */
bool sdfio_next_line(const char *text, size_t size, size_t *position,
                     const char **line, size_t *line_size);
/* Whether every line is blank or of the form `Name = Value`. */
bool sdfio_tagged_valid(const char *text, size_t size);
/* The next line that is a field, with its name and its value trimmed. */
bool sdfio_tagged_next(const char *text, size_t size, size_t *position,
                       const char **name, size_t *name_size, const char **value,
                       size_t *value_size);
bool sdfio_tagged_find(const char *text, size_t size, const char *name,
                       const char **value, size_t *value_size);

/* ---- Sources and sinks --------------------------------------------------- */

typedef struct sdfio_source
{
    sdfio_stream io;
    unsigned char *buffer;
    size_t capacity;
    const sdfio_allocator *allocator;
    size_t position;
    size_t end;
    /* Where buffer[0] is in the stream, if the stream can seek. */
    int64_t base;
    bool seekable;
    bool at_end;
    bool failed;
    /* A line did not fit in memory. */
    bool no_memory;
    /* A CR was the last character of a line: an LF that follows is part of
     * its end. */
    bool skip_lf;
} sdfio_source;

/* The buffer of a source is made once, with the reader. */
bool sdfio_source_create(sdfio_source *source, size_t capacity,
                         const sdfio_allocator *allocator);
void sdfio_source_destroy(sdfio_source *source);
void sdfio_source_init(sdfio_source *source, const sdfio_stream *io);
/* Makes sure `want` bytes are buffered if the stream has them, and returns how
 * many are. */
size_t sdfio_source_fill(sdfio_source *source, size_t want);
int sdfio_source_peek(sdfio_source *source);
int sdfio_source_getc(sdfio_source *source);
size_t sdfio_source_read(sdfio_source *source, void *dst, size_t size);
int64_t sdfio_source_tell(const sdfio_source *source);
bool sdfio_source_seek(sdfio_source *source, int64_t position);
int64_t sdfio_source_size(sdfio_source *source);

typedef struct sdfio_sink
{
    sdfio_stream io;
    unsigned char *buffer;
    size_t capacity;
    const sdfio_allocator *allocator;
    size_t used;
    bool failed;
} sdfio_sink;

bool sdfio_sink_create(sdfio_sink *sink, size_t capacity,
                       const sdfio_allocator *allocator);
void sdfio_sink_destroy(sdfio_sink *sink);
void sdfio_sink_init(sdfio_sink *sink, const sdfio_stream *io);
bool sdfio_sink_write(sdfio_sink *sink, const void *data, size_t size);
bool sdfio_sink_flush(sdfio_sink *sink);

/* A trailer into the file: every line end of the text, LF, CR or CRLF, written
 * as CRLF as the standard has it, and the last line closed by one. */
bool sdfio_write_crlf_lines(sdfio_sink *sink, const char *text, size_t size);
bool sdfio_append_crlf_lines(sdfio_string *out, const char *text, size_t size);

/* What a file adds to a reader or a writer, made by sdfio_open_file and
 * sdfio_create_file: `close` ends a reader, or abandons the file of a writer
 * that was not ended, and `finish` of a writer ends the file and moves it to
 * its place. */
typedef struct sdfio_owner
{
    sdfio_status (*finish)(void *user, sdfio_message *message);
    void (*close)(void *user);
    void *user;
} sdfio_owner;

void sdfio_reader_own(sdfio_reader *reader, sdfio_owner owner);
void sdfio_writer_own(sdfio_writer *writer, sdfio_owner owner);
/* For the messages of the code that adds a file to a handle. */
sdfio_message *sdfio_reader_message(sdfio_reader *reader);
sdfio_message *sdfio_writer_message(sdfio_writer *writer);
bool sdfio_reader_is_open(const sdfio_reader *reader);
bool sdfio_writer_is_new(const sdfio_writer *writer);

/* ---- The formats ----------------------------------------------------------
 */

/* Reads the header at the start of the source, ASCII or binary, and leaves
 * the source at the data area. */
sdfio_status sdfio_read_header(sdfio_source *source, sdfio_header *header,
                               const sdfio_point *point,
                               sdfio_message *message);

/* The binary header: the revision of a magic, and the fields. */
sdfio_status sdfio_binary_revision(const unsigned char *magic,
                                   sdfio_revision *revision,
                                   sdfio_message *message);
sdfio_status sdfio_binary_parse(const unsigned char *bytes,
                                sdfio_header *header, sdfio_message *message);
sdfio_status sdfio_binary_header(const sdfio_header *header, unsigned char *out,
                                 size_t *size, sdfio_message *message);
/* The rest of the source, nothing if it is empty. */
sdfio_status sdfio_binary_trailer(sdfio_source *source, sdfio_string *out,
                                  bool *present, sdfio_message *message);

/* The text header, and the lines of the text data area. */
sdfio_status sdfio_ascii_parse_header(sdfio_source *source,
                                      sdfio_header *header,
                                      const sdfio_point *point,
                                      sdfio_message *message);
/* One value of the data type is parsed into `dst` at `index`. */
sdfio_status sdfio_ascii_value(sdfio_source *source, const sdfio_header *header,
                               const sdfio_point *point, void *dst,
                               size_t index, sdfio_message *message);
/* Reads the closing "*" of a data area that all its values were read from. */
sdfio_status sdfio_ascii_finish(sdfio_source *source,
                                const sdfio_header *header,
                                sdfio_message *message);
/* Passes the values up to the closing "*". */
sdfio_status sdfio_ascii_skip(sdfio_source *source, const sdfio_header *header,
                              sdfio_message *message);
/* Reads the trailer record of a text file; `present` is false if the file
 * ends with the data. */
sdfio_status sdfio_ascii_trailer(sdfio_source *source,
                                 const sdfio_header *header, sdfio_string *out,
                                 bool *present, sdfio_message *message);

sdfio_status sdfio_ascii_header(const sdfio_header *header,
                                const sdfio_point *point, sdfio_string *out,
                                sdfio_message *message);
/* The text of the value at `index`: a number, or BAD. Returns its length. */
size_t sdfio_ascii_format(const sdfio_header *header, const sdfio_point *point,
                          const void *coded, size_t index, char *out,
                          size_t capacity);
/* The closing "*" of the data area, the trailer record and its "*". */
sdfio_status sdfio_ascii_tail(const char *trailer, size_t size,
                              sdfio_string *out, sdfio_message *message);

/* The calendar time on the local clock of the machine, false if the machine
 * has none. */
bool sdfio_local_time(time_t time, struct tm *out);

/* Whether the build reads and writes text files. */
bool sdfio_has_text(void);

/* Checks what makes a file that cannot be written. */
sdfio_status sdfio_validate(const sdfio_header *header, sdfio_message *message);
/* Counts with an overflow check. */
bool sdfio_count(const sdfio_header *header, size_t *count);

#endif /* SDFIO_SRC_INTERNAL_H */
