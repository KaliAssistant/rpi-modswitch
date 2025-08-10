/*
 * utils.h - rpi-modswitch common utility functions
 *
 * SPDX-License-Identifier: GPL-3.0
 *
 * Copyright (C) 2025 KaliAssistant
 * Author: KaliAssistant
 *
 * This file is part of the rpi-modswitch project and is licensed under the GNU
 * General Public License v3.0 or later.
 *
 * This file implements helper functions shared across rpi-modswitch programs,
 * providing consistent, safe parsing and list-checking utilities.
 *
 * Functions:
 *   - xstr2umax(): Convert string to uintmax_t with validation.
 *   - xstr2char(): Convert single-character string to char with validation.
 *   - int_in_list(): Check if an integer is in a given integer list.
 *   - str_in_list(): Check if a string is in a given string list.
 *
 * These functions are designed for strict input validation and error handling,
 * ensuring robustness in command-line argument parsing and configuration loading.
 *
 * Project GitHub: https://github.com/KaliAssistant/rpi-modswitch
 *
 * This project includes components under the following licenses:
 *   - inih: BSD-3-Clause (https://github.com/benhoyt/inih)
 *
 * See LICENSE for more details.
 */

#ifndef UTILS_H
#define UTILS_H

#include <inttypes.h>


/**
 * Convert a string representing an unsigned integer in a given base
 * to a uintmax_t value.
 *
 * @param str   The input string to convert.
 * @param base  The numeric base to interpret the string (e.g., 10, 16).
 * @param val   Pointer to store the converted uintmax_t value.
 * @return      true if conversion succeeded and string fully parsed, false otherwise.
 */
bool xstr2umax(const char *str, int base, uintmax_t *val);

/**
 * Convert a string containing a single character to a char value.
 *
 * @param str   The input string to convert; must be exactly one character long.
 * @param val   Pointer to store the converted char value.
 * @return      true if the string length is exactly one character and conversion succeeded,
 *              false otherwise.
 */
bool xstr2char(const char *str, char *val);

/**
 * Check if an integer value is present in a list of integers.
 *
 * @param value The integer value to check.
 * @param list  Pointer to an array of integers.
 * @param len   Number of elements in the list.
 * @return      true if value is found in the list, false otherwise.
 */
bool int_in_list(int value, const int *list, size_t len);

/**
 * Check if a string is present in a list of strings.
 *
 * @param s     The string to check.
 * @param list  Array of string pointers.
 * @param len   Number of strings in the list.
 * @return      true if the string is found, false otherwise.
 */
bool str_in_list(const char *s, const char * const list[], size_t len);


#endif /* UTILS_H */
