/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief What a file needs of a reader.
 */

#ifndef SDFIO_SRC_READER_FILE_H
#define SDFIO_SRC_READER_FILE_H

#include "owner.h"
#include <stdio.h>

/**
 * Gives a reader the file that it owns.
 *
 * @param reader The reader.
 * @param owner The functions of the file.
 */
void sdfio_reader_own(sdfio_reader *reader, sdfio_owner owner);

/**
 * Returns the message of a reader, for the code that adds a file to it.
 *
 * @param reader The reader.
 */
sdfio_message *sdfio_reader_message(sdfio_reader *reader);

/**
 * Tells whether a file is open in a reader.
 *
 * @param reader The reader.
 */
bool sdfio_reader_is_open(const sdfio_reader *reader);

/**
 * Checks that a reader can open a file.
 *
 * @param reader The reader.
 * @retval SDFIO_ERR_STATE The reader is open already.
 */
sdfio_status sdfio_reader_check(sdfio_reader *reader);

/**
 * Opens a file that is open already as the source of a reader. The reader
 * closes the file, also if this fails.
 *
 * @param reader The reader.
 * @param file The file, or null if it could not be opened.
 * @retval SDFIO_ERR_IO The file is null.
 */
sdfio_status sdfio_file_adopt_reader(sdfio_reader *reader, FILE *file);

#endif /* SDFIO_SRC_READER_FILE_H */
