/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file ascii.c
 * @brief The text representation: the header lines, the values and the
 * trailer.
 */

#include "internal.h"

#include <stdio.h>

static const char crlf[] = "\r\n";
static const char terminator = '*';
static const char bad_token[] = "BAD";

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

/* Why a line or a token could not be read: the memory, the stream, or what
 * the text is, which is `format_text`. */
static sdfio_status read_failure(const sdfio_source *source,
                                 sdfio_message *message,
                                 const char *format_text)
{
    if (source->no_memory)
    {
        return sdfio_fail(message, SDFIO_ERR_MEMORY, "Out of memory");
    }
    if (source->failed)
    {
        return sdfio_fail(message, SDFIO_ERR_IO,
                          "The stream could not be read");
    }
    return sdfio_fail(message, SDFIO_ERR_FORMAT, format_text);
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

static bool matches_magic(const char *magic, size_t size)
{
    if (size != SDFIO_MAGIC_SIZE)
    {
        return false;
    }
    const char prefix = magic[0];
    if (prefix != 'a' && prefix != 'b')
    {
        return false;
    }
    for (size_t i = SDFIO_MAGIC_NAME_AT;
         i < SDFIO_MAGIC_NAME_AT + SDFIO_MAGIC_NAME_SIZE; ++i)
    {
        if (magic[i] < 'A' || magic[i] > 'Z')
        {
            return false;
        }
    }
    return magic[SDFIO_MAGIC_DASH_AT] == '-'
           && sdfio_is_digit(magic[SDFIO_MAGIC_MAJOR_AT])
           && magic[SDFIO_MAGIC_DOT_AT] == '.'
           && sdfio_is_digit(magic[SDFIO_MAGIC_MINOR_AT]);
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
        return sdfio_fail(message, SDFIO_ERR_FORMAT,
                          "A count in the SDF header is out of range");
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
        header_coded(header_text, "DataType", &out->type, message);
    if (status != SDFIO_OK)
    {
        return status;
    }
    status =
        header_coded(header_text, "Compression", &out->compression, message);
    if (status != SDFIO_OK)
    {
        return status;
    }
    return header_coded(header_text, "CheckType", &out->check_type, message);
}

/* The fields that are numbers or texts, into the description. */
static sdfio_status read_fields(const sdfio_string *header_text,
                                const sdfio_point *point, sdfio_header *header,
                                field_text *identifier, field_text *created,
                                field_text *modified, double *resolution,
                                sdfio_message *message)
{
    sdfio_status status = field(header_text, "ManufacID", identifier, message);
    if (status == SDFIO_OK)
    {
        status = field(header_text, "CreateDate", created, message);
    }
    if (status == SDFIO_OK)
    {
        status = field(header_text, "ModDate", modified, message);
    }
    if (status == SDFIO_OK)
    {
        status =
            header_count(header_text, "NumPoints", &header->columns, message);
    }
    if (status == SDFIO_OK)
    {
        status =
            header_count(header_text, "NumProfiles", &header->rows, message);
    }
    if (status == SDFIO_OK)
    {
        status = header_real(header_text, "Xscale", point, &header->x_scale,
                             message);
    }
    if (status == SDFIO_OK)
    {
        status = header_real(header_text, "Yscale", point, &header->y_scale,
                             message);
    }
    if (status == SDFIO_OK)
    {
        status = header_real(header_text, "Zscale", point, &header->z_scale,
                             message);
    }
    if (status == SDFIO_OK)
    {
        status =
            header_real(header_text, "Zresolution", point, resolution, message);
    }
    return status;
}

bool sdfio_has_text(void)
{
    return true;
}

sdfio_status sdfio_ascii_parse_header(sdfio_source *source,
                                      sdfio_header *header,
                                      const sdfio_point *point,
                                      const sdfio_time_zone *zone,
                                      sdfio_message *message)
{
    sdfio_string line = {.allocator = source->allocator};
    sdfio_string header_text = {.allocator = source->allocator};
    sdfio_status status = SDFIO_OK;
    bool ended = false;
    bool done = false;
    const int got =
        next_line(source, &line, &ended, &done, SDFIO_MAX_HEADER_TEXT);
    if (got == GOT_ERROR)
    {
        status = read_failure(source, message,
                              "The first line of the file is not a magic");
        goto done;
    }
    if (got != GOT_ONE || !ended)
    {
        status = sdfio_fail(message, SDFIO_ERR_FORMAT,
                            "SDF file is missing the header and the data area");
        goto done;
    }

    const char *magic_line = sdfio_string_text(&line);
    size_t magic_line_size = line.size;
    sdfio_trim(&magic_line, &magic_line_size);
    const char *magic = magic_line;
    size_t magic_size = magic_line_size;
    if (!matches_magic(magic, magic_size))
    {
        status = sdfio_fail(message, SDFIO_ERR_FORMAT,
                            "Not an ASCII SDF file: unexpected magic");
        goto done;
    }
    if (magic[0] != 'a')
    {
        status = sdfio_fail(message, SDFIO_ERR_FORMAT,
                            "Binary magic found while parsing an ASCII SDF "
                            "file");
        goto done;
    }
    sdfio_revision revision = SDFIO_ISO_2_0;
    if (!sdfio_revision_from_text(magic + 1, magic_size - 1, &revision))
    {
        status = sdfio_fail(message, SDFIO_ERR_FORMAT,
                            "Unknown or unsupported SDF revision in magic");
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
        status = read_failure(source, message,
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
    if (type_code < SDFIO_DATA_BINARY32 || type_code > SDFIO_DATA_BINARY64)
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
    header->revision = revision;
    header->representation = SDFIO_ASCII;
    header->data_type = type;
    header->z_resolution = resolution > 0.0 && isfinite(resolution)
                               ? resolution
                               : SDFIO_NO_RESOLUTION;
    copy_identifier(header, identifier.text, identifier.size);
    header->creation_date =
        sdfio_date_parse(created.text, created.size, revision, zone);
    header->modification_date =
        sdfio_date_parse(modified.text, modified.size, revision, zone);

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

sdfio_status sdfio_ascii_value(sdfio_source *source, const sdfio_header *header,
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
    if (size == sizeof bad_token - 1 && memcmp(token, bad_token, size) == 0)
    {
        return sdfio_put_real(header->data_type, dst, index,
                              sdfio_bad_value(header->data_type));
    }
    sdfio_status status = SDFIO_ERR_FORMAT;
    if (sdfio_is_floating(header->data_type))
    {
        double value = 0.0;
        if (sdfio_parse_number(point, token, size, &value))
        {
            status = sdfio_put_real(header->data_type, dst, index, value);
        }
    }
    else
    {
        /* An integer type does not take a fraction, which would be
         * truncated. */
        long long value = 0;
        if (sdfio_parse_integer(token, size, &value))
        {
            status = sdfio_put_integer(header->data_type, dst, index, value);
        }
    }
    if (status != SDFIO_OK)
    {
        return sdfio_fail(message, SDFIO_ERR_FORMAT,
                          "A data value is not a number of the data type");
    }
    return SDFIO_OK;
}

sdfio_status sdfio_ascii_finish(sdfio_source *source, sdfio_message *message)
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

sdfio_status sdfio_ascii_skip(sdfio_source *source, sdfio_message *message)
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

sdfio_status sdfio_ascii_trailer(sdfio_source *source, sdfio_string *out,
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
    while ((got = next_line(source, &line, &ended, &done, SDFIO_MAX_TRAILER))
           == GOT_ONE)
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
        if (out->size + part.size > SDFIO_MAX_TRAILER)
        {
            status = sdfio_fail(message, SDFIO_ERR_FORMAT,
                                "The SDF trailer is too long");
            goto done;
        }
    }
    if (got == GOT_ERROR)
    {
        status = read_failure(source, message, "The SDF trailer is too long");
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

static bool append_text(sdfio_string *out, const char *text)
{
    return sdfio_string_append(out, text, strlen(text));
}

static bool append_field(sdfio_string *out, const char *name, const char *value)
{
    return append_text(out, name) && append_text(out, " = ")
           && append_text(out, value) && append_text(out, crlf);
}

static bool append_scale(sdfio_string *out, const sdfio_point *point,
                         const char *name, double value)
{
    char text[SDFIO_NUMBER_CAPACITY];
    sdfio_format_scientific(point, text, sizeof text, value,
                            SDFIO_DOUBLE_DIGITS, SDFIO_DOUBLE_EXPONENT);
    return append_field(out, name, text);
}

sdfio_status sdfio_ascii_header(const sdfio_header *header,
                                const sdfio_point *point,
                                const sdfio_time_zone *zone, sdfio_string *out,
                                sdfio_message *message)
{
    char magic[SDFIO_MAGIC_SIZE + 1];
    snprintf(magic, sizeof magic, "a%s", sdfio_revision_name(header->revision));
    char text[SDFIO_NUMBER_CAPACITY];
    char date[SDFIO_DATE_SIZE + 1] = {0};
    bool ok = append_text(out, magic) && append_text(out, crlf)
              && append_field(out, "ManufacID", header->manufacturer_id);
    if (!sdfio_date_format(header->creation_date, header->revision, zone, date))
    {
        return sdfio_fail(message, SDFIO_ERR_RANGE,
                          "The time zone cannot convert the creation date");
    }
    ok = ok && append_field(out, "CreateDate", date);
    if (!sdfio_date_format(header->modification_date, header->revision, zone,
                           date))
    {
        return sdfio_fail(message, SDFIO_ERR_RANGE,
                          "The time zone cannot convert the modification date");
    }
    ok = ok && append_field(out, "ModDate", date);
    snprintf(text, sizeof text, "%zu", header->columns);
    ok = ok && append_field(out, "NumPoints", text);
    snprintf(text, sizeof text, "%zu", header->rows);
    ok = ok && append_field(out, "NumProfiles", text);
    ok = ok && append_scale(out, point, "Xscale", header->x_scale)
         && append_scale(out, point, "Yscale", header->y_scale)
         && append_scale(out, point, "Zscale", header->z_scale)
         && append_scale(out, point, "Zresolution",
                         header->z_resolution > 0.0 ? header->z_resolution
                                                    : SDFIO_NO_RESOLUTION);
    snprintf(text, sizeof text, "%d", SDFIO_NONE);
    ok = ok && append_field(out, "Compression", text);
    snprintf(text, sizeof text, "%d", (int)header->data_type);
    ok = ok && append_field(out, "DataType", text);
    snprintf(text, sizeof text, "%d", SDFIO_NONE);
    ok = ok && append_field(out, "CheckType", text);
    snprintf(text, sizeof text, "%c", terminator);
    ok = ok && append_text(out, text) && append_text(out, crlf);
    if (!ok)
    {
        return sdfio_fail(message, SDFIO_ERR_MEMORY, "Out of memory");
    }
    return SDFIO_OK;
}

/* The decimal digits of an integer, without a terminator, which the C library
 * would format by a call per value. */
static size_t format_integer(long long value, char *out)
{
    unsigned long long magnitude = value < 0 ? 0ULL - (unsigned long long)value
                                             : (unsigned long long)value;
    char reversed[SDFIO_NUMBER_CAPACITY];
    size_t digits = 0;
    do
    {
        reversed[digits++] = (char)('0' + magnitude % 10);
        magnitude /= 10;
    } while (magnitude != 0);
    size_t used = 0;
    if (value < 0)
    {
        out[used++] = '-';
    }
    while (digits > 0)
    {
        out[used++] = reversed[--digits];
    }
    out[used] = '\0';
    return used;
}

size_t sdfio_ascii_format(const sdfio_header *header, const sdfio_point *point,
                          const void *coded, size_t index, char *out,
                          size_t capacity)
{
    const sdfio_data_type type = header->data_type;
    double value = sdfio_get_real(type, coded, index);
    if (value == sdfio_bad_value(type))
    {
        static const char bad[] = "BAD";
        memcpy(out, bad, sizeof bad);
        return sizeof bad - 1;
    }
    if (!sdfio_is_floating(type))
    {
        return format_integer((long long)value, out);
    }
    if (type == SDFIO_DATA_BINARY32)
    {
        return sdfio_format_scientific(point, out, capacity, value,
                                       SDFIO_FLOAT_DIGITS,
                                       SDFIO_FLOAT_EXPONENT);
    }
    return sdfio_format_scientific(point, out, capacity, value,
                                   SDFIO_DOUBLE_DIGITS, SDFIO_DOUBLE_EXPONENT);
}

sdfio_status sdfio_ascii_tail(const char *trailer, size_t size,
                              sdfio_string *out, sdfio_message *message)
{
    /* The record is always closed, also if it is empty. */
    const bool ok =
        sdfio_string_append(out, &terminator, 1) && append_text(out, crlf)
        && sdfio_append_crlf_lines(out, trailer, size)
        && sdfio_string_append(out, &terminator, 1) && append_text(out, crlf);
    return ok ? SDFIO_OK
              : sdfio_fail(message, SDFIO_ERR_MEMORY, "Out of memory");
}
