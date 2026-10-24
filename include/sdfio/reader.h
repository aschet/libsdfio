/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * The reader: opening a file or a stream, and reading the data area and the
 * trailer.
 */

#ifndef SDFIO_READER_H
#define SDFIO_READER_H

#include <sdfio/header.h>
#include <sdfio/heights.h>
#include <sdfio/sdfio_export.h>
#include <sdfio/types.h>

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

/** Opaque handle of a reader. */
typedef struct sdfio_reader sdfio_reader;

/**
 * Creates a reader. The reader reads nothing until a source is opened. It is
 * created before the source is opened, so that a failed open can report its
 * reason.
 *
 * Only the handle is allocated here. The program adds the formats of the
 * reader and may set its options, and the buffers are allocated when a file is
 * opened. Release the reader with sdfio_reader_free().
 *
 * @param[in] allocator The allocator for all memory of the reader, for example
 * sdfio_default_allocator(). The structure is copied, so it need not outlive
 * the call. Both callbacks must be set.
 * @param[out] reader Receives the reader, or null if the call fails.
 * @retval SDFIO_ERR_ARGUMENT A pointer is null, or the allocator lacks a
 * callback.
 * @retval SDFIO_ERR_MEMORY An allocation failed.
 */
SDFIO_API sdfio_status sdfio_reader_create(const sdfio_allocator *allocator,
                                           sdfio_reader **reader);

/**
 * Adds the binary format to the formats that the reader reads, the version
 * numbers `bISO-1.0` and `bISO-2.0`. A reader needs at least one format before
 * a file is opened. The formats are tried in the order in which they are
 * added, and sdfio_reader_magic() lists their magics in that
 * order.
 *
 * Like the other options of a reader, a format is added before the first
 * attempt to open a file.
 *
 * @param[in,out] reader The reader.
 * @retval SDFIO_ERR_ARGUMENT The reader is null or has added the format
 * already.
 * @retval SDFIO_ERR_STATE An attempt to open a file was made earlier.
 */
SDFIO_API sdfio_status sdfio_reader_add_binary(sdfio_reader *reader);

/**
 * Adds the ASCII format to the formats that the reader reads, the version
 * numbers `aISO-1.0` and `aISO-2.0`. The rules of
 * sdfio_reader_add_binary() apply.
 *
 * @param[in,out] reader The reader.
 * @retval SDFIO_ERR_ARGUMENT The reader is null or has added the format
 * already.
 * @retval SDFIO_ERR_STATE An attempt to open a file was made earlier.
 */
SDFIO_API sdfio_status sdfio_reader_add_ascii(sdfio_reader *reader);

/**
 * Sets the size in bytes of the buffer between the reader and its stream. The
 * default is 16384. A value below 64 is raised to 64. A smaller buffer causes
 * more calls of the stream callbacks.
 *
 * @param[in,out] reader The reader.
 * @param size The size, not 0.
 * @retval SDFIO_ERR_ARGUMENT The reader is null or the size is 0.
 * @retval SDFIO_ERR_STATE An attempt to open a file was made earlier.
 */
SDFIO_API sdfio_status sdfio_reader_set_buffer(sdfio_reader *reader,
                                               size_t size);

/**
 * Sets the number of values that the reader converts at a time. The default is
 * 1024. The chunk occupies up to 8 bytes per value.
 *
 * @param[in,out] reader The reader.
 * @param count The number of values, not 0 and not so large that the chunk
 * cannot be counted in bytes.
 * @retval SDFIO_ERR_ARGUMENT The reader is null or the number is not valid.
 * @retval SDFIO_ERR_STATE An attempt to open a file was made earlier.
 */
SDFIO_API sdfio_status sdfio_reader_set_chunk(sdfio_reader *reader,
                                              size_t count);

/**
 * Sets the conversion of the dates of ISO-1.0 files. The structure is copied,
 * so it need not outlive the call. Null selects no conversion, which is the
 * default.
 *
 * @param[in,out] reader The reader.
 * @param[in] time_zone The time zone, or null.
 * @retval SDFIO_ERR_ARGUMENT The reader is null.
 * @retval SDFIO_ERR_STATE An attempt to open a file was made earlier.
 */
SDFIO_API sdfio_status sdfio_reader_set_time_zone(
    sdfio_reader *reader, const sdfio_time_zone *time_zone);

/**
 * Returns the number of magics that a file can start with for the
 * formats that the reader adds: two for each format, one for each revision.
 *
 * @param[in] reader The reader. May be null, which has no formats.
 */
SDFIO_API size_t sdfio_reader_magic_count(const sdfio_reader *reader);

/**
 * Returns a magic that a file can start with, for example "bISO-2.0":
 * the letter b or a for the binary or the ASCII format, and the
 * revision. The text has SDFIO_MAGIC_SIZE characters and a terminator.
 * A program can compare the first bytes of a file with each magic to
 * find out whether the reader reads it.
 *
 * @param[in] reader The reader.
 * @param index A number below sdfio_reader_magic_count().
 * @return The magic, or null if the index is not below the count.
 */
SDFIO_API const char *sdfio_reader_magic(const sdfio_reader *reader,
                                         size_t index);

/**
 * Releases a reader and closes the file that sdfio_open_file() opened, if any.
 * A null pointer is ignored.
 *
 * @param[in,out] reader The reader.
 */
SDFIO_API void sdfio_reader_free(sdfio_reader *reader);

/**
 * Opens a block of memory as the source and reads the header. The memory is not
 * copied: it must remain valid and unchanged while the reader is in use.
 *
 * @param[in,out] reader The reader.
 * @param[in] data The first byte of the file.
 * @param size The size of the file in bytes.
 * @retval SDFIO_ERR_ARGUMENT A pointer is null.
 * @retval SDFIO_ERR_STATE The reader is open already.
 * @retval SDFIO_ERR_FORMAT The data is not an SDF file or is truncated.
 * @retval SDFIO_ERR_UNSUPPORTED The reader format of the file has not been
 * added.
 * @retval SDFIO_ERR_MEMORY An allocation failed.
 */
SDFIO_API sdfio_status sdfio_open_memory(sdfio_reader *reader, const void *data,
                                         size_t size);

/**
 * Opens a stream as the source and reads the header. The structure is copied,
 * and the callbacks and the object that `user` refers to must remain valid
 * while the reader is in use. The stream is read once, from the start.
 *
 * @param[in,out] reader The reader.
 * @param[in] stream The stream; `read` is required.
 * @retval SDFIO_ERR_ARGUMENT A pointer is null or `read` is not set.
 * @retval SDFIO_ERR_STATE The reader is open already.
 * @retval SDFIO_ERR_IO The stream failed.
 * @retval SDFIO_ERR_FORMAT The data is not an SDF file or is truncated.
 * @retval SDFIO_ERR_UNSUPPORTED The reader format of the file has not been
 * added.
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
SDFIO_API const sdfio_header *sdfio_reader_header(const sdfio_reader *reader);

/**
 * Reads the next rows of the data area as the file codes them.
 *
 * `dst` receives `rows` times `header->columns` coded values of
 * `header->data_type` in the byte order of the machine, in the order of the
 * file. The bad value is not changed. ASCII values are parsed into the data
 * type of the file, so that an ASCII file of `float` values is rounded to
 * single precision. Use sdfio_decode_heights() to obtain heights in metres.
 *
 * The buffer lies in memory as the layout describes, whose type has to be the
 * data type of the file: the element (y, x) is stored at
 * `dst + y * layout.row_stride + x * layout.element_stride`, where y counts
 * the rows from the first row that is read. sdfio_layout_packed() describes
 * values that follow each other. The bad value of the data type marks a point
 * that is not measured.
 *
 * The first row read is the first row stored in the file, the row with y = 0.
 *
 * After an error the reader rejects further reads.
 *
 * @param[in,out] reader The reader.
 * @param[in] layout The layout of the buffer.
 * @param[out] dst The buffer for the coded values.
 * @param rows The number of rows to read.
 * @retval SDFIO_ERR_ARGUMENT A pointer is null, `rows` exceeds the rows
 * remaining, the type of the layout is not the data type of the file, or the
 * strides are invalid.
 * @retval SDFIO_ERR_STATE No file is open, or an earlier error ended the
 * reading.
 * @retval SDFIO_ERR_IO The stream failed.
 * @retval SDFIO_ERR_FORMAT The data area is damaged or truncated.
 */
SDFIO_API sdfio_status sdfio_read_data(sdfio_reader *reader,
                                       const sdfio_layout *layout, void *dst,
                                       size_t rows);

/**
 * Reads the remaining rows as heights in metres into a buffer that lies in
 * memory as the layout describes. sdfio_layout_packed() describes a plain
 * array.
 *
 * The element (y, x) is stored at `dst + y * layout.row_stride + x *
 * layout.element_stride`. The row y = 0 is the first row. A negative row
 * stride, with `dst` at the first element of the row y = 0, which is the last
 * row in memory, stores the matrix upside down. A point that is not measured
 * becomes `not_measured`.
 *
 * The function reads from the first row of the data area, so no rows may have
 * been read before.
 *
 * @param[in,out] reader The reader.
 * @param[in] layout The layout of the buffer.
 * @param not_measured The value of a point that is not measured. NaN selects no
 * marker: such a point is NaN. The value is compared in the precision of the
 * type of the buffer, so -9999.1 marks the points of a `float` buffer that
 * hold the `float` nearest to -9999.1.
 * @param[out] dst The buffer for the heights.
 * @retval SDFIO_ERR_ARGUMENT A pointer is null or the strides are invalid.
 * @retval SDFIO_ERR_STATE No file is open, an earlier error ended the reading,
 * or rows were read already.
 * @retval SDFIO_ERR_RANGE A height does not fit the type of the buffer.
 * @retval SDFIO_ERR_IO The stream failed.
 * @retval SDFIO_ERR_FORMAT The data area is damaged or truncated.
 */
SDFIO_API sdfio_status sdfio_read_heights(sdfio_reader *reader,
                                          const sdfio_layout *layout,
                                          double not_measured, void *dst);

/**
 * Reads the next part of the trailer, the free text after the data
 * area, into a buffer of the caller.
 *
 * The call writes at most `capacity` bytes. A trailer that is longer than the
 * buffer is read by calling the function again, and `*size` is 0 when the
 * trailer is at its end, also for a file without one. The text is not
 * terminated. Rows of the data area that have not been read are read and
 * discarded by the first call.
 *
 * Every line end of the text is LF, whether the file has CRLF or LF, in ASCII
 * and in binary files. Nothing else of the text changes: the white space before
 * it and the line ends at its end are part of it. In an ASCII file the trailer
 * ends at the line that consists of `*`.
 *
 * @param[in,out] reader The reader.
 * @param[out] buffer Receives the text.
 * @param capacity The size of `buffer` in bytes, at least 1.
 * @param[out] size Receives the number of bytes written, 0 at the end of the
 * trailer.
 * @retval SDFIO_ERR_ARGUMENT A pointer is null or the capacity is 0.
 * @retval SDFIO_ERR_STATE No file is open, or an earlier error ended the
 * reading.
 * @retval SDFIO_ERR_IO The stream failed.
 * @retval SDFIO_ERR_FORMAT The file is damaged or truncated.
 */
SDFIO_API sdfio_status sdfio_read_trailer(sdfio_reader *reader, char *buffer,
                                          size_t capacity, size_t *size);

/**
 * Finds the next field of a trailer text, a line of the form `Name = Value`,
 * which ISO-2.0 expects. The text is the text that sdfio_read_trailer()
 * returned, or any text with LF or CRLF as line ends. The lines that are not
 * fields are skipped.
 *
 * The name consists of letters, digits and `_`. White space around the name,
 * the separator and the value is not part of them. A value ends at the end of
 * the line, so it can contain `=`, and it can be empty. The name and the value
 * point into `text`, which must outlive their use.
 *
 * @param[in] text The text of the trailer.
 * @param size The length of the text in bytes.
 * @param[in,out] position The offset of the line to start with: 0 for the
 * start of the text. The function sets it behind the line that it found, so the
 * next call continues there.
 * @param[out] name Receives the name.
 * @param[out] name_size Receives the length of the name.
 * @param[out] value Receives the value.
 * @param[out] value_size Receives the length of the value.
 * @return True if a field was found. False at the end of the text, or if a
 * pointer is null.
 */
SDFIO_API bool sdfio_trailer_next_field(const char *text, size_t size,
                                        size_t *position, const char **name,
                                        size_t *name_size, const char **value,
                                        size_t *value_size);

/**
 * Finds the value of the field of a name in a trailer text. The name is
 * compared without regard to case, as the standard has it for field names. If
 * the text has several fields of the name, the first one counts. The rules of
 * sdfio_trailer_next_field() apply.
 *
 * @param[in] text The text of the trailer.
 * @param size The length of the text in bytes.
 * @param[in] name The name of the field.
 * @param[out] value Receives the value, or null if there is no such field.
 * @param[out] value_size Receives the length of the value, or 0.
 * @return True if the field was found. False if there is none, or if a pointer
 * is null.
 */
SDFIO_API bool sdfio_trailer_find_field(const char *text, size_t size,
                                        const char *name, const char **value,
                                        size_t *value_size);

/**
 * Returns a description of the most recent failure of the reader, or an empty
 * string if none occurred. The result is never null.
 *
 * @param[in] reader The reader.
 */
SDFIO_API const char *sdfio_reader_error_message(const sdfio_reader *reader);

#ifdef __cplusplus
}
#endif

#endif /* SDFIO_READER_H */
