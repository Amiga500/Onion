/**
 * @file test_tweaks_reset_keep.c
 * @brief Tweaks > Reset system tweaks: what is kept in .tmp_update/config,
 *        and the default migrations run again (src/tweaks/reset_keep.h)
 *
 * On a real folder:
 *  - the settings go, what is not one (installer state, update channel and
 *    the testers file, Syncthing, the applied theme and icons, per-device
 *    MainUI settings, cores chosen per console, AudioTune) stays, folders
 *    included;
 *  - every "*_default.sh" migration that exists is run again, and each one
 *    listed exists;
 *  - the command, run by sh on the real migration scripts, writes the
 *    defaults back: .appsort, console-view.json, game-menu.json and
 *    .gameInfoAutoScroll (Game Info auto scroll stayed off after a reset).
 *
 * Build and run: make -f Makefile.unit test_tweaks_reset_keep
 */

#include "onion_test.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../src/tweaks/reset_keep.h"

#define MIGRATION_DIR "../static/build/.tmp_update/script/migration"

static char tmp_root[] = "/tmp/onion_test_reset_keep_XXXXXX";

static void put(const char *rel, const char *content)
{
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/%s", tmp_root, rel);
    char *slash = strrchr(path, '/');
    *slash = '\0';
    char cmd[PATH_MAX + 16];
    snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", path);
    if (system(cmd) != 0)
        return;
    *slash = '/';
    FILE *f = fopen(path, "w");
    if (f != NULL) {
        fputs(content, f);
        fclose(f);
    }
}

static bool there(const char *rel)
{
    char path[PATH_MAX];
    struct stat st;
    snprintf(path, sizeof(path), "%s/%s", tmp_root, rel);
    return lstat(path, &st) == 0;
}

static bool listed(const char *const *list, const char *name)
{
    for (; *list != NULL; list++)
        if (strcmp(*list, name) == 0)
            return true;
    return false;
}

TEST(settings_go_the_rest_stays)
{
    static const char *const kept[] = {
        "config/configs.pak", "config/migration_state", "config/netplay_cores.conf",
        "config/hotspot_wpa_supplicant.conf", "config/syncthing/config.xml",
        "config/ota_channel", "config/.otaTesters", "config/system/0123456789ab.json",
        "config/active_theme", "config/active_icon_pack", "config/theme-applied-0123456789ab",
        "config/.systemSkin", "config/system_cores/FC", "config/audiotune.ini", NULL};
    static const char *const gone[] = {
        "config/.logging", "config/smb.conf", "config/display", "config/battery/levels",
        "config/.gameInfoAutoScroll", "config/game-menu.json", "config/console-view.json",
        "config/.appsort", "config/gameSwitcher/showTime", "config/active_theme.old",
        "config/systemX", "config/theme-applied", NULL};

    for (const char *const *k = kept; *k != NULL; k++)
        put(*k, "x\n");
    for (const char *const *g = gone; *g != NULL; g++)
        put(*g, "x\n");

    char dir[PATH_MAX];
    snprintf(dir, sizeof(dir), "%s/config", tmp_root);
    ASSERT_TRUE(reset_clearConfigDir(dir));

    for (const char *const *k = kept; *k != NULL; k++) {
        if (!there(*k))
            printf("    lost: %s\n", *k);
        ASSERT_TRUE(there(*k));
    }
    for (const char *const *g = gone; *g != NULL; g++) {
        if (there(*g))
            printf("    left: %s\n", *g);
        ASSERT_FALSE(there(*g));
    }
    ASSERT_TRUE(there("config/battery") == false);
    ASSERT_TRUE(there("config/gameSwitcher") == false);
}

TEST(no_folder)
{
    ASSERT_FALSE(reset_clearConfigDir("/tmp/onion_test_reset_keep_no_such_folder"));
}

TEST(every_default_migration_runs_again)
{
    DIR *dp = opendir(MIGRATION_DIR);
    ASSERT_NOT_NULL(dp);
    if (dp == NULL)
        return;
    int defaults = 0;
    struct dirent *ep;
    while ((ep = readdir(dp)) != NULL) {
        size_t len = strlen(ep->d_name);
        const char *suffix = "_default.sh";
        if (len > strlen(suffix) && strcmp(ep->d_name + len - strlen(suffix), suffix) == 0) {
            defaults++;
            if (!listed(reset_default_migrations, ep->d_name))
                printf("    not run again: %s\n", ep->d_name);
            ASSERT_TRUE(listed(reset_default_migrations, ep->d_name));
        }
    }
    closedir(dp);
    ASSERT_GE(defaults, 3);

    for (const char *const *m = reset_default_migrations; *m != NULL; m++) {
        char path[PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s", MIGRATION_DIR, *m);
        ASSERT_EQ(access(path, R_OK), 0);
    }
}

TEST(command_writes_the_defaults_back)
{
    char cmd[512];
    ASSERT_TRUE(reset_migrationCommand(cmd, sizeof(cmd), MIGRATION_DIR));
    for (const char *const *m = reset_default_migrations; *m != NULL; m++)
        ASSERT_NOT_NULL(strstr(cmd, *m));

    char small[40];
    ASSERT_FALSE(reset_migrationCommand(small, sizeof(small), MIGRATION_DIR));

    /* The real scripts, pointed at the test folder. */
    char cfg[PATH_MAX], env[PATH_MAX + 64];
    snprintf(cfg, sizeof(cfg), "%s/run/config", tmp_root);
    snprintf(env, sizeof(env), "%s/run", tmp_root);
    setenv("sysdir", env, 1);
    snprintf(env, sizeof(env), "%s/console-view.json", cfg);
    setenv("CONSOLE_VIEW_CONFIG", env, 1);
    snprintf(env, sizeof(env), "%s/run/consoles", tmp_root);
    setenv("CONSOLE_PICTURES_DEST", env, 1);
    snprintf(env, sizeof(env), "%s/game-menu.json", cfg);
    setenv("GAME_MENU_CONFIG", env, 1);
    snprintf(env, sizeof(env), "%s/.gameInfoAutoScroll", cfg);
    setenv("GAME_INFO_SCROLL_CONFIG", env, 1);
    put("run/consoles/_default.png", "png");
    put("run/config/configs.pak", "pak");

    ASSERT_EQ(system(cmd), 0);
    ASSERT_TRUE(there("run/config/.appsort"));
    ASSERT_TRUE(there("run/config/console-view.json"));
    ASSERT_TRUE(there("run/config/game-menu.json"));
    ASSERT_TRUE(there("run/config/.gameInfoAutoScroll"));

    /* A missing folder runs nothing. */
    ASSERT_TRUE(reset_migrationCommand(cmd, sizeof(cmd), "/tmp/onion_test_reset_keep_no_such_folder"));
    snprintf(env, sizeof(env), "%s/never/.gameInfoAutoScroll", tmp_root);
    setenv("GAME_INFO_SCROLL_CONFIG", env, 1);
    ASSERT_NE(system(cmd), 0);
    ASSERT_FALSE(there("never/.gameInfoAutoScroll"));
}

int main(void)
{
    if (mkdtemp(tmp_root) == NULL) {
        perror("mkdtemp");
        return 1;
    }
    printf("\n=== Tweaks: Reset system tweaks keeps ===\n\n");
    RUN_TEST(settings_go_the_rest_stays);
    RUN_TEST(no_folder);
    RUN_TEST(every_default_migration_runs_again);
    RUN_TEST(command_writes_the_defaults_back);

    char cmd[PATH_MAX + 16];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s'", tmp_root);
    if (system(cmd) != 0)
        printf("could not remove %s\n", tmp_root);
    TEST_REPORT();
    return test_failures == 0 ? 0 : 1;
}
