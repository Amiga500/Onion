/* SPDX-License-Identifier: GPL-3.0-only */
#include "app/options.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Set by the build system; see VERSION in the Makefile. */
#ifndef MAINUI_VERSION
#define MAINUI_VERSION "0-dev"
#endif

int mainui_options_parse(MainUIOptions *options, int argc, char **argv)
{
    *options = (MainUIOptions){.battery_percent = 100};
#ifdef MAINUI_ONION
    options->sd = "/mnt/SDCARD";
    options->handoff_dir = "/tmp";
    options->real_device = true;
#endif
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--version")) {
            puts("MainUI " MAINUI_VERSION);
            options->informational = true;
            return 0;
        }
        if (!strcmp(argv[i], "--help")) {
            puts("MainUI " MAINUI_VERSION "\n"
                 "--theme DIR --fallback DIR --config-dir "
                 "DIR [--sd-root DIR | --list NDJSON] [--system LABEL] [--snapshot BMP] [--elapsed "
                 "MS]\n"
                 "Menus: arrows select, Enter opens, Escape returns. Lists: Right opens details.\n"
                 "Escape returns to the main menu; Escape there does nothing. Game launch "
                 "requires the Onion runtime.\n"
                 "--handoff-dir DIR publishes host launch/return protocol files in an existing "
                 "directory.\n"
                 "--systems starts at the console grid. --input UDLREBSXYTM1234 drives "
                 "deterministic "
                 "navigation (prefix - for keyup) before a snapshot. --text NAME seeds the next "
                 "name field. --battery 0..100 / 500 / -1 "
                 "sets simulated header "
                 "status. --device-dir DIR reads simulated device status. --device real uses "
                 "Onion /tmp status, wpa_supplicant control and keymon signals (Linux only).\n"
                 "--refresh-caches rebuilds every console's ROM cache before the first frame "
                 "(exit status 4 if a rebuild fails). --version prints the version. --help "
                 "prints this text.");
            options->informational = true;
            return 0;
        }
        if (!strcmp(argv[i], "--refresh-caches")) {
            options->refresh_caches = true;
            continue;
        }
        if (!strcmp(argv[i], "--systems")) {
            options->start_systems = true;
            continue;
        }
        if (i + 1 >= argc) {
            fprintf(stderr, "Missing option value\n");
            return 2;
        }
        const char *arg = argv[i++], *value = argv[i];
        if (!strcmp(arg, "--sd-root")) {
            options->sd = value;
        }
        else if (!strcmp(arg, "--handoff-dir")) {
            options->handoff_dir = value;
        }
        else if (!strcmp(arg, "--device-dir")) {
            options->device_directory = value;
        }
        else if (!strcmp(arg, "--device")) {
            if (strcmp(value, "real")) {
                fprintf(stderr, "--device expects real\n");
                return 2;
            }
            options->real_device = true;
        }
        else if (!strcmp(arg, "--system")) {
            options->system_name = value;
        }
        else if (!strcmp(arg, "--text")) {
            options->input_text = value;
        }
        else if (!strcmp(arg, "--input")) {
            options->input_script = value;
        }
        else if (!strcmp(arg, "--theme")) {
            options->dir = value;
        }
        else if (!strcmp(arg, "--fallback")) {
            options->base = value;
        }
        else if (!strcmp(arg, "--config-dir")) {
            options->config_dir = value;
        }
        else if (!strcmp(arg, "--list")) {
            options->list_path = value;
        }
        else if (!strcmp(arg, "--snapshot")) {
            options->snapshot = value;
        }
        else if (!strcmp(arg, "--battery")) {
            char *end = NULL;
            long number = strtol(value, &end, 10);
            if (!*value || *end ||
                (number != -1 && number != 500 && (number < 0 || number > 100))) {
                fprintf(stderr, "--battery expects 0..100, 500 (charging), or -1 (hidden)\n");
                return 2;
            }
            options->battery_percent = (int)number;
            options->battery_override = true;
        }
        else if (!strcmp(arg, "--elapsed")) {
            options->snapshot_elapsed = (unsigned)strtoul(value, NULL, 10);
        }
        else {
            fprintf(stderr, "Unknown option: %s\n", arg);
            return 2;
        }
    }
    if ((!options->sd && !options->list_path) || (options->sd && options->list_path) ||
        (!options->sd && !options->dir) || (options->system_name && !options->sd) ||
        (options->handoff_dir && !options->sd) || (options->input_script && !options->snapshot)) {
        fprintf(
            stderr,
            "Choose --sd-root or --theme with --list; --input requires --snapshot; see --help\n");
        return 2;
    }
    return -1;
}
