/*
 * modswitchd.c - rpi-modswitch 2-position DIP switch daemon using shared memory
 *
 * SPDX-License-Identifier: GPL-3.0
 *
 * Copyright (C) 2025 KaliAssistant
 * Author: KaliAssistant
 *
 * This file is part of the rpi-modswitch project and is licensed under the GNU
 * General Public License v3.0 or later.
 *
 * rpi-modswitch is a lightweight userspace daemon for Raspberry Pi that monitors
 * a 2-position DIP switch via the Linux GPIO character device API and publishes
 * its state to a shared memory region. External applications can read this shared
 * memory to determine the current switch position, enabling simple mode selection
 * or configuration changes without restarting services or reading GPIO directly.
 *
 * Features:
 *   - Reads DIP switch state using /dev/gpiochipN line handles.
 *   - Configurable GPIO pins, pull-up/pull-down mode, and polling delay.
 *   - Single-byte shared memory output (ASCII '0', '1', '2', or '3').
 *   - Daemon mode support for SysVinit-based systems.
 *   - Prevents multiple instances via PID lock file.
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
#include <fcntl.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <inttypes.h>
#include <signal.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/file.h>
#include <linux/gpio.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <getopt.h>
#include "ini.h"
#include "utils.h"
//#include "version.h"
#include "config.h"

#define SHM_FILE "/modsw"
#define SHM_SIZE 1        // ascii number 0x30 + 0, 1, 2, 3; use 1 byte only.

#define LOCK_FILE "/var/run/modswitch.lock"
#define MODSWITCH_CONF_FILE "/etc/modswitch/modswitch.conf"
#define MAIN_GPIOCHIP "/dev/gpiochip0"

#define DEFAULT_CONF_SW0_GPIO 10
#define DEFAULT_CONF_SW1_GPIO  7
#define DEFAULT_CONF_GPIO_PULLUPDOWN 1      // 1 = PULLUP; 0 = PULLDOWN
#define DEFAULT_CONF_DELAY_US 1000

static int is_daemon = 0;
static char *modswitch_conf_file = MODSWITCH_CONF_FILE;


typedef struct modswitch_conf_t {
    int sw0_pin;
    int sw1_pin;
    int pullupdown;
    uintmax_t delay_us;
}modswitch_conf_t;

static int lock_fd = -1;
static int shm_fd = -1;
static uint8_t *shm_ptr = NULL;
static int gpio_fd = -1;
static int gpio_line_fd = -1;

static modswitch_conf_t modswitch_default_conf = {
    .sw0_pin = DEFAULT_CONF_SW0_GPIO,
    .sw1_pin = DEFAULT_CONF_SW1_GPIO,
    .pullupdown = DEFAULT_CONF_GPIO_PULLUPDOWN,
    .delay_us = DEFAULT_CONF_DELAY_US
};

static const int available_switch_gpio[] = {
     0,  1,  2,  3,  4,  5,  6,  7,
     8,  9, 10, 11, 12, 13, 14, 15,
    16, 17, 18, 19, 20, 21, 22, 23,
    24, 25, 26, 27,
};

static int conf_handler(void *user, const char *section, const char *name, const char *value) {
    modswitch_conf_t *config = (modswitch_conf_t *)user;
    #define CONF_MATCH(s, n) strcmp(section, s) == 0 && strcmp(name, n) == 0
    if (CONF_MATCH("gpio", "sw0_pin")) {
        config->sw0_pin = atoi(value);
    } else if (CONF_MATCH("gpio", "sw1_pin")) {
        config->sw1_pin = atoi(value);
    } else if (CONF_MATCH("gpio", "pullupdown")) {
        config->pullupdown = atoi(value);
    } else if (CONF_MATCH("user", "delay_us")) {
        return xstr2umax(value, 10, &config->delay_us);
    } else {
        return 0;
    }
    return 1;
}

static int conf_checker(const modswitch_conf_t *conf) {
    if (!conf) {
        errno = EFAULT;
        perror("conf.ini_checker.got_null_conf");
        abort();
    }
    if (!int_in_list(conf->sw0_pin, available_switch_gpio, sizeof(available_switch_gpio)/sizeof(int))) {
        fprintf(stderr, "conf.ini_checker.invalid_config: invalid switch 0 pin: %d\n", conf->sw0_pin);
        return -1;
    }
    if (!int_in_list(conf->sw1_pin, available_switch_gpio, sizeof(available_switch_gpio)/sizeof(int))) {
        fprintf(stderr, "conf.ini_checker.invalid_config: invalid switch 1 pin: %d\n", conf->sw1_pin);
        return -1;
    }
    if (conf->pullupdown > 1 || conf->pullupdown < 0) {
        fprintf(stderr, "conf.ini_checker.invalid_config: invalid pullupdown mode: %d\n", conf->pullupdown);
        return -1;
    }

    return 0;
}

static void cleanup() {
    if (gpio_line_fd >= 0)
        close(gpio_line_fd);
    if (gpio_fd >= 0)
        close(gpio_fd);
    if (shm_ptr)
        munmap(shm_ptr, SHM_SIZE);
    if (shm_fd >= 0) {
        close(shm_fd);
        shm_unlink(SHM_FILE);
    }
    if (lock_fd >= 0) {
        flock(lock_fd, LOCK_UN);
        close(lock_fd);
    }
}

static void signal_handler(int signum)
{
    (void)signum;
    cleanup();
    exit(0);
}

static int setup_gpio() {
    gpio_fd = open(MAIN_GPIOCHIP, O_RDONLY);
    if (gpio_fd < 0) {
        perror("gpio.setup.cannot_open_gpiochip");
        return -1;
    }

    struct gpiohandle_request req = {0};
    req.lineoffsets[0] = modswitch_default_conf.sw0_pin;
    req.lineoffsets[1] = modswitch_default_conf.sw1_pin;
    req.lines = 2;
    
    if (modswitch_default_conf.pullupdown)
        req.flags = GPIOHANDLE_REQUEST_INPUT | GPIOHANDLE_REQUEST_BIAS_PULL_UP;
    else
        req.flags = GPIOHANDLE_REQUEST_INPUT | GPIOHANDLE_REQUEST_BIAS_PULL_DOWN;

    if (ioctl(gpio_fd, GPIO_GET_LINEHANDLE_IOCTL, &req) < 0) {
        perror("gpio.setup.get_linehandle_ioctl_failed");
        close(gpio_fd);
        return -1;
    }

    gpio_line_fd = req.fd;
    return 0;
}

static int get_gpio(int *sw0_ptr, int *sw1_ptr) {
    struct gpiohandle_data data;
    if (ioctl(gpio_line_fd, GPIOHANDLE_GET_LINE_VALUES_IOCTL, &data) < 0) {
        perror("gpio.get.get_line_values_ioctl_failed");
        return -1;
    }
    *sw0_ptr = data.values[0];
    *sw1_ptr = data.values[1];
    return 0;
}

static void usage(const char *prog_name) {
    if (!prog_name) return;
    fprintf(stderr, "modswitchd - rpi-modswitch 2-position DIP switch daemon for raspberry pi\n\n");
    fprintf(stderr, "Usage: %s -c <config file> [-Dhv]\n\n", prog_name);
    fprintf(stderr, "-c :\t<modswitch.conf>, modswitch config file, default is '/etc/modswitch/modswitch.conf'\n");
    fprintf(stderr, "-D :\trun as daemon mode (SysVinit)\n");
    fprintf(stderr, "-h :\tshow this help\n");
    fprintf(stderr, "-v :\tshow version\n\n");
    fprintf(stderr, "Version %s By KaliAssistant\n", VERSION);
    fprintf(stderr, "Github: https://github.com/KaliAssistant/rpi-modswitch.git\n");
    return;
}

int main(int argc, char **argv) {
    int opt;
    while ((opt = getopt(argc, argv, "c:Dhv")) != -1) {
        switch (opt) {
            case 'h':
                usage(argv[0]);
                return 1;
            case 'v':
                fprintf(stdout, "%s\n", VERSION);
                return 0;
            case 'c':
                modswitch_conf_file = optarg;
                break;
            case 'D':
                is_daemon = 1;
                break;
            case '?':
                fprintf(stderr, "See '%s -h' for help.\n", argv[0]);
                return 1;
            default:
                errno = EFAULT;
                perror("main.getopt.got_impossible_default");
                abort();
        }
    }

    for (int i = optind; i < argc; i++) {
        fprintf(stderr, "main.getopt.got_non_option_warning: got non-option argument '%s'.\n", argv[i]);
    }

    int ini_parse_err = ini_parse(modswitch_conf_file, conf_handler, &modswitch_default_conf);
    if (ini_parse_err < 0) {
        perror("main.conf_parse.cannot_load_conf");
        return 1;
    } else if (ini_parse_err) {
        fprintf(stderr, "main.conf_parse.bad_conf_file: bad config file (first error on line %d)\n", ini_parse_err);
        return 1;
    }
    
    if (conf_checker(&modswitch_default_conf) < 0) {
        fprintf(stderr, "main.conf_prarse.conf_checker_error: invalid configuration.\n");
        return 1;
    }

    

    lock_fd = open(LOCK_FILE, O_CREAT | O_RDWR, 0644);
    if (lock_fd < 0) {
        perror("main.process.cannot_open_lock_file");
        return 1;
    }
    if (flock(lock_fd, LOCK_EX | LOCK_NB) < 0) {
        if (errno == EWOULDBLOCK) {
            fprintf(stderr, "main.process.flock_error: another instance is already running.\n");
            return 1;
        } else {
            perror("main.process.flock_error");
            return 1;
        }
    }

    if (is_daemon) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("main.process.daemon_fork_failed");
            return 1;
        }
        if (pid > 0) return 0;
        if (setsid() < 0) {
            perror("main.process.daemon_setsid_failed");
            return 1;
        }
        close(0); close(1); close(2);
        open("/dev/null", O_RDONLY);
        open("/dev/null", O_WRONLY);
        open("/dev/null", O_RDWR);
        chdir("/");
    }
    ftruncate(lock_fd, 0);
    dprintf(lock_fd, "%d\n", getpid());

    if (setup_gpio() < 0) {
        fprintf(stderr, "main.process.setup_gpio: cannot setup gpio.\n");
        return 1;
    }

    shm_fd = shm_open(SHM_FILE, O_RDWR | O_CREAT, 0666);
    if (shm_fd < 0) {
        perror("main.process.cannot_open_shm_file");
        cleanup();
        return 1;
    }
    ftruncate(shm_fd, SHM_SIZE);
    shm_ptr = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm_ptr == MAP_FAILED) {
        perror("main.process.mmap_failed");
        cleanup();
        return 1;
    }

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    while (1) {
        int sw0, sw1;
        if (get_gpio(&sw0, &sw1) < 0) {
            cleanup();
            return 1;
        }
        uint8_t combined = (((modswitch_default_conf.pullupdown ? !sw1 : sw1) << 1) | (modswitch_default_conf.pullupdown ? !sw0 : sw0)) & 0x03;
        shm_ptr[0] = combined + '0'; // ascii
        usleep(modswitch_default_conf.delay_us);
    }
    cleanup();
    return 0;
}
