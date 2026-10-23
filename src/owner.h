/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file owner.h
 * @brief What a file adds to a reader or a writer, and the functions of the
 * files that use the handles.
 */

#ifndef SDFIO_SRC_OWNER_H
#define SDFIO_SRC_OWNER_H

#include <stdio.h>

#include "common.h"

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
 * @retval SDFIO_ERR_UNSUPPORTED The writer codec of the representation is not
 * listed.
 */
sdfio_status sdfio_writer_check(sdfio_writer *writer,
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

#endif /* SDFIO_SRC_OWNER_H */
