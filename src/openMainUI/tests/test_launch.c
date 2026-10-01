/* SPDX-License-Identifier: GPL-3.0-only */
#include "platform/device_request.h"
#include "platform/launch.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static cJSON *record(const char *rom, int type)
{
    cJSON *item = cJSON_CreateObject();
    assert(item && cJSON_AddStringToObject(item, "launch", "/mnt/SDCARD/Emu/FC/launch.sh") &&
           cJSON_AddStringToObject(item, "rompath", rom) &&
           cJSON_AddNumberToObject(item, "type", type));
    return item;
}

int mainui_suite_launch(void)
{
    char *wifi = mainui_device_wifi_command("It's $HOME; network", "password123");
    assert(wifi && strstr(wifi, "ssid '\"It'\\''s $HOME; network\"'"));
    free(wifi);
    wifi = mainui_device_wifi_command("Open network", "");
    assert(wifi && strstr(wifi, "key_mgmt NONE"));
    free(wifi);
    assert(!mainui_device_wifi_command("Bad\"SSID", "password123"));
    assert(!mainui_device_wifi_command("Network", "short"));
    char error[256];
    cJSON *item = record("/mnt/SDCARD/Roms/FC/It's a game.nes", 5);
    char *command = mainui_launch_command(item, error);
    assert(command &&
           !strcmp(command,
                   "LD_PRELOAD=/mnt/SDCARD/miyoo/app/../lib/libpadsp.so "
                   "\"/mnt/SDCARD/Emu/FC/launch.sh\" \"/mnt/SDCARD/Roms/FC/It's a game.nes\"\n"));
    free(command);
    cJSON_Delete(item);
    const char *invalid[] = {"/mnt/SDCARD/Roms/quote\".nes", "/mnt/SDCARD/Roms/$HOME.nes",
                             "/mnt/SDCARD/Roms/`cmd`.nes",   "/mnt/SDCARD/Roms/new\nline.nes",
                             "/mnt/SDCARD/Roms/a:b.nes",     "C:/host.nes"};
    for (size_t i = 0; i < sizeof invalid / sizeof invalid[0]; i++) {
        item = record(invalid[i], 5);
        assert(!mainui_launch_command(item, error) && *error);
        cJSON_Delete(item);
    }
    item = record("/mnt/SDCARD/RApp/Test/launch.sh:/mnt/SDCARD/Roms/Test/game.nes", 17);
    command = mainui_launch_command(item, error);
    assert(
        command &&
        strstr(command, "\"/mnt/SDCARD/RApp/Test/launch.sh\" \"/mnt/SDCARD/Roms/Test/game.nes\""));
    free(command);
    cJSON_Delete(item);
    unsigned char bytes[MAINUI_FAVORITE_RETURN_SIZE];
    assert(mainui_launch_favorite_return(bytes, 5, "/Games/Arcade", "/mnt/SDCARD/Roms/FC/game.nes",
                                         "Game"));
    assert(bytes[0] == 0x31 && bytes[1] == 0x46 && bytes[2] == 0x42 && bytes[3] == 0x43);
    assert(bytes[4] == 1 && bytes[8] == 5);
    assert(!strcmp((char *)bytes + 12, "/Games/Arcade"));
    assert(!strcmp((char *)bytes + 268, "/mnt/SDCARD/Roms/FC/game.nes"));
    for (int i = 1292; i < 1804; i++) {
        assert(bytes[i] == 0);
    }
    assert(!strcmp((char *)bytes + 1804, "Game"));
    assert(!mainui_launch_favorite_return(bytes, 5, "/", "rom", "label"));
    char long_label[129];
    memset(long_label, 'x', 128);
    long_label[128] = 0;
    assert(mainui_launch_favorite_return(bytes, 5, "/Folder", "rom", long_label));
    assert(strlen((char *)bytes + 1804) == 127);
    char unicode_label[130];
    for (int i = 0; i < 43; ++i) {
        memcpy(unicode_label + i * 3, "\xe6\x97\xa5", 3);
    }
    unicode_label[129] = 0;
    assert(mainui_launch_favorite_return(bytes, 5, "/Folder", "rom", unicode_label));
    assert(strlen((char *)bytes + 1804) == 126);
    assert(!memcmp(bytes + 1804, unicode_label, 126));
    char long_folder[300], long_rom[1100];
    memset(long_folder, 'f', sizeof long_folder - 1);
    memset(long_rom, 'r', sizeof long_rom - 1);
    long_folder[sizeof long_folder - 1] = long_rom[sizeof long_rom - 1] = 0;
    assert(mainui_launch_favorite_return(bytes, 5, long_folder, long_rom, "Game"));
    assert(strlen((char *)bytes + 12) == 255 && strlen((char *)bytes + 268) == 1023);
    puts("Launch command and Favorite-return codec tests passed");
    return 0;
}
