/* SPDX-License-Identifier: GPL-3.0-only */
#include "platform/device_request.h"
#include "platform/files.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool publish(const char *directory, const char *name, const char *text)
{
    char path[4096];
    int n = directory ? snprintf(path, sizeof path, "%s/%s", directory, name) : -1;
    return n > 0 && n < (int)sizeof path && mainui_write_bytes_new(path, text, strlen(text));
}

bool mainui_device_shutdown(const char *directory)
{
    return publish(directory, ".offOrder", "");
}

static bool quoted(char out[512], const char *input)
{
    size_t n = 0;
    out[n++] = '\'';
    out[n++] = '"';
    for (const unsigned char *p = (const unsigned char *)input; *p; p++) {
        /* wpa_supplicant parses these inner quotes itself; escaping only the
         * shell cannot preserve embedded quotes/backslashes in that grammar. */
        if (*p < 32 || *p == 127 || *p == '"' || *p == '\\' || n > 505) {
            return false;
        }
        if (*p == '\'') {
            memcpy(out + n, "'\\''", 4);
            n += 4;
        }
        else {
            out[n++] = (char)*p;
        }
    }
    out[n++] = '"';
    out[n++] = '\'';
    out[n] = 0;
    return true;
}

char *mainui_device_wifi_command(const char *ssid, const char *password)
{
    if (!ssid || !password || !*ssid || strlen(ssid) > 32 ||
        (*password && (strlen(password) < 8 || strlen(password) > 63))) {
        return NULL;
    }
    char name[512], secret[512];
    if (!quoted(name, ssid) || !quoted(secret, password)) {
        return NULL;
    }
    char *command = calloc(1, 2048);
    if (!command) {
        return NULL;
    }
    size_t used = 0;
    for (int i = 0; i < 16; i++) {
        used += (size_t)snprintf(command + used, 2048 - used, "wpa_cli   remove_network %d\n", i);
    }
    int n = snprintf(command + used, 2048 - used,
                     "wpa_cli   add_network 0\nwpa_cli   set_network 0 ssid %s\n"
                     "wpa_cli   set_network 0 %s %s\nwpa_cli   select_network 0\n"
                     "wpa_cli   enable_network 0\nwpa_cli   save_config\n",
                     name, *password ? "psk" : "key_mgmt", *password ? secret : "NONE");
    if (n < 0 || (size_t)n >= 2048 - used) {
        free(command);
        return NULL;
    }
    return command;
}

bool mainui_device_wifi_request(const char *directory, const char *ssid, const char *password)
{
    char *command = mainui_device_wifi_command(ssid, password);
    bool ok = command && publish(directory, "mainui-wifi-request.sh", command);
    if (command) {
        memset(command, 0, strlen(command));
    }
    free(command);
    return ok;
}
