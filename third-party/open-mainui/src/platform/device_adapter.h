/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MAINUI_DEVICE_ADAPTER_H
#define MAINUI_DEVICE_ADAPTER_H
#include "core/core.h"

typedef enum {
    DEVICE_SIMULATED,
    DEVICE_ONION
} MainUIDeviceBackend;

typedef struct {
    MainUIDevice model;
    int wifi_signal;         /* Stock level: 0 unknown, 1 weak, 2 medium, 3 strong. */
    int battery;             /* -1 unavailable, 0..100 percent, 500 charging at an unknown level */
    bool charging;           /* also with a known level (Mini Plus and Flip) */
    int wifi, lid, sleeping; /* -1 unknown; otherwise 0/1 */
    int hotspot;             /* 1 when Onion's hotspot (hostapd) runs, else 0 */
    char address[64], ssid[33];
} MainUIDeviceStatus;

/* Injectable command transport: returns a complete, bounded NUL-terminated reply.
 * Each call must honour cancellation and a bounded timeout. */
typedef bool (*MainUIWifiTransport)(void *, const char *, char *, size_t, MainUICancel);

typedef struct {
    MainUIDeviceBackend backend;
    char runtime[4096];
    char error[160];
    MainUIWifiTransport transport;
    void *transport_context;
} MainUIDeviceAdapter;

bool mainui_device_adapter_open(MainUIDeviceAdapter *, MainUIDeviceBackend, const char *runtime);
/* Only the battery level and charging state of `status`, for its model: as
 * Onion's battery_hasChanged(), cheap enough to check every second. On the
 * Mini Plus and Flip charging comes from axp_test, run off this thread and
 * cached for 2 s. */
void mainui_device_battery(const MainUIDeviceAdapter *, MainUIDeviceStatus *status);
MainUIDeviceStatus mainui_device_status(const MainUIDeviceAdapter *);
bool mainui_device_connect(MainUIDeviceAdapter *, const char *ssid, const char *password,
                           MainUICancel);
/* Connect to a network wpa_supplicant has saved, with its saved credentials.
 * False when the SSID is not saved or cannot be selected. */
bool mainui_device_connect_saved(MainUIDeviceAdapter *, const char *ssid, MainUICancel);
int mainui_wifi_signal_level(int dbm);
/* A line /customer/app/axp_test prints says the battery is charging: its
 * "charging" field is 2 or 3, as stock MainUI reads it (Onion's battery.h
 * takes only 3). */
bool mainui_axp_charging(const char *line);
bool mainui_device_refresh(MainUIDeviceAdapter *, MainUICancel);
bool mainui_device_wifi_enable(MainUIDeviceAdapter *, bool enabled, MainUICancel);
bool mainui_device_scan(MainUIDeviceAdapter *, MainUICancel);
bool mainui_device_power_off(const MainUIDeviceAdapter *);
/* Apply the Onion backlight curve, or write a simulated duty_cycle for tests. */
/* Update the stock keymon shared-memory value before notifying its reader. */
bool mainui_device_setting_sync(const MainUIDeviceAdapter *, const char *key, int value);
/* keymon's current value of `key`; false when it keeps none or is unavailable. */
bool mainui_device_setting_value(const MainUIDeviceAdapter *, const char *key, int *value);
bool mainui_device_brightness(const MainUIDeviceAdapter *, int value);
/* As stock does on every start: fill keymon's live copy of the settings from
 * system.json, then set the display to its brightness. Without it a value
 * another program saved to the file only (GameSwitcher's brightness) is put
 * back to the old one by keymon. Not with a damaged system.json, whose
 * defaults keymon could save over the user's settings. */
bool mainui_device_settings_load(const MainUIDeviceAdapter *, const char *sd);
/* Preferences are consumed by keymon; there is no guessed direct suspend IPC. */
bool mainui_device_settings_changed(const MainUIDeviceAdapter *);
/* Real runtime only. Signal handlers just set flags; UI consumes them. */
bool mainui_device_signals_install(void);
void mainui_device_signals_take(bool *resume, bool *quit);
#endif
