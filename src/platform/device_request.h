/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MAINUI_DEVICE_REQUEST_H
#define MAINUI_DEVICE_REQUEST_H
#include <stdbool.h>
bool mainui_device_shutdown(const char *directory);
/* Returns an owned command containing bounded, shell-quoted wpa_cli arguments.
 * No command is executed here. Empty password requests an open network. */
char *mainui_device_wifi_command(const char *ssid, const char *password);
bool mainui_device_wifi_request(const char *directory, const char *ssid, const char *password);
#endif
