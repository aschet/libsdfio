/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * Writer of a file or a stream.
 */

#ifndef SDFIO_WRITER_H
#define SDFIO_WRITER_H

#include <sdfio/header.h>
#include <sdfio/heights.h>
#include <sdfio/reader.h>
#include <sdfio/sdfio_export.h>
#include <sdfio/types.h>

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

/** Opaque handle of a writer. */
typedef struct sdfio_writer sdfio_writer;

/**
 * Creates a writer. The writer writes nothing until a file is created.
 *
 * Only the handle is allocated here. The program adds the formats of the
 * writer and may set its options, and the buffers are allocated when a file is
 * created. Release the writer with sdfio_writer_free().
 *
 * @param[in] allocator The allocator for all memory of the writer, for example
 * sdfio_default_allocator(). The structure is copied, so it need not outlive
 * the call. Both callbacks must be set.
 * @param[out] writer Receives the writer, or null if the call fails.
 * @retval SDFIO_ERR_ARGUMENT A pointer is null, or the allocator lacks a
 * callback.
 * @retval SDFIO_ERR_MEMORY An allocation failed.
 */
SDFIO_API sdfio_status sdfio_writer_create(const sdfio_allocator *allocator,
                                           sdfio_writer **writer);

/**
 * Adds the binary format to the formats that the writer writes. A writer needs
 * at least one format before a file is created, and the format of the header
 * of a file selects the one that writes it.
 *
 * Like the other options of a writer, a format is added before the first
 * attempt to create a file.
 *
 * @param[in,out] writer The writer.
 * @retval SDFIO_ERR_ARGUMENT The writer is null or has added the format
 * already.
 * @retval SDFIO_ERR_STATE An attempt to create a file was made earlier.
 */
SDFIO_API sdfio_status sdfio_writer_add_binary(sdfio_writer *writer);

/**
 * Adds the ASCII format to the formats that the writer writes. The rules of
 * sdfio_writer_add_binary() apply.
 *
 * @param[in,out] writer The writer.
 * @retval SDFIO_ERR_ARGUMENT The writer is null or has added the format
 * already.
 * @retval SDFIO_ERR_STATE An attempt to create a file was made earlier.
 */
SDFIO_API sdfio_status sdfio_writer_add_ascii(sdfio_writer *writer);

/**
 * Sets the size in bytes of the buffer between the writer and its stream. The
 * default is 16384. A value below 64 is raised to 64. A smaller buffer causes
 * more calls of the stream callbacks.
 *
 * @param[in,out] writer The writer.
 * @param size The size, not 0.
 * @retval SDFIO_ERR_ARGUMENT The writer is null or the size is 0.
 * @retval SDFIO_ERR_STATE An attempt to create a file was made earlier.
 */
SDFIO_API sdfio_status sdfio_writer_set_buffer(sdfio_writer *writer,
                                               size_t size);

/**
 * Sets the number of values that the writer converts at a time. The default is
 * 1024. The chunk occupies up to 8 bytes per value; a writer of the ASCII
 * format occupies more.
 *
 * @param[in,out] writer The writer.
 * @param count The number of values, not 0 and not so large that the chunk
 * cannot be counted in bytes.
 * @retval SDFIO_ERR_ARGUMENT The writer is null or the number is not valid.
 * @retval SDFIO_ERR_STATE An attempt to create a file was made earlier.
 */
SDFIO_API sdfio_status sdfio_writer_set_chunk(sdfio_writer *writer,
                                              size_t count);

/**
 * Sets the conversion of the dates of ISO-1.0 files. The structure is copied,
 * so it need not outlive the call. Null selects no conversion, which is the
 * default.
 *
 * @param[in,out] writer The writer.
 * @param[in] time_zone The time zone, or null.
 * @retval SDFIO_ERR_ARGUMENT The writer is null.
 * @retval SDFIO_ERR_STATE An attempt to create a file was made earlier.
 */
SDFIO_API sdfio_status sdfio_writer_set_time_zone(
    sdfio_writer *writer, const sdfio_time_zone *time_zone);

/**
 * Releases a writer. A file that sdfio_create_file() created and
 * sdfio_end_file() did not end is closed in its incomplete state. A null
 * pointer is ignored.
 *
 * @param[in,out] writer The writer.
 */
SDFIO_API void sdfio_writer_free(sdfio_writer *writer);

/**
 * Starts a file that the header describes, including the numbers of columns
 * and rows. The header is written together with the first rows of the data
 * area. Both dates must be set; SDFIO_NO_TIME cannot be written.
 *
 * @param[in,out] writer The writer.
 * @param[in] stream The stream; `write` is required. The structure is copied.
 * @param[in] header The description of the file.
 * @retval SDFIO_ERR_ARGUMENT A pointer is null, or the header cannot be
 * written: a data type that the revision does not permit, a scale that is not
 * positive, a date that is not set, or a number of columns or rows that the
 * header cannot hold.
 * @retval SDFIO_ERR_RANGE The time zone of the writer cannot convert a
 * date. It is reported when the header is written, with the first rows.
 * @retval SDFIO_ERR_STATE The writer has created a file already.
 * @retval SDFIO_ERR_UNSUPPORTED The format of the header has not been added to
 * the writer.
 * @retval SDFIO_ERR_MEMORY An allocation failed.
 */
SDFIO_API sdfio_status sdfio_create_stream(sdfio_writer *writer,
                                           const sdfio_stream *stream,
                                           const sdfio_header *header);

/**
 * Writes the next rows of the data area.
 *
 * `src` holds `rows` times `header.columns` coded values of `header.data_type`
 * in the byte order of the machine, in the order of the file. The bad value is
 * written unchanged.
 *
 * The buffer lies in memory as the layout describes, whose type has to be the
 * data type of the file, as for sdfio_read_data(). sdfio_layout_packed()
 * describes coded values that follow each other.
 *
 * The first row written is the first row stored in the file, the row with
 * y = 0.
 *
 * @param[in,out] writer The writer.
 * @param[in] layout The layout of the buffer.
 * @param[in] src The coded values to write.
 * @param rows The number of rows to write.
 * @retval SDFIO_ERR_ARGUMENT A pointer is null, `rows` exceeds the rows
 * remaining, the type of the layout is not the data type of the file, the
 * strides are invalid, or the data type is an integer type and the z-scale is
 * not set.
 * @retval SDFIO_ERR_STATE No file is created, or an earlier error ended the
 * writing.
 * @retval SDFIO_ERR_IO The stream failed.
 */
SDFIO_API sdfio_status sdfio_write_data(sdfio_writer *writer,
                                        const sdfio_layout *layout,
                                        const void *src, size_t rows);

/**
 * Writes the whole data area from a matrix of heights in metres that lies in
 * memory as the layout describes. sdfio_layout_packed() describes a plain
 * array.
 *
 * The layout is that of sdfio_read_heights(): the element (y, x) is at
 * `src + y * layout.row_stride + x * layout.element_stride`, and the row y = 0
 * is the first row. For an integer data type with SDFIO_AUTO_Z_SCALE the
 * z-scale is selected from the heights. NaN and infinity label points that are
 * not measured. Call sdfio_end_file() afterwards.
 *
 * @param[in,out] writer The writer.
 * @param[in] layout The layout of the buffer.
 * @param[in] src The heights.
 * @retval SDFIO_ERR_ARGUMENT A pointer is null or the strides are invalid.
 * @retval SDFIO_ERR_STATE No file is created, an earlier error ended the
 * writing, or rows were written already.
 * @retval SDFIO_ERR_RANGE A height does not fit the data type with the z-scale.
 * @retval SDFIO_ERR_BAD_VALUE A height would be read as a point that is not
 * measured.
 * @retval SDFIO_ERR_IO The stream failed.
 */
SDFIO_API sdfio_status sdfio_write_heights(sdfio_writer *writer,
                                           const sdfio_layout *layout,
                                           const void *src);

/**
 * Writes a line `field = value` of the trailer, which ISO-2.0
 * expects. The line goes to the stream as it is given, so the program
 * assembles nothing in memory. All rows of the data area must have been
 * written. The fields and the text of sdfio_write_trailer_text() can follow
 * each other in any order. sdfio_end_file() ends the trailer.
 *
 * The value is text. The caller formats numbers, for instance with snprintf()
 * in the C locale, because a locale with a decimal comma would produce `20,5`.
 * White space around a value is lost when the line is read.
 *
 * @param[in,out] writer The writer.
 * @param[in] name The field name: letters, digits and `_`.
 * @param[in] value The value: one line. The encoding is not checked; the
 * standard asks for 7-bit ASCII.
 * @retval SDFIO_ERR_ARGUMENT A pointer is null, the field name is empty or
 * contains another character, or the value contains a CR or an LF.
 * @retval SDFIO_ERR_STATE No file is created, rows are missing, the file is
 * ended, or an earlier error ended the writing.
 * @retval SDFIO_ERR_IO The stream failed.
 */
SDFIO_API sdfio_status sdfio_write_trailer_field(sdfio_writer *writer,
                                                 const char *name,
                                                 const char *value);

/**
 * Writes text to the trailer, for a trailer that does not consist of
 * fields. All rows of the data area must have been written. The text and the
 * fields of sdfio_write_trailer_field() can follow each other in any order.
 * sdfio_end_file() ends the trailer.
 *
 * The text is not interpreted, and a call can write any part of it: the line
 * that a call leaves open continues with the text of the next call, so the text
 * can be split anywhere. A field line terminates the line that is open. So
 * does sdfio_end_file() in an ASCII file, whose closing line `*` starts a line:
 * a text without a line end at its end gets one there. A binary file ends with
 * the text as it is. Every line end of the text (LF or CRLF) is written as
 * CRLF, as the standard specifies, in ASCII and in binary files.
 * sdfio_read_trailer() returns the lines with LF. In an ASCII file a line that
 * consists only of `*` ends the record, so the text must not contain one;
 * lines of sdfio_write_trailer_field() never do.
 *
 * @param[in,out] writer The writer.
 * @param[in] text The text.
 * @param size The length of the text in bytes.
 * @retval SDFIO_ERR_ARGUMENT `text` is null and `size` is not 0.
 * @retval SDFIO_ERR_STATE No file is created, rows are missing, the file is
 * ended, or an earlier error ended the writing.
 * @retval SDFIO_ERR_IO The stream failed.
 */
SDFIO_API sdfio_status sdfio_write_trailer_text(sdfio_writer *writer,
                                                const char *text, size_t size);

/**
 * Copies the trailer of the file that a reader has opened to the
 * file of a writer. The rows of the data area that the reader has not read are
 * read and discarded, and the writer must have written all of its rows. A
 * program that saves a changed file as a new one reads the rows, writes the
 * changed rows and then copies the trailer.
 *
 * The function does not end the trailer, so fields of
 * sdfio_write_trailer_field(), for instance a line that records the change,
 * can follow the copy. sdfio_end_file() ends the file. The copy has the text
 * that sdfio_read_trailer() returns, with the line ends that
 * sdfio_write_trailer_text() writes. A file without a trailer copies nothing.
 *
 * If a call fails, sdfio_writer_error_message() describes the failure, also
 * when the reader failed.
 *
 * @param[in,out] reader The reader that has opened a file.
 * @param[in,out] writer The writer that has created a file.
 * @retval SDFIO_ERR_ARGUMENT A pointer is null.
 * @retval SDFIO_ERR_STATE The reader has no file or failed, or the writer has
 * no file, has not written all rows, or ended the file.
 * @retval SDFIO_ERR_IO A stream failed.
 * @retval SDFIO_ERR_FORMAT The file of the reader is damaged or truncated.
 */
SDFIO_API sdfio_status sdfio_copy_trailer(sdfio_reader *reader,
                                          sdfio_writer *writer);

/**
 * Ends the file: closes the trailer, the last record, flushes the stream and,
 * for a file that sdfio_create_file() created, closes the file. All rows of the
 * data area must have been written. Without trailer lines there is no trailer.
 * Nothing can be written after it.
 *
 * @param[in,out] writer The writer.
 * @retval SDFIO_ERR_STATE No file is created, rows are missing, the file is
 * ended already, or an earlier error ended the writing.
 * @retval SDFIO_ERR_IO The stream failed.
 */
SDFIO_API sdfio_status sdfio_end_file(sdfio_writer *writer);

/**
 * Returns a description of the most recent failure of the writer, or an empty
 * string if none occurred. The result is never null, and a string of the
 * library, valid for the life of the program.
 *
 * @param[in] writer The writer.
 */
SDFIO_API const char *sdfio_writer_error_message(const sdfio_writer *writer);

#ifdef __cplusplus
}
#endif

#endif /* SDFIO_WRITER_H */
