/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MAINUI_OPTIONS_H
#define MAINUI_OPTIONS_H
#include <stdbool.h>

typedef struct {
    const char *dir;
    const char *base;
    const char *config_dir;
    const char *list_path;
    const char *snapshot;
    const char *handoff_dir;
    const char *device_directory;
    const char *sd;
    const char *system_name;
    const char *input_script;
    const char *input_text;
    bool real_device;
    bool battery_override;
    bool start_systems;
    bool refresh_caches;
    /* --help or --version: printed text only, no session and no timing report. */
    bool informational;
    unsigned snapshot_elapsed;
    int battery_percent;
} MainUIOptions;

/* -1 means run; 0 means help; 2 means invalid arguments. Borrows argv strings. */
int mainui_options_parse(MainUIOptions *options, int argc, char **argv);
#endif
