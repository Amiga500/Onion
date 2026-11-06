/**
 * @file test_tweaks_reset.c
 * @brief Tweaks > Advanced > Reset settings, on real folders
 *
 * reset_core.h with a stand-in for 7z (copies from a folder laid out like
 * configs.pak), or the real reset_extract7z() against a fake 7z in PATH:
 *  - Reset system tweaks: the settings are replaced by the pak's, and the
 *    system's own files are kept (the pak, netplay_cores.conf,
 *    hotspot_wpa_supplicant.conf, migration_state, per-device MainUI
 *    settings, theme records, update channel, cores per console);
 *  - a failed or partial extraction, a missing pak, or a pak without the
 *    folder changes nothing and leaves no staging or old folder;
 *  - a reset cut short (between its two renames, or after them) is undone
 *    or finished, and the pak parked by the old code goes back;
 *  - RetroArch/AdvanceMENU: the pak's files are renamed over the old ones,
 *    the user's other files stay, a failure leaves retroarch.cfg whole;
 *  - system.json and wpa_supplicant.conf are replaced, never deleted when
 *    the default can't be copied;
 *  - RedOnion's default-writing migrations run after a reset.
 *
 * Build and run: make -f Makefile.unit test_tweaks_reset
 */

#include "onion_test.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define TEST_DIR "/tmp/onion_test_tweaks_reset"
#define RESET_SD_ROOT TEST_DIR "/sd"
#define RESET_MIGRATION_DIR TEST_DIR "/migration"

#include "../src/tweaks/reset_core.h"

#define PAK_TREE TEST_DIR "/pak_tree" // what configs.pak holds
#define CONFIG RESET_SD_ROOT "/.tmp_update/config"

static void sh(const char *cmd)
{
    if (system(cmd) != 0)
        fprintf(stderr, "command failed: %s\n", cmd);
}

static void put(const char *path, const char *text)
{
    char dir[PATH_MAX];
    snprintf(dir, sizeof(dir), "%s", path);
    char *slash = strrchr(dir, '/');
    if (slash != NULL) {
        *slash = '\0';
        mkdirs(dir);
    }
    FILE *f = fopen(path, "w");
    if (f != NULL) {
        fputs(text, f);
        fclose(f);
    }
}

static bool has(const char *path, const char *text)
{
    char buf[512] = "";
    FILE *f = fopen(path, "r");
    if (f == NULL)
        return false;
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    buf[n] = '\0';
    fclose(f);
    return strcmp(buf, text) == 0;
}

static bool there(const char *path)
{
    return reset_present(path);
}

// 7z stand-in: copies PAK_TREE/<filter without "/*"> to out_dir.
static int extract_calls = 0;
static bool fake_extract(const char *pak, const char *out_dir, const char *filter)
{
    extract_calls++;
    if (!is_file(pak))
        return false;
    char rel[PATH_MAX];
    snprintf(rel, sizeof(rel), "%s", filter);
    size_t len = strlen(rel);
    if (len >= 2 && strcmp(rel + len - 2, "/*") == 0)
        rel[len - 2] = '\0';
    char src[PATH_MAX * 2], cmd[PATH_MAX * 8];
    snprintf(src, sizeof(src), "%s/%s", PAK_TREE, rel);
    if (!is_dir(src))
        return true; // 7z: nothing matched
    snprintf(cmd, sizeof(cmd), "mkdir -p '%s/%s' && cp -a '%s/.' '%s/%s/'", out_dir, rel, src, out_dir, rel);
    return system(cmd) == 0;
}

// 7z failing half-way: some files out, then an error.
static bool partial_extract(const char *pak, const char *out_dir, const char *filter)
{
    fake_extract(pak, out_dir, filter);
    return false;
}

static const ResetSource source = {RESET_SD_ROOT, RESET_CONFIGS_PAK, RESET_STAGING_DIR, fake_extract};
static const ResetSource failing = {RESET_SD_ROOT, RESET_CONFIGS_PAK, RESET_STAGING_DIR, partial_extract};

static void reset_fixture(void)
{
    sh("rm -rf " TEST_DIR);
    mkdirs(TEST_DIR);
    extract_calls = 0;

    // The pak: defaults of the config folder, RetroArch and core overrides.
    put(PAK_TREE "/.tmp_update/config/.noAutoStart_", "");
    put(PAK_TREE "/.tmp_update/config/smb.conf", "default smb");
    put(PAK_TREE "/.tmp_update/config/keymap.json", "default keymap");
    put(PAK_TREE "/.tmp_update/config/active_theme", "pak would reset this");
    put(PAK_TREE "/RetroArch/.retroarch/retroarch.cfg", "default ra");
    put(PAK_TREE "/RetroArch/config.xml", "default xml");
    put(PAK_TREE "/Saves/CurrentProfile/config/VBA-M/VBA-M.opt", "default opt");

    // The card: user settings, and files that are not settings.
    put(CONFIG "/configs.pak", "the pak");
    put(CONFIG "/netplay_cores.conf", "gba;mgba");
    put(CONFIG "/hotspot_wpa_supplicant.conf", "hotspot");
    put(CONFIG "/migration_state", "28");
    put(CONFIG "/system/ABC123.json", "{\"vol\":5}");
    put(CONFIG "/active_theme", "/mnt/SDCARD/Themes/Mine/");
    put(CONFIG "/active_icon_pack", "/mnt/SDCARD/Icons/Mine");
    put(CONFIG "/theme-applied-0042", "/mnt/SDCARD/Themes/Mine/");
    put(CONFIG "/.systemSkin", "theme");
    put(CONFIG "/ota_channel", "beta");
    put(CONFIG "/system_cores/Emu-GBA", "gpsp");
    put(CONFIG "/syncthing/config.xml", "<device id>");
    put(CONFIG "/.ftpState", "");        // a setting: goes
    put(CONFIG "/smb.conf", "user smb"); // a setting: back to default
    put(CONFIG "/gameSwitcher/showTime", "");
    put(RESET_SD_ROOT "/RetroArch/.retroarch/retroarch.cfg", "user ra");
    put(RESET_SD_ROOT "/RetroArch/.retroarch/cores/x.so", "core");
    put(RESET_SD_ROOT "/Saves/CurrentProfile/config/VBA-M/VBA-M.opt", "user opt");
    put(RESET_SD_ROOT "/Saves/CurrentProfile/config/mGBA/Game.cfg", "per game");
}

static bool clean_after(void)
{
    return !there(RESET_STAGING_DIR) && !there(CONFIG RESET_OLD_SUFFIX);
}

TEST(tweaks_reset_replaces_settings_and_keeps_system_files)
{
    reset_fixture();
    ASSERT_TRUE(reset_replaceDir(&source, ".tmp_update/config", reset_config_keep));

    // Settings: the pak's.
    ASSERT_TRUE(has(CONFIG "/smb.conf", "default smb"));
    ASSERT_TRUE(has(CONFIG "/keymap.json", "default keymap"));
    ASSERT_TRUE(there(CONFIG "/.noAutoStart_"));
    ASSERT_FALSE(there(CONFIG "/.ftpState"));
    ASSERT_FALSE(there(CONFIG "/gameSwitcher"));

    // Kept, also over the pak's copy of the same name.
    ASSERT_TRUE(has(CONFIG "/configs.pak", "the pak"));
    ASSERT_TRUE(has(CONFIG "/netplay_cores.conf", "gba;mgba"));
    ASSERT_TRUE(has(CONFIG "/hotspot_wpa_supplicant.conf", "hotspot"));
    ASSERT_TRUE(has(CONFIG "/migration_state", "28"));
    ASSERT_TRUE(has(CONFIG "/system/ABC123.json", "{\"vol\":5}"));
    ASSERT_TRUE(has(CONFIG "/active_theme", "/mnt/SDCARD/Themes/Mine/"));
    ASSERT_TRUE(has(CONFIG "/active_icon_pack", "/mnt/SDCARD/Icons/Mine"));
    ASSERT_TRUE(has(CONFIG "/theme-applied-0042", "/mnt/SDCARD/Themes/Mine/"));
    ASSERT_TRUE(has(CONFIG "/.systemSkin", "theme"));
    ASSERT_TRUE(has(CONFIG "/ota_channel", "beta"));
    ASSERT_TRUE(has(CONFIG "/system_cores/Emu-GBA", "gpsp"));
    ASSERT_TRUE(has(CONFIG "/syncthing/config.xml", "<device id>"));

    ASSERT_TRUE(clean_after());
}

TEST(tweaks_reset_twice)
{
    reset_fixture();
    ASSERT_TRUE(reset_replaceDir(&source, ".tmp_update/config", reset_config_keep));
    ASSERT_TRUE(reset_replaceDir(&source, ".tmp_update/config", reset_config_keep));
    ASSERT_TRUE(has(CONFIG "/configs.pak", "the pak")); // still there to reset again
    ASSERT_TRUE(has(CONFIG "/migration_state", "28"));
    ASSERT_TRUE(clean_after());
}

static bool config_untouched(void)
{
    return has(CONFIG "/smb.conf", "user smb") && there(CONFIG "/.ftpState") &&
           has(CONFIG "/configs.pak", "the pak") && has(CONFIG "/netplay_cores.conf", "gba;mgba") &&
           !there(CONFIG "/keymap.json");
}

TEST(failed_extraction_changes_nothing)
{
    reset_fixture();
    ASSERT_FALSE(reset_replaceDir(&failing, ".tmp_update/config", reset_config_keep));
    ASSERT_TRUE(config_untouched());
    ASSERT_TRUE(clean_after());
}

TEST(missing_pak_changes_nothing)
{
    // The old code deleted the folder first and extracted nothing.
    reset_fixture();
    remove(CONFIG "/configs.pak");
    ASSERT_FALSE(reset_replaceDir(&source, ".tmp_update/config", reset_config_keep));
    ASSERT_EQ(extract_calls, 0);
    ASSERT_TRUE(has(CONFIG "/smb.conf", "user smb"));
    ASSERT_TRUE(there(CONFIG "/.ftpState"));
    ASSERT_TRUE(clean_after());
}

TEST(pak_without_the_folder_changes_nothing)
{
    reset_fixture();
    sh("rm -rf '" PAK_TREE "/.tmp_update'");
    ASSERT_FALSE(reset_replaceDir(&source, ".tmp_update/config", reset_config_keep));
    ASSERT_TRUE(config_untouched());
    ASSERT_TRUE(clean_after());
}

TEST(cut_between_the_renames_is_undone)
{
    reset_fixture();
    // As left by a power cut after "config -> config.reset-old".
    ASSERT_EQ(rename(CONFIG, CONFIG RESET_OLD_SUFFIX), 0);
    put(RESET_STAGING_DIR "/.tmp_update/config/smb.conf", "default smb");
    reset_recoverAll(RESET_SD_ROOT, RESET_STAGING_DIR, RESET_LEGACY_PAK, RESET_CONFIGS_PAK);
    ASSERT_TRUE(config_untouched());
    ASSERT_TRUE(clean_after());
}

TEST(cut_after_the_renames_is_finished)
{
    reset_fixture();
    // New folder in place, kept files still in the old one.
    ASSERT_EQ(rename(CONFIG, CONFIG RESET_OLD_SUFFIX), 0);
    put(CONFIG "/smb.conf", "default smb");
    put(RESET_MIGRATION_DIR "/00021_sort_apps_by_name.sh", "echo a >> " TEST_DIR "/ran\n");
    reset_recoverAll(RESET_SD_ROOT, RESET_STAGING_DIR, RESET_LEGACY_PAK, RESET_CONFIGS_PAK);
    ASSERT_TRUE(has(TEST_DIR "/ran", "a\n")); // the defaults the reset still had to write
    ASSERT_TRUE(has(CONFIG "/smb.conf", "default smb"));
    ASSERT_TRUE(has(CONFIG "/configs.pak", "the pak"));
    ASSERT_TRUE(has(CONFIG "/migration_state", "28"));
    ASSERT_TRUE(has(CONFIG "/system/ABC123.json", "{\"vol\":5}"));
    ASSERT_FALSE(there(CONFIG "/.ftpState"));
    ASSERT_TRUE(clean_after());
}

TEST(pak_parked_by_the_old_reset_goes_back)
{
    reset_fixture();
    ASSERT_EQ(rename(CONFIG "/configs.pak", RESET_LEGACY_PAK), 0);
    reset_recoverAll(RESET_SD_ROOT, RESET_STAGING_DIR, RESET_LEGACY_PAK, RESET_CONFIGS_PAK);
    ASSERT_TRUE(has(CONFIG "/configs.pak", "the pak"));
    ASSERT_FALSE(there(RESET_LEGACY_PAK));
    // And the next reset works.
    ASSERT_TRUE(reset_replaceDir(&source, ".tmp_update/config", reset_config_keep));
}

TEST(nothing_to_recover_is_a_no_op)
{
    reset_fixture();
    put(RESET_MIGRATION_DIR "/00021_sort_apps_by_name.sh", "echo a >> " TEST_DIR "/ran\n");
    reset_recoverAll(RESET_SD_ROOT, RESET_STAGING_DIR, RESET_LEGACY_PAK, RESET_CONFIGS_PAK);
    ASSERT_TRUE(config_untouched());
    ASSERT_FALSE(there(TEST_DIR "/ran")); // no migration at every start
}

TEST(core_overrides_replaced_whole)
{
    reset_fixture();
    ASSERT_TRUE(reset_replaceDir(&source, "Saves/CurrentProfile/config", NULL));
    ASSERT_TRUE(has(RESET_SD_ROOT "/Saves/CurrentProfile/config/VBA-M/VBA-M.opt", "default opt"));
    ASSERT_FALSE(there(RESET_SD_ROOT "/Saves/CurrentProfile/config/mGBA"));
    ASSERT_FALSE(there(RESET_SD_ROOT "/Saves/CurrentProfile/config" RESET_OLD_SUFFIX));
    ASSERT_FALSE(there(RESET_STAGING_DIR));

    reset_fixture();
    ASSERT_FALSE(reset_replaceDir(&failing, "Saves/CurrentProfile/config", NULL));
    ASSERT_TRUE(has(RESET_SD_ROOT "/Saves/CurrentProfile/config/mGBA/Game.cfg", "per game"));
}

TEST(retroarch_files_renamed_over_the_old_ones)
{
    reset_fixture();
    ASSERT_TRUE(reset_overwriteFiles(&source, "RetroArch"));
    ASSERT_TRUE(has(RESET_SD_ROOT "/RetroArch/.retroarch/retroarch.cfg", "default ra"));
    ASSERT_TRUE(has(RESET_SD_ROOT "/RetroArch/config.xml", "default xml"));
    ASSERT_TRUE(has(RESET_SD_ROOT "/RetroArch/.retroarch/cores/x.so", "core")); // not in the pak: stays
    ASSERT_FALSE(there(RESET_STAGING_DIR));
}

TEST(retroarch_failure_keeps_the_config_whole)
{
    reset_fixture();
    ASSERT_FALSE(reset_overwriteFiles(&failing, "RetroArch"));
    ASSERT_TRUE(has(RESET_SD_ROOT "/RetroArch/.retroarch/retroarch.cfg", "user ra"));
    ASSERT_FALSE(there(RESET_STAGING_DIR));
}

TEST(mainui_file_replaced_not_deleted)
{
    reset_fixture();
    put(TEST_DIR "/res/system.json", "{\"default\":1}");
    put(RESET_SD_ROOT "/system.json", "{\"user\":1}");
    ASSERT_TRUE(reset_copyFileAtomic(TEST_DIR "/res/system.json", RESET_SD_ROOT "/system.json"));
    ASSERT_TRUE(has(RESET_SD_ROOT "/system.json", "{\"default\":1}"));

    // No default to copy: the old code had already deleted system.json.
    put(RESET_SD_ROOT "/system.json", "{\"user\":1}");
    ASSERT_FALSE(reset_copyFileAtomic(TEST_DIR "/res/missing.json", RESET_SD_ROOT "/system.json"));
    ASSERT_TRUE(has(RESET_SD_ROOT "/system.json", "{\"user\":1}"));
}

TEST(default_migrations_run_after_a_reset)
{
    reset_fixture();
    // Stand-ins for the three migrations, plus one that must not run.
    put(RESET_MIGRATION_DIR "/00021_sort_apps_by_name.sh", "echo a >> " TEST_DIR "/ran\n");
    put(RESET_MIGRATION_DIR "/00026_console_pictures_default.sh", "echo b >> " TEST_DIR "/ran\n");
    put(RESET_MIGRATION_DIR "/00028_saved_games_overwrite_default.sh", "echo c >> " TEST_DIR "/ran\n");
    put(RESET_MIGRATION_DIR "/00009_reset_smb_config.sh", "echo X >> " TEST_DIR "/ran\n");
    reset_runDefaultMigrations(RESET_MIGRATION_DIR);
    ASSERT_TRUE(has(TEST_DIR "/ran", "a\nb\nc\n"));

    // Missing scripts are skipped.
    sh("rm -rf " RESET_MIGRATION_DIR);
    reset_runDefaultMigrations(RESET_MIGRATION_DIR);
    ASSERT_TRUE(has(TEST_DIR "/ran", "a\nb\nc\n"));
}

TEST(seven_zip_command_and_result)
{
    // The real extractor against a fake 7z: its arguments, and a failing
    // exit status counts as a failure (system()'s result was ignored).
    reset_fixture();
    put(TEST_DIR "/bin/7z",
        "#!/bin/sh\n"
        "printf '%s|' \"$@\" > " TEST_DIR "/args\n"
        "exit ${FAKE_7Z_EXIT:-0}\n");
    chmod(TEST_DIR "/bin/7z", 0755);
    const char *old_path = getenv("PATH");
    char path[PATH_MAX * 2];
    snprintf(path, sizeof(path), TEST_DIR "/bin:%s", old_path != NULL ? old_path : "/usr/bin:/bin");
    setenv("PATH", path, 1);

    ASSERT_TRUE(reset_extract7z("/x/configs.pak", "/x/staging", ".tmp_update/config/*"));
    ASSERT_TRUE(has(TEST_DIR "/args", "x|-y|-aoa|/x/configs.pak|-o/x/staging|-ir!.tmp_update/config/*|"));

    setenv("FAKE_7Z_EXIT", "2", 1);
    ASSERT_FALSE(reset_extract7z("/x/configs.pak", "/x/staging", ".tmp_update/config/*"));
    unsetenv("FAKE_7Z_EXIT");
    setenv("PATH", old_path != NULL ? old_path : "/usr/bin:/bin", 1);
}

TEST(kept_names)
{
    ASSERT_TRUE(reset_keepMatches("theme-applied-ABCD", reset_config_keep));
    ASSERT_TRUE(reset_keepMatches("configs.pak", reset_config_keep));
    ASSERT_FALSE(reset_keepMatches("theme-applied", reset_config_keep));
    ASSERT_FALSE(reset_keepMatches(".ftpState", reset_config_keep));
    ASSERT_FALSE(reset_keepMatches("configs.pak.bak", reset_config_keep));
    ASSERT_FALSE(reset_keepMatches("x", NULL));
}

int main(void)
{
    printf("\n=== tweaks/reset_core.h ===\n\n");
    RUN_TEST(tweaks_reset_replaces_settings_and_keeps_system_files);
    RUN_TEST(tweaks_reset_twice);
    RUN_TEST(failed_extraction_changes_nothing);
    RUN_TEST(missing_pak_changes_nothing);
    RUN_TEST(pak_without_the_folder_changes_nothing);
    RUN_TEST(cut_between_the_renames_is_undone);
    RUN_TEST(cut_after_the_renames_is_finished);
    RUN_TEST(pak_parked_by_the_old_reset_goes_back);
    RUN_TEST(nothing_to_recover_is_a_no_op);
    RUN_TEST(core_overrides_replaced_whole);
    RUN_TEST(retroarch_files_renamed_over_the_old_ones);
    RUN_TEST(retroarch_failure_keeps_the_config_whole);
    RUN_TEST(mainui_file_replaced_not_deleted);
    RUN_TEST(default_migrations_run_after_a_reset);
    RUN_TEST(seven_zip_command_and_result);
    RUN_TEST(kept_names);
    sh("rm -rf " TEST_DIR);
    TEST_REPORT();
    return test_failures > 0 ? 1 : 0;
}
