/*
 * utils.c - rpi-modswitch common utility functions
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


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <math.h>


bool xstr2umax(const char *str, int base, uintmax_t *val) {
    errno = 0;
    char *endptr;
    *val = strtoumax(str, &endptr, base);
    
    if (*endptr != '\0' || str == endptr) {
        errno = EINVAL;
        return false;
    }
    if (errno == ERANGE) return false;
    return true;
}

bool xstr2char(const char *str, char *val) {
    errno = 0;
    if (strlen(str) != 1) {
        errno = EINVAL;
        return false;
    }
    *val = str[0];
    return true;
}

bool int_in_list(int value, const int *list, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (list[i] == value)
            return true;
    }
    return false;
}

bool str_in_list(const char *s, const char * const list[], size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (strcmp(s, list[i]) == 0)
            return true;
    }
    return false;
}

