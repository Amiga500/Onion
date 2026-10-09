/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MAINUI_DEVICE_JOB_H
#define MAINUI_DEVICE_JOB_H
#include "platform/device_adapter.h"
#include <SDL.h>
#include <stdatomic.h>

/* SDL_USEREVENT code posted when a status refresh (operation 0) finishes. */
#define MAINUI_STATUS_CODE 2

typedef struct {
    SDL_Thread *thread;
    atomic_bool cancel, done;
    MainUIDeviceAdapter adapter;
    int operation; /* 0 status, 3 connect, 4 scan, 5 enable, 6 disable */
    int queued_operation;
    char queued_ssid[33], queued_password[64];
    bool success;
    char ssid[33], password[64];
} MainUIDeviceJob;

bool mainui_device_job_start(MainUIDeviceJob *, const MainUIDeviceAdapter *, int, const char *,
                             const char *);
/* Returns -1 while pending, 0 on failure/cancel, 1 on success. */
int mainui_device_job_take(MainUIDeviceJob *);
void mainui_device_job_close(MainUIDeviceJob *);
#endif
