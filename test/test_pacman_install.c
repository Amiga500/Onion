/**
 * @file test_pacman_install.c
 * @brief Package Manager: installing a package, and moving through lists
 *
 * pacman_run.h with the real script/pacman_install.sh, on real folders:
 *  - the files are copied and the user's label/imgpath kept, also with
 *    "&", "," or "\" in them (sed mangled them, or failed and lost them);
 *  - a package name with ", $, `, [ or . is passed as it is (it went
 *    through a shell command line, and into a sed pattern);
 *  - a failed copy is reported (the script always ended with 0, and its
 *    result was ignored);
 *  - the package's install.sh runs in its folder; none counts as success.
 * pacman_nav.h:
 *  - lists of fewer than 7 packages: the selection stays on a package and
 *    the first row is never negative (UP at the top read before the table);
 *  - the summary of less than one screen does not scroll.
 *
 * Build and run: make -f Makefile.unit test_pacman_install
 */

#include "onion_test.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define PACMAN_INSTALL_SCRIPT "../src/packageManager/script/pacman_install.sh"
#include "../src/packageManager/pacman_nav.h"
#include "../src/packageManager/pacman_run.h"

#include "utils/file.h"

#define TEST_DIR "/tmp/onion_test_pacman_install"
#define DATA TEST_DIR "/data/Emu"
#define SD TEST_DIR "/sd"

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
    char *got = file_read(path);
    bool same = got != NULL && strcmp(got, text) == 0;
    if (!same)
        printf("    %s is: %s\n", path, got != NULL ? got : "(none)");
    free(got);
    return same;
}

static void reset_dirs(void)
{
    sh("rm -rf " TEST_DIR);
    mkdirs(DATA);
    mkdirs(SD);
    setenv("PACMAN_SDCARD", SD, 1);
}

#define CONFIG_TEXT(label) "{\n\t\"label\": \"" label "\",\n\t\"imgpath\": \"../../Imgs/GBA\"\n}\n"

TEST(installs_and_keeps_the_users_label)
{
    reset_dirs();
    put(DATA "/GBA (gpSP)/Emu/GBA/config.json", CONFIG_TEXT("Game Boy Advance"));
    put(DATA "/GBA (gpSP)/Emu/GBA/launch.sh", "echo run\n");
    // Installed before, with a label of the user's.
    put(SD "/Emu/GBA/config.json", CONFIG_TEXT("GBA & GBC, \\\\mine"));

    ASSERT_TRUE(pacman_runInstall(PACMAN_INSTALL_SCRIPT, DATA, "GBA (gpSP)"));
    ASSERT_TRUE(has(SD "/Emu/GBA/launch.sh", "echo run\n"));
    ASSERT_TRUE(has(SD "/Emu/GBA/config.json", CONFIG_TEXT("GBA & GBC, \\\\mine")));
}

TEST(odd_package_names)
{
    reset_dirs();
    const char *name = "Odd \"$HOME\" `x` [v1.0] (a.b)";
    char path[PATH_MAX];
    snprintf(path, sizeof(path), DATA "/%s/Emu/ODD/config.json", name);
    put(path, CONFIG_TEXT("Default"));
    put(SD "/Emu/ODD/config.json", CONFIG_TEXT("Kept"));
    ASSERT_TRUE(pacman_runInstall(PACMAN_INSTALL_SCRIPT, DATA, name));
    ASSERT_TRUE(has(SD "/Emu/ODD/config.json", CONFIG_TEXT("Kept")));
}

TEST(failed_copy_is_reported)
{
    reset_dirs();
    put(DATA "/Pkg/Emu/X/launch.sh", "x\n");
    put(SD "/Emu", "a file where the folder goes"); // cp can't copy Emu/
    ASSERT_FALSE(pacman_runInstall(PACMAN_INSTALL_SCRIPT, DATA, "Pkg"));

    // Not there at all.
    ASSERT_FALSE(pacman_runInstall(PACMAN_INSTALL_SCRIPT, DATA, "Missing"));
}

TEST(package_scripts)
{
    reset_dirs();
    // Its own install.sh, run from its folder.
    put(SD "/App/Tool/install.sh", "pwd > installed_from\nexit 0\n");
    ASSERT_TRUE(pacman_runPackageScript(SD "/App/Tool", true));
    ASSERT_TRUE(has(SD "/App/Tool/installed_from", SD "/App/Tool\n"));

    // A failing uninstall.sh is reported; none is fine.
    put(SD "/App/Tool/uninstall.sh", "exit 4\n");
    ASSERT_FALSE(pacman_runPackageScript(SD "/App/Tool", false));
    ASSERT_TRUE(pacman_runPackageScript(SD "/App/Other", true));
}

static bool nav_ok(int count, int sel, int pos)
{
    int rows = pacman_visibleRows(count);
    return pos >= 0 && sel >= 0 && sel < (rows > 0 ? rows : 1) &&
           (count == 0 || pos + sel < count) && pos + rows <= (count > 0 ? count : 0);
}

TEST(short_lists)
{
    for (int count = 0; count <= 20; count++) {
        int sel = 0, pos = 0;
        // UP at the top wraps to the last package.
        pacman_up(count, true, &sel, &pos);
        ASSERT_TRUE(nav_ok(count, sel, pos));
        if (count > 0)
            ASSERT_EQ(pos + sel, count - 1);
        // Every move keeps the selection on a package.
        for (int step = 0; step < 50; step++) {
            switch (step % 5) {
            case 0:
                pacman_down(count, step % 3 == 0, &sel, &pos);
                break;
            case 1:
                pacman_up(count, step % 2 == 0, &sel, &pos);
                break;
            case 2:
                pacman_pageDown(count, &sel, &pos);
                break;
            case 3:
                pacman_down(count, false, &sel, &pos);
                break;
            default:
                pacman_pageUp(&sel, &pos);
            }
            ASSERT_TRUE(nav_ok(count, sel, pos));
        }
    }

    // 3 packages: DOWN stops on the last, then wraps on a new press.
    int sel = 0, pos = 0;
    pacman_down(3, false, &sel, &pos);
    pacman_down(3, false, &sel, &pos);
    pacman_down(3, false, &sel, &pos);
    ASSERT_EQ(sel, 2);
    pacman_down(3, true, &sel, &pos);
    ASSERT_EQ(sel, 0);
}

TEST(long_list_unchanged)
{
    // 55 packages (Verified): as before.
    int sel = 0, pos = 0;
    pacman_up(55, true, &sel, &pos);
    ASSERT_EQ(sel, 6);
    ASSERT_EQ(pos, 48);
    pacman_down(55, true, &sel, &pos);
    ASSERT_EQ(sel, 0);
    ASSERT_EQ(pos, 0);
    pacman_pageDown(55, &sel, &pos);
    ASSERT_EQ(pos, 7);
}

TEST(summary_scroll)
{
    int y = 0;
    ASSERT_FALSE(pacman_scrollBy(&y, 28, 100, 323)); // shorter than the screen
    ASSERT_EQ(y, 0);
    ASSERT_FALSE(pacman_scrollBy(&y, -28, 100, 323));
    ASSERT_EQ(y, 0);

    ASSERT_TRUE(pacman_scrollBy(&y, 323, 1000, 323));
    ASSERT_EQ(y, 323);
    ASSERT_TRUE(pacman_scrollBy(&y, 1000, 1000, 323));
    ASSERT_EQ(y, 677);
    ASSERT_FALSE(pacman_scrollBy(&y, 28, 1000, 323));
    ASSERT_TRUE(pacman_scrollBy(&y, -1000, 1000, 323));
    ASSERT_EQ(y, 0);
}

int main(void)
{
    printf("\n=== packageManager: install, lists ===\n\n");
    RUN_TEST(installs_and_keeps_the_users_label);
    RUN_TEST(odd_package_names);
    RUN_TEST(failed_copy_is_reported);
    RUN_TEST(package_scripts);
    RUN_TEST(short_lists);
    RUN_TEST(long_list_unchanged);
    RUN_TEST(summary_scroll);
    sh("rm -rf " TEST_DIR);
    TEST_REPORT();
    return test_failures > 0 ? 1 : 0;
}
