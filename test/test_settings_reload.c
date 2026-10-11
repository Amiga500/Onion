/**
 * @file test_settings_reload.c
 * @brief keymon's shared-memory copy of the settings after another program
 *        saved them (src/common/system/settings_sync.h).
 *
 * keymon reads the shared memory on every MainUI input and takes any value
 * that differs from its own as a change made in MainUI. After GameSwitcher
 * saved a new brightness, keymon reloaded system.json but left the old
 * value in the shared memory, and the next MainUI input put it back.
 *
 * Build and run: make -f Makefile.unit test_settings_reload
 */

#include "onion_test.h"
#include <stdio.h>
#include <string.h>

#include "system/settings.h"

/* The shared-memory part of settings_sync.h is device-only: compile it here
 * against an in-process stand-in for keymon's shared memory. */
#define PLATFORM_MIYOOMINI
#include "system/settings_sync.h"

static int fake_shm[MONITOR_VALUE_MAX];

int InitKeyShm(KeyShmInfo *info)
{
    info->addr = fake_shm;
    return 0;
}

int SetKeyShm(KeyShmInfo *info, MonitorValue key, int value)
{
    (void)info;
    fake_shm[key] = value;
    return 0;
}

int GetKeyShm(KeyShmInfo *info, MonitorValue key)
{
    (void)info;
    return fake_shm[key];
}

int UninitKeyShm(KeyShmInfo *info)
{
    (void)info;
    return 0;
}

static void keymon_state(int brightness)
{
    settings.brightness = brightness;
    settings.bgm_volume = 10;
    settings.sleep_timer = 5;
    settings.lumination = 7;
    settings.hue = 10;
    settings.saturation = 10;
    settings.contrast = 10;
    settings_shm_write();
}

/* What settings_reload() publishes after settings_load() read the new file. */
TEST(published_reload_survives_mainui_input)
{
    keymon_state(3);
    settings.brightness = 7; /* system.json, as GameSwitcher saved it */
    settings_shm_write();
    settings_shm_read(); /* next MainUI input */
    ASSERT_EQ(settings.brightness, 7);
    ASSERT_EQ(fake_shm[MONITOR_BRIGHTNESS], 7);
}

TEST(stale_shared_memory_differs_after_plain_load)
{
    /* The bug: a reload without publishing leaves the old value where
     * settings_shm_read() takes it as MainUI's. */
    keymon_state(3);
    settings.brightness = 7;
    ASSERT_NE(fake_shm[MONITOR_BRIGHTNESS], settings.brightness);
}

TEST(other_monitored_values_published_too)
{
    keymon_state(3);
    settings.contrast = 15; /* e.g. changed in Tweaks */
    settings.sleep_timer = 10;
    settings_shm_write();
    settings_shm_read();
    ASSERT_EQ(settings.contrast, 15);
    ASSERT_EQ(settings.sleep_timer, 10);
}

TEST(mainui_change_still_taken)
{
    /* A change MainUI made in the shared memory is still picked up. */
    keymon_state(3);
    fake_shm[MONITOR_BRIGHTNESS] = 9;
    settings_shm_read();
    ASSERT_EQ(settings.brightness, 9);
}

TEST(keymon_reloads_through_settings_reload)
{
    FILE *fp = fopen("../src/keymon/keymon.c", "r");
    ASSERT_NOT_NULL(fp);
    static char text[200000];
    size_t n = fread(text, 1, sizeof(text) - 1, fp);
    fclose(fp);
    text[n] = '\0';
    const char *flag = strstr(text, "exists(\"/tmp/settings_changed\")");
    ASSERT_NOT_NULL(flag);
    const char *reload = strstr(flag, "settings_reload();");
    const char *end = strstr(flag, "refresh_cached_flags();");
    ASSERT_NOT_NULL(reload);
    ASSERT_NOT_NULL(end);
    ASSERT_TRUE(reload < end);
}

int main(void)
{
    RUN_TEST(published_reload_survives_mainui_input);
    RUN_TEST(stale_shared_memory_differs_after_plain_load);
    RUN_TEST(other_monitored_values_published_too);
    RUN_TEST(mainui_change_still_taken);
    RUN_TEST(keymon_reloads_through_settings_reload);
    TEST_REPORT();
    return test_failures;
}
