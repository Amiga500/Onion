/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MAINUI_SETTINGS_PAGE_H
#define MAINUI_SETTINGS_PAGE_H
#include "platform/device_info.h"
#include "ui/panels.h"
#include <stdatomic.h>

typedef struct {
    bool open, managed_device, audio_available;
    int sleeping, wifi_connection;
    char device_status[128], connected_ssid[33];
    MainUISettingKind kind;
    int selected, start, values[4], wifi, network_count;
    char ssid[33], password[64], version[128], firmware[128], model[64];
    char device_name[64], max_resolution[32];
    /* Fresh native or simulated readings; missing fields remain unavailable. */
    char serial[128], cpu[64], memory[64], storage[128];
    char networks[32][33], message[256];
    int network_signal[32];
    bool network_secure[32], connect_after_password;
} MainUISettingsPage;

typedef struct {
    SDL_Thread *thread;
    atomic_bool done;
    bool ready, visible;
    MainUIDeviceAdapter adapter;
    char sd[4096];
    MainUISettingsPage cached, pending;
} MainUIAboutJob;

/* File/kernel probes run off the UI thread; cached fields display immediately. */
void mainui_about_start(MainUIAboutJob *, const MainUIDeviceAdapter *, const char *sd);
void mainui_about_update(MainUIAboutJob *, MainUISettingsPage *);
void mainui_about_close(MainUIAboutJob *);

void mainui_settings_page_open(MainUISettingsPage *page, MainUISettingKind kind, const char *sd,
                               const char *runtime);
void mainui_settings_page_device_info(MainUISettingsPage *, const MainUIDeviceAdapter *,
                                      const char *sd);
void mainui_settings_page_scan_results(MainUISettingsPage *, const char *runtime);
/* Managed adapters return 3=connect, 4=scan to the asynchronous dispatcher.
 * 1 requests the SSID keyboard; 2 requests the password keyboard. */
int mainui_settings_page_key(MainUISettingsPage *page, SDLKey key, const char *sd,
                             const char *runtime);
void mainui_settings_page_draw(const MainUISettingsPage *page, SDL_Surface *screen,
                               MainUITheme *theme);
#endif
