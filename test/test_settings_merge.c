/**
 * @file test_settings_merge.c
 * @brief A save of system.json keeps what another program saved meanwhile
 *        (src/common/system/settings.h, _settings_merge_from_disk).
 *
 * Open MainUI saves the language without telling keymon. keymon kept the old
 * language in memory and wrote it back with its next brightness or volume
 * save, so the GameSwitcher came up in the old language. A save now takes
 * from the file every value the saving program has not changed itself, except
 * the ones keymon mirrors in shared memory, which it owns.
 *
 * Build and run: make -f Makefile.unit test_settings_merge
 */

#define MAIN_UI_SETTINGS "/tmp/onion_test_settings_merge/system.json"

#include "onion_test.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "system/settings.h"

/* keymon's shared memory, in process (as in test_settings_reload.c). */
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

static void write_system_json(const char *language, const char *theme, int brightness,
                              int wifi)
{
    mkdir("/tmp/onion_test_settings_merge", 0700);
    FILE *fp = fopen(MAIN_UI_SETTINGS, "w");
    fprintf(fp,
            "{\n\t\"vol\":\t20,\n\t\"keymap\":\t\"L2,L,R2,R,X,A,B,Y\",\n\t\"mute\":\t0,\n"
            "\t\"bgmvol\":\t8,\n\t\"brightness\":\t%d,\n\t\"language\":\t\"%s\",\n"
            "\t\"hibernate\":\t5,\n\t\"lumination\":\t7,\n\t\"hue\":\t10,\n"
            "\t\"saturation\":\t10,\n\t\"contrast\":\t10,\n\t\"theme\":\t\"%s\",\n"
            "\t\"fontsize\":\t24,\n\t\"audiofix\":\t1,\n\t\"wifi\":\t%d\n}",
            brightness, language, theme, wifi);
    fclose(fp);
}

static cJSON *read_back(void)
{
    return json_load(MAIN_UI_SETTINGS);
}

static const char *string_of(cJSON *root, const char *key)
{
    return cJSON_GetStringValue(cJSON_GetObjectItem(root, key));
}

static int int_of(cJSON *root, const char *key)
{
    return (int)cJSON_GetNumberValue(cJSON_GetObjectItem(root, key));
}

/* keymon starts, Open MainUI then saves Arabic, and a MainUI input carries a
 * new brightness to keymon, which saves: Arabic must stay. */
TEST(keymon_save_keeps_language_saved_by_open_mainui)
{
    write_system_json("en.lang", "/mnt/SDCARD/Themes/Silky RedOnion/", 7, 0);
    settings_init();
    ASSERT_STREQ(settings.language, "en.lang");

    write_system_json("ar.lang", "/mnt/SDCARD/Themes/Silky RedOnion/", 7, 0);
    fake_shm[MONITOR_BRIGHTNESS] = 5;
    settings_shm_read();

    cJSON *root = read_back();
    ASSERT_NOT_NULL(root);
    ASSERT_STREQ(string_of(root, "language"), "ar.lang");
    ASSERT_EQ(int_of(root, "brightness"), 5);
    cJSON_Delete(root);
    /* keymon now holds the language on disk too, for its next save. */
    ASSERT_STREQ(settings.language, "ar.lang");
}

/* The same for the other values keymon does not mirror: the theme, Wi-Fi. */
TEST(save_keeps_theme_and_wifi_saved_by_another_program)
{
    write_system_json("en.lang", "/mnt/SDCARD/Themes/Silky by DiMo/", 7, 0);
    settings_init();
    write_system_json("en.lang", "/mnt/SDCARD/Themes/Silky RedOnion/", 7, 1);
    fake_shm[MONITOR_BRIGHTNESS] = 4;
    settings_shm_read();

    cJSON *root = read_back();
    ASSERT_STREQ(string_of(root, "theme"), "/mnt/SDCARD/Themes/Silky RedOnion/");
    ASSERT_EQ(int_of(root, "wifi"), 1);
    ASSERT_EQ(int_of(root, "brightness"), 4);
    cJSON_Delete(root);
}

/* A value this program changed itself is saved, whatever the file says. */
TEST(own_change_wins_over_file)
{
    write_system_json("en.lang", "/mnt/SDCARD/Themes/Silky RedOnion/", 7, 0);
    settings_load();
    write_system_json("ar.lang", "/mnt/SDCARD/Themes/Silky RedOnion/", 7, 0);
    strcpy(settings.language, "it.lang");
    _settings_save_mainui_force();

    cJSON *root = read_back();
    ASSERT_STREQ(string_of(root, "language"), "it.lang");
    cJSON_Delete(root);
}

/* Values keymon mirrors in shared memory stay keymon's: the brightness it
 * holds is saved even if the file has another one. */
TEST(mirrored_values_are_not_taken_from_file)
{
    write_system_json("en.lang", "/mnt/SDCARD/Themes/Silky RedOnion/", 7, 0);
    settings_init();
    write_system_json("en.lang", "/mnt/SDCARD/Themes/Silky RedOnion/", 9, 0);
    strcpy(settings.language, "fr.lang"); /* any change, to save */
    _settings_save_mainui_force();

    cJSON *root = read_back();
    ASSERT_EQ(int_of(root, "brightness"), 7);
    cJSON_Delete(root);
}

/* A missing or broken file: the save writes what this program holds. */
TEST(missing_file_saves_own_values)
{
    write_system_json("en.lang", "/mnt/SDCARD/Themes/Silky RedOnion/", 7, 0);
    settings_load();
    unlink(MAIN_UI_SETTINGS);
    strcpy(settings.language, "de.lang");
    _settings_save_mainui_force();

    cJSON *root = read_back();
    ASSERT_NOT_NULL(root);
    ASSERT_STREQ(string_of(root, "language"), "de.lang");
    ASSERT_STREQ(string_of(root, "theme"), "/mnt/SDCARD/Themes/Silky RedOnion/");
    cJSON_Delete(root);
}

int main(void)
{
    printf("\n=== settings merge Unit Tests ===\n\n");
    RUN_TEST(keymon_save_keeps_language_saved_by_open_mainui);
    RUN_TEST(save_keeps_theme_and_wifi_saved_by_another_program);
    RUN_TEST(own_change_wins_over_file);
    RUN_TEST(mirrored_values_are_not_taken_from_file);
    RUN_TEST(missing_file_saves_own_values);
    unlink(MAIN_UI_SETTINGS);
    rmdir("/tmp/onion_test_settings_merge");
    TEST_REPORT();
    return test_failures;
}
