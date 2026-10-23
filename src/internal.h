/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file internal.h
 * @brief What the parts of the library share and the public header does not
 * offer.
 *
 * The functions of this file are not part of the interface. The library is
 * built with hidden symbols, so a program cannot call them. The tests of the
 * library link the objects and call them directly.
 */

#ifndef SDFIO_SRC_INTERNAL_H
#define SDFIO_SRC_INTERNAL_H

#include <sdfio/sdfio.h>

#include <float.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * A check at compile time, where the compiler has C11: without it the check is
 * not made. The expansion without C11 is a declaration that can repeat, so that
 * the use ends with a semicolon in both cases.
 *
 * @param condition The condition that has to hold.
 * @param message The text of the error if it does not.
 */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define SDFIO_STATIC_ASSERT(condition, message)                                \
    _Static_assert(condition, message)
#else
#define SDFIO_STATIC_ASSERT(condition, message) extern int sdfio_not_checked
#endif

/* What the format needs of the platform, checked here and nowhere else: bytes
 * of 8 bits, and IEEE 754 binary32 and binary64 for float and double. */
SDFIO_STATIC_ASSERT(CHAR_BIT == 8, "the format counts in 8-bit bytes");
SDFIO_STATIC_ASSERT(sizeof(float) == 4 && FLT_RADIX == 2 && FLT_MANT_DIG == 24,
                    "float has to be IEEE 754 binary32");
SDFIO_STATIC_ASSERT(sizeof(double) == 8 && DBL_MANT_DIG == 53,
                    "double has to be IEEE 754 binary64");

/** The fields of the file and the limits of the library. */
enum
{
    /** Where the name of the revision starts in the magic. */
    SDFIO_MAGIC_NAME_AT = 1,
    /** The size of the name of the revision in the magic. */
    SDFIO_MAGIC_NAME_SIZE = 3,
    /** Where the dash after the name is in the magic. */
    SDFIO_MAGIC_DASH_AT = 4,
    /** Where the major number of the revision is in the magic. */
    SDFIO_MAGIC_MAJOR_AT = 5,
    /** Where the dot after the major number is in the magic. */
    SDFIO_MAGIC_DOT_AT = 6,
    /** Where the minor number of the revision is in the magic. */
    SDFIO_MAGIC_MINOR_AT = 7,
    /** The size of the identifier in the header of a binary file. */
    SDFIO_ID_SIZE = 10,
    /** The size of a date field, DDMMYYYYhhmm. */
    SDFIO_DATE_SIZE = 12,
    /** The number of dates in the header of a binary file. */
    SDFIO_HEADER_DATES = 2,
    /** The number of counts (columns and rows) in the header of a binary file.
     */
    SDFIO_HEADER_COUNTS = 2,
    /** The number of binary64 numbers in the header of a binary file. */
    SDFIO_HEADER_DOUBLES = 4,
    /** The number of bytes of codes (compression, data type and checksum) in
     * the header of a binary file. */
    SDFIO_HEADER_CODES = 3,
    /** The size of the largest binary header, that of ISO-2.0. */
    SDFIO_BINARY_HEADER_MAX = 85,
    /** The code of "no compression" and "no checksum". */
    SDFIO_NONE = 0,
    /** The number of values a reader or a writer converts at a time, if the
     * configuration has none. */
    SDFIO_DEFAULT_CHUNK = 1024,
    /** The size of the buffer of a reader or a writer, if the configuration has
     * none. */
    SDFIO_DEFAULT_BUFFER = 16384,
    /** The smallest size of a buffer. */
    SDFIO_MINIMUM_BUFFER = 64,
    /** The number of bytes of a trailer that are read at a time. */
    SDFIO_TRAILER_PIECE = 256,
    /** The size of the largest sample, a binary64. */
    SDFIO_LARGEST_SAMPLE = 8,
    /** The room for the text of one value in an ASCII data area, with its
     * separator. */
    SDFIO_TEXT_PER_VALUE = 32,
    /** The size of the longest word of an ASCII data area. */
    SDFIO_TOKEN_CAPACITY = 64,
    /** The size of the longest number of an ASCII data area. */
    SDFIO_NUMBER_CAPACITY = 64,
    /** The size of the text of a decimal point. */
    SDFIO_POINT_CAPACITY = 8,
    /** The longest header of an ASCII file. A longer text is not one. */
    SDFIO_MAX_HEADER_TEXT = 1 << 20,
    /** The longest trailer. */
    SDFIO_MAX_TRAILER = 1 << 28,
    /** The first year that a date field can hold. */
    SDFIO_FIRST_YEAR = 1,
    /** The last year that a date field can hold. */
    SDFIO_LAST_YEAR = 9999,
    /** The first number that is not a character of ASCII. */
    SDFIO_FIRST_NON_ASCII = 0x80,
    /** The room that a string starts with. */
    SDFIO_STRING_MINIMUM = 64
};

/** The digits that numbers are written with, after the point and in the
 * exponent. */
enum
{
    /** The digits after the point of a binary32. */
    SDFIO_FLOAT_DIGITS = 6,
    /** The digits of the exponent of a binary32. */
    SDFIO_FLOAT_EXPONENT = 2,
    /** The digits after the point of a binary64. */
    SDFIO_DOUBLE_DIGITS = 14,
    /** The digits of the exponent of a binary64. */
    SDFIO_DOUBLE_EXPONENT = 3
};

/**
 * The text of the last failure of a handle.
 *
 * The text is a string of the library, which stays valid, so a handle holds no
 * memory for it.
 */
typedef struct sdfio_message
{
    /** The text, or null if there was no failure. */
    const char *text;
} sdfio_message;

/**
 * Records the text of a failure and returns its code.
 *
 * @param[out] message Receives the text. May be null.
 * @param code The status to return.
 * @param text The text, a string that stays valid.
 * @return `code`.
 */
static inline sdfio_status sdfio_fail(sdfio_message *message, sdfio_status code,
                                      const char *text)
{
    if (message != NULL)
    {
        message->text = text;
    }
    return code;
}

/** A growable text, always 0-terminated once something is in it. */
typedef struct sdfio_string
{
    /** The characters, or null while the string is empty. */
    char *data;
    /** The length without the terminator. */
    size_t size;
    /** The bytes that `data` holds. */
    size_t capacity;
    /** Where the memory comes from: null is malloc and free. */
    const sdfio_allocator *allocator;
} sdfio_string;

/**
 * Allocates memory through the allocator of a configuration.
 *
 * @param allocator The allocator. If it is null or has no allocate function,
 * malloc is used.
 * @param size The size in bytes.
 * @return The memory, or null if there is none.
 */
void *sdfio_allocate(const sdfio_allocator *allocator, size_t size);

/**
 * Releases memory from sdfio_allocate().
 *
 * @param allocator The allocator that made the memory.
 * @param memory The memory. May be null.
 */
void sdfio_release(const sdfio_allocator *allocator, void *memory);

/**
 * Resolves a configuration: the allocator, the size of the buffer, the size of
 * a chunk and the time zone that it means, with the defaults for what it does
 * not set.
 *
 * @param config The configuration. May be null.
 * @param[out] allocator The allocator.
 * @param[out] buffer The size of the buffer in bytes.
 * @param[out] chunk The number of values converted at a time.
 * @param[out] zone The time zone.
 * @return False if the allocator is not complete.
 */
bool sdfio_config_resolve(const sdfio_config *config,
                          sdfio_allocator *allocator, size_t *buffer,
                          size_t *chunk, sdfio_time_zone *zone);

/**
 * Appends text to a string.
 *
 * @param string The string.
 * @param text The characters to append.
 * @param size The number of characters.
 * @return False if there is no memory. The string is not changed then.
 */
bool sdfio_string_append(sdfio_string *string, const char *text, size_t size);

/**
 * Empties a string and keeps its memory.
 *
 * @param string The string.
 */
void sdfio_string_clear(sdfio_string *string);

/**
 * Releases the memory of a string.
 *
 * @param string The string.
 */
void sdfio_string_free(sdfio_string *string);

/**
 * Returns the characters of a string.
 *
 * @param string The string.
 * @return A 0-terminated text, also for an empty string.
 */
const char *sdfio_string_text(const sdfio_string *string);

/* ---- The tables of the types -------------------------------------------- */

/**
 * The data types of the format as a list for X-macros.
 *
 * For each type the list has the data type, its name, its C type, an unsigned
 * type of the same size, its lowest and highest value, and whether it is a
 * floating point type.
 *
 * @param X The macro that is called for each type.
 */
#define SDFIO_SCALARS(X)                                                       \
    X(SDFIO_DATA_INT8, "int8", int8_t, uint8_t, INT8_MIN, INT8_MAX, false)     \
    X(SDFIO_DATA_INT16, "int16", int16_t, uint16_t, INT16_MIN, INT16_MAX,      \
      false)                                                                   \
    X(SDFIO_DATA_INT32, "int32", int32_t, uint32_t, INT32_MIN, INT32_MAX,      \
      false)                                                                   \
    X(SDFIO_DATA_BINARY32, "binary32", float, uint32_t, -FLT_MAX, FLT_MAX,     \
      true)                                                                    \
    X(SDFIO_DATA_BINARY64, "binary64", double, uint64_t, -DBL_MAX, DBL_MAX,    \
      true)

/**
 * The checks of the sizes of the data types, as the file stores them: the type
 * of the program and the unsigned type that holds its bytes are of one size,
 * and no value is larger than the largest that a buffer is made for.
 */
#define SDFIO_CHECK_SCALAR(data, name, ctype, stored, low, high, floating)     \
    SDFIO_STATIC_ASSERT(sizeof(ctype) == sizeof(stored), name                  \
                        ": the type and its stored bytes differ in size");     \
    SDFIO_STATIC_ASSERT(sizeof(ctype) <= SDFIO_LARGEST_SAMPLE,                 \
                        name ": larger than the largest sample");
SDFIO_SCALARS(SDFIO_CHECK_SCALAR)
#undef SDFIO_CHECK_SCALAR
SDFIO_STATIC_ASSERT(
    sizeof(int8_t) == 1 && sizeof(int16_t) == 2 && sizeof(int32_t) == 4
        && sizeof(uint64_t) == 8,
    "the integer types of the file have the sizes of the format");
SDFIO_STATIC_ASSERT(SDFIO_LARGEST_SAMPLE == sizeof(double),
                    "the largest sample is a binary64");
SDFIO_STATIC_ASSERT(SDFIO_BINARY_HEADER_MAX
                        == SDFIO_MAGIC_SIZE + SDFIO_ID_SIZE
                               + SDFIO_HEADER_DATES * SDFIO_DATE_SIZE
                               + SDFIO_HEADER_COUNTS * sizeof(uint32_t)
                               + SDFIO_HEADER_DOUBLES * sizeof(double)
                               + SDFIO_HEADER_CODES,
                    "the largest binary header is the sum of its fields");

/**
 * Tells whether a value is a sample type.
 *
 * @param sample The value to test.
 */
bool sdfio_valid_sample(sdfio_sample sample);

/**
 * Tells whether a value is a data type.
 *
 * @param type The value to test.
 */
bool sdfio_valid_data_type(sdfio_data_type type);

/**
 * Tells whether a data type is a floating point type.
 *
 * @param type The data type. An invalid value is not one.
 */
bool sdfio_is_floating(sdfio_data_type type);

/**
 * Returns the spelling of a revision in the magic, for example "ISO-2.0", or an
 * empty string for an invalid value.
 *
 * @param revision The revision.
 */
const char *sdfio_revision_text(sdfio_revision revision);

/**
 * Finds the revision that a name stands for.
 *
 * @param text The name, as in the magic: "ISO-2.0".
 * @param size The length of the name.
 * @param[out] revision Receives the revision.
 * @return False if the name is not one of a revision.
 */
bool sdfio_revision_from_text(const char *text, size_t size,
                              sdfio_revision *revision);

/* ---- The rules of the revisions ------------------------------------------ */

/**
 * Tells whether the dates of a revision are in UTC.
 *
 * @param revision The revision.
 */
static inline bool sdfio_requires_utc(sdfio_revision revision)
{
    return revision == SDFIO_ISO_2_0;
}

/**
 * Returns the largest number of columns or rows of a revision: the header
 * holds them as 2 bytes in ISO-1.0 and as 4 bytes in ISO-2.0.
 *
 * @param revision The revision.
 */
size_t sdfio_max_count(sdfio_revision revision);

/**
 * Returns the size of the binary header of a revision.
 *
 * @param revision The revision.
 */
size_t sdfio_binary_header_size(sdfio_revision revision);

/* ---- Values -------------------------------------------------------------- */

/** Tells whether the machine stores the least significant byte first. */
bool sdfio_host_is_little_endian(void);

/**
 * Puts values in file order (little endian), or back, in place.
 *
 * @param[in,out] values The values.
 * @param count The number of values.
 * @param size The size of a value in bytes.
 */
void sdfio_fix_endianness(void *values, size_t count, size_t size);

/**
 * Returns the distance in bytes between the samples of a row of the heights of
 * the caller.
 *
 * @param format The format of the buffer.
 * @return The element stride of the format, or the size of a sample if that is
 * 0.
 */
ptrdiff_t sdfio_element_stride(sdfio_format format);

/**
 * Checks that the strides of a matrix of heights keep its elements apart.
 *
 * The rows lie beside each other, or the columns do (a column-major matrix),
 * and the samples do not overlap. A row stride of 0 becomes the stride of rows
 * that follow each other: the number of columns times the distance between the
 * elements.
 *
 * @param[in,out] format The format of the buffer, whose row stride is
 * resolved.
 * @param rows The number of rows.
 * @param columns The number of columns.
 * @param[out] message Receives the reason. May be null.
 * @retval SDFIO_ERR_ARGUMENT The strides do not keep the elements apart, or
 * the matrix is larger than memory.
 */
sdfio_status sdfio_check_layout(sdfio_format *format, size_t rows,
                                size_t columns, sdfio_message *message);

/* ---- Numbers ------------------------------------------------------------- */

/**
 * The character that the locale uses for the decimal point.
 *
 * It is measured with a print and not read from a table that the C library
 * shares between threads.
 */
typedef struct sdfio_point
{
    /** The characters, 0-terminated. */
    char text[SDFIO_POINT_CAPACITY];
} sdfio_point;

/** Measures the decimal point that the locale has now. */
sdfio_point sdfio_point_now(void);

/**
 * Tells whether a character is white space, whatever the locale: a space, a
 * tab, a line feed, and the carriage return of a CRLF.
 *
 * The function is inline because the parsers call it for every character.
 *
 * @param c The character.
 */
static inline bool sdfio_is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

/**
 * Tells whether a character is a digit from 0 to 9, whatever the locale.
 *
 * @param c The character.
 */
static inline bool sdfio_is_digit(char c)
{
    return c >= '0' && c <= '9';
}

/**
 * Returns a character in lower case, whatever the locale. Only the letters A to
 * Z change.
 *
 * @param c The character.
 */
static inline char sdfio_lower(char c)
{
    return c >= 'A' && c <= 'Z' ? (char)(c + ('a' - 'A')) : c;
}

/**
 * Compares a text with a word in lower case, without regard to case.
 *
 * @param text The text.
 * @param size The length of the text.
 * @param lower The word, in lower case and 0-terminated.
 */
bool sdfio_equals_nocase(const char *text, size_t size, const char *lower);

/**
 * Removes white space from both ends of a text.
 *
 * @param[in,out] text The start of the text.
 * @param[in,out] size The length of the text.
 */
void sdfio_trim(const char **text, size_t *size);

/**
 * Parses a number, whatever the locale.
 *
 * The text is a number as the format has it, or inf, infinity or nan, with an
 * optional sign. White space around it is ignored.
 *
 * @param point The decimal point of the locale.
 * @param text The text.
 * @param size The length of the text.
 * @param[out] out Receives the number.
 * @return False if the text is not a number.
 */
bool sdfio_parse_number(const sdfio_point *point, const char *text, size_t size,
                        double *out);

/**
 * Parses an integer in decimal digits, with an optional sign.
 *
 * @param text The text.
 * @param size The length of the text.
 * @param[out] out Receives the integer.
 * @return False if the text is not an integer or does not fit.
 */
bool sdfio_parse_integer(const char *text, size_t size, long long *out);

/**
 * Writes a number as 1.5e+000.
 *
 * @param point The decimal point of the locale.
 * @param[out] out Receives the text, without a terminator.
 * @param capacity The size of `out`.
 * @param value The number.
 * @param digits The digits after the point.
 * @param exponent_width The digits of the exponent.
 * @return The length of the text.
 */
size_t sdfio_format_scientific(const sdfio_point *point, char *out,
                               size_t capacity, double value, int digits,
                               int exponent_width);

/* ---- Dates ----------------------------------------------------------------
 */

/**
 * Returns the time now, in whole minutes.
 *
 * @return The time, or SDFIO_NO_TIME if the clock is not available.
 */
time_t sdfio_now(void);

/**
 * Formats the field of a date.
 *
 * A file of ISO-1.0 holds the local clock of the writer: the time
 * zone of the caller converts it, and without one the clock reading is the
 * date.
 *
 * @param time The time in UTC.
 * @param revision The revision of the file.
 * @param zone The time zone of the caller. May be null.
 * @param[out] out Receives the field, without a terminator.
 * @return False if the time zone cannot convert the date, or the date is not
 * one that the field can hold.
 */
bool sdfio_date_format(time_t time, sdfio_revision revision,
                       const sdfio_time_zone *zone, char out[SDFIO_DATE_SIZE]);

/**
 * Tells whether the year of a time is one that the field can hold.
 *
 * @param time The time.
 */
bool sdfio_date_representable(time_t time);

/**
 * Tells whether a text is a date as the standard has it: DDMMYYYYhhmm with the
 * ranges of the fields, and a day that the month has.
 *
 * @param text The field.
 * @param size The length of the field.
 */
bool sdfio_date_is_valid(const char *text, size_t size);

/**
 * Parses the field of a date.
 *
 * @param text The field.
 * @param size The length of the field.
 * @param revision The revision of the file.
 * @param zone The time zone of the caller. May be null.
 * @return The time in UTC, or SDFIO_NO_TIME if the field is not a valid date
 * (see sdfio_date_is_valid()) or the time zone cannot convert it.
 */
time_t sdfio_date_parse(const char *text, size_t size, sdfio_revision revision,
                        const sdfio_time_zone *zone);

/* ---- Tagged fields: the lines `Name = Value` ------------------------------
 */

/**
 * Converts a trailer from the file: every line end becomes LF, and the end of
 * the last line is dropped.
 *
 * @param[in,out] text The trailer.
 */
void sdfio_lines_to_lf(sdfio_string *text);

/**
 * Finds the next line of a text, ended by LF, CR or CRLF.
 *
 * @param text The text.
 * @param size The length of the text.
 * @param[in,out] position Where to start, and where the next line starts.
 * @param[out] line Receives the line without its end.
 * @param[out] line_size Receives the length of the line.
 * @return False if there is no line left.
 */
bool sdfio_next_line(const char *text, size_t size, size_t *position,
                     const char **line, size_t *line_size);

/**
 * Tells whether every line of a text is blank or of the form `Name = Value`.
 *
 * @param text The text.
 * @param size The length of the text.
 */
bool sdfio_tagged_valid(const char *text, size_t size);

/**
 * Finds the next line that is a field.
 *
 * @param text The text.
 * @param size The length of the text.
 * @param[in,out] position Where to start, and where the search goes on.
 * @param[out] name Receives the name, trimmed.
 * @param[out] name_size Receives the length of the name.
 * @param[out] value Receives the value, trimmed.
 * @param[out] value_size Receives the length of the value.
 * @return False if there is no field left.
 */
bool sdfio_tagged_next(const char *text, size_t size, size_t *position,
                       const char **name, size_t *name_size, const char **value,
                       size_t *value_size);

/**
 * Finds the field of a name.
 *
 * @param text The text.
 * @param size The length of the text.
 * @param name The name, 0-terminated.
 * @param[out] value Receives the value, trimmed.
 * @param[out] value_size Receives the length of the value.
 * @return False if there is no such field.
 */
bool sdfio_tagged_find(const char *text, size_t size, const char *name,
                       const char **value, size_t *value_size);

/* ---- Sources and sinks --------------------------------------------------- */

/**
 * A buffered stream for reading.
 *
 * The functions of the source work with the buffer, so a stream that gives a
 * byte at a time costs one call for the buffer.
 */
typedef struct sdfio_source
{
    /** The stream. */
    sdfio_stream io;
    /** The buffer. */
    unsigned char *buffer;
    /** The size of the buffer. */
    size_t capacity;
    /** The allocator of the buffer. */
    const sdfio_allocator *allocator;
    /** The index of the next byte in the buffer. */
    size_t position;
    /** The index after the last byte in the buffer. */
    size_t end;
    /** Whether the stream gave no more bytes. */
    bool at_end;
    /** Whether the stream failed. */
    bool failed;
    /** Whether a line did not fit in memory. */
    bool no_memory;
} sdfio_source;

/**
 * Makes the buffer of a source, once, with the reader.
 *
 * @param[out] source The source.
 * @param capacity The size of the buffer.
 * @param allocator The allocator. May be null.
 * @return False if there is no memory.
 */
bool sdfio_source_create(sdfio_source *source, size_t capacity,
                         const sdfio_allocator *allocator);

/**
 * Releases the buffer of a source.
 *
 * @param source The source.
 */
void sdfio_source_destroy(sdfio_source *source);

/**
 * Attaches a stream to a source, and starts at its current position.
 *
 * @param source The source, with a buffer.
 * @param io The stream.
 */
void sdfio_source_init(sdfio_source *source, const sdfio_stream *io);

/**
 * Makes sure that `want` bytes are buffered, if the stream has them.
 *
 * @param source The source.
 * @param want The number of bytes. A number above the size of the buffer is
 * taken as the size of the buffer.
 * @return The number of bytes that are buffered.
 */
size_t sdfio_source_fill(sdfio_source *source, size_t want);

/**
 * Returns the next byte without taking it.
 *
 * The function is inline because a text is read byte by byte.
 *
 * @param source The source.
 * @return The byte, or -1 at the end or after a failure.
 */
static inline int sdfio_source_peek(sdfio_source *source)
{
    if (source->position == source->end && sdfio_source_fill(source, 1) == 0)
    {
        return -1;
    }
    return source->buffer[source->position];
}

/**
 * Takes the next byte.
 *
 * @param source The source.
 * @return The byte, or -1 at the end or after a failure.
 */
static inline int sdfio_source_getc(sdfio_source *source)
{
    const int c = sdfio_source_peek(source);
    if (c >= 0)
    {
        ++source->position;
    }
    return c;
}

/**
 * Reads bytes.
 *
 * @param source The source.
 * @param[out] dst Receives the bytes.
 * @param size The number of bytes to read.
 * @return The number of bytes read, less than `size` at the end or after a
 * failure.
 */
size_t sdfio_source_read(sdfio_source *source, void *dst, size_t size);

/** A buffered stream for writing. */
typedef struct sdfio_sink
{
    /** The stream. */
    sdfio_stream io;
    /** The buffer. */
    unsigned char *buffer;
    /** The size of the buffer. */
    size_t capacity;
    /** The allocator of the buffer. */
    const sdfio_allocator *allocator;
    /** The number of bytes in the buffer. */
    size_t used;
    /** Whether the stream failed. */
    bool failed;
} sdfio_sink;

/**
 * Makes the buffer of a sink, once, with the writer.
 *
 * @param[out] sink The sink.
 * @param capacity The size of the buffer.
 * @param allocator The allocator. May be null.
 * @return False if there is no memory.
 */
bool sdfio_sink_create(sdfio_sink *sink, size_t capacity,
                       const sdfio_allocator *allocator);

/**
 * Releases the buffer of a sink.
 *
 * @param sink The sink.
 */
void sdfio_sink_destroy(sdfio_sink *sink);

/**
 * Attaches a stream to a sink.
 *
 * @param sink The sink, with a buffer.
 * @param io The stream.
 */
void sdfio_sink_init(sdfio_sink *sink, const sdfio_stream *io);

/**
 * Writes bytes.
 *
 * @param sink The sink.
 * @param data The bytes.
 * @param size The number of bytes.
 * @return False if the stream failed, now or before.
 */
bool sdfio_sink_write(sdfio_sink *sink, const void *data, size_t size);

/**
 * Writes the bytes in the buffer to the stream.
 *
 * @param sink The sink.
 * @return False if the stream failed, now or before.
 */
bool sdfio_sink_flush(sdfio_sink *sink);

/**
 * Writes a trailer into the file.
 *
 * Every line end of the text, LF, CR or CRLF, is written as CRLF as the
 * standard has it, and the last line is closed by one.
 *
 * @param sink The sink.
 * @param text The trailer.
 * @param size The length of the trailer.
 * @return False if the stream failed.
 */
bool sdfio_write_crlf_lines(sdfio_sink *sink, const char *text, size_t size);

/**
 * Appends a trailer to a string, with the line ends of
 * sdfio_write_crlf_lines().
 *
 * @param[in,out] out The string.
 * @param text The trailer.
 * @param size The length of the trailer.
 * @return False if there is no memory.
 */
bool sdfio_append_crlf_lines(sdfio_string *out, const char *text, size_t size);

/**
 * What a file adds to a reader or a writer, made by sdfio_open_file() and
 * sdfio_create_file().
 */
typedef struct sdfio_owner
{
    /** Ends the file of a writer, after which `close` is not called. Null for a
     * reader. */
    sdfio_status (*finish)(void *user, sdfio_message *message);
    /** Closes the file of a reader, or of a writer that was not ended. */
    void (*close)(void *user);
    /** The state that the functions get. */
    void *user;
} sdfio_owner;

/**
 * Gives a reader the file that it owns.
 *
 * @param reader The reader.
 * @param owner The functions of the file.
 */
void sdfio_reader_own(sdfio_reader *reader, sdfio_owner owner);

/**
 * Gives a writer the file that it owns.
 *
 * @param writer The writer.
 * @param owner The functions of the file.
 */
void sdfio_writer_own(sdfio_writer *writer, sdfio_owner owner);

/**
 * Returns the message of a reader, for the code that adds a file to it.
 *
 * @param reader The reader.
 */
sdfio_message *sdfio_reader_message(sdfio_reader *reader);

/**
 * Returns the message of a writer, for the code that adds a file to it.
 *
 * @param writer The writer.
 */
sdfio_message *sdfio_writer_message(sdfio_writer *writer);

/**
 * Tells whether a file is open in a reader.
 *
 * @param reader The reader.
 */
bool sdfio_reader_is_open(const sdfio_reader *reader);

/**
 * Tells whether a writer has not started a file.
 *
 * @param writer The writer.
 */
bool sdfio_writer_is_new(const sdfio_writer *writer);

/**
 * Checks that a reader can open a file.
 *
 * @param reader The reader.
 * @retval SDFIO_ERR_STATE The reader is open already.
 */
sdfio_status sdfio_file_check_reader(sdfio_reader *reader);

/**
 * Checks that a writer can create a file, before the file is created.
 *
 * @param writer The writer.
 * @param header The header of the file.
 * @retval SDFIO_ERR_STATE The writer has created a file already.
 * @retval SDFIO_ERR_ARGUMENT The header cannot be written.
 * @retval SDFIO_ERR_UNSUPPORTED The header is ASCII and the build has no
 * support for it.
 */
sdfio_status sdfio_file_check_writer(sdfio_writer *writer,
                                     const sdfio_header *header);

/**
 * Opens a file that is open already as the source of a reader. The reader
 * closes the file, also if this fails.
 *
 * @param reader The reader.
 * @param file The file, or null if it could not be opened.
 * @retval SDFIO_ERR_IO The file is null.
 */
sdfio_status sdfio_file_adopt_reader(sdfio_reader *reader, FILE *file);

/**
 * Creates the file of a writer from a file that is open already. The writer
 * closes the file, also if this fails.
 *
 * @param writer The writer.
 * @param file The file, or null if it could not be created.
 * @param header The header of the file.
 * @retval SDFIO_ERR_IO The file is null.
 */
sdfio_status sdfio_file_adopt_writer(sdfio_writer *writer, FILE *file,
                                     const sdfio_header *header);

/* ---- The formats ----------------------------------------------------------
 */

/**
 * Reads the header at the start of the source, ASCII or binary, and leaves the
 * source at the data area.
 *
 * @param source The source.
 * @param[out] header Receives the header.
 * @param point The decimal point of the locale.
 * @param zone The time zone of the caller. May be null.
 * @param[out] message Receives the reason of a failure.
 * @retval SDFIO_ERR_FORMAT The header is damaged.
 * @retval SDFIO_ERR_UNSUPPORTED The file is ASCII and the build has no support
 * for it.
 * @retval SDFIO_ERR_IO The stream failed.
 */
sdfio_status sdfio_read_header(sdfio_source *source, sdfio_header *header,
                               const sdfio_point *point,
                               const sdfio_time_zone *zone,
                               sdfio_message *message);

/**
 * Finds the revision of a magic.
 *
 * @param magic The magic, `SDFIO_MAGIC_SIZE` bytes of a binary file.
 * @param[out] revision Receives the revision.
 * @param[out] message Receives the reason of a failure.
 * @retval SDFIO_ERR_FORMAT The magic is unknown, or that of an ASCII file.
 */
sdfio_status sdfio_binary_revision(const unsigned char *magic,
                                   sdfio_revision *revision,
                                   sdfio_message *message);

/**
 * Parses the header of a binary file.
 *
 * @param bytes The header, as long as sdfio_binary_header_size() says.
 * @param[out] header Receives the header.
 * @param zone The time zone of the caller. May be null.
 * @param[out] message Receives the reason of a failure.
 * @retval SDFIO_ERR_FORMAT The header is damaged.
 */
sdfio_status sdfio_binary_parse(const unsigned char *bytes,
                                sdfio_header *header,
                                const sdfio_time_zone *zone,
                                sdfio_message *message);

/**
 * Writes the header of a binary file.
 *
 * @param header The header.
 * @param zone The time zone of the caller. May be null.
 * @param[out] out Receives the bytes, `SDFIO_BINARY_HEADER_MAX` at most.
 * @param[out] size Receives the number of bytes.
 * @param[out] message Receives the reason of a failure.
 * @retval SDFIO_ERR_RANGE A date cannot be written.
 */
sdfio_status sdfio_binary_header(const sdfio_header *header,
                                 const sdfio_time_zone *zone,
                                 unsigned char *out, size_t *size,
                                 sdfio_message *message);

/**
 * Reads the rest of the source as the trailer of a binary file.
 *
 * @param source The source, after the data area.
 * @param[in,out] out Receives the trailer.
 * @param[out] present False if the source is empty.
 * @param[out] message Receives the reason of a failure.
 * @retval SDFIO_ERR_FORMAT The trailer is too long.
 * @retval SDFIO_ERR_MEMORY There is no memory.
 * @retval SDFIO_ERR_IO The stream failed.
 */
sdfio_status sdfio_binary_trailer(sdfio_source *source, sdfio_string *out,
                                  bool *present, sdfio_message *message);

/**
 * Parses the header of an ASCII file.
 *
 * @param source The source, at the start of the file.
 * @param[out] header Receives the header.
 * @param point The decimal point of the locale.
 * @param zone The time zone of the caller. May be null.
 * @param[out] message Receives the reason of a failure.
 * @retval SDFIO_ERR_FORMAT The header is damaged.
 * @retval SDFIO_ERR_UNSUPPORTED The build has no support for ASCII files.
 */
sdfio_status sdfio_ascii_parse_header(sdfio_source *source,
                                      sdfio_header *header,
                                      const sdfio_point *point,
                                      const sdfio_time_zone *zone,
                                      sdfio_message *message);

/**
 * Parses the next value of an ASCII data area.
 *
 * @param source The source.
 * @param header The header of the file.
 * @param point The decimal point of the locale.
 * @param[out] dst The data area in memory, in the data type of the header.
 * @param index Where the value goes in `dst`.
 * @param[out] message Receives the reason of a failure.
 * @retval SDFIO_ERR_FORMAT The value is not a number that the data type can
 * hold, or the data area ends early.
 * @retval SDFIO_ERR_RANGE The number does not fit the data type.
 * @retval SDFIO_ERR_IO The stream failed.
 */
sdfio_status sdfio_ascii_value(sdfio_source *source, const sdfio_header *header,
                               const sdfio_point *point, void *dst,
                               size_t index, sdfio_message *message);

/**
 * Reads the closing "*" of a data area that all its values were read from.
 *
 * @param source The source.
 * @param[out] message Receives the reason of a failure.
 * @retval SDFIO_ERR_FORMAT The data area does not end here.
 */
sdfio_status sdfio_ascii_finish(sdfio_source *source, sdfio_message *message);

/**
 * Passes the values up to the closing "*".
 *
 * @param source The source.
 * @param[out] message Receives the reason of a failure.
 * @retval SDFIO_ERR_FORMAT The data area is damaged.
 */
sdfio_status sdfio_ascii_skip(sdfio_source *source, sdfio_message *message);

/**
 * Reads the trailer record of an ASCII file.
 *
 * @param source The source, after the data area.
 * @param[in,out] out Receives the trailer.
 * @param[out] present False if the file ends with the data.
 * @param[out] message Receives the reason of a failure.
 * @retval SDFIO_ERR_FORMAT The record is damaged.
 */
sdfio_status sdfio_ascii_trailer(sdfio_source *source, sdfio_string *out,
                                 bool *present, sdfio_message *message);

/**
 * Writes the header of an ASCII file as text.
 *
 * @param header The header.
 * @param point The decimal point of the locale.
 * @param zone The time zone of the caller. May be null.
 * @param[in,out] out Receives the text.
 * @param[out] message Receives the reason of a failure.
 * @retval SDFIO_ERR_RANGE A date cannot be written.
 * @retval SDFIO_ERR_MEMORY There is no memory.
 */
sdfio_status sdfio_ascii_header(const sdfio_header *header,
                                const sdfio_point *point,
                                const sdfio_time_zone *zone, sdfio_string *out,
                                sdfio_message *message);

/**
 * Writes the text of the value at an index: a number, or BAD.
 *
 * @param header The header of the file.
 * @param point The decimal point of the locale.
 * @param coded The data area in memory, in the data type of the header.
 * @param index The index of the value.
 * @param[out] out Receives the text, followed by the separator.
 * @param capacity The size of `out`, `SDFIO_TEXT_PER_VALUE` at least.
 * @return The length of the text.
 */
size_t sdfio_ascii_format(const sdfio_header *header, const sdfio_point *point,
                          const void *coded, size_t index, char *out,
                          size_t capacity);

/**
 * Writes the end of an ASCII file: the closing "*" of the data area, the
 * trailer record and its "*".
 *
 * @param trailer The trailer. May be null.
 * @param size The length of the trailer.
 * @param[in,out] out Receives the text.
 * @param[out] message Receives the reason of a failure.
 * @retval SDFIO_ERR_MEMORY There is no memory.
 */
sdfio_status sdfio_ascii_tail(const char *trailer, size_t size,
                              sdfio_string *out, sdfio_message *message);

/** Tells whether the build reads and writes ASCII files. */
bool sdfio_has_text(void);

/**
 * Checks what makes a file that cannot be written.
 *
 * @param header The header.
 * @param[out] message Receives the reason of a failure.
 * @retval SDFIO_ERR_ARGUMENT A field of the header is invalid.
 * @retval SDFIO_ERR_UNSUPPORTED The header is ASCII and the build has no
 * support for it.
 */
sdfio_status sdfio_validate(const sdfio_header *header, sdfio_message *message);

/**
 * Counts the values of the data area, with a check for overflow.
 *
 * @param header The header.
 * @param[out] count Receives columns times rows.
 * @return False if the count does not fit in size_t.
 */
bool sdfio_count(const sdfio_header *header, size_t *count);

#endif /* SDFIO_SRC_INTERNAL_H */
