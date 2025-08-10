/*
 * cat4mod.c - rpi-modswitch shared memory reader utility
 *
 * SPDX-License-Identifier: GPL-3.0
 *
 * Copyright (C) 2025 KaliAssistant
 * Author: KaliAssistant
 *
 * This file is part of the rpi-modswitch project and is licensed under the GNU
 * General Public License v3.0 or later.
 *
 * cat4mod is a lightweight userspace tool that reads the current mode/state
 * of a 2-position DIP switch from a shared memory region maintained by the
 * rpi-modswitch daemon. It can be used in scripts or other programs to
 * quickly retrieve the switch position without direct GPIO access.
 *
 * Features:
 *   - Reads single-byte ASCII mode value ('0'–'3') from shared memory.
 *   - Optional looping until the switch changes state or matches a
 *     specified character.
 *   - Configurable microsecond polling delay.
 *   - Designed for simple shell integration and automation.
 *
 * Project GitHub: https://github.com/KaliAssistant/rpi-modswitch
 *
 * This project includes components under the following licenses:
 *   - inih: BSD-3-Clause (https://github.com/benhoyt/inih)
 *
 * See LICENSE for more details.
 */


#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <math.h>
#include "utils.h"
//#include "version.h"
#include "config.h"

#define SHM_FILE "/modsw"
#define SHM_SIZE 1

static uint8_t *shm_ptr = NULL;
static int shm_fd = -1;


static int use_loop_until = 0;
static int use_specific_char = 0;
static uint8_t specific_char;

static uintmax_t delay_us = 1000;

static int setup_shm_reader(void) {
    shm_fd = shm_open(SHM_FILE, O_RDWR, 0);
    if (shm_fd < 0) {
        perror("setup.shm.cannot_open_shm_file");
        return -1;
    }
    
    shm_ptr = mmap(NULL, SHM_SIZE, PROT_READ, MAP_SHARED, shm_fd, 0);
    if (shm_ptr == MAP_FAILED) {
        perror("setup.shm.mmap_failed");
        close(shm_fd);
        return -1;
    }

    return 0;
}

static int read_byte(uint8_t *abyte) {
    if (!shm_ptr) {
        errno = EFAULT;
        return -1;
    }
    *abyte = shm_ptr[0];
    return 0;
}

static void cleanup(void) {
    if (shm_ptr) munmap((void *)shm_ptr, SHM_SIZE);
    if (shm_fd >= 0) close(shm_fd);
}

static void handle_signal(int sig) {
    (void)sig;
    cleanup();
    exit(1);
}

static void return_to_cleanup(int ret) {
    cleanup();
    exit(ret);
}

static void usage(const char *prog_name) {
    if (!prog_name) return;
    fprintf(stderr, "cat4mod - rpi-modswitch 2-position DIP switch daemon for raspberry pi\n\n");
    fprintf(stderr, "Usage: %s [-l -c char] [-s µs]\n\n", prog_name);
    fprintf(stderr, "-l :\tloop until change\n");
    fprintf(stderr, "    -c :\tspecific char (ascii)\n\n");
    fprintf(stderr, "-s :\tdelay µs per read\n");
    fprintf(stderr, "-h :\tshow this help\n");
    fprintf(stderr, "-v : \tshow version\n\n");
    fprintf(stderr, "Version %s By KaliAssistant\n", VERSION);
    fprintf(stderr, "Github: https://github.com/KaliAssistant/rpi-modswitch.git\n");
    return;
}

int main(int argc, char **argv) {
    int opt;
    while ((opt = getopt(argc, argv, "lc:hvs:")) != -1) {
        switch (opt) {
            case 'l': use_loop_until = 1; break;
            case 'c':
                use_specific_char = 1;
                if (!xstr2char(optarg, (char *)&specific_char)) {
                    perror("main.optarg.cannot_parse_specific_char");
                    return 1;
                }
                break;
            case 'h': usage(argv[0]); return 0;
            case 's':
                if (!xstr2umax(optarg, 10, &delay_us)) {
                    perror("main.optarg.cannot_parse_delay_us");
                    return 1;
                }
                break;
            case 'v':
                fprintf(stdout, "%s\n", VERSION); 
                return 0;
            case '?':
                fprintf(stderr, "See '%s -h' for help.\n", argv[0]);
                return 1;
            default:
                errno = EFAULT;
                perror("main.getopt.got_impossible_default");
                abort();
        }
    }

    signal(SIGINT, handle_signal);
    signal(SIGBUS, handle_signal);
    signal(SIGSEGV, handle_signal);
    signal(SIGTERM, handle_signal);

    if (setup_shm_reader() < 0) return 1;
    
        
    if (!use_loop_until) {
        uint8_t modbyte;
        if (read_byte(&modbyte) < 0) {
            perror("main.read.read_shm_byte_failed");
            return_to_cleanup(1);
        }
        fprintf(stdout, "%c\n", modbyte);
        return_to_cleanup(0);
    }

    uint8_t last_byte;
    if (read_byte(&last_byte) < 0) {
        perror("main.read.read_shm_byte_failed");
        return_to_cleanup(1);
    }

    while (1) {
        uint8_t modbyte;
        if (read_byte(&modbyte) < 0) {
            perror("main.read.read_shm_byte_failed");
            return_to_cleanup(1);
        }

        char c = (char)modbyte;

        if (use_specific_char) {
            if (c == specific_char) {
                fprintf(stdout, "%c\n", c);
                return_to_cleanup(0);
            }
        } else {
            if (c != (char)last_byte) {
                fprintf(stdout, "%c\n", c);
                return_to_cleanup(0);
            }
        }
        usleep(delay_us);
    }    
    return_to_cleanup(0);
}
