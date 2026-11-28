#ifndef TWEAKS_NETWORK_H__
#define TWEAKS_NETWORK_H__

#include <SDL/SDL_image.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

#include "components/list.h"
#include "system/keymap_sw.h"
#include "theme/render/dialog.h"
#include "theme/sound.h"
#include "utils/apply_icons.h"
#include "utils/json.h"
#include "utils/keystate.h"
#include "utils/netinfo.h"
#include "utils/process.h"

#include "./appstate.h"

#define NET_SCRIPT_PATH "/mnt/SDCARD/.tmp_update/script/network"
#define SMBD_CONFIG_PATH "/mnt/SDCARD/.tmp_update/config/smb.conf"

static struct network_s {
    bool smbd;
    bool http;
    bool ssh;
    bool telnet;
    bool ftp;
    bool hotspot;
    bool ntp;
    bool ntp_wait;
    bool auth_smbd;
    bool auth_ftp;
    bool auth_http;
    bool auth_ssh;
    bool manual_tz;
    bool check_updates;
    bool keep_alive;
    bool vncserv;
    bool syncthing;
    bool loaded;
    bool force_wifi_on_startup;
    int vncfps;
} network_state = {
    .vncfps = 20,
};

void network_loadState(void)
{
    if (network_state.loaded)
        return;

    network_state.smbd = config_flag_get(".smbdState");
    network_state.http = config_flag_get(".httpState");
    network_state.ssh = config_flag_get(".sshState");
    network_state.telnet = config_flag_get(".telnetState");
    network_state.ftp = config_flag_get(".ftpState");
    network_state.hotspot = config_flag_get(".hotspotState");
    network_state.ntp = config_flag_get(".ntpState");
    network_state.ntp_wait = config_flag_get(".ntpWait");
    network_state.force_wifi_on_startup = config_flag_get(".ntpForce");
    network_state.auth_ftp = config_flag_get(".authftpState");
    network_state.auth_http = config_flag_get(".authhttpState");
    network_state.auth_ssh = config_flag_get(".authsshState");
    network_state.manual_tz = config_flag_get(".manual_tz");
    network_state.check_updates = config_flag_get(".checkUpdates");
    network_state.keep_alive = config_flag_get(".keepServicesAlive");
    network_state.vncserv = config_flag_get(".vncServer");
    network_state.syncthing = config_flag_get(".syncthingState");
    config_get(".vncfps", CONFIG_INT, &network_state.vncfps);
    network_state.loaded = true;
}

typedef struct {
    char name[STR_MAX - 11];
    char path[STR_MAX];
    int available;     // 1 if available = yes, 0 otherwise
    long availablePos; // in file position for the available property
} Share;

static Share *_network_shares = NULL;
static int network_numShares;

void network_freeSmbShares()
{
    free(_network_shares);
    _network_shares = NULL;
    network_numShares = 0;
}

// smb.conf invites edits ("## Edit your shares below"): a line is read
// whatever its indentation and line ends (CRLF), "path" and "available" only
// inside a share, a share without "available" is on (Samba's default) and
// gets no toggle position, so a toggle never rewrites another share's line.
void network_getSmbShares()
{
    if (_network_shares != NULL) {
        return;
    }

    int numShares = 0;

    FILE *file = fopen(SMBD_CONFIG_PATH, "r");
    if (file == NULL) {
        printf("Failed to open smb.conf file.\n");
        return;
    }

    char line[STR_MAX];
    Share *share = NULL; // the share whose lines are being read

    for (;;) {
        long lineStart = ftell(file);
        if (fgets(line, sizeof(line), file) == NULL)
            break;
        line[strcspn(line, "\r\n")] = '\0';
        char *t = line;
        while (*t == ' ' || *t == '\t')
            t++;
        size_t len = strlen(t);
        if (len == 0)
            continue;

        if (t[0] == '[' && t[len - 1] == ']') {
            share = NULL;
            t[len - 1] = '\0';
            char *shareName = t + 1;
            if (*shareName == '\0' || strcmp(shareName, "global") == 0)
                continue;

            Share *grown = (Share *)realloc(_network_shares, (numShares + 1) * sizeof(Share));
            if (grown == NULL)
                break;
            _network_shares = grown;
            share = &_network_shares[numShares++];
            memset(share, 0, sizeof(*share));
            share->available = 1;
            share->availablePos = -1;

            if (strncmp("__", shareName, 2) == 0)
                snprintf(share->name, sizeof(share->name), "%s (!)", shareName + 2);
            else
                snprintf(share->name, sizeof(share->name), "%s", shareName);
            continue;
        }

        if (share == NULL)
            continue;

        if (strncmp(t, "available = ", 12) == 0) {
            const char *v = t + 12;
            share->available = v[0] == '1' || strncasecmp(v, "yes", 3) == 0 ||
                               strncasecmp(v, "true", 4) == 0;
            // Toggled by rewriting the one digit: only "available = 0|1"
            share->availablePos = ((v[0] == '0' || v[0] == '1') && v[1] == '\0')
                                      ? lineStart + (long)(t - line)
                                      : -1;
        }
        else if (strncmp(t, "path = ", 7) == 0) {
            snprintf(share->path, sizeof(share->path), "%s", t + 7);
        }
    }

    network_numShares = numShares;

    fclose(file);
}

void network_toggleSmbAvailable(void *item)
{
    ListItem *listItem = (ListItem *)item;
    Share *share = (Share *)listItem->payload_ptr;

    if (share == NULL || share->availablePos < 0) {
        printf("No \"available = 0|1\" line to toggle for this share.\n");
        return;
    }

    FILE *file = fopen(SMBD_CONFIG_PATH, "r+");
    if (file == NULL) {
        printf("Failed to open smb.conf file.\n");
        return;
    }

    char line[STR_MAX];
    if (fseek(file, share->availablePos, SEEK_SET) != 0 ||
        fgets(line, sizeof(line), file) == NULL ||
        strncmp(line, "available = ", 12) != 0 ||
        (line[12] != '0' && line[12] != '1') ||
        (line[13] != '\n' && line[13] != '\r' && line[13] != '\0')) {
        // The file changed since it was read: touch nothing
        printf("smb.conf changed: the available property of share '%s' is not where it was.\n", share->name);
        fclose(file);
        return;
    }

    share->available = line[12] == '0'; // toggle

    fseek(file, share->availablePos + 12, SEEK_SET);
    fputc(share->available ? '1' : '0', file);
    fflush(file);

    fclose(file);
}

void network_setState(bool *state_ptr, const char *flag_name, bool value)
{
    *state_ptr = value;
    config_flag_set(flag_name, value);
}

void network_execServiceState(const char *service_name, bool background)
{
    char state[256];
    char command[512];

    sync();

    sprintf(state, NET_SCRIPT_PATH "/update_networking.sh %s toggle", service_name);
    sprintf(command, "%s 2>&1", state);
    if (background)
        strcat(command, " &");
    system(command);

    printf_debug("network_execServiceState: %s\n", state);
}

void network_execServiceAuth(const char *service_name)
{
    char authed[256];
    char command[512];

    sync();

    sprintf(authed, NET_SCRIPT_PATH "/update_networking.sh %s authed", service_name);
    sprintf(command, "%s 2>&1", authed);

    system(command);

    printf_debug("network_execServiceAuth: %s\n", authed);
}

void network_commonEnableToggle(List *list, ListItem *item, bool *value_pt, const char *service_name, const char *service_flag)
{
    bool enabled = item->value == 1;
    network_setState(value_pt, service_flag, enabled);
    if (_menu_network._created) {
        list_currentItem(&_menu_network)->value = enabled;
    }
    network_execServiceState(service_name, false);
    reset_menus = true;
    all_changed = true;
}

void network_setSmbdState(void *pt)
{
    network_commonEnableToggle(&_menu_smbd, (ListItem *)pt, &network_state.smbd, "smbd", ".smbdState");
}

void network_setHttpState(void *pt)
{
    network_commonEnableToggle(&_menu_http, (ListItem *)pt, &network_state.http, "http", ".httpState");
}

void network_setSshState(void *pt)
{
    network_commonEnableToggle(&_menu_ssh, (ListItem *)pt, &network_state.ssh, "ssh", ".sshState");
}

void network_setFtpState(void *pt)
{
    network_commonEnableToggle(&_menu_ftp, (ListItem *)pt, &network_state.ftp, "ftp", ".ftpState");
}

void network_setTelnetState(void *pt)
{
    network_commonEnableToggle(&_menu_telnet, (ListItem *)pt, &network_state.telnet, "telnet", ".telnetState");
}

// In the background: the first start downloads Syncthing (about 25 MB),
// which would keep Tweaks waiting.
void network_setSyncthingState(void *pt)
{
    network_setState(&network_state.syncthing, ".syncthingState", ((ListItem *)pt)->value);
    network_execServiceState("syncthing", true);
    reset_menus = true;
    all_changed = true;
}

void network_setHotspotState(void *pt)
{
    network_setState(&network_state.hotspot, ".hotspotState", ((ListItem *)pt)->value);
    network_execServiceState("hotspot", false);
    reset_menus = true;
    all_changed = true;
}

void network_setNtpState(void *pt)
{
    network_setState(&network_state.ntp, ".ntpState", ((ListItem *)pt)->value);
    temp_flag_set("ntp_synced", false);
    network_execServiceState("ntp", true);
    reset_menus = true;
    all_changed = true;
}

void network_setNtpForceState(void *pt)
{
    network_setState(&network_state.force_wifi_on_startup, ".ntpForce", ((ListItem *)pt)->value);
}

void network_setNtpWaitState(void *pt)
{
    network_setState(&network_state.ntp_wait, ".ntpWait", ((ListItem *)pt)->value);
}

void network_setCheckUpdates(void *pt)
{
    network_setState(&network_state.check_updates, ".checkUpdates", ((ListItem *)pt)->value);
}

void network_keepServicesAlive(void *pt)
{
    network_setState(&network_state.keep_alive, ".keepServicesAlive", !((ListItem *)pt)->value);
}

void network_setFtpAuthState(void *pt)
{
    network_setState(&network_state.auth_ftp, ".authftpState", ((ListItem *)pt)->value);
    network_execServiceAuth("ftp");
}

void network_setHttpAuthState(void *pt)
{
    network_setState(&network_state.auth_http, ".authhttpState", ((ListItem *)pt)->value);
    network_execServiceAuth("http");
}

void network_setSshAuthState(void *pt)
{
    network_setState(&network_state.auth_ssh, ".authsshState", ((ListItem *)pt)->value);
    network_execServiceAuth("ssh");
}

void network_wpsConnect(void *pt)
{
    system("sh " NET_SCRIPT_PATH "/wpsclient.sh");
}

void network_setTzManualState(void *pt)
{
    bool enabled = ((ListItem *)pt)->value;
    network_setState(&network_state.manual_tz, ".manual_tz", !enabled);
    if (enabled) {
        char utc_str[10];
        if (config_getString(".tz_sync", utc_str, sizeof(utc_str))) {
            setenv("TZ", utc_str, 1);
            tzset();
            config_setString(".tz", utc_str);
        }
        else {
            temp_flag_set("ntp_synced", false);
        }
    }
    reset_menus = true;
    all_changed = true;
}

void network_setTzSelectState(void *pt)
{
    char utc_str[10];
    int select_value = ((ListItem *)pt)->value;
    double utc_value = ((double)select_value / 2.0) - 12.0;
    bool half_past = round(utc_value) != utc_value;

    if (utc_value == 0.0) {
        strcpy(utc_str, "UTC");
    }
    else {
        // UTC +/- is reversed for export TZ
        sprintf(utc_str, utc_value > 0 ? "UTC-%02d:%02d" : "UTC+%02d:%02d", (int)floor(abs(utc_value)), half_past ? 30 : 0);
    }

    printf_debug("Set timezone: %s\n", utc_str);

    setenv("TZ", utc_str, 1);
    tzset();
    config_setString(".tz", utc_str);
}

void network_toggleVNC(void *pt)
{
    char command_start[STR_MAX];
    char command_stop[STR_MAX];

    int new_fps = (int)network_state.vncfps;

    sprintf(command_start, "/mnt/SDCARD/.tmp_update/bin/vncserver -k /dev/input/event0 -F %d -r 180 > /dev/null 2>&1 &", new_fps);
    sprintf(command_stop, "killall -9 vncserver");

    if (!network_state.vncserv) {
        network_state.vncserv = true;
        network_setState(&network_state.vncserv, ".vncServer", true);
        reset_menus = true;
        if (!process_isRunning("vncserver")) {
            system(command_start);
        }
    }
    else {
        network_state.vncserv = false;
        network_setState(&network_state.vncserv, ".vncServer", false);
        reset_menus = true;
        if (process_isRunning("vncserver")) {
            system(command_stop);
        }
    }
}

void network_setVNCFPS(void *pt)
{
    network_state.vncfps = ((ListItem *)pt)->value;
    config_setNumber(".vncfps", network_state.vncfps);

    if (network_state.vncserv) {
        network_toggleVNC(pt);
        network_state.vncserv = false;
        network_setState(&network_state.vncserv, ".vncServer", false);
        reset_menus = true;
    }
}

void menu_smbd(void *pt)
{
    ListItem *item = (ListItem *)pt;
    item->value = (int)network_state.smbd;

    if (!_menu_smbd._created) {
        network_getSmbShares();

        _menu_smbd = list_createWithSticky(1 + network_numShares, "File sharing (Samba)");

        list_addItemWithInfoNote(&_menu_smbd,
                                 (ListItem){
                                     .label = "Enable",
                                     .sticky_note = "Enable Samba file sharing",
                                     .item_type = TOGGLE,
                                     .value = (int)network_state.smbd,
                                     .action = network_setSmbdState},
                                 item->info_note);

        for (int i = 0; i < network_numShares; i++) {
            ListItem shareItem = {
                .item_type = TOGGLE,
                .disabled = !network_state.smbd,
                .action = network_toggleSmbAvailable, // set the action to the wrapper function
                .value = _network_shares[i].available,
                .payload_ptr = _network_shares + i // store a pointer to the share in the payload
            };
            snprintf(shareItem.label, STR_MAX - 1, "Share: %s", _network_shares[i].name);
            strncpy(shareItem.sticky_note, str_replace(_network_shares[i].path, "/mnt/SDCARD", "SD:"), STR_MAX - 1);
            list_addItem(&_menu_smbd, shareItem);
        }
    }

    menu_stack[++menu_level] = &_menu_smbd;
    header_changed = true;
}

void menu_http(void *pt)
{
    ListItem *item = (ListItem *)pt;
    item->value = (int)network_state.http;
    if (!_menu_http._created) {
        _menu_http = list_create(2, LIST_SMALL);
        strcpy(_menu_http.title, "Web file manager (HTTP)");
        list_addItemWithInfoNote(&_menu_http,
                                 (ListItem){
                                     .label = "Enable",
                                     .item_type = TOGGLE,
                                     .value = (int)network_state.http,
                                     .action = network_setHttpState},
                                 item->info_note);
        list_addItemWithInfoNote(&_menu_http,
                                 (ListItem){
                                     .label = "Enable authentication",
                                     .item_type = TOGGLE,
                                     .disabled = !network_state.http,
                                     .value = (int)network_state.auth_http,
                                     .action = network_setHttpAuthState},
                                 "Username: admin\n"
                                 "Password: admin\n"
                                 " \n"
                                 "It's recommended you change this\n"
                                 "at first login.");
    }
    menu_stack[++menu_level] = &_menu_http;
    header_changed = true;
}

void menu_ftp(void *pt)
{
    ListItem *item = (ListItem *)pt;
    item->value = (int)network_state.ftp;
    if (!_menu_ftp._created) {
        _menu_ftp = list_create(2, LIST_SMALL);
        strcpy(_menu_ftp.title, "File transfer (FTP)");
        list_addItemWithInfoNote(&_menu_ftp,
                                 (ListItem){
                                     .label = "Enable",
                                     .item_type = TOGGLE,
                                     .value = (int)network_state.ftp,
                                     .action = network_setFtpState},
                                 item->info_note);
        list_addItemWithInfoNote(&_menu_ftp,
                                 (ListItem){
                                     .label = "Enable authentication",
                                     .item_type = TOGGLE,
                                     .disabled = !network_state.ftp,
                                     .value = (int)network_state.auth_ftp,
                                     .action = network_setFtpAuthState},
                                 "Username: onion\n"
                                 "Password: onion\n"
                                 " \n"
                                 "Passwords of your own will come in a\n"
                                 "future update.");
    }
    menu_stack[++menu_level] = &_menu_ftp;
    header_changed = true;
}

void menu_wps(void *_)
{
    if (!_menu_wps._created) {
        _menu_wps = list_create(1, LIST_SMALL);
        strcpy(_menu_wps.title, "WPS");
        list_addItem(&_menu_wps,
                     (ListItem){
                         .label = "WPS connect",
                         .action = network_wpsConnect});
    }
    menu_stack[++menu_level] = &_menu_wps;
    header_changed = true;
}

void menu_ssh(void *pt)
{
    ListItem *item = (ListItem *)pt;
    item->value = (int)network_state.ssh;
    if (!_menu_ssh._created) {
        _menu_ssh = list_create(2, LIST_SMALL);
        strcpy(_menu_ssh.title, "Terminal access (SSH)");
        list_addItemWithInfoNote(&_menu_ssh,
                                 (ListItem){
                                     .label = "Enable",
                                     .item_type = TOGGLE,
                                     .value = (int)network_state.ssh,
                                     .action = network_setSshState},
                                 item->info_note);
        list_addItemWithInfoNote(&_menu_ssh,
                                 (ListItem){
                                     .label = "Enable authentication",
                                     .item_type = TOGGLE,
                                     .disabled = !network_state.ssh,
                                     .value = (int)network_state.auth_ssh,
                                     .action = network_setSshAuthState},
                                 "Username: onion\n"
                                 "Password: onion\n"
                                 " \n"
                                 "Passwords of your own will come in a\n"
                                 "future update.");
    }
    menu_stack[++menu_level] = &_menu_ssh;
    header_changed = true;
}

void menu_vnc(void *pt)
{
    ListItem *item = (ListItem *)pt;
    item->value = (int)network_state.vncserv;
    if (!_menu_vnc._created) {
        _menu_vnc = list_create(2, LIST_SMALL);
        strcpy(_menu_vnc.title, "Screen sharing (VNC)");
        list_addItemWithInfoNote(&_menu_vnc,
                                 (ListItem){
                                     .label = "Enable",
                                     .item_type = TOGGLE,
                                     .value = (int)network_state.vncserv,
                                     .action = network_toggleVNC},
                                 "Turn screen sharing on, then connect to\n"
                                 "the device's IP address from a VNC\n"
                                 "viewer on your PC, phone or tablet.");
        list_addItemWithInfoNote(&_menu_vnc,
                                 (ListItem){
                                     .label = "Framerate",
                                     .item_type = MULTIVALUE,
                                     .disabled = !network_state.vncserv,
                                     .value_max = 20,
                                     .value_min = 1,
                                     .value = (int)network_state.vncfps,
                                     .value_formatter = formatter_vncFps,
                                     .action = network_setVNCFPS},
                                 "Frame rate of the VNC server, from 1 to\n"
                                 "20. The higher it is, the more CPU it\n"
                                 "uses.");
    }
    menu_stack[++menu_level] = &_menu_vnc;
    header_changed = true;
}

void menu_wifi(void *_)
{
    if (!_menu_wifi._created) {
        _menu_wifi = list_create(3, LIST_SMALL);
        strcpy(_menu_wifi.title, "Wi-Fi hotspot and WPS");
        list_addItem(&_menu_wifi,
                     (ListItem){
                         .label = "IP address: not connected",
                         .disabled = true,
                         .action = NULL});
        list_addItemWithInfoNote(&_menu_wifi,
                                 (ListItem){
                                     .label = "Wi-Fi hotspot",
                                     .item_type = TOGGLE,
                                     .value = (int)network_state.hotspot,
                                     .action = network_setHotspotState},
                                 "The device makes its own Wi-Fi network\n"
                                 "for the network services: no router\n"
                                 "needed. Works with Easy Netplay and\n"
                                 "regular netplay.");
        list_addItemWithInfoNote(&_menu_wifi,
                                 (ListItem){
                                     .label = "WPS connect",
                                     .action = network_wpsConnect},
                                 "Connect to your router with its WPS\n"
                                 "button.\n"
                                 " \n"
                                 "First press the WPS button on your router,\n"
                                 "then select this option.");
        // list_addItem(&_menu_wifi,
        //              (ListItem){
        //                  .label = "WPS...",
        //                  .action = menu_wps});
    }
    strcpy(_menu_wifi.items[0].label, ip_address_label);
    menu_stack[++menu_level] = &_menu_wifi;
    header_changed = true;
}

void menu_network(void *_)
{
    if (!_menu_network._created) {
        _menu_network = list_create(10, LIST_SMALL);
        strcpy(_menu_network.title, "Network");

        network_loadState();

        list_addItem(&_menu_network,
                     (ListItem){
                         .label = "IP address: not connected",
                         .disabled = true,
                         .action = NULL});
        list_addItemWithInfoNote(&_menu_network,
                                 (ListItem){
                                     .label = "Wi-Fi hotspot and WPS...",
                                     .action = menu_wifi},
                                 "Make the device's own Wi-Fi network\n"
                                 "(hotspot), or connect to your router\n"
                                 "with its WPS button.");
        list_addItemWithInfoNote(&_menu_network,
                                 (ListItem){
                                     .label = "File sharing (Samba)...",
                                     .item_type = TOGGLE,
                                     .disabled = !settings.wifi_on,
                                     .alternative_arrow_action = true,
                                     .arrow_action = network_setSmbdState,
                                     .value = (int)network_state.smbd,
                                     .action = menu_smbd},
                                 "Samba shares files and folders between\n"
                                 "your device and your PC.\n"
                                 " \n"
                                 "Username: onion\n"
                                 "Password: onion");
        list_addItemWithInfoNote(&_menu_network,
                                 (ListItem){
                                     .label = "Web file manager (HTTP)...",
                                     .item_type = TOGGLE,
                                     .disabled = !settings.wifi_on,
                                     .alternative_arrow_action = true,
                                     .arrow_action = network_setHttpState,
                                     .value = (int)network_state.http,
                                     .action = menu_http},
                                 "Manage your files from a web browser on\n"
                                 "your phone, PC or tablet.\n"
                                 " \n"
                                 "Think of it as a website on RedOnion:\n"
                                 "enter its IP address in your browser.");
        list_addItemWithInfoNote(&_menu_network,
                                 (ListItem){
                                     .label = "Terminal access (SSH)...",
                                     .item_type = TOGGLE,
                                     .disabled = !settings.wifi_on,
                                     .alternative_arrow_action = true,
                                     .arrow_action = network_setSshState,
                                     .value = (int)network_state.ssh,
                                     .action = menu_ssh},
                                 "Use the device's command line from your\n"
                                 "PC, securely (SSH).\n"
                                 " \n"
                                 "SFTP sends files the same secure way,\n"
                                 "with an SFTP client (e.g. WinSCP).");
        list_addItemWithInfoNote(&_menu_network,
                                 (ListItem){
                                     .label = "File transfer (FTP)...",
                                     .item_type = TOGGLE,
                                     .disabled = !settings.wifi_on,
                                     .alternative_arrow_action = true,
                                     .arrow_action = network_setFtpState,
                                     .value = (int)network_state.ftp,
                                     .action = menu_ftp},
                                 "FTP provides a method of transferring files\n"
                                 "between RedOnion and a PC, phone, or tablet.\n"
                                 "You'll need an FTP client installed on the\n"
                                 "other device.");
        list_addItemWithInfoNote(&_menu_network,
                                 (ListItem){
                                     .label = "Remote shell (Telnet)",
                                     .item_type = TOGGLE,
                                     .disabled = !settings.wifi_on,
                                     .value = (int)network_state.telnet,
                                     .action = network_setTelnetState},
                                 "Telnet provides unencrypted remote shell\n"
                                 "access to your device.");
        list_addItemWithInfoNote(&_menu_network,
                                 (ListItem){
                                     .label = "Screen sharing (VNC)...",
                                     .item_type = TOGGLE,
                                     .disabled = !settings.wifi_on,
                                     .alternative_arrow_action = true,
                                     .arrow_action = network_toggleVNC,
                                     .value = (int)network_state.vncserv,
                                     .action = menu_vnc},
                                 "See and control the screen from another\n"
                                 "device.");
        list_addItemWithInfoNote(&_menu_network,
                                 (ListItem){
                                     .label = "Folder sync (Syncthing)",
                                     .item_type = TOGGLE,
                                     .disabled = !settings.wifi_on,
                                     .value = (int)network_state.syncthing,
                                     .action = network_setSyncthingState},
                                 "Keeps folders in sync with your PC or\n"
                                 "phone, e.g. Saves. The first start\n"
                                 "downloads Syncthing (about 25 MB).\n"
                                 " \n"
                                 "Set it up in a browser:\n"
                                 "http://<IP address>:8384\n"
                                 "Username: onion  Password: onion");
        list_addItemWithInfoNote(&_menu_network,
                                 (ListItem){
                                     .label = "Pause these while playing",
                                     .item_type = TOGGLE,
                                     .value = !network_state.keep_alive,
                                     .action = network_keepServicesAlive},
                                 "Turn off the network services (not the\n"
                                 "Wi-Fi itself) while a game runs.\n"
                                 " \n"
                                 "This saves battery and keeps games\n"
                                 "running at full speed.");
    }
    strcpy(_menu_network.items[0].label, ip_address_label);
    menu_stack[++menu_level] = &_menu_network;
    header_changed = true;
}

#endif // TWEAKS_NETWORK_H__
