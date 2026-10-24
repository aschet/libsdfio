/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file writer_file.h
 * @brief What a file needs of a writer.
 */

#ifndef SDFIO_SRC_WRITER_FILE_H
#define SDFIO_SRC_WRITER_FILE_H

#include "owner.h"
#include <stdio.h>

/**
 * Gives a writer the file that it owns.
 *
 * @param writer The writer.
 * @param owner The functions of the file.
 */
void sdfio_writer_own(sdfio_writer *writer, sdfio_owner owner);

/**
 * Returns the message of a writer, for the code that adds a file to it.
 *
 * @param writer The writer.
 */
sdfio_message *sdfio_writer_message(sdfio_writer *writer);

/**
 * Checks that a writer can create a file, before the file is created.
 *
 * @param writer The writer.
 * @param header The header of the file.
 * @retval SDFIO_ERR_STATE The writer has created a file already.
 * @retval SDFIO_ERR_ARGUMENT The header cannot be written.
 * @retval SDFIO_ERR_UNSUPPORTED The format of the header has not been added to
 * listed.
 */
sdfio_status sdfio_writer_check(sdfio_writer *writer,
                                const sdfio_header *header);

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

#endif /* SDFIO_SRC_WRITER_FILE_H */
