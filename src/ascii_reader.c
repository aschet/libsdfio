/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file ascii_reader.c
 * @brief Reading of the ASCII format.
 */

#include "ascii.h"
#include "data_type.h"
#include "datetime.h"
#include "dynstring.h"
#include "fields.h"
#include "message.h"
#include "number_text.h"
#include "reader_format.h"
#include "revision.h"
#include "source.h"
#include "tagged.h"
#include "text.h"
#include <sdfio/sdfio.h>
#include <string.h>

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
                     bool *done)
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

/* The next white-space separated word, in `token`. */
static int next_token(sdfio_source *source, sdfio_string *token)
{
    int c = 0;
    for (;;)
    {
        c = sdfio_source_peek(source);
        if (c < 0)
        {
            return source->failed ? GOT_ERROR : GOT_NONE;
        }
        if (!sdfio_is_space((char)c))
        {
            break;
        }
        ++source->position;
    }
    sdfio_string_clear(token);
    while (source->position < source->end || sdfio_source_fill(source, 1) > 0)
    {
        const char *start = (const char *)source->buffer + source->position;
        const size_t available = source->end - source->position;
        size_t length = 0;
        while (length < available && !sdfio_is_space(start[length]))
        {
            ++length;
        }
        if (!sdfio_string_append(token, start, length))
        {
            source->no_memory = true;
            return GOT_ERROR;
        }
        source->position += length;
        if (length < available)
        {
            break;
        }
    }
    return source->failed ? GOT_ERROR : GOT_ONE;
}

/* A line that has only the character `*`, with white space around it. */
static bool is_terminator(const char *line, size_t size)
{
    sdfio_trim(&line, &size);
    return size == 1 && line[0] == terminator;
}

static const char *const version_numbers[] = {
    [SDFIO_ISO_1_0] = "aISO-1.0", [SDFIO_ISO_2_0] = "aISO-2.0"};

/* The revision whose version number the text is, according to the version
 * numbers that the format lists. */
static bool revision_of(const char *version_number, size_t size,
                        sdfio_revision *revision)
{
    for (size_t i = 0; i < sizeof version_numbers / sizeof version_numbers[0];
         ++i)
    {
        const char *listed = version_numbers[i];
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

/** The fields of the header, in the order of their ASCII field names. */
enum field
{
    MANUFACTURER_ID,
    CREATE_DATE,
    MODIFICATION_DATE,
    POINTS,
    PROFILES,
    X_SCALE,
    Y_SCALE,
    Z_SCALE,
    Z_RESOLUTION,
    COMPRESSION,
    DATA_TYPE,
    CHECK_TYPE,
    FIELD_COUNT
};

static const char *const field_names[FIELD_COUNT] = {
    [MANUFACTURER_ID] = SDFIO_ASCII_MANUFACTURER_ID,
    [CREATE_DATE] = SDFIO_ASCII_CREATE_DATE,
    [MODIFICATION_DATE] = SDFIO_ASCII_MODIFICATION_DATE,
    [POINTS] = SDFIO_ASCII_POINTS,
    [PROFILES] = SDFIO_ASCII_PROFILES,
    [X_SCALE] = SDFIO_ASCII_X_SCALE,
    [Y_SCALE] = SDFIO_ASCII_Y_SCALE,
    [Z_SCALE] = SDFIO_ASCII_Z_SCALE,
    [Z_RESOLUTION] = SDFIO_ASCII_Z_RESOLUTION,
    [COMPRESSION] = SDFIO_ASCII_COMPRESSION,
    [DATA_TYPE] = SDFIO_ASCII_DATA_TYPE,
    [CHECK_TYPE] = SDFIO_ASCII_CHECK_TYPE};

/** The values of the fields of a header, as the file gives them. */
typedef struct header_fields
{
    /** Whether the file has the field. */
    bool found[FIELD_COUNT];
    /** The manufacturer's ID, terminated. */
    char identifier[SDFIO_ID_SIZE + 1];
    /** The creation date and time. */
    char created[SDFIO_DATETIME_SIZE];
    /** The last modification date and time. */
    char modified[SDFIO_DATETIME_SIZE];
    /** The number of columns in the data matrix. */
    size_t columns;
    /** The number of rows in the data matrix. */
    size_t rows;
    /** The X-scale factor. */
    double x_scale;
    /** The Y-scale factor. */
    double y_scale;
    /** The Z-scale factor. */
    double z_scale;
    /** The Z-resolution. */
    double resolution;
    /** The compression type. */
    long long compression;
    /** The code of the data type. */
    long long data_type;
    /** The checksum type. */
    long long check_type;
} header_fields;

static bool same_name(const char *name, size_t size, const char *field_name)
{
    if (strlen(field_name) != size)
    {
        return false;
    }
    for (size_t i = 0; i < size; ++i)
    {
        if (sdfio_lower(name[i]) != sdfio_lower(field_name[i]))
        {
            return false;
        }
    }
    return true;
}

static bool parse_extent(const char *value, size_t size, size_t *out)
{
    long long number = 0;
    if (!sdfio_parse_integer(value, size, &number) || number < 0
        || (unsigned long long)number > SIZE_MAX)
    {
        return false;
    }
    *out = (size_t)number;
    return true;
}

static bool copy_datetime(char *out, const char *value, size_t size)
{
    if (size != SDFIO_DATETIME_SIZE)
    {
        return false;
    }
    memcpy(out, value, size);
    return true;
}

/* Converts the value of a field, which the file gave with white space around
 * it removed. */
static sdfio_status store_field(header_fields *fields, enum field field,
                                const char *value, size_t size,
                                const sdfio_decimal_point *decimal_point,
                                sdfio_message *message)
{
    bool valid = true;
    switch (field)
    {
    case MANUFACTURER_ID:
        if (size > SDFIO_ID_SIZE)
        {
            return sdfio_fail(message, SDFIO_ERR_FORMAT,
                              "The manufacturer's ID of the SDF header has "
                              "more than 10 characters");
        }
        memcpy(fields->identifier, value, size);
        fields->identifier[size] = '\0';
        break;
    case CREATE_DATE:
        valid = copy_datetime(fields->created, value, size);
        break;
    case MODIFICATION_DATE:
        valid = copy_datetime(fields->modified, value, size);
        break;
    case POINTS:
        valid = parse_extent(value, size, &fields->columns);
        break;
    case PROFILES:
        valid = parse_extent(value, size, &fields->rows);
        break;
    case X_SCALE:
        valid =
            sdfio_parse_number(decimal_point, value, size, &fields->x_scale);
        break;
    case Y_SCALE:
        valid =
            sdfio_parse_number(decimal_point, value, size, &fields->y_scale);
        break;
    case Z_SCALE:
        valid =
            sdfio_parse_number(decimal_point, value, size, &fields->z_scale);
        break;
    case Z_RESOLUTION:
        valid =
            sdfio_parse_number(decimal_point, value, size, &fields->resolution);
        break;
    case COMPRESSION:
        valid = sdfio_parse_integer(value, size, &fields->compression);
        break;
    case DATA_TYPE:
        valid = sdfio_parse_integer(value, size, &fields->data_type);
        break;
    case CHECK_TYPE:
        valid = sdfio_parse_integer(value, size, &fields->check_type);
        break;
    case FIELD_COUNT:
        break;
    }
    return valid ? SDFIO_OK
                 : sdfio_fail(message, SDFIO_ERR_FORMAT,
                              "A field of the SDF header has an invalid value");
}

/** What the reader of this format keeps for a file. */
struct reading
{
    /** The decimal point of the locale, measured when the file was opened. */
    sdfio_decimal_point decimal_point;
};

/* Reads the lines of the header up to the line that terminates the record. */
static sdfio_status read_fields(sdfio_source *source, sdfio_string *line,
                                const sdfio_decimal_point *decimal_point,
                                header_fields *fields, sdfio_message *message)
{
    bool ended = false;
    bool done = false;
    bool terminated = false;
    bool malformed = false;
    while (next_line(source, line, &ended, &done) == GOT_ONE)
    {
        const char *text = sdfio_string_text(line);
        if (is_terminator(text, line->size))
        {
            terminated = true;
            break;
        }
        const char *name = NULL;
        const char *value = NULL;
        size_t name_size = 0;
        size_t value_size = 0;
        if (!sdfio_tagged_split(text, line->size, &name, &name_size, &value,
                                &value_size))
        {
            const char *blank = text;
            size_t blank_size = line->size;
            sdfio_trim(&blank, &blank_size);
            malformed = malformed || blank_size > 0;
            continue;
        }
        for (enum field field = 0; field < FIELD_COUNT; ++field)
        {
            if (!same_name(name, name_size, field_names[field]))
            {
                continue;
            }
            const sdfio_status status = store_field(
                fields, field, value, value_size, decimal_point, message);
            if (status != SDFIO_OK)
            {
                return status;
            }
            fields->found[field] = true;
            break;
        }
    }
    if (!terminated)
    {
        return sdfio_source_failure(source, message,
                                    "SDF file must contain the header, the "
                                    "data area and the trailer");
    }
    if (malformed)
    {
        return sdfio_fail(message, SDFIO_ERR_FORMAT,
                          "Malformed SDF tagged field line in the header");
    }
    for (enum field field = 0; field < FIELD_COUNT; ++field)
    {
        if (!fields->found[field])
        {
            return sdfio_fail(message, SDFIO_ERR_FORMAT,
                              "A required field of the SDF header is missing");
        }
    }
    return SDFIO_OK;
}

static sdfio_status read_header(sdfio_source *source, sdfio_header *header,
                                void *state, const sdfio_time_zone *zone,
                                sdfio_message *message)
{
    struct reading *reading = state;
    reading->decimal_point = sdfio_decimal_point_now();
    sdfio_string line = {.allocator = source->allocator};
    sdfio_status status = SDFIO_OK;
    bool ended = false;
    bool done = false;
    const int got = next_line(source, &line, &ended, &done);
    if (got == GOT_ERROR)
    {
        status = sdfio_source_failure(
            source, message,
            "The first line of the file is not a version number");
    }
    else if (got != GOT_ONE || !ended)
    {
        status = sdfio_fail(message, SDFIO_ERR_FORMAT,
                            "SDF file is missing the header and the data area");
    }
    sdfio_revision revision = SDFIO_ISO_2_0;
    if (status == SDFIO_OK)
    {
        const char *version_number = sdfio_string_text(&line);
        size_t version_number_size = line.size;
        sdfio_trim(&version_number, &version_number_size);
        if (!revision_of(version_number, version_number_size, &revision))
        {
            status = sdfio_fail(message, SDFIO_ERR_FORMAT,
                                "Unknown or unsupported SDF version number");
        }
    }
    header_fields fields = {0};
    if (status == SDFIO_OK)
    {
        status = read_fields(source, &line, &reading->decimal_point, &fields,
                             message);
    }
    sdfio_string_free(&line);
    if (status != SDFIO_OK)
    {
        return status;
    }
    sdfio_data_type type = SDFIO_TYPE_FLOAT;
    if (!sdfio_data_type_from_code(fields.data_type, &type))
    {
        return sdfio_fail(message, SDFIO_ERR_FORMAT,
                          "Unknown SDF data type code");
    }
    if (!sdfio_revision_allows(revision, type))
    {
        return sdfio_fail(message, SDFIO_ERR_FORMAT,
                          "The data type is not valid for the SDF revision");
    }
    if (fields.compression != SDFIO_NONE)
    {
        return sdfio_fail(message, SDFIO_ERR_FORMAT,
                          "The compression type of an SDF file has to be 0");
    }
    if (fields.check_type != SDFIO_NONE)
    {
        return sdfio_fail(message, SDFIO_ERR_FORMAT,
                          "The checksum type of an SDF file has to be 0");
    }
    if (!sdfio_datetime_is_valid(fields.created, SDFIO_DATETIME_SIZE)
        || !sdfio_datetime_is_valid(fields.modified, SDFIO_DATETIME_SIZE))
    {
        return sdfio_fail(message, SDFIO_ERR_FORMAT,
                          "A date of the SDF header is not valid");
    }
    if (fields.columns > sdfio_max_columns_and_rows(revision)
        || fields.rows > sdfio_max_columns_and_rows(revision))
    {
        return sdfio_fail(message, SDFIO_ERR_FORMAT,
                          "The number of columns or rows in the SDF header is "
                          "out of range for the revision");
    }
    header->revision = revision;
    header->representation = SDFIO_ASCII;
    header->data_type = type;
    header->columns = fields.columns;
    header->rows = fields.rows;
    header->x_scale = fields.x_scale;
    header->y_scale = fields.y_scale;
    header->z_scale = fields.z_scale;
    header->z_resolution =
        fields.resolution > 0.0 && isfinite(fields.resolution)
            ? fields.resolution
            : SDFIO_NO_RESOLUTION;
    memcpy(header->manufacturer_id, fields.identifier,
           sizeof fields.identifier);
    header->creation_date = sdfio_datetime_parse(
        fields.created, SDFIO_DATETIME_SIZE, revision, zone);
    header->modification_date = sdfio_datetime_parse(
        fields.modified, SDFIO_DATETIME_SIZE, revision, zone);
    return SDFIO_OK;
}

/* ---- The data area ---------------------------------------------------------
 */

static sdfio_status ended_early(sdfio_source *source, sdfio_message *message)
{
    return sdfio_source_failure(source, message,
                                "SDF file must contain the header, the data "
                                "area and the trailer");
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
                               const sdfio_decimal_point *decimal_point,
                               void *dst, size_t index, sdfio_string *word,
                               sdfio_message *message)
{
    const int got = next_token(source, word);
    if (got == GOT_NONE)
    {
        return ended_early(source, message);
    }
    if (got == GOT_ERROR)
    {
        return sdfio_source_failure(source, message, "");
    }
    const char *token = sdfio_string_text(word);
    const size_t size = word->size;
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
        if (sdfio_parse_number(decimal_point, token, size, &value))
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
    sdfio_string word = {.allocator = source->allocator};
    sdfio_status status = SDFIO_OK;
    if (next_token(source, &word) != GOT_ONE)
    {
        status = ended_early(source, message);
    }
    else if (word.size != 1 || word.data[0] != terminator)
    {
        status = sdfio_fail(message, SDFIO_ERR_FORMAT,
                            "The data area has more values than the header "
                            "says");
    }
    else
    {
        skip_line(source);
    }
    sdfio_string_free(&word);
    return status;
}

static sdfio_status skip_data(sdfio_source *source, sdfio_message *message)
{
    sdfio_string word = {.allocator = source->allocator};
    sdfio_status status = SDFIO_OK;
    bool found = false;
    while (!found && next_token(source, &word) == GOT_ONE)
    {
        found = word.size == 1 && word.data[0] == terminator;
    }
    if (found)
    {
        skip_line(source);
    }
    else
    {
        status = ended_early(source, message);
    }
    sdfio_string_free(&word);
    return status;
}

static sdfio_status read_values(sdfio_source *source,
                                const sdfio_header *header, const void *state,
                                void *dst, size_t count, sdfio_message *message)
{
    const struct reading *reading = state;
    sdfio_string word = {.allocator = source->allocator};
    sdfio_status status = SDFIO_OK;
    for (size_t i = 0; i < count && status == SDFIO_OK; ++i)
    {
        status = read_value(source, header, &reading->decimal_point, dst, i,
                            &word, message);
    }
    sdfio_string_free(&word);
    return status;
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
    while ((got = next_line(source, &line, &ended, &done)) == GOT_ONE)
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

const sdfio_reader_format sdfio_ascii_reader_format = {
    .state_size = sizeof(struct reading),
    .prefix = 'a',
    .version_numbers = version_numbers,
    .version_number_count = sizeof version_numbers / sizeof version_numbers[0],
    .read_header = read_header,
    .read_values = read_values,
    .end_data = end_data,
    .read_trailer = read_trailer,
};
