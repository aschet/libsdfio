/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file dates.h
 * @brief Declares the functions that format, parse and validate the date fields
 * of the header and that obtain the current time.
 */

#ifndef SDFIO_SRC_DATES_H
#define SDFIO_SRC_DATES_H

#include "common.h"
#include "fields.h"

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

#endif /* SDFIO_SRC_DATES_H */
