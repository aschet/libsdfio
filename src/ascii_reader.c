/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file ascii_reader.c
 * @brief Reader codec of the ASCII representation.
 */

#include "ascii.h"
#include "data_type.h"
#include "datetime.h"
#include "dynstring.h"
#include "fields.h"
#include "message.h"
#include "number_text.h"
#include "reader_codec.h"
#include "revision.h"
#include "source.h"
#include "tagged.h"
#include "text.h"
#include <sdfio/sdfio.h>
#include <string.h>

enum
{
    /** The size of the longest word of an ASCII data area. */
    SDFIO_TOKEN_CAPACITY = 64,
    /** The longest header of an ASCII file. A longer text is not one. */
    SDFIO_MAX_HEADER_TEXT = 1 << 20
};

static const char terminator = SDFIO_ASCII_TERMINATOR;

/* The result of reading a line or a token. */
enum
{
    GOT_NONE = 0,
    GOT_ONE = 1,
    GOT_ERROR = -1
};

/* The next line of the source, without its end. The last line of a text that
 * does not end with a line end comes back once with `ended` false. */
static int next_line(sdfio_source *source, sdfio_string *line, bool *ended,
                     bool *done, size_t limit)
{
    sdfio_string_clear(line);
    *ended = false;
    if (*done)
    {
        return GOT_NONE;
    }
    for (;;)
    {
        const int c = sdfio_source_getc(source);
        if (c < 0)
        {
            *done = true;
            return source->failed ? GOT_ERROR : GOT_ONE;
        }
        if (c == '\n')
        {
            /* The CR of a CRLF is not part of the line. */
            if (line->size > 0 && line->data[line->size - 1] == '\r')
            {
                --line->size;
                line->data[line->size] = '\0';
            }
            *ended = true;
            return GOT_ONE;
        }
        const char character = (char)c;
        if (line->size >= limit)
        {
            return GOT_ERROR;
        }
        if (!sdfio_string_append(line, &character, 1))
        {
            source->no_memory = true;
            return GOT_ERROR;
        }
    }
}

static void skip_line(sdfio_source *source)
{
    int c = 0;
    while ((c = sdfio_source_getc(source)) >= 0)
    {
        if (c == '\n')
        {
            return;
        }
    }
}

/* The next white-space separated word. */
static int next_token(sdfio_source *source, char *token, size_t capacity,
                      size_t *size)
{
    int c = 0;
    for (;;)
    {
        c = sdfio_source_peek(source);
        if (c < 0)
        {
            return source->failed ? GOT_ERROR : GOT_NONE;
        }
        if (sdfio_is_space((char)c))
        {
            ++source->position;
        }
        else
        {
            break;
        }
    }
    *size = 0;
    while ((c = sdfio_source_peek(source)) >= 0 && !sdfio_is_space((char)c))
    {
        if (*size + 1 >= capacity)
        {
            return GOT_ERROR;
        }
        token[(*size)++] = (char)c;
        ++source->position;
    }
    token[*size] = '\0';
    return GOT_ONE;
}

/* A line that has only the character `*`, with white space around it. */
static bool is_terminator(const char *line, size_t size)
{
    sdfio_trim(&line, &size);
    return size == 1 && line[0] == terminator;
}

/* The revision whose version number the text is, according to the version
 * numbers that the codec lists. */
static bool revision_of(const char *version_number, size_t size,
                        sdfio_revision *revision)
{
    for (size_t i = 0; i < SDFIO_REVISION_COUNT; ++i)
    {
        const char *listed = sdfio_ascii_reader_codec.version_numbers[i];
        if (strlen(listed) == size && memcmp(listed, version_number, size) == 0)
        {
            *revision = (sdfio_revision)i;
            return true;
        }
    }
    return false;
}

/* ---- The header -----------------------------------------------------------
 */

/** The value of a field in the text of a header, not copied. */
typedef struct field_text
{
    /** The first character of the value. */
    const char *text;
    /** The length of the value. */
    size_t size;
} field_text;

static sdfio_status field(const sdfio_string *header_text, const char *name,
                          field_text *out, sdfio_message *message)
{
    if (!sdfio_tagged_find(header_text->data != NULL ? header_text->data : "",
                           header_text->size, name, &out->text, &out->size))
    {
        return sdfio_fail(message, SDFIO_ERR_FORMAT,
                          "A required field of the SDF header is missing");
    }
    return SDFIO_OK;
}

static sdfio_status invalid_value(sdfio_message *message)
{
    return sdfio_fail(message, SDFIO_ERR_FORMAT,
                      "A field of the SDF header has an invalid value");
}

static sdfio_status header_real(const sdfio_string *header_text,
                                const char *name, const sdfio_point *point,
                                double *out, sdfio_message *message)
{
    field_text value;
    const sdfio_status status = field(header_text, name, &value, message);
    if (status != SDFIO_OK)
    {
        return status;
    }
    if (!sdfio_parse_number(point, value.text, value.size, out))
    {
        return invalid_value(message);
    }
    return SDFIO_OK;
}

static sdfio_status header_count(const sdfio_string *header_text,
                                 const char *name, size_t *out,
                                 sdfio_message *message)
{
    field_text value;
    const sdfio_status status = field(header_text, name, &value, message);
    if (status != SDFIO_OK)
    {
        return status;
    }
    long long number = 0;
    if (!sdfio_parse_integer(value.text, value.size, &number))
    {
        return invalid_value(message);
    }
    if (number < 0 || (unsigned long long)number > SIZE_MAX)
    {
        return sdfio_fail(
            message, SDFIO_ERR_FORMAT,
            "NumPoints or NumProfiles in the SDF header is out of "
            "range");
    }
    *out = (size_t)number;
    return SDFIO_OK;
}

static sdfio_status header_coded(const sdfio_string *header_text,
                                 const char *name, long long *out,
                                 sdfio_message *message)
{
    field_text value;
    const sdfio_status status = field(header_text, name, &value, message);
    if (status != SDFIO_OK)
    {
        return status;
    }
    return sdfio_parse_integer(value.text, value.size, out)
               ? SDFIO_OK
               : invalid_value(message);
}

/* The characters of an identifier without the white space around them. */
static size_t identifier_size(field_text identifier)
{
    const char *text = identifier.text;
    size_t size = identifier.size;
    sdfio_trim(&text, &size);
    return size;
}

static void copy_identifier(sdfio_header *header, const char *text, size_t size)
{
    sdfio_trim(&text, &size);
    if (size >= sizeof header->manufacturer_id)
    {
        size = sizeof header->manufacturer_id - 1;
    }
    memcpy(header->manufacturer_id, text, size);
    header->manufacturer_id[size] = '\0';
}

/** The codes of the header of an ASCII file that describe its data area. */
typedef struct codes
{
    /** The data type. */
    long long type;
    /** The compression. */
    long long compression;
    /** The checksum type. */
    long long check_type;
} codes;

static sdfio_status read_codes(const sdfio_string *header_text, codes *out,
                               sdfio_message *message)
{
    sdfio_status status =
        header_coded(header_text, SDFIO_ASCII_DATA_TYPE, &out->type, message);
    if (status != SDFIO_OK)
    {
        return status;
    }
    status = header_coded(header_text, SDFIO_ASCII_COMPRESSION,
                          &out->compression, message);
    if (status != SDFIO_OK)
    {
        return status;
    }
    return header_coded(header_text, SDFIO_ASCII_CHECK_TYPE, &out->check_type,
                        message);
}

/* The fields that are numbers or texts, into the description. */
static sdfio_status read_fields(const sdfio_string *header_text,
                                const sdfio_point *point, sdfio_header *header,
                                field_text *identifier, field_text *created,
                                field_text *modified, double *resolution,
                                sdfio_message *message)
{
    sdfio_status status =
        field(header_text, SDFIO_ASCII_MANUFACTURER_ID, identifier, message);
    if (status == SDFIO_OK)
    {
        status = field(header_text, SDFIO_ASCII_CREATE_DATE, created, message);
    }
    if (status == SDFIO_OK)
    {
        status = field(header_text, SDFIO_ASCII_MODIFICATION_DATE, modified,
                       message);
    }
    if (status == SDFIO_OK)
    {
        status = header_count(header_text, SDFIO_ASCII_POINTS, &header->columns,
                              message);
    }
    if (status == SDFIO_OK)
    {
        status = header_count(header_text, SDFIO_ASCII_PROFILES, &header->rows,
                              message);
    }
    if (status == SDFIO_OK)
    {
        status = header_real(header_text, SDFIO_ASCII_X_SCALE, point,
                             &header->x_scale, message);
    }
    if (status == SDFIO_OK)
    {
        status = header_real(header_text, SDFIO_ASCII_Y_SCALE, point,
                             &header->y_scale, message);
    }
    if (status == SDFIO_OK)
    {
        status = header_real(header_text, SDFIO_ASCII_Z_SCALE, point,
                             &header->z_scale, message);
    }
    if (status == SDFIO_OK)
    {
        status = header_real(header_text, SDFIO_ASCII_Z_RESOLUTION, point,
                             resolution, message);
    }
    return status;
}

/** What the reader of this codec keeps for a file. */
struct reading
{
    /** The decimal point of the locale, measured when the file was opened. */
    sdfio_point point;
};

static sdfio_status read_header(sdfio_source *source, sdfio_header *header,
                                void *state, const sdfio_time_zone *zone,
                                sdfio_message *message)
{
    struct reading *reading = state;
    reading->point = sdfio_point_now();
    const sdfio_point *point = &reading->point;
    sdfio_string line = {.allocator = source->allocator};
    sdfio_string header_text = {.allocator = source->allocator};
    sdfio_status status = SDFIO_OK;
    bool ended = false;
    bool done = false;
    const int got =
        next_line(source, &line, &ended, &done, SDFIO_MAX_HEADER_TEXT);
    if (got == GOT_ERROR)
    {
        status = sdfio_source_failure(
            source, message,
            "The first line of the file is not a version number");
        goto done;
    }
    if (got != GOT_ONE || !ended)
    {
        status = sdfio_fail(message, SDFIO_ERR_FORMAT,
                            "SDF file is missing the header and the data area");
        goto done;
    }

    const char *version_number_line = sdfio_string_text(&line);
    size_t version_number_line_size = line.size;
    sdfio_trim(&version_number_line, &version_number_line_size);
    sdfio_revision revision = SDFIO_ISO_2_0;
    if (!revision_of(version_number_line, version_number_line_size, &revision))
    {
        status = sdfio_fail(message, SDFIO_ERR_FORMAT,
                            "Unknown or unsupported SDF version number");
        goto done;
    }

    /* The records end with a line that has only a "*". */
    bool header_started = false;
    bool header_ended = false;
    while (next_line(source, &line, &ended, &done, SDFIO_MAX_HEADER_TEXT)
           == GOT_ONE)
    {
        const char *text = sdfio_string_text(&line);
        if (is_terminator(text, line.size))
        {
            header_ended = true;
            break;
        }
        if ((header_started && !sdfio_string_append(&header_text, "\n", 1))
            || !sdfio_string_append(&header_text, text, line.size))
        {
            status = sdfio_fail(message, SDFIO_ERR_MEMORY, "Out of memory");
            goto done;
        }
        if (header_text.size > SDFIO_MAX_HEADER_TEXT)
        {
            status = sdfio_fail(message, SDFIO_ERR_FORMAT,
                                "The SDF header is too long");
            goto done;
        }
        header_started = true;
    }
    if (!header_ended)
    {
        status =
            sdfio_source_failure(source, message,
                                 "SDF file must contain the header, the data "
                                 "area and the trailer");
        goto done;
    }
    if (!sdfio_tagged_valid(sdfio_string_text(&header_text), header_text.size))
    {
        status = sdfio_fail(message, SDFIO_ERR_FORMAT,
                            "Malformed SDF tagged field line in the header");
        goto done;
    }

    codes parsed_codes;
    status = read_codes(&header_text, &parsed_codes, message);
    if (status != SDFIO_OK)
    {
        goto done;
    }
    const long long type_code = parsed_codes.type;
    const long long compression = parsed_codes.compression;
    const long long check_type = parsed_codes.check_type;
    if (type_code < SDFIO_TYPE_FLOAT || type_code > SDFIO_TYPE_DOUBLE)
    {
        status =
            sdfio_fail(message, SDFIO_ERR_FORMAT, "Unknown SDF data type code");
        goto done;
    }
    const sdfio_data_type type = (sdfio_data_type)type_code;
    if (!sdfio_revision_allows(revision, type))
    {
        status = sdfio_fail(message, SDFIO_ERR_FORMAT,
                            "The data type is not valid for the SDF revision");
        goto done;
    }
    if (compression != SDFIO_NONE)
    {
        status = sdfio_fail(message, SDFIO_ERR_FORMAT,
                            "The compression type of an SDF file has to be 0");
        goto done;
    }
    if (check_type != SDFIO_NONE)
    {
        status = sdfio_fail(message, SDFIO_ERR_FORMAT,
                            "The checksum type of an SDF file has to be 0");
        goto done;
    }

    field_text identifier;
    field_text created;
    field_text modified;
    double resolution = 0.0;
    status = read_fields(&header_text, point, header, &identifier, &created,
                         &modified, &resolution, message);
    if (status != SDFIO_OK)
    {
        goto done;
    }
    if (!sdfio_datetime_is_valid(created.text, created.size)
        || !sdfio_datetime_is_valid(modified.text, modified.size))
    {
        status = sdfio_fail(message, SDFIO_ERR_FORMAT,
                            "A date of the SDF header is not valid");
        goto done;
    }
    if (header->columns > sdfio_max_columns_and_rows(revision)
        || header->rows > sdfio_max_columns_and_rows(revision))
    {
        status =
            sdfio_fail(message, SDFIO_ERR_FORMAT,
                       "NumPoints or NumProfiles in the SDF header is out of "
                       "range for the revision");
        goto done;
    }
    if (identifier_size(identifier) > SDFIO_ID_SIZE)
    {
        status = sdfio_fail(message, SDFIO_ERR_FORMAT,
                            "The manufacturer identifier of the SDF header has "
                            "more than 10 characters");
        goto done;
    }
    header->revision = revision;
    header->representation = SDFIO_ASCII;
    header->data_type = type;
    header->z_resolution = resolution > 0.0 && isfinite(resolution)
                               ? resolution
                               : SDFIO_NO_RESOLUTION;
    copy_identifier(header, identifier.text, identifier.size);
    header->creation_date =
        sdfio_datetime_parse(created.text, created.size, revision, zone);
    header->modification_date =
        sdfio_datetime_parse(modified.text, modified.size, revision, zone);

done:
    sdfio_string_free(&line);
    sdfio_string_free(&header_text);
    return status;
}

/* ---- The data area ---------------------------------------------------------
 */

static sdfio_status ended_early(sdfio_source *source, sdfio_message *message)
{
    return source->failed ? sdfio_fail(message, SDFIO_ERR_IO,
                                       "The stream could not be read")
                          : sdfio_fail(message, SDFIO_ERR_FORMAT,
                                       "SDF file must contain the header, the "
                                       "data area and the trailer");
}

/* ---- The values in memory -------------------------------------------------
 * A value of the data area is read and written in the type of the data type,
 * and the text of the file is converted to it here. */

/** A case of the storing of the bad value, for SDFIO_SCALARS(). */
#define STORE_BAD_CASE(data, type, low, high, floating)                        \
    case data:                                                                 \
    {                                                                          \
        const type bad = (type)(low);                                          \
        memcpy((unsigned char *)dst + index * sizeof bad, &bad, sizeof bad);   \
        return;                                                                \
    }

static void store_bad(sdfio_data_type type, void *dst, size_t index)
{
    switch (type)
    {
        SDFIO_SCALARS(STORE_BAD_CASE)
    }
}

/**
 * A case of the storing of an integer, for SDFIO_SCALARS(): false if the type
 * is not an integer type or the integer does not fit.
 */
#define STORE_INTEGER_CASE(data, type, low, high, floating)                    \
    case data:                                                                 \
    {                                                                          \
        if ((floating) || (double)value < (double)(low)                        \
            || (double)value > (double)(high))                                 \
        {                                                                      \
            return false;                                                      \
        }                                                                      \
        const type stored = (type)value;                                       \
        memcpy((unsigned char *)dst + index * sizeof stored, &stored,          \
               sizeof stored);                                                 \
        return true;                                                           \
    }

static bool store_integer(sdfio_data_type type, void *dst, size_t index,
                          long long value)
{
    switch (type)
    {
        SDFIO_SCALARS(STORE_INTEGER_CASE)
    }
    return false;
}

/**
 * A case of the storing of a real number, for SDFIO_SCALARS(): false if the
 * type is not a floating point type or a finite number does not fit.
 */
#define STORE_REAL_CASE(data, type, low, high, floating)                       \
    case data:                                                                 \
    {                                                                          \
        if (!(floating)                                                        \
            || (sizeof(type) < sizeof value && isfinite(value)                 \
                && fabs(value) > (double)(high)))                              \
        {                                                                      \
            return false;                                                      \
        }                                                                      \
        const type stored = (type)value;                                       \
        memcpy((unsigned char *)dst + index * sizeof stored, &stored,          \
               sizeof stored);                                                 \
        return true;                                                           \
    }

static bool store_real(sdfio_data_type type, void *dst, size_t index,
                       double value)
{
    switch (type)
    {
        SDFIO_SCALARS(STORE_REAL_CASE)
    }
    return false;
}

static sdfio_status read_value(sdfio_source *source, const sdfio_header *header,
                               const sdfio_point *point, void *dst,
                               size_t index, sdfio_message *message)
{
    char token[SDFIO_TOKEN_CAPACITY];
    size_t size = 0;
    const int got = next_token(source, token, sizeof token, &size);
    if (got == GOT_NONE)
    {
        return ended_early(source, message);
    }
    if (got == GOT_ERROR)
    {
        return source->failed ? sdfio_fail(message, SDFIO_ERR_IO,
                                           "The stream could not be read")
                              : sdfio_fail(message, SDFIO_ERR_FORMAT,
                                           "A data value is too long");
    }
    if (size == 1 && token[0] == terminator)
    {
        return sdfio_fail(message, SDFIO_ERR_FORMAT,
                          "The data area has fewer values than the header "
                          "says");
    }
    if (size == sizeof SDFIO_ASCII_BAD - 1
        && memcmp(token, SDFIO_ASCII_BAD, size) == 0)
    {
        store_bad(header->data_type, dst, index);
        return SDFIO_OK;
    }
    sdfio_status status = SDFIO_ERR_FORMAT;
    if (sdfio_is_floating(header->data_type))
    {
        double value = 0.0;
        if (sdfio_parse_number(point, token, size, &value))
        {
            status = store_real(header->data_type, dst, index, value)
                         ? SDFIO_OK
                         : SDFIO_ERR_FORMAT;
        }
    }
    else
    {
        /* An integer type does not take a fraction, which would be
         * truncated. */
        long long value = 0;
        if (sdfio_parse_integer(token, size, &value))
        {
            status = store_integer(header->data_type, dst, index, value)
                         ? SDFIO_OK
                         : SDFIO_ERR_FORMAT;
        }
    }
    if (status != SDFIO_OK)
    {
        return sdfio_fail(message, SDFIO_ERR_FORMAT,
                          "A data value is not a number of the data type");
    }
    return SDFIO_OK;
}

static sdfio_status finish_data(sdfio_source *source, sdfio_message *message)
{
    char token[SDFIO_TOKEN_CAPACITY];
    size_t size = 0;
    if (next_token(source, token, sizeof token, &size) != GOT_ONE)
    {
        return ended_early(source, message);
    }
    if (size != 1 || token[0] != terminator)
    {
        /* Count what is there, for the message. */
        return sdfio_fail(message, SDFIO_ERR_FORMAT,
                          "The data area has more values than the header "
                          "says");
    }
    skip_line(source);
    return SDFIO_OK;
}

static sdfio_status skip_data(sdfio_source *source, sdfio_message *message)
{
    char token[SDFIO_TOKEN_CAPACITY];
    size_t size = 0;
    while (next_token(source, token, sizeof token, &size) == GOT_ONE)
    {
        if (size == 1 && token[0] == terminator)
        {
            skip_line(source);
            return SDFIO_OK;
        }
    }
    return ended_early(source, message);
}

static sdfio_status read_values(sdfio_source *source,
                                const sdfio_header *header, const void *state,
                                void *dst, size_t count, sdfio_message *message)
{
    const struct reading *reading = state;
    for (size_t i = 0; i < count; ++i)
    {
        const sdfio_status status =
            read_value(source, header, &reading->point, dst, i, message);
        if (status != SDFIO_OK)
        {
            return status;
        }
    }
    return SDFIO_OK;
}

static sdfio_status end_data(sdfio_source *source, const sdfio_header *header,
                             bool complete, size_t values_left, void *scratch,
                             size_t scratch_size, sdfio_message *message)
{
    (void)header;
    (void)values_left;
    (void)scratch;
    (void)scratch_size;
    return complete ? finish_data(source, message) : skip_data(source, message);
}

static sdfio_status read_trailer(sdfio_source *source, sdfio_string *out,
                                 bool *present, sdfio_message *message)
{
    sdfio_string line = {.allocator = source->allocator};
    sdfio_string part = {.allocator = source->allocator};
    sdfio_status status = SDFIO_OK;
    bool ended = false;
    bool done = false;
    bool first = true;
    bool part_started = false;
    size_t parts = 0;
    *present = true;
    int got = GOT_ONE;
    while ((got = next_line(source, &line, &ended, &done, SIZE_MAX)) == GOT_ONE)
    {
        if (first && line.size == 0 && !ended)
        {
            *present = false; /* the text ends with the data */
            goto done;
        }
        first = false;
        const char *bare = sdfio_string_text(&line);
        const size_t bare_size = line.size;
        if (is_terminator(bare, bare_size))
        {
            /* The parts are joined with a "*". */
            if ((parts > 0 && !sdfio_string_append(out, &terminator, 1))
                || !sdfio_string_append(out, sdfio_string_text(&part),
                                        part.size))
            {
                status = sdfio_fail(message, SDFIO_ERR_MEMORY, "Out of memory");
                goto done;
            }
            ++parts;
            sdfio_string_clear(&part);
            part_started = false;
        }
        else
        {
            if ((part_started && !sdfio_string_append(&part, "\n", 1))
                || !sdfio_string_append(&part, sdfio_string_text(&line),
                                        line.size))
            {
                status = sdfio_fail(message, SDFIO_ERR_MEMORY, "Out of memory");
                goto done;
            }
            part_started = true;
        }
    }
    if (got == GOT_ERROR)
    {
        status = sdfio_source_failure(source, message,
                                      "The SDF trailer cannot be read");
        goto done;
    }
    /* The trailer is itself ended by its own "*": the empty part that follows
     * is not part of it. */
    if (part.size > 0
        && ((parts > 0 && !sdfio_string_append(out, &terminator, 1))
            || !sdfio_string_append(out, sdfio_string_text(&part), part.size)))
    {
        status = sdfio_fail(message, SDFIO_ERR_MEMORY, "Out of memory");
        goto done;
    }
    /* White space around the trailer is not part of it. */
    {
        const char *begin = sdfio_string_text(out);
        size_t size = out->size;
        sdfio_trim(&begin, &size);
        if (out->data != NULL)
        {
            memmove(out->data, begin, size);
            out->size = size;
            out->data[size] = '\0';
        }
    }
done:
    sdfio_string_free(&line);
    sdfio_string_free(&part);
    return status;
}

/* ---- Writing ----------------------------------------------------------------
 */

const sdfio_reader_codec sdfio_ascii_reader_codec = {
    .state_size = sizeof(struct reading),
    .prefix = 'a',
    .version_numbers = {"aISO-1.0", "aISO-2.0"},
    .read_header = read_header,
    .read_values = read_values,
    .end_data = end_data,
    .read_trailer = read_trailer,
};
