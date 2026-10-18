/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* The text representation: the header lines, the values and the trailer. */

#include "internal.h"

#include <stdio.h>

static const char crlf[] = "\r\n";
static const char comment_marker = ';';
static const char terminator = '*';
static const char bad_token[] = "bad";
static const char nan_token[] = "nan";
/* Some writers write NaN for a point that is not measured, in every
 * revision. */

/* The result of reading a line or a token. */
enum
{
    GOT_NONE = 0,
    GOT_ONE = 1,
    GOT_ERROR = -1
};

/* The next line of the source, without its end. The last line of a text that
 * does not end with a line end comes back once with `ended` false. */
static int next_line(sdfi_source *source, sdfi_string *line, bool *ended,
                     bool *done, size_t limit)
{
    sdfi_string_clear(line);
    *ended = false;
    if (*done)
    {
        return GOT_NONE;
    }
    for (;;)
    {
        const int c = sdfi_source_getc(source);
        if (c < 0)
        {
            *done = true;
            return source->failed ? GOT_ERROR : GOT_ONE;
        }
        if (source->skip_lf)
        {
            source->skip_lf = false;
            if (c == '\n')
            {
                continue;
            }
        }
        if (c == '\n' || c == '\r')
        {
            source->skip_lf = c == '\r';
            *ended = true;
            return GOT_ONE;
        }
        const char character = (char)c;
        if (line->size >= limit || !sdfi_string_append(line, &character, 1))
        {
            return GOT_ERROR;
        }
    }
}

static void skip_line(sdfi_source *source)
{
    source->skip_lf = false;
    int c = 0;
    while ((c = sdfi_source_getc(source)) >= 0)
    {
        if (c == '\n')
        {
            return;
        }
        if (c == '\r')
        {
            source->skip_lf = true;
            return;
        }
    }
}

static bool ends_token(char c, bool comments)
{
    return sdfi_is_space(c) || (comments && c == comment_marker);
}

/* The next white-space separated word, comments of BCR-1.0 skipped. */
static int next_token(sdfi_source *source, bool comments, char *token,
                      size_t capacity, size_t *size)
{
    source->skip_lf = false;
    int c = 0;
    for (;;)
    {
        c = sdfi_source_peek(source);
        if (c < 0)
        {
            return source->failed ? GOT_ERROR : GOT_NONE;
        }
        if (sdfi_is_space((char)c))
        {
            ++source->position;
        }
        else if (comments && c == comment_marker)
        {
            while ((c = sdfi_source_peek(source)) >= 0 && c != '\n'
                   && c != '\r')
            {
                ++source->position;
            }
        }
        else
        {
            break;
        }
    }
    *size = 0;
    while ((c = sdfi_source_peek(source)) >= 0
           && !ends_token((char)c, comments))
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

static const char *strip_comment(const char *line, size_t *size)
{
    const char *found = memchr(line, comment_marker, *size);
    if (found != NULL)
    {
        *size = (size_t)(found - line);
    }
    return line;
}

/* A line that has only the character `*`, with white space around it. */
static bool is_terminator(const char *line, size_t size)
{
    sdfi_trim(&line, &size);
    return size == 1 && line[0] == terminator;
}

static bool matches_magic(const char *magic, size_t size)
{
    if (size != SDFI_MAGIC_SIZE)
    {
        return false;
    }
    const char prefix = sdfi_lower(magic[0]);
    if (prefix != 'a' && prefix != 'b')
    {
        return false;
    }
    for (size_t i = SDFI_MAGIC_NAME_AT;
         i < SDFI_MAGIC_NAME_AT + SDFI_MAGIC_NAME_SIZE; ++i)
    {
        if (magic[i] < 'A' || magic[i] > 'Z')
        {
            return false;
        }
    }
    return magic[SDFI_MAGIC_DASH_AT] == '-'
           && sdfi_is_digit(magic[SDFI_MAGIC_MAJOR_AT])
           && magic[SDFI_MAGIC_DOT_AT] == '.'
           && sdfi_is_digit(magic[SDFI_MAGIC_MINOR_AT]);
}

/* ---- The header -----------------------------------------------------------
 */

typedef struct field_text
{
    const char *text;
    size_t size;
} field_text;

static sdf_status field(const sdfi_string *header, const char *name,
                        field_text *out, sdfi_message *message)
{
    if (!sdfi_tagged_find(header->data != NULL ? header->data : "",
                          header->size, name, &out->text, &out->size))
    {
        return sdfi_fail(message, SDF_ERR_FORMAT,
                         "Missing required SDF header field '%s'", name);
    }
    return SDF_OK;
}

static sdf_status invalid_value(sdfi_message *message, const char *kind,
                                const char *name, field_text value)
{
    return sdfi_fail(message, SDF_ERR_FORMAT,
                     "Invalid %s value for SDF header field '%s': '%.*s'", kind,
                     name, (int)value.size, value.text);
}

static sdf_status header_real(const sdfi_string *header, const char *name,
                              const sdfi_point *point, double *out,
                              sdfi_message *message)
{
    field_text value;
    const sdf_status status = field(header, name, &value, message);
    if (status != SDF_OK)
    {
        return status;
    }
    if (!sdfi_parse_number(point, value.text, value.size, out))
    {
        return invalid_value(message, "float", name, value);
    }
    return SDF_OK;
}

static sdf_status header_count(const sdfi_string *header, const char *name,
                               size_t *out, sdfi_message *message)
{
    field_text value;
    const sdf_status status = field(header, name, &value, message);
    if (status != SDF_OK)
    {
        return status;
    }
    long long number = 0;
    if (!sdfi_parse_integer(value.text, value.size, &number))
    {
        return invalid_value(message, "integer", name, value);
    }
    if (number < 0 || (unsigned long long)number > SIZE_MAX)
    {
        return sdfi_fail(message, SDF_ERR_FORMAT,
                         "SDF header field '%s' is out of range: %lld", name,
                         number);
    }
    *out = (size_t)number;
    return SDF_OK;
}

/* BCR-1.0 lets an ASCII header spell some fields as names instead of codes,
 * without regard to the case. The names of the data types are the short ones
 * that the specification suggests; "NULL" is what its own example uses for
 * "none". */
typedef struct symbol
{
    const char *name;
    long long code;
} symbol;

enum
{
    COMPRESSION_RLL = 1,
    CHECK_UINTTRACE = 1
};

static const symbol data_type_symbols[] = {
    {"uchar", SDF_DATA_UINT8},     {"uinteger", SDF_DATA_UINT16},
    {"ulongint", SDF_DATA_UINT32}, {"float", SDF_DATA_BINARY32},
    {"char", SDF_DATA_INT8},       {"integer", SDF_DATA_INT16},
    {"longint", SDF_DATA_INT32},   {"double", SDF_DATA_BINARY64},
};
static const symbol compression_symbols[] = {
    {"null", SDFI_NONE}, {"none", SDFI_NONE}, {"rll", COMPRESSION_RLL}};
static const symbol check_symbols[] = {
    {"null", SDFI_NONE}, {"none", SDFI_NONE}, {"uinttrace", CHECK_UINTTRACE}};

static sdf_status header_coded(const sdfi_string *header, const char *name,
                               sdf_revision revision, const symbol *symbols,
                               size_t symbol_count, long long *out,
                               sdfi_message *message)
{
    field_text value;
    const sdf_status status = field(header, name, &value, message);
    if (status != SDF_OK)
    {
        return status;
    }
    if (sdfi_parse_integer(value.text, value.size, out))
    {
        return SDF_OK;
    }
    if (sdfi_allows_symbols(revision))
    {
        const char *spelled = value.text;
        size_t spelled_size = value.size;
        sdfi_trim(&spelled, &spelled_size);
        for (size_t i = 0; i < symbol_count; ++i)
        {
            if (sdfi_equals_nocase(spelled, spelled_size, symbols[i].name))
            {
                *out = symbols[i].code;
                return SDF_OK;
            }
        }
    }
    return invalid_value(message, "integer", name, value);
}

static void copy_identifier(sdf_info *info, const char *text, size_t size)
{
    sdfi_trim(&text, &size);
    if (size >= sizeof info->manufacturer_id)
    {
        size = sizeof info->manufacturer_id - 1;
    }
    memcpy(info->manufacturer_id, text, size);
    info->manufacturer_id[size] = '\0';
}

typedef struct codes
{
    long long type;
    long long compression;
    long long check_type;
} codes;

static sdf_status read_codes(const sdfi_string *header, sdf_revision revision,
                             codes *out, sdfi_message *message)
{
    sdf_status status =
        header_coded(header, "DataType", revision, data_type_symbols,
                     sizeof data_type_symbols / sizeof data_type_symbols[0],
                     &out->type, message);
    if (status != SDF_OK)
    {
        return status;
    }
    status =
        header_coded(header, "Compression", revision, compression_symbols,
                     sizeof compression_symbols / sizeof compression_symbols[0],
                     &out->compression, message);
    if (status != SDF_OK)
    {
        return status;
    }
    return header_coded(header, "CheckType", revision, check_symbols,
                        sizeof check_symbols / sizeof check_symbols[0],
                        &out->check_type, message);
}

/* The fields that are numbers or texts, into the description. */
static sdf_status read_fields(const sdfi_string *header,
                              const sdfi_point *point, sdf_info *info,
                              field_text *identifier, field_text *created,
                              field_text *modified, double *resolution,
                              sdfi_message *message)
{
    sdf_status status = field(header, "ManufacID", identifier, message);
    if (status == SDF_OK)
    {
        status = field(header, "CreateDate", created, message);
    }
    if (status == SDF_OK)
    {
        status = field(header, "ModDate", modified, message);
    }
    if (status == SDF_OK)
    {
        status = header_count(header, "NumPoints", &info->columns, message);
    }
    if (status == SDF_OK)
    {
        status = header_count(header, "NumProfiles", &info->rows, message);
    }
    if (status == SDF_OK)
    {
        status = header_real(header, "Xscale", point, &info->x_scale, message);
    }
    if (status == SDF_OK)
    {
        status = header_real(header, "Yscale", point, &info->y_scale, message);
    }
    if (status == SDF_OK)
    {
        status = header_real(header, "Zscale", point, &info->z_scale, message);
    }
    if (status == SDF_OK)
    {
        status = header_real(header, "Zresolution", point, resolution, message);
    }
    return status;
}

sdf_status sdfi_ascii_parse_header(sdfi_source *source, sdf_info *info,
                                   const sdfi_point *point,
                                   sdfi_message *message)
{
    sdfi_string line = {0};
    sdfi_string header = {0};
    sdf_status status = SDF_OK;
    bool ended = false;
    bool done = false;
    const int got =
        next_line(source, &line, &ended, &done, SDFI_MAX_HEADER_TEXT);
    if (got == GOT_ERROR)
    {
        status =
            source->failed
                ? sdfi_fail(message, SDF_ERR_IO, "The stream could not be read")
                : sdfi_fail(message, SDF_ERR_FORMAT,
                            "The first line of the file is not a magic");
        goto done;
    }
    if (got != GOT_ONE || !ended)
    {
        status = sdfi_fail(message, SDF_ERR_FORMAT,
                           "SDF file is missing the header and the data area");
        goto done;
    }

    const char *magic_line = sdfi_string_text(&line);
    size_t magic_line_size = line.size;
    sdfi_trim(&magic_line, &magic_line_size);
    const char *magic = magic_line;
    size_t magic_size = magic_line_size;
    strip_comment(magic, &magic_size);
    sdfi_trim(&magic, &magic_size);
    if (!matches_magic(magic, magic_size))
    {
        status = sdfi_fail(message, SDF_ERR_FORMAT,
                           "Not an ASCII SDF file, unexpected magic: '%.*s'",
                           (int)magic_line_size, magic_line);
        goto done;
    }
    if (sdfi_lower(magic[0]) != 'a')
    {
        status = sdfi_fail(message, SDF_ERR_FORMAT,
                           "Binary magic found while parsing an ASCII SDF "
                           "file");
        goto done;
    }
    sdf_revision revision = SDF_ISO_2_0;
    if (!sdfi_revision_from_text(magic + 1, magic_size - 1, &revision))
    {
        status = sdfi_fail(message, SDF_ERR_FORMAT,
                           "Unknown or unsupported SDF revision in magic "
                           "'%.*s'",
                           (int)magic_line_size, magic_line);
        goto done;
    }
    const bool comments = sdfi_allows_comments(revision);
    if (memchr(magic_line, comment_marker, magic_line_size) != NULL
        && !comments)
    {
        status = sdfi_fail(message, SDF_ERR_FORMAT,
                           "Not an ASCII SDF file, unexpected magic: '%.*s'",
                           (int)magic_line_size, magic_line);
        goto done;
    }

    /* The records end with a line that has only a "*". With comments, a "*"
     * followed by a comment still ends its record, and the comments are taken
     * out of the header and the data. */
    bool header_started = false;
    bool header_ended = false;
    while (next_line(source, &line, &ended, &done, SDFI_MAX_HEADER_TEXT)
           == GOT_ONE)
    {
        size_t bare_size = line.size;
        const char *bare = sdfi_string_text(&line);
        if (comments)
        {
            strip_comment(bare, &bare_size);
        }
        if (is_terminator(bare, bare_size))
        {
            header_ended = true;
            break;
        }
        if ((header_started && !sdfi_string_append(&header, "\n", 1))
            || !sdfi_string_append(&header, bare, bare_size)
            || header.size > SDFI_MAX_HEADER_TEXT)
        {
            status = sdfi_fail(message, SDF_ERR_FORMAT,
                               "The SDF header is too long");
            goto done;
        }
        header_started = true;
    }
    if (!header_ended)
    {
        status =
            source->failed
                ? sdfi_fail(message, SDF_ERR_IO, "The stream could not be read")
                : sdfi_fail(message, SDF_ERR_FORMAT,
                            "SDF file must contain the header, the data "
                            "area and the trailer");
        goto done;
    }
    if (!sdfi_tagged_valid(sdfi_string_text(&header), header.size))
    {
        status = sdfi_fail(message, SDF_ERR_FORMAT,
                           "Malformed SDF tagged field line in the header");
        goto done;
    }

    codes parsed_codes;
    status = read_codes(&header, revision, &parsed_codes, message);
    if (status != SDF_OK)
    {
        goto done;
    }
    const long long type_code = parsed_codes.type;
    const long long compression = parsed_codes.compression;
    const long long check_type = parsed_codes.check_type;
    if (type_code < 0 || type_code > SDF_DATA_BINARY64)
    {
        status = sdfi_fail(message, SDF_ERR_FORMAT,
                           "Unknown SDF data type code %lld", type_code);
        goto done;
    }
    const sdf_data_type type = (sdf_data_type)type_code;
    if (!sdf_revision_allows(revision, type))
    {
        status =
            sdfi_fail(message, SDF_ERR_FORMAT,
                      "Data type %s is not valid for SDF revision %s",
                      sdf_data_type_name(type), sdf_revision_name(revision));
        goto done;
    }
    if (compression != SDFI_NONE)
    {
        status = sdfi_fail(message, SDF_ERR_UNSUPPORTED,
                           "Compressed SDF data areas are not supported");
        goto done;
    }
    if (check_type != SDFI_NONE)
    {
        status = sdfi_fail(message, SDF_ERR_UNSUPPORTED,
                           "Checksummed SDF data areas are not supported");
        goto done;
    }

    field_text identifier;
    field_text created;
    field_text modified;
    double resolution = 0.0;
    status = read_fields(&header, point, info, &identifier, &created, &modified,
                         &resolution, message);
    if (status != SDF_OK)
    {
        goto done;
    }
    info->revision = revision;
    info->storage = SDF_ASCII;
    info->data_type = type;
    info->z_resolution = resolution > 0.0 && isfinite(resolution)
                             ? resolution
                             : SDF_NO_RESOLUTION;
    copy_identifier(info, identifier.text, identifier.size);
    info->create_date = sdfi_date_parse(created.text, created.size, revision);
    info->mod_date = sdfi_date_parse(modified.text, modified.size, revision);

done:
    sdfi_string_free(&line);
    sdfi_string_free(&header);
    return status;
}

/* ---- The data area ---------------------------------------------------------
 */

static sdf_status ended_early(sdfi_source *source, sdfi_message *message)
{
    return source->failed
               ? sdfi_fail(message, SDF_ERR_IO, "The stream could not be read")
               : sdfi_fail(message, SDF_ERR_FORMAT,
                           "SDF file must contain the header, the "
                           "data area and the trailer");
}

sdf_status sdfi_ascii_value(sdfi_source *source, const sdf_info *info,
                            const sdfi_point *point, void *dst, size_t index,
                            size_t position, sdfi_message *message)
{
    const bool comments = sdfi_allows_comments(info->revision);
    char token[SDFI_TOKEN_CAPACITY];
    size_t size = 0;
    const int got = next_token(source, comments, token, sizeof token, &size);
    if (got == GOT_NONE)
    {
        return ended_early(source, message);
    }
    if (got == GOT_ERROR)
    {
        return source->failed
                   ? sdfi_fail(message, SDF_ERR_IO,
                               "The stream could not be read")
                   : sdfi_fail(message, SDF_ERR_FORMAT,
                               "A data value is too long at position %zu",
                               position);
    }
    size_t count = 0;
    sdfi_count(info, &count);
    if (size == 1 && token[0] == terminator)
    {
        return sdfi_fail(message, SDF_ERR_FORMAT,
                         "Expected %zu data values, found %zu", count,
                         position);
    }
    if (sdfi_equals_nocase(token, size, bad_token)
        || sdfi_equals_nocase(token, size, nan_token))
    {
        return sdfi_put_real(info->data_type, dst, index,
                             sdf_sentinel(info->data_type, info->revision));
    }
    sdf_status status = SDF_ERR_FORMAT;
    if (sdfi_is_floating(info->data_type))
    {
        double value = 0.0;
        if (sdfi_parse_number(point, token, size, &value))
        {
            status = sdfi_put_real(info->data_type, dst, index, value);
        }
    }
    else
    {
        /* An integer type does not take a fraction, which would be
         * truncated. */
        long long value = 0;
        if (sdfi_parse_integer(token, size, &value))
        {
            status = sdfi_put_integer(info->data_type, dst, index, value);
        }
    }
    if (status != SDF_OK)
    {
        return sdfi_fail(message, SDF_ERR_FORMAT,
                         "Invalid data value at position %zu: '%s'", position,
                         token);
    }
    return SDF_OK;
}

sdf_status sdfi_ascii_finish(sdfi_source *source, const sdf_info *info,
                             sdfi_message *message)
{
    const bool comments = sdfi_allows_comments(info->revision);
    char token[SDFI_TOKEN_CAPACITY];
    size_t size = 0;
    if (next_token(source, comments, token, sizeof token, &size) != GOT_ONE)
    {
        return ended_early(source, message);
    }
    if (size != 1 || token[0] != terminator)
    {
        /* Count what is there, for the message. */
        size_t count = 0;
        sdfi_count(info, &count);
        size_t found = count + 1;
        while (next_token(source, comments, token, sizeof token, &size)
                   == GOT_ONE
               && !(size == 1 && token[0] == terminator))
        {
            ++found;
        }
        return sdfi_fail(message, SDF_ERR_FORMAT,
                         "Expected %zu data values, found %zu", count, found);
    }
    skip_line(source);
    return SDF_OK;
}

sdf_status sdfi_ascii_skip(sdfi_source *source, const sdf_info *info,
                           sdfi_message *message)
{
    const bool comments = sdfi_allows_comments(info->revision);
    char token[SDFI_TOKEN_CAPACITY];
    size_t size = 0;
    while (next_token(source, comments, token, sizeof token, &size) == GOT_ONE)
    {
        if (size == 1 && token[0] == terminator)
        {
            skip_line(source);
            return SDF_OK;
        }
    }
    return ended_early(source, message);
}

sdf_status sdfi_ascii_trailer(sdfi_source *source, const sdf_info *info,
                              sdfi_string *out, bool *present,
                              sdfi_message *message)
{
    const bool comments = sdfi_allows_comments(info->revision);
    sdfi_string line = {0};
    sdfi_string part = {0};
    sdf_status status = SDF_OK;
    bool ended = false;
    bool done = false;
    bool first = true;
    bool part_started = false;
    size_t parts = 0;
    *present = true;
    int got = GOT_ONE;
    while ((got = next_line(source, &line, &ended, &done, SDFI_MAX_TRAILER))
           == GOT_ONE)
    {
        if (first && line.size == 0 && !ended)
        {
            *present = false; /* the text ends with the data */
            goto done;
        }
        first = false;
        size_t bare_size = line.size;
        const char *bare = sdfi_string_text(&line);
        if (comments)
        {
            strip_comment(bare, &bare_size);
        }
        if (is_terminator(bare, bare_size))
        {
            /* The parts are joined with a "*". */
            if ((parts > 0 && !sdfi_string_append(out, &terminator, 1))
                || !sdfi_string_append(out, sdfi_string_text(&part), part.size))
            {
                status = sdfi_fail(message, SDF_ERR_MEMORY, "Out of memory");
                goto done;
            }
            ++parts;
            sdfi_string_clear(&part);
            part_started = false;
        }
        else
        {
            if ((part_started && !sdfi_string_append(&part, "\n", 1))
                || !sdfi_string_append(&part, sdfi_string_text(&line),
                                       line.size))
            {
                status = sdfi_fail(message, SDF_ERR_MEMORY, "Out of memory");
                goto done;
            }
            part_started = true;
        }
        if (out->size + part.size > SDFI_MAX_TRAILER)
        {
            status = sdfi_fail(message, SDF_ERR_FORMAT,
                               "The SDF trailer is too long");
            goto done;
        }
    }
    if (got == GOT_ERROR)
    {
        status = source->failed ? sdfi_fail(message, SDF_ERR_IO,
                                            "The stream could not be read")
                                : sdfi_fail(message, SDF_ERR_FORMAT,
                                            "The SDF trailer is too long");
        goto done;
    }
    /* The trailer is itself ended by its own "*": the empty part that follows
     * is not part of it. */
    if (part.size > 0
        && ((parts > 0 && !sdfi_string_append(out, &terminator, 1))
            || !sdfi_string_append(out, sdfi_string_text(&part), part.size)))
    {
        status = sdfi_fail(message, SDF_ERR_MEMORY, "Out of memory");
        goto done;
    }
    /* White space around the trailer is not part of it. */
    {
        const char *begin = sdfi_string_text(out);
        size_t size = out->size;
        sdfi_trim(&begin, &size);
        if (out->data != NULL)
        {
            memmove(out->data, begin, size);
            out->size = size;
            out->data[size] = '\0';
        }
    }
done:
    sdfi_string_free(&line);
    sdfi_string_free(&part);
    return status;
}

/* ---- Writing ----------------------------------------------------------------
 */

static bool append_text(sdfi_string *out, const char *text)
{
    return sdfi_string_append(out, text, strlen(text));
}

static bool append_field(sdfi_string *out, const char *name, const char *value)
{
    return append_text(out, name) && append_text(out, " = ")
           && append_text(out, value) && append_text(out, crlf);
}

static bool append_scale(sdfi_string *out, const sdfi_point *point,
                         const char *name, double value)
{
    char text[SDFI_NUMBER_CAPACITY];
    sdfi_format_scientific(point, text, sizeof text, value, SDFI_DOUBLE_DIGITS,
                           SDFI_DOUBLE_EXPONENT);
    return append_field(out, name, text);
}

sdf_status sdfi_ascii_header(const sdf_info *info, const sdfi_point *point,
                             sdfi_string *out, sdfi_message *message)
{
    char magic[SDFI_MAGIC_SIZE + 1];
    snprintf(magic, sizeof magic, "a%s", sdf_revision_name(info->revision));
    char text[SDFI_NUMBER_CAPACITY];
    char date[SDFI_DATE_SIZE + 1] = {0};
    bool ok = append_text(out, magic) && append_text(out, crlf)
              && append_field(out, "ManufacID", info->manufacturer_id);
    sdfi_date_format(info->create_date, info->revision, date);
    ok = ok && append_field(out, "CreateDate", date);
    sdfi_date_format(info->mod_date, info->revision, date);
    ok = ok && append_field(out, "ModDate", date);
    snprintf(text, sizeof text, "%zu", info->columns);
    ok = ok && append_field(out, "NumPoints", text);
    snprintf(text, sizeof text, "%zu", info->rows);
    ok = ok && append_field(out, "NumProfiles", text);
    ok = ok && append_scale(out, point, "Xscale", info->x_scale)
         && append_scale(out, point, "Yscale", info->y_scale)
         && append_scale(out, point, "Zscale", info->z_scale)
         && append_scale(out, point, "Zresolution",
                         info->z_resolution > 0.0 ? info->z_resolution
                                                  : SDFI_FILE_NO_RESOLUTION);
    snprintf(text, sizeof text, "%d", SDFI_NONE);
    ok = ok && append_field(out, "Compression", text);
    snprintf(text, sizeof text, "%d", (int)info->data_type);
    ok = ok && append_field(out, "DataType", text);
    snprintf(text, sizeof text, "%d", SDFI_NONE);
    ok = ok && append_field(out, "CheckType", text);
    snprintf(text, sizeof text, "%c", terminator);
    ok = ok && append_text(out, text) && append_text(out, crlf);
    if (!ok)
    {
        return sdfi_fail(message, SDF_ERR_MEMORY, "Out of memory");
    }
    return SDF_OK;
}

size_t sdfi_ascii_format(const sdf_info *info, const sdfi_point *point,
                         const void *stored, size_t index, char *out,
                         size_t capacity)
{
    const sdf_data_type type = info->data_type;
    double value = sdfi_get_real(type, stored, index);
    if (sdfi_matches(sdfi_sentinel_test_for(type, info->revision), value))
    {
        if (sdfi_has_bad_token(info->revision))
        {
            return (size_t)snprintf(out, capacity, "BAD");
        }
        value = sdfi_ascii_sentinel(type, info->revision);
    }
    if (!sdfi_is_floating(type))
    {
        return (size_t)snprintf(out, capacity, "%lld", (long long)value);
    }
    if (type == SDF_DATA_BINARY32)
    {
        return sdfi_format_scientific(point, out, capacity, value,
                                      SDFI_FLOAT_DIGITS, SDFI_FLOAT_EXPONENT);
    }
    return sdfi_format_scientific(point, out, capacity, value,
                                  SDFI_DOUBLE_DIGITS, SDFI_DOUBLE_EXPONENT);
}

sdf_status sdfi_ascii_tail(const char *trailer, size_t size, sdfi_string *out,
                           sdfi_message *message)
{
    /* A trailer that ends in its own line end must not get a second one, which
     * would leave a blank line before the "*" that closes it. */
    if (size >= 2 && trailer[size - 2] == '\r' && trailer[size - 1] == '\n')
    {
        size -= 2;
    }
    else if (size >= 1 && trailer[size - 1] == '\n')
    {
        size -= 1;
    }
    /* The record is always closed, also if it is empty. */
    bool ok = sdfi_string_append(out, &terminator, 1) && append_text(out, crlf);
    if (size > 0)
    {
        ok = ok && sdfi_string_append(out, trailer, size)
             && append_text(out, crlf);
    }
    ok =
        ok && sdfi_string_append(out, &terminator, 1) && append_text(out, crlf);
    return ok ? SDF_OK : sdfi_fail(message, SDF_ERR_MEMORY, "Out of memory");
}
